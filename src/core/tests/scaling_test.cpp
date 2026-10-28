// Virtual resolution: where a fixed-size back buffer lands in the window, mouse positions
// mapped into it, and a headless game whose back buffer keeps the virtual size while the
// window changes. What reaches the screen is a blit of a frame the render tests already
// check. The placement arithmetic is checked here directly.

#include <coldx/coldx.h>

#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "check.h"
#include "scaling.h"

using namespace coldx;
using namespace coldx::detail;
using coldx_test::CheckResult;

namespace {

void Placement() {
    // 320x180 into 1280x720: exactly 4x, no bars.
    CHECK(FitRectangle(1280, 720, 320, 180, ScaleMode::Fit) == Rectangle(0, 0, 1280, 720));
    CHECK(FitRectangle(1280, 720, 320, 180, ScaleMode::Integer) == Rectangle(0, 0, 1280, 720));
    // Into 1000x1000: Fit scales 3.125 (1000x563, bars above and below). Integer is 3x, centred.
    CHECK(FitRectangle(1000, 1000, 320, 180, ScaleMode::Fit) == Rectangle(0, 218, 1000, 563));
    CHECK(FitRectangle(1000, 1000, 320, 180, ScaleMode::Integer) == Rectangle(20, 230, 960, 540));
    // A tall window: bars left and right.
    CHECK(FitRectangle(800, 1200, 400, 300, ScaleMode::Fit) == Rectangle(0, 300, 800, 600));
    CHECK(FitRectangle(1200, 600, 400, 300, ScaleMode::Fit) == Rectangle(200, 0, 800, 600));
    // Stretch fills, whatever the aspect.
    CHECK(FitRectangle(1000, 1000, 320, 180, ScaleMode::Stretch) == Rectangle(0, 0, 1000, 1000));
    // Smaller than the image: Integer shrinks as Fit does rather than vanish.
    CHECK(FitRectangle(160, 90, 320, 180, ScaleMode::Integer) == Rectangle(0, 0, 160, 90));
    // Degenerate sizes do not divide by zero.
    CHECK(FitRectangle(0, 0, 320, 180, ScaleMode::Fit).IsEmpty());
    CHECK(FitRectangle(640, 480, 0, 0, ScaleMode::Fit) == Rectangle(0, 0, 640, 480));
}

void Mapping() {
    const Rectangle placed = FitRectangle(1000, 1000, 320, 180, ScaleMode::Integer);  // 3x at (20, 230)
    int x = 0, y = 0;
    OutputToImage(placed, 320, 180, 20.0f, 230.0f, x, y);
    CHECK(x == 0 && y == 0);
    OutputToImage(placed, 320, 180, 22.9f, 232.9f, x, y);  // still inside image pixel 0
    CHECK(x == 0 && y == 0);
    OutputToImage(placed, 320, 180, 979.0f, 769.0f, x, y);  // last output pixel of the image
    CHECK(x == 319 && y == 179);
    OutputToImage(placed, 320, 180, 5.0f, 100.0f, x, y);  // in the bars: outside the image
    CHECK(x < 0 && y < 0);
    OutputToImage(placed, 320, 180, 995.0f, 990.0f, x, y);
    CHECK(x >= 320 && y >= 180);
    // Every image pixel maps out and back to itself.
    bool round = true;
    for (int py = 0; py < 180; py += 7) {
        for (int px = 0; px < 320; px += 13) {
            float ox = 0, oy = 0;
            ImageToOutput(placed, 320, 180, px, py, ox, oy);
            OutputToImage(placed, 320, 180, ox, oy, x, y);
            round = round && x == px && y == py;
        }
    }
    CHECK(round);
}

class VirtualGame : public Game {
public:
    int frame = 0;
    std::vector<std::pair<int, int>> resizes;
    MouseState mouse;

protected:
    void OnResize(int width, int height) override { resizes.push_back({width, height}); }

    void Update(const GameTime&) override {
        ++frame;
        if (frame == 1) {
            CHECK(Graphics().BackBufferWidth() == 320 && Graphics().BackBufferHeight() == 180);
            // The hidden window is 1280x720, 4x: window (402, 203) is virtual (100, 50).
            int count = 0;
            SDL_Window** windows = SDL_GetWindows(&count);
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = count == 1 ? SDL_GetWindowID(windows[0]) : 0;
            motion.motion.x = 402.0f;
            motion.motion.y = 203.0f;
            SDL_free(windows);
            SDL_PushEvent(&motion);
            SetWindowSize(640, 480);  // the back buffer keeps its size, no OnResize
        } else if (frame == 2) {
            mouse = Mouse::GetState();
            CHECK(Graphics().BackBufferWidth() == 320 && Graphics().BackBufferHeight() == 180);
            SetVirtualResolution(200, 100);  // OnResize(200, 100)
        } else if (frame == 3) {
            CHECK(Graphics().BackBufferWidth() == 200 && Graphics().BackBufferHeight() == 100);
            SetVirtualResolution(0, 0);  // back to the window: OnResize(640, 480)
        } else if (frame == 4) {
            CHECK(Graphics().BackBufferWidth() == 640 && Graphics().BackBufferHeight() == 480);
            SetVirtualResolution(64, 32);
            SetScaleMode(ScaleMode::Integer, true);
            CHECK(Settings().scaleMode == ScaleMode::Integer && Settings().smoothScaling);
        } else if (frame == 5) {
            Exit();
        }
    }

    void Draw(const GameTime&) override { Graphics().Clear(Color::Green); }
};

void Headless() {
    const std::string capture = (std::filesystem::temp_directory_path() / "coldx_scaling_test.png").string();
    std::filesystem::remove(capture);
    SDL_Environment* env = SDL_GetEnvironment();
    SDL_SetEnvironmentVariable(env, "COLDX_HEADLESS", "1", true);
    SDL_SetEnvironmentVariable(env, "COLDX_CAPTURE", capture.c_str(), true);

    VirtualGame game;
    game.Settings().console = ConsoleMode::Never;
    game.Settings().virtualWidth = 320;
    game.Settings().virtualHeight = 180;
    CHECK(game.Run() == 0);
    CHECK(game.frame == 5);
    CHECK(game.mouse.x == 100 && game.mouse.y == 50);
    for (const auto& [w, h] : game.resizes) std::printf("OnResize(%d, %d)\n", w, h);
    const std::vector<std::pair<int, int>> expected = {{200, 100}, {640, 480}, {64, 32}};
    CHECK(game.resizes == expected);

    // The captured frame is the virtual size, not the window's.
    SDL_Surface* frame = SDL_LoadPNG(capture.c_str());
    CHECK(frame && frame->w == 64 && frame->h == 32);
    SDL_DestroySurface(frame);
    std::filesystem::remove(capture);
}

} // namespace

int main() {
    Placement();
    Mapping();
    Headless();
    return CheckResult();
}
