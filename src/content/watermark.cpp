#include "watermark.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>

#include <SDL3/SDL.h>

#include "zlib_codec.h"

namespace coldx::content::detail {

namespace {

constexpr std::size_t PayloadSize = 80;  // bits: the fingerprint, then a 16-bit check

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

std::uint16_t Check(std::uint64_t secret, std::uint64_t fingerprint) {
    std::string bytes(8, '\0');
    for (int i = 0; i < 8; ++i) bytes[static_cast<size_t>(i)] = static_cast<char>(fingerprint >> (8 * i));
    return static_cast<std::uint16_t>(rime::HashName(secret, "check" + bytes));
}

using Payload = std::array<std::uint8_t, PayloadSize>;

// Each layer carries these bits, whitened so they look random.
Payload PayloadOf(std::uint64_t secret, std::uint64_t fingerprint) {
    Stream white(secret, "payload");
    const std::uint16_t check = Check(secret, fingerprint);
    Payload bits{};
    for (std::size_t i = 0; i < 64; ++i) bits[i] = static_cast<std::uint8_t>(((fingerprint >> i) & 1) ^ white.Bit(i));
    for (std::size_t i = 0; i < 16; ++i) bits[64 + i] = static_cast<std::uint8_t>(((check >> i) & 1) ^ white.Bit(64 + i));
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
        observations_.push_back({index % PayloadSize, static_cast<std::uint8_t>(bit & 1)});
        ones_[index % PayloadSize] += bit & 1;
        seen_[index % PayloadSize] += 1;
    }

    Finding Decide(const char* layer, std::uint64_t secret) const {
        Finding f;
        f.layer = layer;
        f.total = observations_.size();
        for (std::size_t n : seen_)
            if (n == 0) return f;
        Payload bits{};
        for (std::size_t i = 0; i < PayloadSize; ++i) bits[i] = ones_[i] * 2 > seen_[i] ? 1 : 0;
        Stream white(secret, "payload");
        std::uint64_t fingerprint = 0;
        std::uint16_t check = 0;
        for (std::size_t i = 0; i < 64; ++i) fingerprint |= std::uint64_t(bits[i] ^ white.Bit(i)) << i;
        for (std::size_t i = 0; i < 16; ++i) check = static_cast<std::uint16_t>(check | ((bits[64 + i] ^ white.Bit(64 + i)) << i));
        for (const auto& [index, bit] : observations_) f.agree += bits[index] == bit ? 1 : 0;
        f.found = Check(secret, fingerprint) == check;
        f.fingerprint = fingerprint;
        return f;
    }

private:
    std::vector<std::pair<std::size_t, std::uint8_t>> observations_;
    std::array<std::size_t, PayloadSize> ones_{}, seen_{};
};

// count positions out of candidates, a different order for every secret: a start and a
// step coprime with the number of candidates, so no position repeats.
class Positions {
public:
    Positions(std::size_t candidates, std::size_t count, std::uint64_t secret, const char* purpose) : n_(candidates), count_(count) {
        if (n_ == 0) return;
        Stream s(secret, purpose);
        start_ = s.Word(0) % n_;
        step_ = n_ > 1 ? 1 + s.Word(1) % (n_ - 1) : 1;
        while (std::gcd(step_, n_) != 1) step_ = step_ + 1 < n_ ? step_ + 1 : 1;
    }
    std::size_t Count() const { return count_; }
    std::size_t operator[](std::size_t k) const { return (start_ + k * step_) % n_; }

private:
    std::size_t n_, count_, start_ = 0, step_ = 1;
};

// How many places a layer marks: everything when there is little room, else a quarter (or
// less), enough for many copies of the payload.
std::size_t MarkCount(std::size_t candidates, std::size_t divisor) {
    if (candidates <= 2 * PayloadSize) return candidates;
    return std::clamp(candidates / divisor, 2 * PayloadSize, PayloadSize * 64);
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

std::vector<std::size_t> PixelCandidates(const std::vector<std::uint8_t>& rgba, PixelMark mode) {
    std::vector<std::size_t> out;
    for (std::size_t p = 0; p < rgba.size() / 4; ++p) {
        const std::uint8_t a = rgba[p * 4 + 3];
        // Both tests ignore the bit being written, so marking never changes who qualifies.
        if (mode == PixelMark::Rgb ? a > 0 : (a & 0xFE) == 254) out.push_back(p);
    }
    return out;
}

} // namespace

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

    // Pixels: the lowest bit of a channel, or alpha 254/255.
    const std::vector<std::size_t> candidates = PixelCandidates(rgba, mode);
    const Positions places(candidates.size(), MarkCount(candidates.size(), 4), secret, "pixel places");
    Stream noise(secret, "pixel bits"), channels(secret, "pixel channels");
    for (std::size_t k = 0; k < places.Count(); ++k) {
        const std::size_t p = candidates[places[k]];
        const int bit = bits[k % PayloadSize] ^ noise.Bit(k);
        std::uint8_t& v = mode == PixelMark::Rgb ? rgba[p * 4 + channels.Byte(k) % 3] : rgba[p * 4 + 3];
        v = static_cast<std::uint8_t>((v & 0xFE) | bit);
    }
    // Fully transparent pixels: their colour, which nothing ever shows.
    Stream clear(secret, "transparent");
    std::size_t q = 0;
    for (std::size_t p = 0; p < rgba.size() / 4; ++p) {
        if (rgba[p * 4 + 3] != 0) continue;
        for (int c = 0; c < 3; ++c, ++q) rgba[p * 4 + static_cast<size_t>(c)] = static_cast<std::uint8_t>(bytes[q % bytes.size()] ^ clear.Byte(q));
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
    {   // transparent pixels
        Tally t;
        Stream clear(secret, "transparent");
        std::size_t q = 0;
        for (std::size_t p = 0; p < rgba.size() / 4; ++p) {
            if (rgba[p * 4 + 3] != 0) continue;
            for (int c = 0; c < 3; ++c, ++q) {
                const int v = rgba[p * 4 + static_cast<size_t>(c)] ^ clear.Byte(q);
                for (int b = 0; b < 8; ++b) t.Add((q % (PayloadSize / 8)) * 8 + static_cast<size_t>(b), (v >> b) & 1);
            }
        }
        found.push_back(t.Decide("transparent pixels", secret));
    }
    for (PixelMark mode : {PixelMark::Rgb, PixelMark::Alpha}) {
        Tally t;
        const std::vector<std::size_t> candidates = PixelCandidates(rgba, mode);
        const Positions places(candidates.size(), MarkCount(candidates.size(), 4), secret, "pixel places");
        Stream noise(secret, "pixel bits"), channels(secret, "pixel channels");
        for (std::size_t k = 0; k < places.Count(); ++k) {
            const std::size_t p = candidates[places[k]];
            const std::uint8_t v = mode == PixelMark::Rgb ? rgba[p * 4 + channels.Byte(k) % 3] : rgba[p * 4 + 3];
            t.Add(k, (v & 1) ^ noise.Bit(k));
        }
        found.push_back(t.Decide(mode == PixelMark::Rgb ? "pixel bits (rgb)" : "pixel bits (alpha)", secret));
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

std::vector<std::size_t> SampleCandidates(const std::vector<std::uint8_t>& b, const WavFile& wav) {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < wav.size / 2; ++i) {
        // Ignoring the bit being written, as with pixels; quiet samples stay untouched, so
        // silence stays exactly silent.
        const int sample = static_cast<std::int16_t>(Get16LE(&b[wav.data + i * 2]) & 0xFFFE);
        if (std::abs(sample) >= 64) out.push_back(i);
    }
    return out;
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
        const std::vector<std::size_t> candidates = SampleCandidates(out, file);
        const Positions places(candidates.size(), MarkCount(candidates.size(), 16), secret, "sample places");
        Stream noise(secret, "sample bits");
        for (std::size_t k = 0; k < places.Count(); ++k) {
            std::uint8_t& low = out[file.data + candidates[places[k]] * 2];
            low = static_cast<std::uint8_t>((low & 0xFE) | (bits[k % PayloadSize] ^ noise.Bit(k)));
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
        Tally t;
        const std::vector<std::size_t> candidates = SampleCandidates(wav, file);
        const Positions places(candidates.size(), MarkCount(candidates.size(), 16), secret, "sample places");
        Stream noise(secret, "sample bits");
        for (std::size_t k = 0; k < places.Count(); ++k) t.Add(k, (wav[file.data + candidates[places[k]] * 2] & 1) ^ noise.Bit(k));
        found.push_back(t.Decide("sample bits", secret));
    }
    return found;
}

} // namespace coldx::content::detail
