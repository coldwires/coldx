// Window control: settings before Run, and size, title and fullscreen changes while it
// runs. Headless, where a size change resizes the back buffer and calls OnResize. The
// captured frame's dimensions confirm the back buffer followed.

#include <coldx/coldx.h>

#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "check.h"

using namespace coldx;
using coldx_test::CheckResult;

namespace {

SDL_Window* OnlyWindow() {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* w = count == 1 ? windows[0] : nullptr;
    SDL_free(windows);
    return w;
}

class WindowGame : public Game {
public:
    int frame = 0;
    std::vector<std::pair<int, int>> resizes;

protected:
    void OnResize(int width, int height) override { resizes.push_back({width, height}); }

    void Update(const GameTime&) override {
        ++frame;
        if (frame == 1) {
            CHECK(Graphics().BackBufferWidth() == 200 && Graphics().BackBufferHeight() == 100);
            SDL_Window* w = OnlyWindow();
            CHECK(w && std::string(SDL_GetWindowTitle(w)) == "before");
            CHECK(IsFullscreen());  // remembered, headless
            SetWindowSize(320, 200);
            SetWindowTitle("during");
            ToggleFullscreen();
            SetVSync(false);
        } else if (frame == 2) {
            CHECK(Graphics().BackBufferWidth() == 320 && Graphics().BackBufferHeight() == 200);
            CHECK(std::string(SDL_GetWindowTitle(OnlyWindow())) == "during");
            CHECK(!IsFullscreen());
            CHECK(Settings().width == 320 && Settings().title == "during" && !Settings().vsync);
            SetWindowSize(0, 50);  // ignored
            Exit();
        }
    }

    void Draw(const GameTime&) override { Graphics().Clear(Color::Blue); }
};

} // namespace

int main() {
    const std::string capture = (std::filesystem::temp_directory_path() / "coldx_window_test.png").string();
    std::filesystem::remove(capture);
    SDL_Environment* env = SDL_GetEnvironment();
    SDL_SetEnvironmentVariable(env, "COLDX_HEADLESS", "1", true);
    SDL_SetEnvironmentVariable(env, "COLDX_CAPTURE", capture.c_str(), true);

    WindowGame game;
    game.Settings().console = ConsoleMode::Never;
    game.SetWindowSize(200, 100);
    game.SetWindowTitle("before");
    game.SetFullscreen(true);
    CHECK(game.Settings().width == 200 && game.Settings().height == 100);
    CHECK(game.Settings().title == "before" && game.IsFullscreen());

    CHECK(game.Run() == 0);
    CHECK(game.frame == 2);
    CHECK(game.resizes.size() == 1 && game.resizes[0] == std::make_pair(320, 200));
    for (const auto& [w, h] : game.resizes) std::printf("OnResize(%d, %d)\n", w, h);

    SDL_Surface* frame = SDL_LoadPNG(capture.c_str());
    CHECK(frame && frame->w == 320 && frame->h == 200);
    SDL_DestroySurface(frame);
    std::filesystem::remove(capture);
    return CheckResult();
}
