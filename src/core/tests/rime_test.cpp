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
#include "chacha20.h"
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

// Entries that shrink are stored as LZ4 blocks. Ones that do not (already compressed
// data) are stored as they are. Compressed entries that lie about their size, or do not
// decompress, are refused without reading past anything.
void Compression() {
    std::vector<std::uint8_t> text;
    for (int i = 0; i < 20000; ++i) {
        const std::string line = "line " + std::to_string(i % 97) + " of a very repetitive level file\n";
        text.insert(text.end(), line.begin(), line.end());
    }
    std::vector<std::uint8_t> noise(50000);
    std::uint32_t x = 12345;
    for (auto& b : noise) {
        x = x * 1664525u + 1013904223u;
        b = static_cast<std::uint8_t>(x >> 24);
    }
    rime::PackageWriter w(3);
    w.Add("level.txt", rime::AssetType::Data, text);
    w.Add("noise.bin", rime::AssetType::Data, noise);
    w.Add("raw.txt", rime::AssetType::Data, text, false);
    const std::vector<std::uint8_t> p = w.Finish();
    CHECK(p.size() < text.size() + noise.size() + text.size() / 2);  // level.txt shrank

    auto entryFor = [&](const char* name) -> const std::uint8_t* {
        const std::uint64_t hash = rime::HashName(3, name);
        for (int i = 0; i < 3; ++i)
            if (detail::Get64(&p[48 + 40 * i]) == hash) return &p[48 + 40 * i];
        return nullptr;
    };
    const std::uint8_t* level = entryFor("level.txt");
    const std::uint8_t* rand = entryFor("noise.bin");
    const std::uint8_t* raw = entryFor("raw.txt");
    CHECK(level && rand && raw);
    if (!level || !rand || !raw) return;
    CHECK(level[9] == rime::Compressed && detail::Get64(level + 24) < text.size() / 4 && detail::Get64(level + 32) == text.size());
    CHECK(rand[9] == 0 && detail::Get64(rand + 24) == noise.size());
    CHECK(raw[9] == 0 && detail::Get64(raw + 24) == text.size());
    std::printf("level.txt: %zu bytes stored as %llu\n", text.size(), static_cast<unsigned long long>(detail::Get64(level + 24)));

    auto package = detail::Package::FromMemory(p, "compressed");
    std::vector<std::uint8_t> out;
    CHECK(package && package->Read(*package->Find("level.txt"), out) && out == text);
    CHECK(package && package->Read(*package->Find("noise.bin"), out) && out == noise);

    // A claimed size far beyond what LZ4 can expand to: refused when mounting.
    const size_t at = static_cast<size_t>(level - p.data());
    std::vector<std::uint8_t> bomb = p;
    detail::Put64(&bomb[at + 32], 1ull << 40);
    CHECK(detail::Package::FromMemory(bomb, "bomb") == nullptr);
    // A plausible but wrong size: mounts, and the read fails.
    std::vector<std::uint8_t> shorter = p;
    detail::Put64(&shorter[at + 32], text.size() - 1);
    auto wrong = detail::Package::FromMemory(shorter, "wrong size");
    CHECK(wrong && !wrong->Read(*wrong->Find("level.txt"), out));
    // Garbage in the block with a CRC to match: the decoder stops inside its buffers.
    std::vector<std::uint8_t> garbled = p;
    const size_t data = static_cast<size_t>(detail::Get64(level + 16)), size = static_cast<size_t>(detail::Get64(level + 24));
    for (size_t i = 0; i < size; i += 7) garbled[data + i] ^= 0x5A;
    detail::Put32(&garbled[at + 12], rime::Crc32(&garbled[data], size));
    auto bad = detail::Package::FromMemory(garbled, "garbled");
    CHECK(bad && !bad->Read(*bad->Find("level.txt"), out));
}

std::vector<std::uint8_t> Hex(const char* hex) {
    std::vector<std::uint8_t> out;
    for (const char* c = hex; c[0] && c[1]; c += 2) {
        while (*c == ' ') ++c;
        out.push_back(static_cast<std::uint8_t>(std::stoi(std::string(c, 2), nullptr, 16)));
    }
    return out;
}

// ChaCha20 against RFC 8439, then encrypted packages: readable with the key, refused
// without it or with another, and nothing of the plaintext left in the file.
void Encryption() {
    std::uint8_t key[32];
    for (int i = 0; i < 32; ++i) key[i] = static_cast<std::uint8_t>(i);
    {   // Section 2.3.2: the block function, as keystream XORed into zeros.
        const std::uint8_t nonce[12] = {0, 0, 0, 9, 0, 0, 0, 0x4a, 0, 0, 0, 0};
        std::vector<std::uint8_t> block(64, 0);
        detail::ChaCha20(key, nonce, 1, block.data(), block.size());
        CHECK(block == Hex("10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e"
                           "d2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e"));
    }
    {   // Section 2.4.2: a 114-byte message across two blocks.
        const std::uint8_t nonce[12] = {0, 0, 0, 0, 0, 0, 0, 0x4a, 0, 0, 0, 0};
        const std::string text = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, "
                                 "sunscreen would be it.";
        std::vector<std::uint8_t> data(text.begin(), text.end());
        detail::ChaCha20(key, nonce, 1, data.data(), data.size());
        CHECK(data == Hex("6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0b"
                          "f91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d8"
                          "07ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab7793736"
                          "5af90bbf74a35be6b40b8eedf2785e42874d"));
        detail::ChaCha20(key, nonce, 1, data.data(), data.size());
        CHECK(std::string(data.begin(), data.end()) == text);  // the same call decrypts
    }

    rime::Key k{}, other{};
    for (int i = 0; i < 32; ++i) {
        k[i] = static_cast<std::uint8_t>(i * 7 + 1);
        other[i] = static_cast<std::uint8_t>(i * 7 + 2);
    }
    CHECK(rime::KeyCheck(k) != rime::KeyCheck(other) && rime::KeyCheck(k) != 0);
    const std::string secret = "THE SECRET LEVEL IS BEHIND THE WATERFALL";
    std::vector<std::uint8_t> repeated;
    for (int i = 0; i < 200; ++i) repeated.insert(repeated.end(), secret.begin(), secret.end());
    rime::PackageWriter w(11);
    w.SetKey(k);
    w.Add("secret.txt", rime::AssetType::Data, Bytes(secret));
    w.Add("repeated.txt", rime::AssetType::Data, repeated);  // compressed, then encrypted
    const std::vector<std::uint8_t> p = w.Finish();
    const std::string file(p.begin(), p.end());
    CHECK(file.find("SECRET") == std::string::npos && file.find("WATERFALL") == std::string::npos);
    CHECK(detail::Get64(&p[32]) == rime::KeyCheck(k));

    auto package = detail::Package::FromMemory(p, "encrypted");
    CHECK(package != nullptr);
    if (!package) return;
    std::vector<std::uint8_t> out;
    const auto* a = package->Find("secret.txt");
    const auto* b = package->Find("repeated.txt");
    CHECK(a && b && (a->flags & rime::Encrypted) && (b->flags & rime::Encrypted) && (b->flags & rime::Compressed));
    CHECK(package->Read(*a, out, &k) && out == Bytes(secret));
    CHECK(package->Read(*b, out, &k) && out == repeated);
    CHECK(!package->Read(*a, out));          // no key
    CHECK(!package->Read(*a, out, &other));  // another key

    // The same name with other content gets another nonce. The two ciphertexts do not
    // XOR to the XOR of the plaintexts, as they would under a reused nonce.
    rime::PackageWriter w2(11);
    w2.SetKey(k);
    std::string changed = secret;
    changed[0] = 'X';
    w2.Add("secret.txt", rime::AssetType::Data, Bytes(changed));
    const std::vector<std::uint8_t> p2 = w2.Finish();
    auto second = detail::Package::FromMemory(p2, "second");
    const auto* a2 = second ? second->Find("secret.txt") : nullptr;
    CHECK(a2 && a2->crc != a->crc);
    if (a2) {
        int sameXor = 0;
        for (size_t i = 1; i < secret.size(); ++i)
            sameXor += (p[static_cast<size_t>(a->offset) + i] ^ p2[static_cast<size_t>(a2->offset) + i]) == 0 ? 1 : 0;
        CHECK(sameXor < 5);  // equal plaintext bytes after the first would match under a reused nonce
    }
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
        // LooseFiles::Auto: a debug build still reads loose files beside a package. A
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

        // A later mount wins. Unload drops the cache so the patched texture is seen.
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
    Compression();
    Encryption();
    const fs::path dir = fs::temp_directory_path() / "coldx_rime_test";
    fs::remove_all(dir);
    Loading(dir);
    DefaultPackage(dir);
    fs::remove_all(dir);
    return CheckResult();
}
