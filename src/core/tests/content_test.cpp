// ContentManager: finding files by name, caching, every built-in type, fonts per size, a
// game-registered loader, Unload, and the handle semantics that let a game keep copies.
// Builds a real content folder in the temp directory for the run.

#include <coldx/coldx.h>

#include <filesystem>
#include <fstream>

#include <SDL3/SDL.h>

#include "check.h"
#include "texture_access.h"

using namespace coldx;
using coldx_test::CheckResult;
namespace fs = std::filesystem;

namespace {

struct Level {
    std::string text;
};
struct NoLoaderType {
    int value = 0;
};

void WriteText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

void WritePng(const fs::path& path, int w, int h) {
    fs::create_directories(path.parent_path());
    SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    SDL_FillSurfaceRect(s, nullptr, SDL_MapSurfaceRGBA(s, 255, 0, 0, 255));
    CHECK(SDL_SavePNG(s, path.string().c_str()));
    SDL_DestroySurface(s);
}

void WriteWav(const fs::path& path) {
    const std::int16_t pcm[4] = {0, 1000, -1000, 0};
    std::string wav;
    auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) wav += static_cast<char>(v >> (8 * i)); };
    auto u16 = [&](std::uint16_t v) { wav += static_cast<char>(v); wav += static_cast<char>(v >> 8); };
    wav += "RIFF"; u32(36 + sizeof(pcm)); wav += "WAVE";
    wav += "fmt "; u32(16); u16(1); u16(1); u32(8000); u32(16000); u16(2); u16(16);
    wav += "data"; u32(sizeof(pcm));
    wav.append(reinterpret_cast<const char*>(pcm), sizeof(pcm));
    WriteText(path, wav);
}

class Driver : public Game {
public:
    std::function<void()> load;
    Driver() { Settings().console = ConsoleMode::Never; }

protected:
    void LoadContent() override { load(); }
    void Update(const GameTime&) override { Exit(); }
};

} // namespace

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);
    const fs::path root = fs::temp_directory_path() / "coldx_content_test";
    fs::remove_all(root);
    WritePng(root / "sprites" / "player.png", 4, 2);
    WriteWav(root / "sounds" / "blip.wav");
    fs::create_directories(root / "fonts");
    fs::copy_file(COLDX_TEST_FONT, root / "fonts" / "main.ttf");
    fs::create_directories(root / "music");
    fs::copy_file(COLDX_TEST_DATA "/tone.ogg", root / "music" / "theme.ogg");
    WriteText(root / "levels" / "one.lvl", "level one");

    Texture2D survivor;
    {
        Driver game;
        game.load = [&] {
            ContentManager& content = game.Content();
            CHECK(content.RootDirectory() == "content");
            content.SetRootDirectory(root.string());

            // Found without its extension, cached, and shared.
            const Texture2D player = content.Load<Texture2D>("sprites/player");
            CHECK(player.Width() == 4 && player.Height() == 2);
            CHECK(content.IsLoaded<Texture2D>("sprites/player"));
            const Texture2D again = content.Load<Texture2D>("sprites/player");
            CHECK(detail::TextureAccess::Handle(again) == detail::TextureAccess::Handle(player));
            CHECK(!content.Load<Texture2D>("sprites/player.png").IsEmpty());  // with extension

            // A miss is not cached: once the file exists, the next Load finds it.
            CHECK(content.Load<Texture2D>("sprites/enemy").IsEmpty());
            CHECK(!content.IsLoaded<Texture2D>("sprites/enemy"));
            WritePng(root / "sprites" / "enemy.png", 2, 2);
            CHECK(content.Load<Texture2D>("sprites/enemy").Width() == 2);

            const SoundEffect blip = content.Load<SoundEffect>("sounds/blip");
            CHECK(!blip.IsEmpty() && blip.SampleRate() == 8000 && blip.Channels() == 1);
            CHECK(!content.Load<Music>("music/theme").IsEmpty());

            const SpriteFont small = content.LoadFont("fonts/main", 18.0f);
            const SpriteFont big = content.LoadFont("fonts/main", 24.0f);
            CHECK(small.PixelHeight() == 18.0f && big.PixelHeight() == 24.0f);
            CHECK(detail::TextureAccess::Handle(content.LoadFont("fonts/main", 18.0f).Texture()) ==
                  detail::TextureAccess::Handle(small.Texture()));
            CHECK(content.LoadFont("fonts/missing", 18.0f).IsEmpty());

            // A game's own type: its loader runs once until Unload.
            int loads = 0;
            content.RegisterLoader<Level>({".lvl"}, [&loads](const std::string& path) {
                ++loads;
                std::ifstream file(path);
                return Level{std::string(std::istreambuf_iterator<char>(file), {})};
            });
            CHECK(content.Load<Level>("levels/one").text == "level one");
            CHECK(content.Load<Level>("levels/one").text == "level one");
            CHECK(loads == 1);

            CHECK(content.Load<NoLoaderType>("anything").value == 0);  // logged, empty

            // Copies outlive Unload; the next Load reads the file again.
            survivor = player;
            content.Unload();
            CHECK(!content.IsLoaded<Texture2D>("sprites/player"));
            CHECK(!survivor.IsEmpty() && detail::TextureAccess::Handle(survivor) == detail::TextureAccess::Handle(player));
            CHECK(content.Load<Level>("levels/one").text == "level one");
            CHECK(loads == 2);
            CHECK(detail::TextureAccess::Handle(content.Load<Texture2D>("sprites/player")) !=
                  detail::TextureAccess::Handle(player));

            // Handles: a copy shares the texture and keeps it alive past the original.
            Texture2D a = content.Load<Texture2D>("sprites/enemy");
            Texture2D b = a;
            CHECK(detail::TextureAccess::Handle(a) == detail::TextureAccess::Handle(b));
            a = Texture2D();
            CHECK(a.IsEmpty() && b.Width() == 2);
        };
        CHECK(game.Run() == 0);
    }
    // Outlived its game: destroying it now must not crash.
    survivor = Texture2D();
    fs::remove_all(root);
    return CheckResult();
}
