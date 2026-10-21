// The tests a person judges. Each scene shows what to do and live readouts; the tester
// presses P, F or S. Tests that need other machines have instructions and no scene.

#include "framework_test.h"

#include <array>
#include <cmath>
#include <memory>

#include <SDL3/SDL.h>

using namespace coldx;

namespace ft {

namespace {

std::string Onoff(bool b) {
    return b ? "on" : "off";
}

// Corners, center and a render target: what a resize or fullscreen change must keep right.
void AnchoredScene(HumanScene& s) {
    auto target = std::make_shared<RenderTarget2D>();
    s.draw = [&s, target](SpriteBatch& b, const GameTime&) {
        GraphicsDevice& g = s.Graphics();
        if (target->IsEmpty()) {
            *target = RenderTarget2D::Create(g, 96, 96);
            g.SetRenderTarget(target.get());
            g.Clear(Color(0, 140, 0));
            b.Begin();
            b.Draw(s.White(), Rectangle(24, 24, 48, 48), Color::Red);
            b.End();
            g.SetRenderTarget(nullptr);
        }
        const int w = g.BackBufferWidth(), h = g.BackBufferHeight();
        b.Begin();
        for (auto [x, y] : {std::pair{0, 0}, {w - 24, 0}, {0, h - 24}, {w - 24, h - 24}}) b.Draw(s.White(), Rectangle(x, y, 24, 24), Color::Yellow);
        b.Draw(*target, Vector2(w / 2.0f - 48, h / 2.0f - 48), Color::White);
        b.End();
    };
    s.readout = [&s] {
        return std::vector<std::string>{Format("back buffer %d x %d, fullscreen %s", s.Graphics().BackBufferWidth(),
                                               s.Graphics().BackBufferHeight(), Onoff(s.IsFullscreen()).c_str())};
    };
    s.update = [&s](const GameTime&) {
        if (Keyboard::WasPressed(Key::F11)) s.ToggleFullscreen();
    };
}

void GamepadScene(HumanScene& s) {
    s.update = [](const GameTime&) {
        for (int p = 0; p < 4; ++p) {
            if (Keyboard::WasPressed(static_cast<Key>(static_cast<int>(Key::D1) + p))) GamePad::SetVibration(p, 0.3f, 0.3f);
            if (Keyboard::WasPressed(Key::D0)) GamePad::SetVibration(p, 0, 0);
        }
    };
    s.readout = [] {
        std::vector<std::string> lines;
        const char* names[] = {"A", "B", "X", "Y", "Back", "Start", "Guide", "LB", "RB", "LS", "RS", "Up", "Down", "Left", "Right"};
        for (int p = 0; p < 4; ++p) {
            const GamePadState st = GamePad::GetState(p);
            const GamePadState raw = GamePad::GetState(p, GamePadDeadZone::None);
            if (!st.isConnected) {
                lines.push_back(Format("player %d: not connected", p));
                continue;
            }
            std::string held;
            for (int b = 0; b < 15; ++b)
                if (st.buttons & (1u << b)) held += std::string(names[b]) + " ";
            lines.push_back(Format("player %d: L (%.2f, %.2f) raw (%.2f, %.2f)  R (%.2f, %.2f)  LT %.2f RT %.2f  %s", p, st.leftStick.x,
                                   st.leftStick.y, raw.leftStick.x, raw.leftStick.y, st.rightStick.x, st.rightStick.y,
                                   st.leftTrigger, st.rightTrigger, held.c_str()));
        }
        lines.push_back("1-4: rumble that player at 30%   0: stop rumble");
        return lines;
    };
}

} // namespace

void RegisterHuman() {
    auto add = [](std::string id, std::string priority, std::string title, std::vector<std::string> steps,
                  std::function<void(HumanScene&)> setup) {
        HumanTests().push_back({std::move(id), std::move(priority), std::move(title), std::move(steps), std::move(setup)});
    };

    add("GFX-01", "P0", "Window resize by hand",
        {"Drag the window edges quickly, very small and very large.",
         "Pass: no crash; yellow squares stay in the corners; the green target with its red square stays centred and sharp."},
        AnchoredScene);
    add("GFX-02", "P0", "Fullscreen, alt-tab, monitors",
        {"F11 toggles fullscreen. Toggle several times; alt-tab away and back while fullscreen;",
         "drag the window to another monitor if there is one.",
         "Pass: never black, corners and target intact, the right resolution after returning."},
        AnchoredScene);
    add("GFX-03", "P0", "High-DPI sharpness and cursor",
        {"Run at 150% or 200% Windows scaling, or on a Retina Mac.",
         "Pass: the 1 px grid and the small text are sharp, not blurred; the cross sits exactly under the pointer tip."},
        [](HumanScene& s) {
            auto small = std::make_shared<SpriteFont>();
            s.load = [&s, small] { *small = SpriteFont::FromFile(s.Graphics(), FontPath, 11.0f); };
            s.draw = [&s, small](SpriteBatch& b, const GameTime&) {
                const int w = s.Graphics().BackBufferWidth(), h = s.Graphics().BackBufferHeight();
                const MouseState m = Mouse::GetState();
                b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
                for (int x = 0; x < w; x += 8) b.Draw(s.White(), Rectangle(x, 300, 1, h - 300), Color::Gray);
                for (int y = 300; y < h; y += 8) b.Draw(s.White(), Rectangle(0, y, w, 1), Color::Gray);
                b.DrawString(*small, "11 px text: The quick brown fox jumps over the lazy dog 0123456789", Vector2(20, 270), Color::White);
                b.Draw(s.White(), Rectangle(m.x - 10, m.y, 21, 1), Color::Red);
                b.Draw(s.White(), Rectangle(m.x, m.y - 10, 1, 21), Color::Red);
                b.End();
            };
            s.readout = [] {
                const MouseState m = Mouse::GetState();
                SDL_Window* w = OnlyWindow();
                return std::vector<std::string>{Format("mouse %d, %d   display scale %.2f   pixel density %.2f", m.x, m.y,
                                                       w ? SDL_GetWindowDisplayScale(w) : 0.0f, w ? SDL_GetWindowPixelDensity(w) : 0.0f)};
            };
        });
    add("GFX-04", "P1", "Other GPUs and backends",
        {"On an Intel, an AMD and an NVIDIA machine, run: coldx_framework_tests --auto --only GFX-04",
         "and compare the captures in the temp folder. Pass: identical output, no crash."},
        nullptr);
    add("TIME-01", "P0", "Smooth motion at 60 updates",
        {"Watch the square cross the screen on a 60 Hz and a high-refresh display.",
         "Pass: even, smooth motion; updates per second stay at 60."},
        [](HumanScene& s) {
            auto counters = std::make_shared<std::array<double, 4>>();  // updates, draws, start, shown rate
            s.update = [counters](const GameTime& t) {
                auto& c = *counters;
                c[0] += 1;
                if (t.Total() - c[2] >= 1.0) {
                    c[3] = c[0];
                    c[0] = 0;
                    c[2] = t.Total();
                }
            };
            s.draw = [&s](SpriteBatch& b, const GameTime& t) {
                const float w = static_cast<float>(s.Graphics().BackBufferWidth());
                b.Begin();
                b.Draw(s.White(), Rectangle(static_cast<int>(std::fmod(static_cast<float>(t.Total()) * 400.0f, w)), 400, 40, 40), Color::Cyan);
                b.End();
            };
            s.readout = [counters] { return std::vector<std::string>{Format("updates last second: %.0f", (*counters)[3])}; };
        });
    add("TIME-03", "P1", "Tearing with vsync off",
        {"V toggles vsync. Watch the moving bars.", "Pass: tearing only with vsync off; no crash or stutter when toggling."},
        [](HumanScene& s) {
            auto vsync = std::make_shared<bool>(true);
            s.update = [&s, vsync](const GameTime&) {
                if (Keyboard::WasPressed(Key::V)) s.SetVSync(*vsync = !*vsync);
            };
            s.draw = [&s](SpriteBatch& b, const GameTime& t) {
                const int w = s.Graphics().BackBufferWidth(), h = s.Graphics().BackBufferHeight();
                const int x = static_cast<int>(std::fmod(t.Total() * 1500.0, w));
                b.Begin();
                for (int k = 0; k < 4; ++k) b.Draw(s.White(), Rectangle((x + k * w / 4) % w, 0, 30, h), Color::White);
                b.End();
            };
            s.readout = [vsync] { return std::vector<std::string>{"vsync " + Onoff(*vsync)}; };
        });
    add("AUD-01", "P0", "Sound bursts, heard",
        {"B plays 500 sounds at once. Press it repeatedly with the music on.",
         "Pass: the music never cuts out; the game does not hitch."},
        [](HumanScene& s) {
            auto blip = std::make_shared<SoundEffect>();
            auto music = std::make_shared<Music>();
            s.load = [blip, music] {
                std::vector<float> x(4800);
                for (size_t i = 0; i < x.size(); ++i) x[i] = 0.2f * std::sin(i * 0.12f) * (1.0f - i / 4800.0f);
                *blip = SoundEffect::FromSamples(x.data(), static_cast<int>(x.size()), 1, 48000);
                *music = Music::FromFile("content/loop.ogg");
                MediaPlayer::Play(*music);
            };
            s.update = [blip](const GameTime&) {
                if (Keyboard::WasPressed(Key::B))
                    for (int i = 0; i < 500; ++i) blip->Play(0.05f, (i % 12) / 12.0f);
            };
        });
    add("AUD-02", "P0", "Output device change",
        {"Music and a hum are playing. Unplug or plug in headphones; change the default output in Windows settings.",
         "Pass: sound continues on the new device within a second; no crash, no lasting silence."},
        [](HumanScene& s) {
            auto music = std::make_shared<Music>();
            auto hum = std::make_shared<SoundEffectInstance>();
            auto hums = std::make_shared<SoundEffect>();
            s.load = [music, hum, hums] {
                std::vector<float> x(48000);
                for (size_t i = 0; i < x.size(); ++i) x[i] = 0.1f * std::sin(6.2831853f * 110.0f * i / 48000.0f);
                *hums = SoundEffect::FromSamples(x.data(), 48000, 1, 48000);
                *hum = hums->CreateInstance();
                hum->SetLooped(true);
                hum->Play();
                *music = Music::FromFile("content/loop.ogg");
                MediaPlayer::Play(*music);
            };
        });
    add("AUD-03", "P1", "Music loop seam",
        {"Listen through several loops of the music (8 s each). N restarts the track.",
         "Pass: no gap or click at the loop point; restarting does not hitch."},
        [](HumanScene& s) {
            auto music = std::make_shared<Music>();
            s.load = [music] {
                *music = Music::FromFile("content/loop.ogg");
                MediaPlayer::Play(*music);
            };
            s.update = [music](const GameTime&) {
                if (Keyboard::WasPressed(Key::N)) MediaPlayer::Play(*music);
            };
        });
    add("AUD-04", "P1", "Live pitch, pan and volume",
        {"Hold Left/Right (pan), Up/Down (pitch), Q/A (volume) while the tone plays.",
         "Pass: changes sound smooth, no zipper noise or clicks."},
        [](HumanScene& s) {
            auto tone = std::make_shared<SoundEffect>();
            auto inst = std::make_shared<SoundEffectInstance>();
            s.load = [tone, inst] {
                std::vector<float> x(480);
                for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * std::sin(6.2831853f * i / 480.0f);
                *tone = SoundEffect::FromSamples(x.data(), 480, 1, 48000);
                *inst = tone->CreateInstance();
                inst->SetLooped(true);
                inst->Play();
            };
            s.update = [inst](const GameTime& t) {
                const KeyboardState k = Keyboard::GetState();
                const float d = t.Delta();
                if (k.IsKeyDown(Key::Left)) inst->SetPan(inst->Pan() - d);
                if (k.IsKeyDown(Key::Right)) inst->SetPan(inst->Pan() + d);
                if (k.IsKeyDown(Key::Up)) inst->SetPitch(inst->Pitch() + d);
                if (k.IsKeyDown(Key::Down)) inst->SetPitch(inst->Pitch() - d);
                if (k.IsKeyDown(Key::Q)) inst->SetVolume(inst->Volume() + d);
                if (k.IsKeyDown(Key::A)) inst->SetVolume(inst->Volume() - d);
            };
            s.readout = [inst] {
                return std::vector<std::string>{Format("pan %.2f   pitch %.2f   volume %.2f", inst->Pan(), inst->Pitch(), inst->Volume())};
            };
        });
    add("INP-01", "P0", "Real controllers",
        {"Connect an Xbox, a PlayStation, a Switch Pro and a generic controller in turn.",
         "Press every button and move every stick and trigger. Pass: each shows as the matching XNA button."},
        GamepadScene);
    add("INP-03", "P1", "Real sticks at rest, triggers",
        {"Leave the sticks alone, then press the triggers slowly.",
         "Pass: sticks read 0.00 at rest (raw values may not); triggers rise smoothly 0 to 1."},
        GamepadScene);
    add("INP-04", "P2", "Real rumble", {"Press 1-4 to rumble each player's pad, 0 to stop. Pass: rumble where the pad has motors, no crash where not."},
        GamepadScene);
    add("INP-05", "P1", "AZERTY and Dvorak",
        {"Switch Windows to AZERTY, then Dvorak. Press the key in the W position (top row, second letter).",
         "Pass: it shows as W on every layout: coldx keys are physical positions."},
        [](HumanScene& s) {
            s.readout = [] {
                std::string held;
                const KeyboardState k = Keyboard::GetState();
                const std::pair<Key, const char*> named[] = {{Key::W, "W"}, {Key::A, "A"}, {Key::S, "S"}, {Key::D, "D"}, {Key::Q, "Q"},
                                                             {Key::Z, "Z"}, {Key::Space, "Space"}, {Key::LeftShift, "LeftShift"}};
                for (auto [key, name] : named)
                    if (k.IsKeyDown(key)) held += std::string(name) + " ";
                return std::vector<std::string>{"physical keys held: " + held};
            };
        });
    add("INP-07", "P1", "Mouse, by hand",
        {"Scroll both ways; drag with the left button out of the window and back; hold a button and alt-tab away.",
         "Pass: wheel totals move one per notch; no button stays held after alt-tab."},
        [](HumanScene& s) {
            s.readout = [] {
                const MouseState m = Mouse::GetState();
                return std::vector<std::string>{Format("position %d, %d   left %d right %d middle %d   wheel %.1f   horizontal %.1f", m.x,
                                                       m.y, m.left, m.right, m.middle, m.scrollWheel, m.horizontalScrollWheel)};
            };
        });
    add("PLAT-01", "P0", "Linux, macOS, Steam Deck",
        {"Build and run ctest and coldx_framework_tests --auto on each platform. Pass: everything builds and passes."}, nullptr);
    add("PLAT-03", "P0", "Low-end hardware",
        {"On an old laptop with integrated graphics run coldx_framework_tests --auto --only GFX-08 and samples/demo.",
         "Pass: frame times reported; playable at a reduced sprite count."},
        nullptr);
    add("PLAT-04", "P1", "Alt-tab and focus",
        {"Alt-tab to another window and back; minimize and restore.", "Pass: no crash; focus readout follows; no jump in game time."},
        [](HumanScene& s) {
            s.readout = [&s] {
                SDL_Window* w = OnlyWindow();
                const bool focused = w && (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS);
                (void)s;
                return std::vector<std::string>{std::string("window focused: ") + (focused ? "yes" : "no")};
            };
        });
}

} // namespace ft
