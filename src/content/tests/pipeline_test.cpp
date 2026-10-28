// The content pipeline end to end: a project in the temp folder, built by Build with the
// built-in processors and a custom one, mounted by a headless game that loads every entry.
// Also the glob rules, project errors, the cache, unchanged packages and build failures.

#include <coldx/coldx.h>
#include <coldx/content/build.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>

#include <SDL3/SDL.h>

#include "check.h"
#include "project.h"
#include "protect_build.h"

using namespace coldx;
using namespace coldx::content;
using coldx_test::CheckResult;
namespace fs = std::filesystem;
namespace cdetail = coldx::content::detail;

namespace {

void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), {});
}

void WritePng(const fs::path& path, int w, int h) {
    fs::create_directories(path.parent_path());
    SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    SDL_FillSurfaceRect(s, nullptr, SDL_MapSurfaceRGBA(s, 0, 0, 255, 255));
    CHECK(SDL_SavePNG(s, path.string().c_str()));
    SDL_DestroySurface(s);
}

// [upper]: text files become upper case, under the name with .up instead of .txt.
class UpperProcessor : public Processor {
public:
    std::string Name() const override { return "upper"; }
    std::vector<std::string> SettingKeys() const override { return {"suffix"}; }
    bool Process(const Source& source, const Settings& settings, std::vector<Output>& outputs, std::string& error) const override {
        ++runs;
        if (source.bytes.empty()) {
            error = "empty";
            return false;
        }
        std::string text(source.bytes.begin(), source.bytes.end());
        for (char& c : text)
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (auto s = settings.find("suffix"); s != settings.end()) text += s->second;
        std::string name = source.name.substr(0, source.name.size() - 4) + ".up";
        outputs.push_back({name, rime::AssetType::FirstCustom, std::vector<std::uint8_t>(text.begin(), text.end())});
        return true;
    }
    mutable int runs = 0;
};

void Globs() {
    using cdetail::GlobMatch;
    CHECK(GlobMatch("*.png", "a.png") && !GlobMatch("*.png", "dir/a.png"));
    CHECK(GlobMatch("sprites/*.png", "sprites/Player.PNG"));  // case is ignored
    CHECK(GlobMatch("**/*.png", "a.png") && GlobMatch("**/*.png", "x/y/a.png"));
    CHECK(GlobMatch("ui/**", "ui/a/b.txt") && !GlobMatch("ui/**", "uix/a"));
    CHECK(GlobMatch("a?c.txt", "abc.txt") && !GlobMatch("a?c.txt", "a/c.txt"));
    CHECK(!GlobMatch("*.png", "a.png.bak"));
}

void Parsing() {
    cdetail::Project p;
    std::vector<std::string> errors;
    CHECK(cdetail::ParseProject("# c\nseed = my game\n[texture] a/*.png b.png  # two\n[upper] *.txt\nsuffix = !\n", p, errors));
    CHECK(p.seedSet && p.seed == "my game" && p.sections.size() == 2);
    CHECK(p.sections[0].patterns.size() == 2 && p.sections[0].line == 3);
    CHECK(p.sections[1].settings.at("suffix") == "!");

    const char* bad[] = {"[texture\n", "[Tex ture] a\n", "[texture]\n", "what\n", "nope = 1\n", "[data] ../x\n",
                         "[data] /abs\n", "[data] a\nk = 1\nk = 2\n"};
    for (const char* text : bad) {
        cdetail::Project q;
        std::vector<std::string> e;
        CHECK(!cdetail::ParseProject(text, q, e) && !e.empty() && e[0].rfind("line ", 0) == 0);
    }
}

struct Built {
    std::vector<std::unique_ptr<Processor>> owned;
    std::vector<const Processor*> list;
    UpperProcessor* upper = nullptr;
    Built() {
        owned = BuiltInProcessors();
        auto u = std::make_unique<UpperProcessor>();
        upper = u.get();
        owned.push_back(std::move(u));
        for (const auto& p : owned) list.push_back(p.get());
    }
};

class Driver : public Game {
public:
    std::function<void()> load;
    Driver() { Settings().console = ConsoleMode::Never; }

protected:
    void LoadContent() override { load(); }
    void Update(const GameTime&) override { Exit(); }
};

void EndToEnd(const fs::path& dir) {
    const fs::path project = dir / "content" / "content.coldx";
    Write(project,
          "seed = pipeline test\n"
          "[texture] sprites/*.png\n"
          "[sound] sounds/*.wav\n"
          "[music] music/*.ogg\n"
          "[font] main.ttf\n"
          "from = ../shared\n"
          "[data] levels/**\n"
          "[upper] notes/*.txt\n"
          "suffix = !\n"
          "[texture] raw/*.png\n"
          "format = raw\n");
    WritePng(dir / "content" / "sprites" / "player.png", 6, 3);
    WritePng(dir / "content" / "raw" / "tiles.png", 64, 32);
    fs::create_directories(dir / "content" / "sounds");
    fs::copy_file(COLDX_TEST_WAV, dir / "content" / "sounds" / "blip.wav");
    fs::create_directories(dir / "content" / "music");
    fs::copy_file(COLDX_TEST_OGG, dir / "content" / "music" / "theme.ogg");
    fs::create_directories(dir / "shared");
    fs::copy_file(COLDX_TEST_FONT, dir / "shared" / "main.ttf");
    Write(dir / "content" / "levels" / "one.lvl", "level one");
    Write(dir / "content" / "levels" / "deep" / "two.lvl", "level two");
    Write(dir / "content" / "notes" / "hello.txt", "hello");
    Write(dir / "content" / "unlisted.txt", "not in any section");

    Built tools;
    BuildOptions options;
    options.project = project.string();
    options.output = (dir / "out" / "content.rime").string();
    options.cacheDirectory = (dir / "cache").string();

    BuildResult r = Build(options, tools.list);
    for (const std::string& e : r.errors) std::printf("error: %s\n", e.c_str());
    CHECK(r.ok && r.written && r.sources == 8 && r.entries == 8 && r.processed == 8 && r.cached == 0);
    CHECK(tools.upper->runs == 1);
    const std::string first = Read(options.output);
    CHECK(first.find("player") == std::string::npos && first.find("levels") == std::string::npos);

    // Again: everything from the cache, and the package is left as it was.
    r = Build(options, tools.list);
    CHECK(r.ok && !r.written && r.cached == 8 && r.processed == 0 && tools.upper->runs == 1);
    // Without the cache, the same bytes: builds are deterministic.
    BuildOptions fresh = options;
    fresh.cacheDirectory.clear();
    fresh.output = (dir / "out" / "fresh.rime").string();
    CHECK(Build(fresh, tools.list).ok && Read(fresh.output) == first);
    // One change: one source processed, the package rewritten.
    Write(dir / "content" / "notes" / "hello.txt", "hello again");
    r = Build(options, tools.list);
    CHECK(r.ok && r.written && r.processed == 1 && r.cached == 7 && tools.upper->runs == 3);

    // The game reads it all, by the names it would use for loose files.
    Driver game;
    game.load = [&] {
        ContentManager& c = game.Content();
        c.SetLooseFiles(LooseFiles::Never);
        CHECK(c.Mount(options.output));
        CHECK(c.Load<Texture2D>("sprites/player").Width() == 6);
        // Stored decoded: the same pixels as the PNG, premultiplied.
        const Texture2D tiles = c.Load<Texture2D>("raw/tiles");
        std::vector<Color> px(64 * 32);
        CHECK(tiles.Width() == 64 && tiles.Height() == 32 && tiles.GetData(px.data()));
        CHECK(px.front() == Color(0, 0, 255, 255) && px.back() == Color(0, 0, 255, 255));
        CHECK(!c.Load<SoundEffect>("sounds/blip").IsEmpty());
        CHECK(!c.Load<Music>("music/theme").IsEmpty());
        CHECK(!c.LoadFont("main", 20).IsEmpty());
        auto two = c.ReadBytes("levels/deep/two.lvl");
        CHECK(two && std::string(two->begin(), two->end()) == "level two");
        auto note = c.ReadBytes("notes/hello.up");
        CHECK(note && std::string(note->begin(), note->end()) == "HELLO AGAIN!");
        CHECK(!c.ReadBytes("notes/hello.txt") && !c.ReadBytes("unlisted.txt"));
    };
    CHECK(game.Run() == 0);
}

// Every failure names the file or line, and leaves the package as it was.
void Failures(const fs::path& dir) {
    Built tools;
    const fs::path project = dir / "bad" / "content.coldx";
    const fs::path output = dir / "bad" / "out.rime";
    Write(output, "previous package");
    auto build = [&](const std::string& text) {
        Write(project, text);
        BuildOptions o;
        o.project = project.string();
        o.output = output.string();
        BuildResult r = Build(o, tools.list);
        std::string all;
        for (const std::string& e : r.errors) all += e + "\n";
        CHECK(!r.ok && !r.errors.empty() && Read(output) == "previous package");
        return all;
    };
    Write(dir / "bad" / "broken.png", "\x89PNG\r\n\x1a\nnot really");
    Write(dir / "bad" / "notes" / "empty.txt", "");
    Write(dir / "bad" / "a.txt", "a");
    Write(dir / "bad" / "a.up.txt", "x");
    CHECK(build("[texture] broken.png\n").find("broken.png: [texture] PNG does not decode") != std::string::npos);
    CHECK(build("[texture] broken.png\nformat = jpeg\n").find("format is png or raw") != std::string::npos);
    CHECK(build("[sound] a.txt\n").find("neither WAV nor OGG") != std::string::npos);
    CHECK(build("[shader] a.txt\n").find("line 1: no processor \"shader\"") != std::string::npos);
    CHECK(build("[data] a.txt\nlevel = 3\n").find("has no setting \"level\"") != std::string::npos);
    CHECK(build("[data] *.txt\n[upper] a.txt\n").find("matched by line 1 and line 2") != std::string::npos);
    CHECK(build("[upper] notes/*.txt\n").find("empty.txt: [upper] empty") != std::string::npos);
    CHECK(build("[data] nope\nfrom = missing\n").find("no folder") != std::string::npos);
    CHECK(build("[texture\n").find("line 1") != std::string::npos);
    // Two sources whose outputs share a name.
    Write(dir / "bad" / "x" / "a.txt", "1");
    Write(dir / "bad" / "y" / "a.txt", "2");
    CHECK(build("[data] a.txt\nfrom = x\n[data] a.txt\nfrom = y\n").find("already in the package") != std::string::npos);

    char name[] = "coldxc";
    char* noArgs[] = {name};
    CHECK(RunTool(1, noArgs) == 2);
}

// Keys, encryption through Build, fingerprints and the generated game source.
void Protection(const fs::path& dir) {
    const fs::path keyFile = dir / "p" / "game.key";
    fs::create_directories(keyFile.parent_path());
    std::string error;
    CHECK(cdetail::WriteNewKey(keyFile.string(), error));
    CHECK(!cdetail::WriteNewKey(keyFile.string(), error) && error.find("exists") != std::string::npos);
    rime::Key key{};
    CHECK(cdetail::ReadKeyFile(keyFile.string(), key, error));
    rime::Key second{};
    const fs::path otherKey = dir / "p" / "other.key";
    CHECK(cdetail::WriteNewKey(otherKey.string(), error) && cdetail::ReadKeyFile(otherKey.string(), second, error));
    CHECK(key != second);  // random, not fixed
    Write(dir / "p" / "short.key", "abcd\n");
    CHECK(!cdetail::ReadKeyFile((dir / "p" / "short.key").string(), key, error) && error.find("64 hex digits") != std::string::npos);
    Write(dir / "p" / "bad.key", std::string(63, 'a') + "z\n");
    CHECK(!cdetail::ReadKeyFile((dir / "p" / "bad.key").string(), key, error));
    CHECK(cdetail::ReadKeyFile(keyFile.string(), key, error));

    Write(dir / "p" / "content" / "content.coldx", "seed = protected\n[data] *.txt\n");
    Write(dir / "p" / "content" / "secret.txt", "THE PLAINTEXT SECRET");
    Built tools;
    BuildOptions o;
    o.project = (dir / "p" / "content" / "content.coldx").string();
    o.output = (dir / "p" / "out" / "content.rime").string();
    o.keyFile = keyFile.string();
    o.buildId = "tester-07";
    o.gameSource = (dir / "p" / "out" / "game.cpp").string();
    BuildResult r = Build(o, tools.list);
    CHECK(r.ok && r.fingerprint == cdetail::FingerprintOf("tester-07"));
    CHECK(Read(o.output).find("PLAINTEXT") == std::string::npos);
    const std::string source = Read(o.gameSource);
    CHECK(source.find("SetDefaultKey") != std::string::npos && source.find("f[16]") != std::string::npos);
    // The key is in the source only scattered. Its hex never appears in order.
    char hex[3];
    std::string keyHex;
    for (std::uint8_t b : key) {
        std::snprintf(hex, sizeof(hex), "%02x", b);
        keyHex += std::string("0x") + hex + ", ";
    }
    CHECK(source.find(keyHex.substr(0, 24)) == std::string::npos);
    // A build id without a key: a source with the fingerprint and no key.
    BuildOptions marked = o;
    marked.keyFile.clear();
    marked.output = (dir / "p" / "out" / "plain.rime").string();
    marked.gameSource = (dir / "p" / "out" / "plain.cpp").string();
    CHECK(Build(marked, tools.list).ok);
    CHECK(Read(marked.gameSource).find("SetDefaultKey") == std::string::npos && Read(marked.gameSource).find("f[16]") != std::string::npos);

    // The game: with the key, without it, with another.
    Driver game;
    game.load = [&] {
        ContentManager& c = game.Content();
        c.SetLooseFiles(LooseFiles::Never);
        CHECK(c.Mount(o.output));
        CHECK(!c.ReadBytes("secret.txt"));
        c.SetKey(second);
        CHECK(!c.ReadBytes("secret.txt"));
        c.SetKey(key);
        auto secret = c.ReadBytes("secret.txt");
        CHECK(secret && std::string(secret->begin(), secret->end()) == "THE PLAINTEXT SECRET");
    };
    CHECK(game.Run() == 0);

    // Identify: the masked fingerprint in the header, and the marker in any file.
    const std::uint64_t seed = rime::HashName(0, "protected");
    CHECK(cdetail::Identify(o.output, seed));
    cdetail::Identify(o.output, seed + 1);  // another project's seed: a meaningless value, but it reads safely
    std::string blob = "padding";
    const std::uint64_t marker = cdetail::FingerprintMarker(seed), value = cdetail::MaskFingerprint(seed, r.fingerprint);
    for (int i = 0; i < 8; ++i) blob += static_cast<char>(marker >> (8 * i));
    for (int i = 0; i < 8; ++i) blob += static_cast<char>(value >> (8 * i));
    blob += "tail";
    Write(dir / "p" / "game.exe", blob);
    CHECK(cdetail::Identify((dir / "p" / "game.exe").string(), seed));
    Write(dir / "p" / "clean.exe", "nothing here");
    CHECK(!cdetail::Identify((dir / "p" / "clean.exe").string(), seed));
    CHECK(cdetail::MaskFingerprint(seed, cdetail::MaskFingerprint(seed, 42)) == 42);
}

} // namespace

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);
    Globs();
    Parsing();
    const fs::path dir = fs::temp_directory_path() / "coldx_pipeline_test";
    fs::remove_all(dir);
    EndToEnd(dir);
    Failures(dir);
    Protection(dir);
    fs::remove_all(dir);
    return CheckResult();
}
