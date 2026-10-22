// Watermarks: every layer of a marked PNG and WAV reads back the fingerprint; the marks
// stay invisible and inaudible (at most 1 per channel or sample, palettes and silence
// untouched); a wrong secret finds nothing; a re-save by another encoder keeps exactly the
// layers it should; and Build marks only when asked and when there is a build id.

#include <coldx/coldx.h>
#include <coldx/content/build.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

#include <SDL3/SDL.h>

#include "check.h"
#include "watermark.h"

using namespace coldx;
using namespace coldx::content;
using coldx_test::CheckResult;
namespace fs = std::filesystem;
namespace wm = coldx::content::detail;

namespace {

std::vector<std::uint8_t> EncodePng(int w, int h, const std::function<void(int, int, std::uint8_t*)>& pixel) {
    SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) pixel(x, y, static_cast<std::uint8_t*>(s->pixels) + y * s->pitch + x * 4);
    SDL_IOStream* io = SDL_IOFromDynamicMem();
    CHECK(SDL_SavePNG_IO(s, io, false));
    const Sint64 size = SDL_GetIOSize(io);
    std::vector<std::uint8_t> bytes(static_cast<size_t>(size));
    SDL_SeekIO(io, 0, SDL_IO_SEEK_SET);
    SDL_ReadIO(io, bytes.data(), bytes.size());
    SDL_CloseIO(io);
    SDL_DestroySurface(s);
    return bytes;
}

std::vector<std::uint8_t> Pixels(const std::vector<std::uint8_t>& png, int& w, int& h) {
    SDL_Surface* loaded = SDL_LoadPNG_IO(SDL_IOFromConstMem(png.data(), png.size()), true);
    SDL_Surface* s = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32) : nullptr;
    SDL_DestroySurface(loaded);
    CHECK(s != nullptr);
    if (!s) return {};
    w = s->w;
    h = s->h;
    std::vector<std::uint8_t> out(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) std::memcpy(&out[static_cast<size_t>(y) * w * 4], static_cast<std::uint8_t*>(s->pixels) + y * s->pitch, static_cast<size_t>(w) * 4);
    SDL_DestroySurface(s);
    return out;
}

const wm::Finding* Layer(const std::vector<wm::Finding>& all, const char* name) {
    for (const wm::Finding& f : all)
        if (f.layer == name) return &f;
    return nullptr;
}

bool Found(const std::vector<wm::Finding>& all, const char* name, std::uint64_t fingerprint) {
    const wm::Finding* f = Layer(all, name);
    if (f) std::printf("  %-20s %s 0x%016llx, %zu of %zu agree\n", name, f->found ? "found" : "-----",
                       static_cast<unsigned long long>(f->fingerprint), f->agree, f->total);
    return f && f->found && f->fingerprint == fingerprint;
}

const std::uint64_t Secret = 0x1234ABCD5678EF01ull;
const std::uint64_t Print = 0x0BADC0FFEE15600Dull;

void Painted() {
    // A painted-looking image with a transparent border.
    const auto original = EncodePng(96, 80, [](int x, int y, std::uint8_t* p) {
        const bool border = x < 8 || y < 8 || x >= 88 || y >= 72;
        p[0] = static_cast<std::uint8_t>(x * 2 + y);
        p[1] = static_cast<std::uint8_t>(128 + 100 * std::sin(x * 0.2));
        p[2] = static_cast<std::uint8_t>(y * 3);
        p[3] = border ? 0 : 255;
    });
    std::vector<std::uint8_t> marked;
    std::string error;
    CHECK(wm::MarkPng(original, Secret, Print, wm::PixelMark::Rgb, marked, error));
    std::printf("painted, rgb: %zu bytes, marked %zu\n", original.size(), marked.size());
    const auto layers = wm::ReadPng(marked, Secret);
    CHECK(Found(layers, "chunk", Print));
    CHECK(Found(layers, "row filters", Print));
    CHECK(Found(layers, "transparent pixels", Print));
    CHECK(Found(layers, "pixel bits (rgb)", Print));
    CHECK(!Layer(layers, "pixel bits (alpha)")->found);

    // Invisible: every visible channel within 1, alpha unchanged, most pixels untouched.
    int w = 0, h = 0, w2 = 0, h2 = 0;
    const auto a = Pixels(original, w, h), b = Pixels(marked, w2, h2);
    CHECK(w == w2 && h == h2 && a.size() == b.size());
    int worst = 0, changed = 0, visible = 0;
    for (size_t p = 0; p < a.size() / 4; ++p) {
        CHECK(a[p * 4 + 3] == b[p * 4 + 3]);
        if (a[p * 4 + 3] == 0) continue;
        ++visible;
        int d = 0;
        for (int c = 0; c < 3; ++c) d = std::max(d, std::abs(a[p * 4 + c] - b[p * 4 + c]));
        worst = std::max(worst, d);
        changed += d > 0 ? 1 : 0;
    }
    std::printf("  visible pixels changed: %d of %d, by at most %d\n", changed, visible, worst);
    CHECK(worst == 1 && changed < visible / 4);

    // Another secret: nothing.
    for (const wm::Finding& f : wm::ReadPng(marked, Secret + 1)) CHECK(!f.found);
    // The unmarked original: nothing.
    for (const wm::Finding& f : wm::ReadPng(original, Secret)) CHECK(!f.found);

    // Re-saved by another encoder (SDL's), as an editor would: the chunk and row filters
    // are gone, the pixel bits stay.
    const auto resaved = EncodePng(w2, h2, [&](int x, int y, std::uint8_t* p) { std::memcpy(p, &b[(static_cast<size_t>(y) * w2 + x) * 4], 4); });
    const auto after = wm::ReadPng(resaved, Secret);
    std::printf("after a re-save:\n");
    CHECK(!Layer(after, "chunk")->found);
    CHECK(Found(after, "pixel bits (rgb)", Print));
    Found(after, "transparent pixels", Print);  // SDL keeps their colour; other editors may not
}

void PixelArt() {
    // Flat colours from a four-colour palette, fully opaque.
    const std::uint8_t palette[4][3] = {{20, 12, 28}, {68, 36, 52}, {208, 70, 72}, {222, 238, 214}};
    const auto original = EncodePng(48, 48, [&](int x, int y, std::uint8_t* p) {
        const auto& c = palette[((x / 6) + (y / 6)) % 4];
        p[0] = c[0];
        p[1] = c[1];
        p[2] = c[2];
        p[3] = 255;
    });
    std::vector<std::uint8_t> marked;
    std::string error;
    CHECK(wm::MarkPng(original, Secret, Print, wm::PixelMark::Alpha, marked, error));
    const auto layers = wm::ReadPng(marked, Secret);
    std::printf("pixel art, alpha:\n");
    CHECK(Found(layers, "pixel bits (alpha)", Print));
    CHECK(Found(layers, "row filters", Print));
    int w = 0, h = 0;
    const auto b = Pixels(marked, w, h);
    std::set<std::uint32_t> colours;
    int marks = 0;
    for (size_t p = 0; p < b.size() / 4; ++p) {
        colours.insert(std::uint32_t{b[p * 4]} << 16 | std::uint32_t{b[p * 4 + 1]} << 8 | b[p * 4 + 2]);
        CHECK(b[p * 4 + 3] >= 254);
        marks += b[p * 4 + 3] == 254 ? 1 : 0;
    }
    std::printf("  colours after marking: %zu (palette of 4); %d pixels at alpha 254\n", colours.size(), marks);
    CHECK(colours.size() == 4 && marks > 0);
}

std::vector<std::uint8_t> Wav(const std::vector<std::int16_t>& samples) {
    std::vector<std::uint8_t> b;
    auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    auto u16 = [&](std::uint16_t v) { b.push_back(static_cast<std::uint8_t>(v)); b.push_back(static_cast<std::uint8_t>(v >> 8)); };
    const std::uint32_t bytes = static_cast<std::uint32_t>(samples.size() * 2);
    b.insert(b.end(), {'R', 'I', 'F', 'F'}); u32(36 + bytes); b.insert(b.end(), {'W', 'A', 'V', 'E'});
    b.insert(b.end(), {'f', 'm', 't', ' '}); u32(16); u16(1); u16(1); u32(22050); u32(44100); u16(2); u16(16);
    b.insert(b.end(), {'d', 'a', 't', 'a'}); u32(bytes);
    for (std::int16_t s : samples) u16(static_cast<std::uint16_t>(s));
    return b;
}

void Sound() {
    // Half a second of tone, a quarter of silence, a tone again.
    std::vector<std::int16_t> samples;
    for (int i = 0; i < 22050; ++i) {
        const bool silent = i >= 11025 && i < 16538;
        samples.push_back(silent ? 0 : static_cast<std::int16_t>(9000 * std::sin(i * 0.05)));
    }
    const auto original = Wav(samples);
    std::vector<std::uint8_t> marked;
    std::string error;
    CHECK(wm::MarkWav(original, Secret, Print, marked, error));
    const auto layers = wm::ReadWav(marked, Secret);
    std::printf("sound:\n");
    CHECK(Found(layers, "chunk", Print));
    CHECK(Found(layers, "sample bits", Print));
    for (const wm::Finding& f : wm::ReadWav(marked, Secret + 1)) CHECK(!f.found);
    int worst = 0, changed = 0;
    bool silenceKept = true;
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto s = static_cast<std::int16_t>(marked[44 + i * 2] | (marked[45 + i * 2] << 8));
        worst = std::max(worst, std::abs(s - samples[i]));
        changed += s != samples[i] ? 1 : 0;
        if (samples[i] == 0) silenceKept = silenceKept && s == 0;
    }
    std::printf("  samples changed: %d of %zu, by at most %d; silence untouched: %s\n", changed, samples.size(), worst,
                silenceKept ? "yes" : "NO");
    CHECK(worst == 1 && silenceKept);
    // Still a WAV every player reads: coldx decodes it to the same number of frames.
    const SoundEffect a = SoundEffect::FromMemory(original.data(), original.size());
    const SoundEffect b = SoundEffect::FromMemory(marked.data(), marked.size());
    CHECK(!a.IsEmpty() && !b.IsEmpty() && std::fabs(a.Duration() - b.Duration()) < 1e-6f);
}

void Write(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> ReadAll(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

// Build marks a section only when it asks and there is a build id; two ids, two marks.
void Pipeline() {
    const fs::path dir = fs::temp_directory_path() / "coldx_watermark_test";
    fs::remove_all(dir);
    const auto art = EncodePng(64, 64, [](int x, int y, std::uint8_t* p) {
        p[0] = static_cast<std::uint8_t>(x * 4);
        p[1] = static_cast<std::uint8_t>(y * 4);
        p[2] = 90;
        p[3] = 255;
    });
    Write(dir / "content" / "art" / "a.png", art);
    Write(dir / "content" / "plain" / "b.png", art);
    std::ofstream(dir / "content" / "content.coldx") << "seed = marks\n[texture] art/*.png\nwatermark = rgb\n[texture] plain/*.png\n";

    auto owned = BuiltInProcessors();
    std::vector<const Processor*> list;
    for (const auto& p : owned) list.push_back(p.get());
    auto build = [&](const std::string& id, const char* out) {
        BuildOptions o;
        o.project = (dir / "content" / "content.coldx").string();
        o.output = (dir / out).string();
        o.buildId = id;
        BuildResult r = Build(o, list);
        for (const std::string& e : r.errors) std::printf("error: %s\n", e.c_str());
        CHECK(r.ok);
        return r;
    };
    const BuildResult none = build("", "none.rime");
    const BuildResult one = build("tester-1", "one.rime");
    const BuildResult two = build("tester-2", "two.rime");
    CHECK(one.fingerprint != two.fingerprint);
    const auto n = ReadAll(dir / "none.rime"), p1 = ReadAll(dir / "one.rime"), p2 = ReadAll(dir / "two.rime");
    CHECK(n != p1 && p1 != p2);

    // Out of the game: load each package's marked texture back through ContentManager's
    // bytes, and read the fingerprint from it.
    const std::uint64_t secret = wm::WatermarkSecret(rime::HashName(0, "marks"), nullptr);
    {  // its own scope: the game holds the package open until it is destroyed
        class Driver : public Game {
        public:
            std::function<void()> load;
            Driver() { Settings().console = ConsoleMode::Never; }

        protected:
            void LoadContent() override { load(); }
            void Update(const GameTime&) override { Exit(); }
        } game;
        game.load = [&] {
            ContentManager& c = game.Content();
            c.SetLooseFiles(LooseFiles::Never);
            CHECK(c.Mount((dir / "one.rime").string()));
            const auto a = c.ReadBytes("art/a.png");
            const auto b = c.ReadBytes("plain/b.png");
            CHECK(a && b && *b == art);  // the unmarked section is untouched
            if (a) {
                std::printf("from the package of tester-1:\n");
                CHECK(Found(wm::ReadPng(*a, secret), "pixel bits (rgb)", one.fingerprint));
                // The same through the command line, as a person would: coldxc --identify.
                Write(dir / "suspect.png", *a);
                const std::string project = (dir / "content" / "content.coldx").string(), suspect = (dir / "suspect.png").string();
                char tool[] = "coldxc", p[] = "--project", i[] = "--identify";
                char* args[] = {tool, p, const_cast<char*>(project.c_str()), i, const_cast<char*>(suspect.c_str())};
                CHECK(RunTool(5, args) == 0);
                Write(dir / "clean.png", art);
                const std::string clean = (dir / "clean.png").string();
                char* none[] = {tool, p, const_cast<char*>(project.c_str()), i, const_cast<char*>(clean.c_str())};
                CHECK(RunTool(5, none) == 1);
            }
            CHECK(c.Load<Texture2D>("art/a").Width() == 64);  // and it still loads as a texture
        };
        CHECK(game.Run() == 0);
    }

    // OGG cannot be marked: a clear error.
    Write(dir / "bad" / "s.ogg", {'O', 'g', 'g', 'S', 0, 0, 0, 0});
    std::ofstream(dir / "bad" / "content.coldx") << "[sound] *.ogg\nwatermark = on\n";
    BuildOptions o;
    o.project = (dir / "bad" / "content.coldx").string();
    o.output = (dir / "bad.rime").string();
    o.buildId = "x";
    const BuildResult r = Build(o, list);
    CHECK(!r.ok && !r.errors.empty() && r.errors[0].find("needs WAV") != std::string::npos);
    fs::remove_all(dir);
}

// Suspect files can be damaged on purpose: every truncation, and bytes flipped all over,
// must read safely (run under AddressSanitizer to mean anything) and never claim a mark
// the file does not have.
void Damaged() {
    const auto png = EncodePng(40, 40, [](int x, int y, std::uint8_t* p) {
        p[0] = static_cast<std::uint8_t>(x * 6);
        p[1] = static_cast<std::uint8_t>(y * 6);
        p[2] = 7;
        p[3] = (x + y) % 9 == 0 ? 0 : 255;
    });
    std::vector<std::uint8_t> markedPng, markedWav;
    std::string error;
    CHECK(wm::MarkPng(png, Secret, Print, wm::PixelMark::Rgb, markedPng, error));
    std::vector<std::int16_t> samples;
    for (int i = 0; i < 4000; ++i) samples.push_back(static_cast<std::int16_t>(7000 * std::sin(i * 0.1)));
    CHECK(wm::MarkWav(Wav(samples), Secret, Print, markedWav, error));

    bool honest = true;
    auto read = [&](const std::vector<std::uint8_t>& bytes, bool asPng) {
        for (const wm::Finding& f : asPng ? wm::ReadPng(bytes, Secret) : wm::ReadWav(bytes, Secret))
            if (f.found && f.fingerprint != Print) honest = false;  // a wrong build named
    };
    for (size_t cut = 0; cut < markedPng.size(); cut += 7) read(std::vector<std::uint8_t>(markedPng.begin(), markedPng.begin() + static_cast<std::ptrdiff_t>(cut)), true);
    for (size_t cut = 0; cut < markedWav.size(); cut += 5) read(std::vector<std::uint8_t>(markedWav.begin(), markedWav.begin() + static_cast<std::ptrdiff_t>(cut)), false);
    std::uint32_t x = 99;
    for (int round = 0; round < 300; ++round) {
        std::vector<std::uint8_t> p = markedPng, w = markedWav;
        for (int k = 0; k < 20; ++k) {
            x = x * 1664525u + 1013904223u;
            p[x % p.size()] ^= static_cast<std::uint8_t>(x >> 24);
            w[(x >> 8) % w.size()] ^= static_cast<std::uint8_t>(x >> 16);
        }
        read(p, true);
        read(w, false);
    }
    CHECK(honest);
}

} // namespace

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);
    Painted();
    PixelArt();
    Sound();
    Pipeline();
    Damaged();
    return CheckResult();
}
