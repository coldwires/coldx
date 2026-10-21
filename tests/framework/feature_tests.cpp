// Checks for the features added after the first stress test run: virtual resolution and
// the others that filled its missing-features list. Automated where a number decides,
// a scene where a person has to look.

#include "framework_test.h"

#include <array>
#include <cmath>
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

// FS-01: exclusive fullscreen at a resolution below the desktop's, entered while running
// and at startup; the display must come back to its desktop mode afterwards.
Result ExclusiveFullscreen() {
    Result r{"FS-01", "P1"};
    std::string notes, problems;
    DisplayMode desktop;
    size_t modeCount = 0;
    {
        Probe game;
        double since = 0;
        int phase = 0;
        game.update = [&](Probe& p, const GameTime&) {
            const int w = p.Graphics().BackBufferWidth(), h = p.Graphics().BackBufferHeight();
            if (phase == 0) {
                desktop = p.DesktopDisplayMode();
                const std::vector<DisplayMode> modes = p.DisplayModes();
                modeCount = modes.size();
                if (modes.empty()) problems += " no display modes listed;";
                p.SetFullscreenMode(FullscreenMode::Exclusive, 1920, 1080);
                p.SetFullscreen(true);
                phase = 1;
                since = Now();
            } else if (phase == 1 && ((w == 1920 && h == 1080) || Now() - since > 5)) {
                const SDL_DisplayMode* m = SDL_GetWindowFullscreenMode(OnlyWindow());
                notes += Format("running: back buffer %dx%d after %.1f s, window mode %s; ", w, h, Now() - since,
                                m ? Format("%dx%d at %.0f Hz", m->w, m->h, m->refresh_rate).c_str() : "borderless");
                if (w != 1920 || h != 1080 || !m) problems += " exclusive 1920x1080 not entered;";
                p.SetFullscreen(false);
                phase = 2;
                since = Now();
            } else if (phase == 2 && ((w == 1280 && h == 720) || Now() - since > 5)) {
                if (w != 1280 || h != 720) problems += Format(" back to a window gave %dx%d;", w, h);
                p.Exit();
            }
        };
        if (game.Run() != 0) problems += " first run failed;";
    }
    {
        Probe game;
        game.Settings().fullscreen = true;
        game.Settings().fullscreenMode = FullscreenMode::Exclusive;
        game.Settings().fullscreenWidth = 1920;
        game.Settings().fullscreenHeight = 1080;
        int first = 0;
        game.update = [&](Probe& p, const GameTime&) {
            if (p.updates == 1) first = p.Graphics().BackBufferWidth();
            if (p.updates == 30) p.Exit();
        };
        game.Run();
        notes += Format("started fullscreen: first frame %d wide; ", first);
        if (first != 1920) problems += " a fullscreen start did not begin at 1920;";
    }
    // After both runs the display is back at its desktop mode.
    SDL_Init(SDL_INIT_VIDEO);
    const SDL_DisplayMode* now = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (now && (now->w != desktop.width || now->h != desktop.height)) problems += " the display did not return to its desktop mode;";
    notes += Format("%zu modes listed; desktop %dx%d at %.0f Hz, now %dx%d. ", modeCount, desktop.width, desktop.height,
                    desktop.refreshRate, now ? now->w : 0, now ? now->h : 0);
    SDL_Quit();
    r.status = problems.empty() ? Status::NeedsHuman : Status::Fail;
    r.notes = notes + "Alt-tab out of exclusive mode needs a person (FS-02)." + (problems.empty() ? "" : " Problems:" + problems);
    return r;
}

// TIME-05: drawing between updates in a real window with vsync: draws at the display's
// rate, updates at 60.
Result DrawBetweenUpdates() {
    Result r{"TIME-05", "P1"};
    Probe game;
    game.Settings().drawBetweenUpdates = true;
    double start = 0;
    int startUpdates = 0, startDraws = 0;
    float refresh = 0;
    game.update = [&](Probe& p, const GameTime&) {
        if (p.updates == 60) {
            start = Now();
            startUpdates = p.updates;
            startDraws = p.draws;
            const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(OnlyWindow()));
            refresh = m ? m->refresh_rate : 0;
        }
        if (p.updates == 60 + 300) p.Exit();
    };
    game.Run();
    const double seconds = Now() - start;
    const double ups = (game.updates - startUpdates) / seconds, fps = (game.draws - startDraws) / seconds;
    const bool ok = std::abs(ups - 60) < 1.5 && (refresh <= 61 || fps > refresh * 0.9);
    r.status = ok ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("%.1f updates and %.1f draws a second over %.1f s on a %.0f Hz display. ", ups, fps, seconds, refresh) +
              "Whether motion looks smoother needs a person (TIME-06).";
    return r;
}

// Removes the last UTF-8 character.
void PopCharacter(std::string& text) {
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) text.pop_back();
    if (!text.empty()) text.pop_back();
}

void TypingScene(HumanScene& s) {
    auto text = std::make_shared<std::string>();
    s.load = [] { TextInput::Start(); };
    s.update = [text](const GameTime&) {
        *text += TextInput::GetText();
        const KeyboardState k = Keyboard::GetState();
        const bool ctrl = k.IsKeyDown(Key::LeftControl) || k.IsKeyDown(Key::RightControl);
        if (Keyboard::WasPressed(Key::Backspace)) PopCharacter(*text);
        if (Keyboard::WasPressed(Key::Enter)) text->clear();
        if (ctrl && Keyboard::WasPressed(Key::V)) *text += Clipboard::GetText();
        if (ctrl && Keyboard::WasPressed(Key::C)) Clipboard::SetText(*text);
    };
    s.draw = [&s, text](SpriteBatch& b, const GameTime&) {
        const Vector2 at(40, 320);
        const Vector2 size = s.Font().MeasureString(*text);
        b.Begin();
        b.Draw(s.White(), Rectangle(30, 310, 900, 50), Color(30, 30, 50));
        b.DrawString(s.Font(), *text, at, Color::White);
        b.DrawString(s.Font(), TextInput::Composition(), Vector2(at.x + size.x, at.y), Color::Yellow);
        b.Draw(s.White(), Rectangle(static_cast<int>(at.x + size.x), 316, 2, 36), Color::Cyan);
        b.End();
        TextInput::SetCaretArea(Rectangle(static_cast<int>(at.x + size.x), 316, 2, 36));
    };
    s.readout = [text] {
        int characters = 0;
        for (unsigned char c : *text) characters += (c & 0xC0) != 0x80 ? 1 : 0;
        return std::vector<std::string>{Format("%d characters, %zu bytes; composing: %zu bytes", characters, text->size(),
                                               TextInput::Composition().size()),
                                        "Ctrl+V pastes, Ctrl+C copies the line, Backspace deletes, Enter clears"};
    };
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
    AutoTests().push_back({"FS-01", "P1", "Exclusive fullscreen", ExclusiveFullscreen});
    AutoTests().push_back({"TIME-05", "P1", "Drawing between updates", DrawBetweenUpdates});
    HumanTests().push_back({"INP-06", "P1", "Text input",
                            {"Type a name with accents (e with acute, u with umlaut) and dead keys; paste text with Ctrl+V;",
                             "turn on a Japanese or Chinese IME and type. The font draws Latin-1 only: other scripts show as",
                             "gaps, so check the counts. Pass: accents arrive as one character each; Backspace removes one",
                             "character; the IME's candidate list sits beside the cyan caret; paste and copy work."},
                            TypingScene});
    HumanTests().push_back(
        {"TIME-06", "P1", "Interpolated motion by eye",
         {"Two squares cross at 60 updates a second. The top one is drawn interpolated, the bottom one is not.",
          "Best on a display above 60 Hz. I turns drawing between updates off and on.",
          "Pass: with it on, the top square moves visibly smoother than the bottom one; neither stutters or jumps back."},
         [](HumanScene& s) {
             s.Settings().drawBetweenUpdates = true;
             auto pos = std::make_shared<std::array<float, 2>>();  // previous, current
             s.update = [&s, pos](const GameTime& t) {
                 if (Keyboard::WasPressed(Key::I)) s.Settings().drawBetweenUpdates = !s.Settings().drawBetweenUpdates;
                 (*pos)[0] = (*pos)[1];
                 (*pos)[1] += 600.0f * t.Delta();
                 if ((*pos)[1] > s.Graphics().BackBufferWidth()) (*pos)[0] = (*pos)[1] = -40.0f;
             };
             s.draw = [&s, pos](SpriteBatch& b, const GameTime& t) {
                 const float smooth = (*pos)[0] + ((*pos)[1] - (*pos)[0]) * t.Alpha();
                 b.Begin();
                 b.Draw(s.White(), Rectangle(static_cast<int>(smooth), 300, 40, 40), Color::Cyan);
                 b.Draw(s.White(), Rectangle(static_cast<int>((*pos)[1]), 380, 40, 40), Color(255, 140, 0));
                 b.End();
             };
             s.readout = [&s] {
                 return std::vector<std::string>{Format("drawing between updates: %s (I toggles)",
                                                        s.Settings().drawBetweenUpdates ? "on" : "off")};
             };
         }});
    HumanTests().push_back({"FS-02", "P1", "Exclusive fullscreen by hand",
                            {"E: exclusive at 1920x1080. B: borderless. F11 toggles fullscreen.",
                             "In exclusive, alt-tab away and back, and press the Windows key.",
                             "Pass: the game comes back at 1920x1080, corners intact; the desktop is untouched after quitting."},
                            [](HumanScene& s) {
                                s.update = [&s](const GameTime&) {
                                    if (Keyboard::WasPressed(Key::E)) s.SetFullscreenMode(FullscreenMode::Exclusive, 1920, 1080);
                                    if (Keyboard::WasPressed(Key::B)) s.SetFullscreenMode(FullscreenMode::Borderless);
                                    if (Keyboard::WasPressed(Key::F11)) s.ToggleFullscreen();
                                };
                                s.draw = [&s](SpriteBatch& b, const GameTime&) {
                                    const int w = s.Graphics().BackBufferWidth(), h = s.Graphics().BackBufferHeight();
                                    b.Begin();
                                    for (auto [x, y] : {std::pair{0, 0}, {w - 24, 0}, {0, h - 24}, {w - 24, h - 24}})
                                        b.Draw(s.White(), Rectangle(x, y, 24, 24), Color::Yellow);
                                    b.End();
                                };
                                s.readout = [&s] {
                                    return std::vector<std::string>{Format(
                                        "back buffer %d x %d, %s, %s", s.Graphics().BackBufferWidth(), s.Graphics().BackBufferHeight(),
                                        s.IsFullscreen() ? "fullscreen" : "windowed",
                                        s.Settings().fullscreenMode == FullscreenMode::Exclusive ? "exclusive" : "borderless")};
                                };
                            }});
    HumanTests().push_back({"VRES-02", "P1", "Virtual resolution by eye",
                            {"640x360 scaled into the window. Resize it, try each mode, go fullscreen.",
                             "Pass: the yellow frame is always fully visible; Integer keeps the checker crisp;",
                             "bars are purple; the red cross stays under the pointer, also in the bars' edges."},
                            VirtualScene});
}

} // namespace ft
