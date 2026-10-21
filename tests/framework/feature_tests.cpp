// Checks for the features added after the first stress test run: virtual resolution and
// the others that filled its missing-features list. Automated where a number decides,
// a scene where a person has to look.

#include "framework_test.h"

#include <memory>

#include <SDL3/SDL.h>

using namespace coldx;

namespace ft {

namespace {

const char* ModeName(ScaleMode mode) {
    switch (mode) {
    case ScaleMode::Fit: return "Fit";
    case ScaleMode::Integer: return "Integer";
    case ScaleMode::Stretch: return "Stretch";
    }
    return "?";
}

// VRES-01: a real window, where the back buffer is placed and where the mouse lands.
Result VirtualResolution() {
    Result r{"VRES-01", "P1"};
    Probe game;
    game.Settings().width = 1000;
    game.Settings().height = 1000;
    game.Settings().resizable = true;
    game.Settings().virtualWidth = 320;
    game.Settings().virtualHeight = 180;
    game.Settings().scaleMode = ScaleMode::Integer;
    std::string problems, notes;
    int resizes = 0;
    game.resized = [&](int, int) { ++resizes; };
    auto expect = [&](const char* what, Rectangle want) {
        const Rectangle got = game.Graphics().PresentedArea();
        notes += Format("%s: %d,%d %dx%d; ", what, got.x, got.y, got.width, got.height);
        if (got != want) problems += Format(" %s placed wrong (want %d,%d %dx%d);", what, want.x, want.y, want.width, want.height);
    };
    game.update = [&](Probe& p, const GameTime&) {
        switch (p.updates) {
        case 5:
            if (p.Graphics().BackBufferWidth() != 320 || p.Graphics().BackBufferHeight() != 180) problems += " back buffer not 320x180;";
            expect("Integer in 1000x1000", Rectangle(20, 230, 960, 540));
            Mouse::SetPosition(100, 50);
            break;
        case 8: {
            const MouseState m = Mouse::GetState();
            notes += Format("mouse set to (100, 50) reads (%d, %d); ", m.x, m.y);
            if (m.x != 100 || m.y != 50) problems += " mouse position does not round-trip;";
            p.SetScaleMode(ScaleMode::Fit);
            break;
        }
        case 11:
            expect("Fit in 1000x1000", Rectangle(0, 218, 1000, 563));
            p.SetWindowSize(1280, 720);
            break;
        case 20:
            expect("Fit in 1280x720", Rectangle(0, 0, 1280, 720));
            if (p.Graphics().BackBufferWidth() != 320) problems += " back buffer followed the window;";
            p.Exit();
            break;
        default: break;
        }
    };
    const int rc = game.Run();
    if (resizes != 0) problems += Format(" OnResize fired %d times for window changes;", resizes);
    r.status = rc == 0 && problems.empty() ? Status::NeedsHuman : Status::Fail;
    r.notes = notes + "Scaling quality and the bars need a person (VRES-02)." + (problems.empty() ? "" : " Problems:" + problems);
    return r;
}

void VirtualScene(HumanScene& s) {
    s.Settings().resizable = true;
    s.Settings().virtualWidth = 640;
    s.Settings().virtualHeight = 360;
    s.Settings().scaleMode = ScaleMode::Integer;
    s.Settings().letterboxColor = Color(40, 0, 60);
    s.update = [&s](const GameTime&) {
        GameSettings& c = s.Settings();
        if (Keyboard::WasPressed(Key::D1)) s.SetScaleMode(ScaleMode::Fit, c.smoothScaling);
        if (Keyboard::WasPressed(Key::D2)) s.SetScaleMode(ScaleMode::Integer, c.smoothScaling);
        if (Keyboard::WasPressed(Key::D3)) s.SetScaleMode(ScaleMode::Stretch, c.smoothScaling);
        if (Keyboard::WasPressed(Key::D4)) s.SetScaleMode(c.scaleMode, !c.smoothScaling);
        if (Keyboard::WasPressed(Key::F11)) s.ToggleFullscreen();
    };
    s.draw = [&s](SpriteBatch& b, const GameTime&) {
        const MouseState m = Mouse::GetState();
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        // A 1-pixel checker in one corner shows whether pixels stay square and sharp.
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 48; ++x)
                if ((x + y) % 2 == 0) b.Draw(s.White(), Rectangle(580 + x, 300 + y, 1, 1), Color::White);
        // A 1-pixel frame on the very edge of the virtual screen.
        b.Draw(s.White(), Rectangle(0, 0, 640, 1), Color::Yellow);
        b.Draw(s.White(), Rectangle(0, 359, 640, 1), Color::Yellow);
        b.Draw(s.White(), Rectangle(0, 0, 1, 360), Color::Yellow);
        b.Draw(s.White(), Rectangle(639, 0, 1, 360), Color::Yellow);
        b.Draw(s.White(), Rectangle(m.x - 6, m.y, 13, 1), Color::Red);
        b.Draw(s.White(), Rectangle(m.x, m.y - 6, 1, 13), Color::Red);
        b.End();
    };
    s.readout = [&s] {
        const Rectangle a = s.Graphics().PresentedArea();
        const MouseState m = Mouse::GetState();
        return std::vector<std::string>{
            Format("%s, %s   shown at %d,%d %dx%d   mouse %d, %d", ModeName(s.Settings().scaleMode),
                   s.Settings().smoothScaling ? "smooth" : "sharp", a.x, a.y, a.width, a.height, m.x, m.y),
            "1 Fit  2 Integer  3 Stretch  4 smooth/sharp  F11 fullscreen"};
    };
}

} // namespace

void RegisterFeatures() {
    AutoTests().push_back({"VRES-01", "P1", "Virtual resolution placement", VirtualResolution});
    HumanTests().push_back({"VRES-02", "P1", "Virtual resolution by eye",
                            {"640x360 scaled into the window. Resize it, try each mode, go fullscreen.",
                             "Pass: the yellow frame is always fully visible; Integer keeps the checker crisp;",
                             "bars are purple; the red cross stays under the pointer, also in the bars' edges."},
                            VirtualScene});
}

} // namespace ft
