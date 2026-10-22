// .rime packages: the format byte by byte, the reader refusing anything malformed, and
// ContentManager loading every built-in type from a package, mount order, the default
// package, loose-file rules, ReadBytes and a game's own data loader.

#include <coldx/coldx.h>
#include <coldx/core/rime.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>

#include <SDL3/SDL.h>

#include "check.h"
#include "rime_bytes.h"
#include "rime_package.h"

using namespace coldx;
using coldx_test::CheckResult;
namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> Bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

std::vector<std::uint8_t> ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

void WriteFile(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> Png(int w, int h, Uint8 r, Uint8 g, Uint8 b) {
    const fs::path path = fs::temp_directory_path() / "coldx_rime_png.png";
    SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    SDL_FillSurfaceRect(s, nullptr, SDL_MapSurfaceRGBA(s, r, g, b, 255));
    CHECK(SDL_SavePNG(s, path.string().c_str()));
    SDL_DestroySurface(s);
    std::vector<std::uint8_t> bytes = ReadFile(path);
    fs::remove(path);
    return bytes;
}

void Format() {
    CHECK(rime::NormalizeName("Sprites\\Player.PNG") == "sprites/player.png");
    CHECK(rime::NormalizeName("./a//b/c.txt") == "a/b/c.txt");
    CHECK(rime::NormalizeName("/a") == "a");
    CHECK(rime::Crc32("123456789", 9) == 0xCBF43926u);  // the standard check value
    CHECK(rime::HashName(1, "a") != rime::HashName(2, "a"));  // seeded
    CHECK(rime::HashName(1, "a") != rime::HashName(1, "b"));

    rime::PackageWriter w(0x1234);
    CHECK(w.Add("b.txt", rime::AssetType::Data, Bytes("bee")));
    CHECK(w.Add("A.txt", rime::AssetType::Data, Bytes("ay")));
    CHECK(!w.Add("a.txt", rime::AssetType::Data, Bytes("again")));  // same name once normalized
    CHECK(w.Add("empty", rime::AssetType::Data, {}));
    const std::vector<std::uint8_t> p = w.Finish();

    // Header, byte for byte.
    CHECK(p.size() >= rime::HeaderSize && std::equal(std::begin(rime::Magic), std::end(rime::Magic), p.begin()));
    CHECK(detail::Get16(&p[4]) == 1 && detail::Get16(&p[6]) == 48 && detail::Get32(&p[8]) == 0);
    CHECK(detail::Get32(&p[12]) == 3 && detail::Get64(&p[16]) == 0x1234 && detail::Get64(&p[24]) == 48);
    // No name appears anywhere in the file.
    const std::string text(p.begin(), p.end());
    CHECK(text.find("b.txt") == std::string::npos && text.find("a.txt") == std::string::npos);
    // Entries sorted by hash, data on 16-byte boundaries, CRCs right.
    for (int i = 0; i < 3; ++i) {
        const std::uint8_t* e = &p[48 + 40 * i];
        if (i > 0) CHECK(detail::Get64(e) > detail::Get64(e - 40));
        CHECK(detail::Get64(e + 16) % 16 == 0);
        CHECK(detail::Get32(e + 12) == rime::Crc32(p.data() + detail::Get64(e + 16), detail::Get64(e + 24)));
    }
    // The same entries in another order give the same bytes.
    rime::PackageWriter again(0x1234);
    again.Add("empty", rime::AssetType::Data, {});
    again.Add("a.txt", rime::AssetType::Data, Bytes("ay"));
    again.Add("b.txt", rime::AssetType::Data, Bytes("bee"));
    CHECK(again.Finish() == p);

    auto package = detail::Package::FromMemory(p, "format");
    CHECK(package && package->Count() == 3);
    if (!package) return;
    std::vector<std::uint8_t> out;
    const auto* a = package->Find("a.TXT");
    CHECK(a && package->Read(*a, out) && out == Bytes("ay"));
    const auto* empty = package->Find("empty");
    CHECK(empty && package->Read(*empty, out) && out.empty());
    CHECK(package->Find("c.txt") == nullptr);
}

// Every way a file can be wrong is refused, never read out of bounds.
void Malformed() {
    rime::PackageWriter w(7);
    w.Add("x", rime::AssetType::Data, Bytes("payload"));
    const std::vector<std::uint8_t> good = w.Finish();
    auto refused = [](std::vector<std::uint8_t> bytes) { return detail::Package::FromMemory(std::move(bytes), "bad") == nullptr; };

    CHECK(!refused(good));
    for (size_t cut : {size_t{0}, size_t{3}, size_t{47}, size_t{60}}) CHECK(refused(std::vector<std::uint8_t>(good.begin(), good.begin() + cut)));
    auto changed = [&](size_t at, std::uint8_t value) {
        std::vector<std::uint8_t> b = good;
        b[at] = value;
        return b;
    };
    CHECK(refused(changed(0, 0x50)));      // magic
    CHECK(refused(changed(4, 2)));         // version 2
    CHECK(refused(changed(6, 40)));        // header size
    CHECK(refused(changed(8, 1)));         // package flags
    CHECK(refused(changed(12, 200)));      // more entries than fit
    CHECK(refused(changed(24, 0xF0)));     // table beyond the end
    CHECK(refused(changed(48 + 23, 0x7F)));  // entry offset beyond the end
    CHECK(refused(changed(48 + 31, 0x7F)));  // entry size beyond the end
    CHECK(refused(changed(48 + 32, 99)));  // sizes disagree

    // Flags and damage are caught when the entry is read.
    auto flagged = detail::Package::FromMemory(changed(48 + 9, rime::Encrypted), "flagged");
    std::vector<std::uint8_t> out;
    CHECK(flagged && !flagged->Read(*flagged->Find("x"), out));
    std::vector<std::uint8_t> damaged = good;
    damaged[detail::Get64(&good[48 + 16])] ^= 1;
    auto hurt = detail::Package::FromMemory(damaged, "damaged");
    CHECK(hurt && !hurt->Read(*hurt->Find("x"), out));
}

struct Level {
    std::string text;
};

class Driver : public Game {
public:
    std::function<void()> load;
    Driver() { Settings().console = ConsoleMode::Never; }

protected:
    void LoadContent() override { load(); }
    void Update(const GameTime&) override { Exit(); }
};

void Loading(const fs::path& dir) {
    // The base package: one of every built-in type, plus a data file and a level.
    rime::PackageWriter base(42);
    base.Add("sprites/player.png", rime::AssetType::Texture, Png(4, 2, 255, 0, 0));
    base.Add("sounds/tone.ogg", rime::AssetType::Sound, ReadFile(COLDX_TEST_DATA "/tone.ogg"));
    base.Add("music/theme.ogg", rime::AssetType::Music, ReadFile(COLDX_TEST_DATA "/tone.ogg"));
    base.Add("fonts/main.ttf", rime::AssetType::Font, ReadFile(COLDX_TEST_FONT));
    base.Add("text/hello.txt", rime::AssetType::Data, Bytes("hello from a package"));
    base.Add("levels/one.lvl", rime::AssetType::Data, Bytes("packed level"));
    WriteFile(dir / "game.rime", base.Finish());
    // A patch replacing the player with a wider, green one.
    rime::PackageWriter patch(99);
    patch.Add("sprites/player.png", rime::AssetType::Texture, Png(8, 2, 0, 255, 0));
    WriteFile(dir / "patch.rime", patch.Finish());
    // Loose files: one only loose, one also packed (the package must win).
    WriteFile(dir / "content" / "loose.txt", Bytes("loose"));
    WriteFile(dir / "content" / "text" / "hello.txt", Bytes("loose hello"));

    Driver game;
    game.load = [&] {
        ContentManager& c = game.Content();
        c.SetRootDirectory((dir / "content").string());
        CHECK(c.Mount((dir / "game.rime").string()));
        CHECK(!c.Mount((dir / "missing.rime").string()));

        const Texture2D player = c.Load<Texture2D>("sprites/player");
        CHECK(player.Width() == 4 && player.Height() == 2);
        Color px[16];
        CHECK(player.GetData(px) && px[0] == Color(255, 0, 0, 255));
        CHECK(!c.Load<SoundEffect>("sounds/tone").IsEmpty());
        CHECK(c.Load<Music>("music/theme").Duration() > 0.4f);
        const SpriteFont small = c.LoadFont("fonts/main", 12), big = c.LoadFont("fonts/main", 40);
        CHECK(!small.IsEmpty() && !big.IsEmpty() && big.LineSpacing() > small.LineSpacing());

        auto hello = c.ReadBytes("text/hello.txt");
        CHECK(hello && *hello == Bytes("hello from a package"));
        // LooseFiles::Auto: a debug build still reads loose files beside a package; a
        // release build does not.
        auto loose = c.ReadBytes("loose.txt");
#ifdef NDEBUG
        CHECK(!loose);
#else
        CHECK(loose && *loose == Bytes("loose"));
#endif
        c.SetLooseFiles(LooseFiles::Always);
        loose = c.ReadBytes("loose.txt");
        CHECK(loose && *loose == Bytes("loose"));
        c.SetLooseFiles(LooseFiles::Auto);
        CHECK(!c.ReadBytes("nothing.txt"));

        c.RegisterDataLoader<Level>({".lvl"}, [](const std::vector<std::uint8_t>& bytes, const std::string&) {
            return Level{std::string(bytes.begin(), bytes.end())};
        });
        CHECK(c.Load<Level>("levels/one").text == "packed level");

        // A later mount wins; Unload drops the cache so the patched texture is seen.
        CHECK(c.Mount((dir / "patch.rime").string()));
        CHECK(c.Load<Texture2D>("sprites/player").Width() == 4);  // still cached
        c.Unload();
        const Texture2D patched = c.Load<Texture2D>("sprites/player");
        CHECK(patched.Width() == 8);
        CHECK(patched.GetData(px) && px[0] == Color(0, 255, 0, 255));

        // Loose files off: only packages.
        c.SetLooseFiles(LooseFiles::Never);
        CHECK(!c.ReadBytes("loose.txt"));
        CHECK(c.ReadBytes("text/hello.txt"));
    };
    CHECK(game.Run() == 0);
}

// "<root>.rime" beside the executable mounts by itself.
void DefaultPackage(const fs::path& dir) {
    rime::PackageWriter w(5);
    w.Add("greeting.txt", rime::AssetType::Data, Bytes("default package"));
    WriteFile(dir / "auto.rime", w.Finish());
    Driver game;
    game.load = [&] {
        game.Content().SetRootDirectory((dir / "auto").string());  // absolute: dir/auto.rime
        auto greeting = game.Content().ReadBytes("greeting.txt");
        CHECK(greeting && *greeting == Bytes("default package"));
    };
    CHECK(game.Run() == 0);
}

} // namespace

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);
    Format();
    Malformed();
    const fs::path dir = fs::temp_directory_path() / "coldx_rime_test";
    fs::remove_all(dir);
    Loading(dir);
    DefaultPackage(dir);
    fs::remove_all(dir);
    return CheckResult();
}
