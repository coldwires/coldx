#include "watermark.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numeric>

#include <SDL3/SDL.h>

#include "zlib_codec.h"

namespace coldx::content::detail {

namespace {

// Bits: a 40-bit fingerprint, then a 40-bit check. The check is long because the reader
// tries thousands of alignments, and flat colours read the same at every copy, so the
// check is the only thing between a chance reading and a false claim.
constexpr std::size_t PayloadSize = 80;
constexpr std::size_t FingerprintBits = 40;
constexpr std::size_t CheckBits = PayloadSize - FingerprintBits;
constexpr std::uint64_t FingerprintMask = (std::uint64_t{1} << FingerprintBits) - 1;
// Pixels: every Tile x Tile window holds the payload. A prime, so sprite sheets on 8, 16, 32
// or 64 pixel grids never line up with it: each sprite lands on different places, and more
// sprites bring more evidence rather than the same places again.
constexpr std::size_t Tile = 37;
constexpr std::size_t Block = 2048;      // samples: every Block samples hold the payload

// Pseudo-random bits from the secret, the same every time, one stream per purpose, so the
// marks look like noise and their places cannot be known without the secret.
class Stream {
public:
    Stream(std::uint64_t secret, const char* purpose) : secret_(secret), purpose_(purpose) {}
    std::uint64_t Word(std::uint64_t i) {
        if (i != index_) {
            word_ = rime::HashName(secret_ ^ ((i + 1) * 0x9E3779B97F4A7C15ull), purpose_);
            index_ = i;
        }
        return word_;
    }
    std::uint8_t Byte(std::uint64_t i) { return static_cast<std::uint8_t>(Word(i / 8) >> (8 * (i % 8))); }
    int Bit(std::uint64_t i) { return (Byte(i / 8) >> (i % 8)) & 1; }

private:
    std::uint64_t secret_;
    const char* purpose_;
    std::uint64_t index_ = ~std::uint64_t{0};
    std::uint64_t word_ = 0;
};

std::uint64_t Mix(std::uint64_t h) {
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ull;
    h ^= h >> 33;
    return h;
}

std::uint64_t Check(std::uint64_t secret, std::uint64_t fingerprint) {
    std::string bytes(8, '\0');
    for (int i = 0; i < 8; ++i) bytes[static_cast<size_t>(i)] = static_cast<char>(fingerprint >> (8 * i));
    return rime::HashName(secret, "check" + bytes) & ((std::uint64_t{1} << CheckBits) - 1);
}

using Payload = std::array<std::uint8_t, PayloadSize>;

// Each layer carries these bits, whitened so they look random.
Payload PayloadOf(std::uint64_t secret, std::uint64_t fingerprint) {
    Stream white(secret, "payload");
    fingerprint &= FingerprintMask;
    const std::uint64_t check = Check(secret, fingerprint);
    Payload bits{};
    for (std::size_t i = 0; i < FingerprintBits; ++i) bits[i] = static_cast<std::uint8_t>(((fingerprint >> i) & 1) ^ white.Bit(i));
    for (std::size_t i = 0; i < CheckBits; ++i)
        bits[FingerprintBits + i] = static_cast<std::uint8_t>(((check >> i) & 1) ^ white.Bit(FingerprintBits + i));
    return bits;
}

std::array<std::uint8_t, PayloadSize / 8> BytesOf(const Payload& bits) {
    std::array<std::uint8_t, PayloadSize / 8> bytes{};
    for (std::size_t i = 0; i < PayloadSize; ++i) bytes[i / 8] = static_cast<std::uint8_t>(bytes[i / 8] | (bits[i] << (i % 8)));
    return bytes;
}

// Votes for each payload bit across every copy a layer holds, then the majority.
class Tally {
public:
    void Add(std::size_t index, int bit) {
        ones_[index % PayloadSize] += static_cast<std::size_t>(bit & 1);
        seen_[index % PayloadSize] += 1;
    }

    // A layer whose alignment was searched for asks for several copies of every bit and
    // near-total agreement; the 40-bit check is what makes a chance pass negligible.
    Finding Decide(const char* layer, std::uint64_t secret, std::size_t minCopies = 1, double minAgreement = 0.0) const {
        Finding f;
        f.layer = layer;
        for (std::size_t i = 0; i < PayloadSize; ++i) f.total += seen_[i];
        Payload bits{};
        for (std::size_t i = 0; i < PayloadSize; ++i) {
            if (seen_[i] < minCopies || seen_[i] == 0) return f;
            bits[i] = ones_[i] * 2 > seen_[i] ? 1 : 0;
            f.agree += bits[i] ? ones_[i] : seen_[i] - ones_[i];
        }
        Stream white(secret, "payload");
        std::uint64_t fingerprint = 0, check = 0;
        for (std::size_t i = 0; i < FingerprintBits; ++i) fingerprint |= std::uint64_t(bits[i] ^ white.Bit(i)) << i;
        for (std::size_t i = 0; i < CheckBits; ++i) check |= std::uint64_t(bits[FingerprintBits + i] ^ white.Bit(FingerprintBits + i)) << i;
        f.fingerprint = fingerprint;
        f.found = Check(secret, fingerprint) == check && static_cast<double>(f.agree) >= minAgreement * static_cast<double>(f.total);
        return f;
    }

private:
    std::array<std::size_t, PayloadSize> ones_{}, seen_{};
};

// The places one period (a tile of pixels, a block of samples) marks, chosen by a shuffle
// keyed with the secret: place i carries payload bit i % 80, so 240 places hold three copies.
// Every period uses the same places, which is what lets a crop or a trimmed clip be read:
// any window one period long holds each place exactly once, wherever it starts. Each place
// also has its own whitening bit and (pixels) channel.
class Layout {
public:
    Layout(std::size_t period, std::size_t count, std::uint64_t secret, const char* purpose)
        : slot_(period, -1), place_(count), noise_(count), channel_(count) {
        std::vector<std::size_t> order(period);
        std::iota(order.begin(), order.end(), std::size_t{0});
        Stream s(secret, purpose);
        const std::string extra = std::string(purpose) + " whitening";
        Stream n(secret, extra.c_str());
        for (std::size_t i = 0; i < count; ++i) {  // the first steps of a Fisher-Yates shuffle
            std::swap(order[i], order[i + s.Word(i) % (period - i)]);
            place_[i] = order[i];
            slot_[order[i]] = static_cast<int>(i);
            noise_[i] = static_cast<std::uint8_t>(n.Bit(i));
            channel_[i] = static_cast<std::uint8_t>(n.Byte(1024 + i) % 3);
        }
    }
    std::size_t Count() const { return place_.size(); }
    int Slot(std::size_t inPeriod) const { return slot_[inPeriod]; }
    std::size_t Place(std::size_t i) const { return place_[i]; }
    int Noise(std::size_t i) const { return noise_[i]; }
    std::size_t Channel(std::size_t i) const { return channel_[i]; }

private:
    std::vector<int> slot_;
    std::vector<std::size_t> place_;
    std::vector<std::uint8_t> noise_, channel_;
};

// One pixel layout per mode: alpha marks three times as many places (240 of a tile's 1369),
// as alpha 254 is invisible to eyes and to colour tools alike, and pixel art has few opaque
// pixels to spare: every place is evidence a single sprite needs.
Layout PixelLayout(PixelMark mode, std::uint64_t secret) {
    return mode == PixelMark::Rgb ? Layout(Tile * Tile, PayloadSize, secret, "rgb places")
                                  : Layout(Tile * Tile, 3 * PayloadSize, secret, "alpha places");
}
Layout SampleLayout(std::uint64_t secret) {
    return Layout(Block, PayloadSize, secret, "sample places");
}

// log10 of P(X >= k) for X ~ Binomial(n, 1/2): how likely agreeing this well is by chance.
double ChanceLog10(std::size_t k, std::size_t n) {
    if (k == 0) return 0.0;
    const double ln2 = std::log(2.0);
    double sum = 0.0, top = -INFINITY;
    std::vector<double> terms;
    for (std::size_t j = k; j <= n; ++j) {
        const double t = std::lgamma(n + 1.0) - std::lgamma(j + 1.0) - std::lgamma(n - j + 1.0) - n * ln2;
        terms.push_back(t);
        top = std::max(top, t);
    }
    for (double t : terms) sum += std::exp(t - top);
    return (top + std::log(sum)) / std::log(10.0);
}

std::uint32_t Get32BE(const std::uint8_t* p) {
    return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) | p[3];
}
void Put32BE(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
std::uint32_t Get32LE(const std::uint8_t* p) {
    return std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8) | (std::uint32_t{p[2]} << 16) | (std::uint32_t{p[3]} << 24);
}
std::uint16_t Get16LE(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

// ---- PNG ----

const std::uint8_t PngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

// A chunk name from the secret: ancillary (lower), private (lower), reserved bit clear
// (upper), safe to copy (lower). Never one PNG defines, whose second letter is upper case.
std::string ChunkName(std::uint64_t secret) {
    Stream s(secret, "png chunk name");
    std::string name(4, 'a');
    for (int i = 0; i < 4; ++i) name[static_cast<size_t>(i)] = static_cast<char>((i == 2 ? 'A' : 'a') + s.Byte(static_cast<std::uint64_t>(i)) % 26);
    return name;
}

struct PngFile {
    std::uint32_t width = 0, height = 0;
    std::uint8_t depth = 0, color = 0, interlace = 0;
    std::vector<std::uint8_t> idat;
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> chunks;
};

bool ParsePng(const std::vector<std::uint8_t>& b, PngFile& png) {
    if (b.size() < 8 || std::memcmp(b.data(), PngSignature, 8) != 0) return false;
    for (std::size_t at = 8; at + 12 <= b.size();) {
        const std::uint32_t length = Get32BE(&b[at]);
        if (length > b.size() - at - 12) return false;
        const std::string type(reinterpret_cast<const char*>(&b[at + 4]), 4);
        const std::uint8_t* data = &b[at + 8];
        if (type == "IHDR" && length >= 13) {
            png.width = Get32BE(data);
            png.height = Get32BE(data + 4);
            png.depth = data[8];
            png.color = data[9];
            png.interlace = data[12];
        } else if (type == "IDAT") {
            png.idat.insert(png.idat.end(), data, data + length);
        } else if (type == "IEND") {
            break;
        }
        png.chunks.push_back({type, std::vector<std::uint8_t>(data, data + length)});
        at += 12 + length;
    }
    return png.width > 0 && png.height > 0;
}

void PutChunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& data) {
    Put32BE(out, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> crcInput(type, type + 4);
    crcInput.insert(crcInput.end(), data.begin(), data.end());
    out.insert(out.end(), crcInput.begin(), crcInput.end());
    Put32BE(out, rime::Crc32(crcInput.data(), crcInput.size()));
}

int Paeth(int a, int b, int c) {
    const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return pa <= pb && pa <= pc ? a : (pb <= pc ? b : c);
}

// Decodes any PNG SDL reads to straight-alpha RGBA, rows packed.
bool DecodeRgba(const std::vector<std::uint8_t>& png, std::vector<std::uint8_t>& rgba, int& w, int& h) {
    SDL_IOStream* io = SDL_IOFromConstMem(png.data(), png.size());
    SDL_Surface* loaded = io ? SDL_LoadPNG_IO(io, true) : nullptr;
    SDL_Surface* s = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32) : nullptr;
    SDL_DestroySurface(loaded);
    if (!s) return false;
    w = s->w;
    h = s->h;
    rgba.resize(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y)
        std::memcpy(&rgba[static_cast<size_t>(y) * w * 4], static_cast<const std::uint8_t*>(s->pixels) + static_cast<size_t>(y) * s->pitch,
                    static_cast<size_t>(w) * 4);
    SDL_DestroySurface(s);
    return true;
}

// Both tests ignore the bit being written, so marking never changes which pixels qualify.
bool Candidate(std::uint8_t alpha, PixelMark mode) {
    return mode == PixelMark::Rgb ? alpha > 0 : (alpha & 0xFE) == 254;
}

// The transparent-pixel layer: each fully transparent pixel describes itself, so it reads
// from any crop without alignment. R names which payload byte it holds, G holds it, B is
// an 8-bit check of both; all three whitened.
struct ClearCode {
    std::uint8_t r = 0, g = 0, b = 0;
    std::array<std::array<std::uint8_t, 256>, PayloadSize / 8> check{};
    explicit ClearCode(std::uint64_t secret) {
        Stream s(secret, "transparent");
        r = s.Byte(0);
        g = s.Byte(1);
        b = s.Byte(2);
        for (std::size_t i = 0; i < check.size(); ++i)
            for (int v = 0; v < 256; ++v) check[i][static_cast<size_t>(v)] = static_cast<std::uint8_t>(Mix(secret ^ (i << 8) ^ static_cast<std::uint64_t>(v)));
    }
};

} // namespace

namespace {

bool Loud(const std::vector<std::uint8_t>& b, std::size_t at);

// Calls f(place, bit) for every marked place in the image, with the tiles aligned at
// (ox, oy): the bit read, unwhitened, carrying payload bit place % 80.
template <class F>
void VisitPixels(const std::vector<std::uint8_t>& rgba, int w, int h, PixelMark mode, const Layout& layout, std::size_t ox,
                 std::size_t oy, F&& f) {
    for (std::size_t i = 0; i < layout.Count(); ++i) {
        const std::size_t place = layout.Place(i);
        const std::size_t sx = (place % Tile + Tile - ox) % Tile, sy = (place / Tile + Tile - oy) % Tile;
        for (std::size_t y = sy; y < static_cast<size_t>(h); y += Tile) {
            for (std::size_t x = sx; x < static_cast<size_t>(w); x += Tile) {
                const std::size_t p = y * static_cast<size_t>(w) + x;
                if (!Candidate(rgba[p * 4 + 3], mode)) continue;
                const std::uint8_t v = mode == PixelMark::Rgb ? rgba[p * 4 + layout.Channel(i)] : rgba[p * 4 + 3];
                f(i, (v & 1) ^ layout.Noise(i));
            }
        }
    }
}

template <class F>
void VisitSamples(const std::vector<std::uint8_t>& wav, std::size_t data, std::size_t samples, const Layout& layout,
                  std::size_t offset, F&& f) {
    for (std::size_t i = 0; i < layout.Count(); ++i) {
        for (std::size_t s = (layout.Place(i) + Block - offset) % Block; s < samples; s += Block) {
            const std::size_t at = data + s * 2;
            if (Loud(wav, at)) f(i, (wav[at] & 1) ^ layout.Noise(i));
        }
    }
}

// The best agreement with each fingerprint over every alignment, and its chance.
template <class Visit>
std::vector<Match> MatchAll(const char* layer, std::size_t alignments, std::size_t modes, std::uint64_t secret,
                            const std::vector<std::uint64_t>& fingerprints, Visit&& visit) {
    std::vector<Match> out;
    const double tries = std::log10(static_cast<double>(alignments * modes * std::max<std::size_t>(fingerprints.size(), 1)));
    for (std::uint64_t fp : fingerprints) {
        const Payload expect = PayloadOf(secret, fp);
        Match best;
        best.layer = layer;
        best.fingerprint = fp & FingerprintMask;
        double bestLog = 0.0;
        // Evidence is counted per distinct place, not per read: the copies of one place in
        // different tiles of a flat colour read alike whatever is there, so they are one
        // coin flip, not many.
        std::vector<std::size_t> yes, seen;
        for (std::size_t a = 0; a < alignments; ++a) {
            yes.assign(512, 0);
            seen.assign(512, 0);
            visit(a, [&](std::size_t i, int bit) {
                seen[i] += 1;
                yes[i] += bit == expect[i % PayloadSize] ? 1 : 0;
            });
            std::size_t agree = 0, total = 0;
            for (std::size_t i = 0; i < seen.size(); ++i) {
                if (seen[i] == 0) continue;
                ++total;
                agree += yes[i] * 2 > seen[i] ? 1 : 0;
            }
            const double log = ChanceLog10(agree, total);
            if (total > 0 && (best.total == 0 || log < bestLog)) {
                bestLog = log;
                best.agree = agree;
                best.total = total;
            }
        }
        best.chance = std::pow(10.0, std::min(0.0, bestLog + tries));
        best.matched = best.total > 0 && bestLog + tries < -6.0 && best.agree * 10 >= best.total * 9;
        out.push_back(best);
    }
    return out;
}

} // namespace

std::uint64_t OwnerMark(std::uint64_t secret) {
    return rime::HashName(secret, "owner mark") & FingerprintMask;
}

std::uint64_t WatermarkSecret(std::uint64_t seed, const rime::Key* key) {
    std::uint64_t secret = rime::HashName(seed, "watermark");
    if (key) secret = rime::HashName(secret, std::string(key->begin(), key->end()));
    return secret;
}

bool MarkPng(const std::vector<std::uint8_t>& png, std::uint64_t secret, std::uint64_t fingerprint, PixelMark mode,
             std::vector<std::uint8_t>& out, std::string& error) {
    std::vector<std::uint8_t> rgba;
    int w = 0, h = 0;
    if (!DecodeRgba(png, rgba, w, h)) {
        error = std::string("PNG does not decode: ") + SDL_GetError();
        return false;
    }
    const Payload bits = PayloadOf(secret, fingerprint);
    const auto bytes = BytesOf(bits);

    // Pixels: in every tile, the lowest bit of a channel (or alpha 254/255) at the places.
    const Layout layout = PixelLayout(mode, secret);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int slot = layout.Slot(static_cast<size_t>(y) % Tile * Tile + static_cast<size_t>(x) % Tile);
            const std::size_t p = static_cast<size_t>(y) * w + static_cast<size_t>(x);
            if (slot < 0 || !Candidate(rgba[p * 4 + 3], mode)) continue;
            const auto i = static_cast<std::size_t>(slot);
            std::uint8_t& v = mode == PixelMark::Rgb ? rgba[p * 4 + layout.Channel(i)] : rgba[p * 4 + 3];
            v = static_cast<std::uint8_t>((v & 0xFE) | (bits[i % PayloadSize] ^ layout.Noise(i)));
        }
    }
    // Fully transparent pixels: their colour, which nothing ever shows.
    const ClearCode clear(secret);
    Stream pick(secret, "transparent pick");
    for (std::size_t p = 0, q = 0; p < rgba.size() / 4; ++p) {
        if (rgba[p * 4 + 3] != 0) continue;
        const std::size_t index = q++ % bytes.size();
        const std::uint8_t r = static_cast<std::uint8_t>(index * 25 + pick.Byte(p) % 25);
        rgba[p * 4] = static_cast<std::uint8_t>(r ^ clear.r);
        rgba[p * 4 + 1] = static_cast<std::uint8_t>(bytes[index] ^ clear.g);
        rgba[p * 4 + 2] = static_cast<std::uint8_t>(clear.check[index][bytes[index]] ^ clear.b);
    }

    // Rows: filtered with filter 1 to 4 as the payload says.
    const std::size_t stride = static_cast<std::size_t>(w) * 4;
    std::vector<std::uint8_t> raw;
    raw.reserve((stride + 1) * static_cast<size_t>(h));
    Stream rows(secret, "row filters");
    std::vector<std::uint8_t> zero(stride, 0);
    for (int y = 0; y < h; ++y) {
        const std::size_t i = static_cast<std::size_t>(y) * 2;
        const int symbol = (bits[i % PayloadSize] ^ rows.Bit(i)) | ((bits[(i + 1) % PayloadSize] ^ rows.Bit(i + 1)) << 1);
        const int filter = 1 + symbol;
        raw.push_back(static_cast<std::uint8_t>(filter));
        const std::uint8_t* cur = &rgba[static_cast<size_t>(y) * stride];
        const std::uint8_t* prev = y > 0 ? cur - stride : zero.data();
        for (std::size_t x = 0; x < stride; ++x) {
            const int a = x >= 4 ? cur[x - 4] : 0, b = prev[x], c = x >= 4 ? prev[x - 4] : 0;
            const int predicted = filter == 1 ? a : filter == 2 ? b : filter == 3 ? (a + b) / 2 : Paeth(a, b, c);
            raw.push_back(static_cast<std::uint8_t>(cur[x] - predicted));
        }
    }

    out.assign(std::begin(PngSignature), std::end(PngSignature));
    std::vector<std::uint8_t> header;
    Put32BE(header, static_cast<std::uint32_t>(w));
    Put32BE(header, static_cast<std::uint32_t>(h));
    header.insert(header.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA, no interlace
    PutChunk(out, "IHDR", header);
    // The private chunk: the payload and filler, all looking random.
    Stream chunkBytes(secret, "png chunk");
    std::vector<std::uint8_t> body(16);
    for (std::size_t i = 0; i < body.size(); ++i)
        body[i] = static_cast<std::uint8_t>((i < bytes.size() ? bytes[i] : 0) ^ chunkBytes.Byte(i));
    PutChunk(out, ChunkName(secret).c_str(), body);
    PutChunk(out, "IDAT", Deflate(raw));
    PutChunk(out, "IEND", {});
    return true;
}

std::vector<Finding> ReadPng(const std::vector<std::uint8_t>& png, std::uint64_t secret) {
    std::vector<Finding> found;
    PngFile file;
    if (!ParsePng(png, file)) return found;

    {   // chunk
        Tally t;
        const std::string name = ChunkName(secret);
        Stream chunkBytes(secret, "png chunk");
        for (const auto& [type, data] : file.chunks) {
            if (type != name || data.size() < 10) continue;
            for (std::size_t i = 0; i < 10; ++i)
                for (int b = 0; b < 8; ++b) t.Add(i * 8 + static_cast<size_t>(b), ((data[i] ^ chunkBytes.Byte(i)) >> b) & 1);
        }
        found.push_back(t.Decide("chunk", secret));
    }
    {   // rows
        Tally t;
        static const int channelsOf[7] = {1, 0, 3, 1, 2, 0, 4};
        std::vector<std::uint8_t> raw;
        if (file.interlace == 0 && file.color <= 6 && channelsOf[file.color] > 0 && Inflate(file.idat, raw)) {
            const std::size_t stride = 1 + (static_cast<std::size_t>(file.width) * channelsOf[file.color] * file.depth + 7) / 8;
            Stream rows(secret, "row filters");
            for (std::size_t y = 0; y < file.height && (y + 1) * stride <= raw.size(); ++y) {
                const int filter = raw[y * stride];
                if (filter < 1 || filter > 4) continue;
                t.Add(2 * y, ((filter - 1) & 1) ^ rows.Bit(2 * y));
                t.Add(2 * y + 1, ((filter - 1) >> 1) ^ rows.Bit(2 * y + 1));
            }
        }
        found.push_back(t.Decide("row filters", secret));
    }
    std::vector<std::uint8_t> rgba;
    int w = 0, h = 0;
    if (!DecodeRgba(png, rgba, w, h)) return found;
    {   // transparent pixels: each checks itself, wherever it is
        Tally t;
        const ClearCode clear(secret);
        for (std::size_t p = 0; p < rgba.size() / 4; ++p) {
            if (rgba[p * 4 + 3] != 0) continue;
            const int r = rgba[p * 4] ^ clear.r;
            if (r >= 250) continue;
            const std::size_t index = static_cast<std::size_t>(r) / 25;
            const std::uint8_t value = static_cast<std::uint8_t>(rgba[p * 4 + 1] ^ clear.g);
            if ((rgba[p * 4 + 2] ^ clear.b) != clear.check[index][value]) continue;
            for (int b = 0; b < 8; ++b) t.Add(index * 8 + static_cast<size_t>(b), (value >> b) & 1);
        }
        found.push_back(t.Decide("transparent pixels", secret, 1, 0.9));
    }
    // Pixel bits: the image may be a crop, so every alignment of the tiles is tried; the
    // one that decodes with several copies and near-total agreement wins.
    for (PixelMark mode : {PixelMark::Rgb, PixelMark::Alpha}) {
        const Layout layout = PixelLayout(mode, secret);
        Finding best;
        best.layer = mode == PixelMark::Rgb ? "pixel bits (rgb)" : "pixel bits (alpha)";
        for (std::size_t oy = 0; oy < Tile && !(best.found && best.agree == best.total); ++oy) {
            for (std::size_t ox = 0; ox < Tile; ++ox) {
                Tally t;
                VisitPixels(rgba, w, h, mode, layout, ox, oy, [&](std::size_t i, int bit) { t.Add(i, bit); });
                Finding f = t.Decide(best.layer.c_str(), secret, 2, 0.9);
                const bool better = f.found && (!best.found || f.agree * best.total > best.agree * f.total);
                if (better || (!best.found && f.total > best.total)) best = f;
                if (best.found && best.agree == best.total) break;
            }
        }
        found.push_back(best);
    }
    return found;
}

// ---- WAV ----

namespace {

struct WavFile {
    bool pcm16 = false;
    std::size_t data = 0, size = 0;
};

bool ParseWav(const std::vector<std::uint8_t>& b, WavFile& wav) {
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0) return false;
    bool pcm = false;
    int bits = 0;
    for (std::size_t at = 12; at + 8 <= b.size();) {
        const std::uint32_t size = Get32LE(&b[at + 4]);
        const std::size_t body = at + 8, available = std::min<std::size_t>(size, b.size() - body);
        if (std::memcmp(&b[at], "fmt ", 4) == 0 && available >= 16) {
            const std::uint16_t format = Get16LE(&b[body]);
            bits = Get16LE(&b[body + 14]);
            pcm = format == 1 || (format == 0xFFFE && available >= 26 && Get16LE(&b[body + 24]) == 1);
        } else if (std::memcmp(&b[at], "data", 4) == 0) {
            wav.data = body;
            wav.size = available & ~std::size_t{1};
        }
        at = body + size + (size & 1);
    }
    wav.pcm16 = pcm && bits == 16 && wav.size > 0;
    return wav.data > 0;
}

std::string WavChunkName(std::uint64_t secret) {
    Stream s(secret, "wav chunk name");
    std::string name(4, 'a');
    for (int i = 0; i < 4; ++i) name[static_cast<size_t>(i)] = static_cast<char>('a' + s.Byte(static_cast<std::uint64_t>(i)) % 26);
    return name;
}

// Ignoring the bit being written, as with pixels; quiet samples stay untouched, so silence
// stays exactly silent.
bool Loud(const std::vector<std::uint8_t>& b, std::size_t at) {
    return std::abs(static_cast<int>(static_cast<std::int16_t>(Get16LE(&b[at]) & 0xFFFE))) >= 64;
}

} // namespace

bool MarkWav(const std::vector<std::uint8_t>& wav, std::uint64_t secret, std::uint64_t fingerprint, std::vector<std::uint8_t>& out,
             std::string& error) {
    WavFile file;
    if (!ParseWav(wav, file)) {
        error = "not a WAV file with a data chunk";
        return false;
    }
    out = wav;
    const Payload bits = PayloadOf(secret, fingerprint);
    if (file.pcm16) {
        const Layout layout = SampleLayout(secret);
        for (std::size_t s = 0; s < file.size / 2; ++s) {
            const int slot = layout.Slot(s % Block);
            const std::size_t at = file.data + s * 2;
            if (slot < 0 || !Loud(out, at)) continue;
            const auto i = static_cast<std::size_t>(slot);
            out[at] = static_cast<std::uint8_t>((out[at] & 0xFE) | (bits[i % PayloadSize] ^ layout.Noise(i)));
        }
    }
    // The private chunk at the end, where players skip chunks they do not know.
    if (out.size() % 2) out.push_back(0);
    const auto bytes = BytesOf(bits);
    Stream chunkBytes(secret, "wav chunk");
    const std::string name = WavChunkName(secret);
    out.insert(out.end(), name.begin(), name.end());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(16 >> (8 * i)));
    for (std::size_t i = 0; i < 16; ++i) out.push_back(static_cast<std::uint8_t>((i < bytes.size() ? bytes[i] : 0) ^ chunkBytes.Byte(i)));
    const std::uint32_t riff = static_cast<std::uint32_t>(out.size() - 8);
    for (int i = 0; i < 4; ++i) out[4 + static_cast<size_t>(i)] = static_cast<std::uint8_t>(riff >> (8 * i));
    return true;
}

std::vector<Finding> ReadWav(const std::vector<std::uint8_t>& wav, std::uint64_t secret) {
    std::vector<Finding> found;
    WavFile file;
    if (!ParseWav(wav, file)) return found;
    {
        Tally t;
        const std::string name = WavChunkName(secret);
        Stream chunkBytes(secret, "wav chunk");
        for (std::size_t at = 12; at + 8 <= wav.size();) {
            const std::uint32_t size = Get32LE(&wav[at + 4]);
            if (std::memcmp(&wav[at], name.data(), 4) == 0 && size >= 10 && at + 8 + 10 <= wav.size())
                for (std::size_t i = 0; i < 10; ++i)
                    for (int b = 0; b < 8; ++b) t.Add(i * 8 + static_cast<size_t>(b), ((wav[at + 8 + i] ^ chunkBytes.Byte(i)) >> b) & 1);
            at += 8 + std::size_t{size} + (size & 1);
        }
        found.push_back(t.Decide("chunk", secret));
    }
    if (file.pcm16) {
        // The clip may be trimmed: every alignment of the blocks is tried, over at most the
        // first million samples, which hold hundreds of copies.
        const Layout layout = SampleLayout(secret);
        const std::size_t samples = std::min<std::size_t>(file.size / 2, std::size_t{1} << 20);
        Finding best;
        best.layer = "sample bits";
        for (std::size_t offset = 0; offset < Block; ++offset) {
            Tally t;
            VisitSamples(wav, file.data, samples, layout, offset, [&](std::size_t i, int bit) { t.Add(i, bit); });
            Finding f = t.Decide("sample bits", secret, 2, 0.9);
            const bool better = f.found && (!best.found || f.agree * best.total > best.agree * f.total);
            if (better || (!best.found && f.total > best.total)) best = f;
            if (best.found && best.agree == best.total) break;
        }
        found.push_back(best);
    }
    return found;
}

std::vector<Match> MatchPng(const std::vector<std::uint8_t>& png, std::uint64_t secret, const std::vector<std::uint64_t>& fingerprints) {
    std::vector<Match> out;
    std::vector<std::uint8_t> rgba;
    int w = 0, h = 0;
    if (!DecodeRgba(png, rgba, w, h)) return out;
    for (PixelMark mode : {PixelMark::Rgb, PixelMark::Alpha}) {
        const Layout layout = PixelLayout(mode, secret);
        auto found = MatchAll(mode == PixelMark::Rgb ? "pixel bits (rgb)" : "pixel bits (alpha)", Tile * Tile, 2, secret, fingerprints,
                              [&](std::size_t a, auto&& f) { VisitPixels(rgba, w, h, mode, layout, a % Tile, a / Tile, f); });
        out.insert(out.end(), found.begin(), found.end());
    }
    return out;
}

std::vector<Match> MatchWav(const std::vector<std::uint8_t>& wav, std::uint64_t secret, const std::vector<std::uint64_t>& fingerprints) {
    WavFile file;
    if (!ParseWav(wav, file) || !file.pcm16) return {};
    const Layout layout = SampleLayout(secret);
    const std::size_t samples = std::min<std::size_t>(file.size / 2, std::size_t{1} << 20);
    return MatchAll("sample bits", Block, 1, secret, fingerprints,
                    [&](std::size_t a, auto&& f) { VisitSamples(wav, file.data, samples, layout, a, f); });
}

} // namespace coldx::content::detail
