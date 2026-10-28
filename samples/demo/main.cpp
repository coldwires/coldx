// The coldx demo: one small game that uses all of coldx. Catch stars and gems, dodge
// meteors, put your name on the high score table.
//
// Move with the arrows or WASD, a gamepad stick, or hold the left mouse button. P (or
// Start) pauses, M toggles the music, the mouse wheel sets the volume, F11 fullscreen,
// F12 saves a screenshot, V switches vsync, F3 shows where the image sits, F9 ends the run
// (to try the game-over screens), Escape goes back and then quits.
//
// Where each part of coldx is used:
//   Game hooks, settings, fullscreen, vsync, window title, display modes   this file
//   Pixel art: Settings().pixelArt, 640x360 scaled by whole numbers         this file
//   Components, draw order, enabling                                        all files
//   GameTime: Delta, Total, Alpha (drawn between updates), IsRunningSlowly  play.h, screens.h
//   Content: textures, sounds, music, fonts, ReadBytes, a custom loader     this file, screens.h
//   Packed content: encrypted, fingerprinted, watermarked (CMakeLists.txt)  content/content.coldx
//   Texture2D: FromPixels, GetData (collision masks), SetData (the radar),
//     SaveAsPng (screenshots). RenderTarget2D (the playfield)             world.h, play.h, this file
//   SpriteBatch: every sort mode but Immediate, every blend state, the
//     wrap sampler, flips, depth, scale, a transform (screen shake)         play.h, screens.h
//   Effect: grayscale (pause), flash (hit), wave (title)                    play.h, screens.h
//   SpriteFont: sharp pixel font, MeasureString, LineSpacing, HasCharacter  screens.h
//   Keyboard, KeyName, KeyForCharacter. Mouse, wheel, cursor. GamePad,
//     vibration, capabilities, button labels. TextInput, Clipboard          this file, screens.h
//   SoundEffect with pitch and pan. FromSamples. A looping instance.
//     MediaPlayer play, pause, resume, fade out, crossfade                  this file, play.h
//   Storage (high scores, screenshots). Log. COLDX_HIDDEN. Matrix, Rectangle,
//     Point, MathHelper                                                     everywhere
//
// Not shown: Immediate sort mode (an effect test uses it), SetFullscreenMode for exclusive
// fullscreen, SetVirtualResolution while running, Mount for a second package,
// Load<Effect> (the effects are compiled into the game here, so building the demo needs no
// Vulkan SDK. See QUICKSTART "Custom shaders" for packing them instead).

#include <coldx/coldx.h>
#include <coldx/protect/hidden.h>

#include <string>

#include "compiled/flash.h"
#include "compiled/grayscale.h"
#include "compiled/wave.h"
#include "play.h"
#include "screens.h"

using namespace coldx;
using namespace demo;

class Demo : public Game {
public:
    // profile: "demo --profile test" keeps a scripted test's scores apart from yours.
    explicit Demo(const std::string& profile) {
        Settings().title = "coldx demo";
        Settings().company = "coldx";
        if (!profile.empty()) Settings().name = "coldx demo " + profile;
        Settings().resizable = true;
        // The game is 640x360. The window shows it at whole multiples, bars filling the rest,
        // and every SpriteBatch samples point-wise and snaps to whole pixels.
        Settings().pixelArt = true;
        Settings().virtualWidth = Width;
        Settings().virtualHeight = Height;
        // Draw every display frame, between the 60 Hz updates (see GameTime::Alpha).
        Settings().drawBetweenUpdates = true;
        Components().Emplace<Background>(*this, world_);
        fallers_ = &Components().Emplace<Fallers>(*this, world_);
        Components().Emplace<Compositor>(*this, world_, *fallers_);
        screens_ = &Components().Emplace<Screens>(*this, world_, Actions{[this] { Start(); }, [this] { Exit(); }});
    }

protected:
    void Initialize() override {
        // Scrambled in the executable: not findable as text.
        Log("%s", COLDX_HIDDEN("coldx demo: every part of coldx in one game").c_str());
        const DisplayMode desktop = DesktopDisplayMode();
        Log("demo: desktop %dx%d at %.0f Hz, %zu display modes", desktop.width, desktop.height, desktop.refreshRate,
            DisplayModes().size());
        Log("demo: saving to %s", Storage::Folder().c_str());
    }

    void LoadContent() override {
        ContentManager& content = Content();
        // The demo's own file format: levels/waves.wave, packed as data.
        content.RegisterDataLoader<Waves>({".wave"}, [](const std::vector<std::uint8_t>& bytes, const std::string&) {
            return Waves::Parse(bytes);
        });
        world_.sheet = content.Load<Texture2D>("sheet");
        world_.waves = content.Load<Waves>("levels/waves");
        // Silkscreen is drawn on a grid of 125 font units, 8 to the em. 10.24 px (ascender
        // to descender) puts one of its pixels on one screen pixel, 20.48 on two.
        world_.font = content.LoadFont("silkscreen/Silkscreen-Regular", 10.24f, FontSmoothing::Sharp);
        world_.big = content.LoadFont("silkscreen/Silkscreen-Regular", 20.48f, FontSmoothing::Sharp);
        world_.pickup = content.Load<SoundEffect>("pickup");
        world_.hit = content.Load<SoundEffect>("hit");
        world_.music = content.Load<Music>("loop");

        const Color white = Color::White;
        world_.pixel = Texture2D::FromPixels(Graphics(), 1, 1, &white);
        world_.nebula = MakeNebula();
        world_.field = RenderTarget2D::Create(Graphics(), Width, Height);
        world_.shipMask = Mask::From(world_.sheet, sheet::Get("ship"));
        world_.meteorMask = Mask::From(world_.sheet, sheet::Get("meteor"));
        world_.gray = Effect::FromMemory(Graphics(), effects::grayscale, sizeof(effects::grayscale), "grayscale");
        world_.flash = Effect::FromMemory(Graphics(), effects::flash, sizeof(effects::flash), "flash");
        world_.wave = Effect::FromMemory(Graphics(), effects::wave, sizeof(effects::wave), "wave");

        // An engine hum made here, not loaded: two detuned sines, one second, looped.
        std::vector<float> samples(44100);
        for (size_t i = 0; i < samples.size(); ++i) {
            const float t = static_cast<float>(i) / 44100.0f;
            samples[i] = 0.15f * std::sin(MathHelper::TwoPi * 55.0f * t) + 0.08f * std::sin(MathHelper::TwoPi * 82.0f * t);
        }
        world_.hum = SoundEffect::FromSamples(samples.data(), static_cast<int>(samples.size()), 1, 44100);
        world_.engine = world_.hum.CreateInstance();
        world_.engine.SetLooped(true);
        world_.engine.SetVolume(0.0f);

        world_.scores.Load();
        world_.Say(Storage::Exists(HighScores::File) ? "welcome back" : "welcome: catch the stars, dodge the meteors");
        musicKey_ = Keyboard::KeyForCharacter('m');  // the key that types m, on any layout
        MediaPlayer::SetVolume(0.5f);
        MediaPlayer::Play(world_.music);
    }

    void UnloadContent() override {
        world_.engine.Stop();
        GamePad::SetVibration(0, 0.0f, 0.0f);
    }

    void OnFocusChanged(bool focused) override {
        if (!focused && world_.Playing()) Pause(true);
    }

    void OnExiting() override { Log("demo: exiting, best %d", world_.scores.Best()); }

    void Update(const GameTime& time) override {
        const float dt = time.Delta();
        world_.shake = std::max(0.0f, world_.shake - dt);
        world_.flashLeft = std::max(0.0f, world_.flashLeft - dt);
        if (world_.rumbleLeft > 0.0f && (world_.rumbleLeft -= dt) <= 0.0f) GamePad::SetVibration(0, 0.0f, 0.0f);
        GlobalKeys();
        if (world_.screen == Screen::NameEntry && !TextInput::IsActive()) screens_->BeginNameEntry();
        if (cursorShown_ == world_.Playing()) Mouse::SetCursorVisible(cursorShown_ = !world_.Playing());

        if (world_.Playing()) {
            if (Keyboard::WasPressed(Key::P) || GamePad::WasPressed(0, Buttons::Start)) {
                Pause(true);
                return;
            }
            if (Keyboard::WasPressed(Key::F9)) fallers_->Crash();
            MovePlayer(dt);
        } else if (world_.screen == Screen::Paused) {
            if (Keyboard::WasPressed(Key::P) || GamePad::WasPressed(0, Buttons::Start)) Pause(false);
        }
        const bool engineOn = world_.Playing();
        if (engineOn && world_.engine.State() != SoundState::Playing) world_.engine.Play();
        if (!engineOn && world_.engine.State() == SoundState::Playing) world_.engine.Pause();
        // The hum follows the ship's speed and side.
        const float speed = std::abs(world_.player.x - world_.previousPlayer.x) / std::max(dt, 0.001f);
        world_.engine.SetVolume(MathHelper::Clamp(0.15f + speed / 600.0f, 0.0f, 0.5f));
        world_.engine.SetPitch(MathHelper::Clamp(speed / 400.0f - 0.2f, -0.5f, 0.6f));
        world_.engine.SetPan((world_.player.x / Width) * 2.0f - 1.0f);

        const std::string title = world_.Playing() ? "coldx demo - score " + std::to_string(world_.score) : "coldx demo";
        if (title != windowTitle_) SetWindowTitle(windowTitle_ = title);
    }

    void Draw(const GameTime&) override {
        // The playfield components draw into the render target. The Compositor puts it on
        // the back buffer and the screens draw over it.
        Graphics().SetRenderTarget(&world_.field);
        Graphics().Clear(Color(8, 10, 20));
    }

private:
    void Start() {
        world_.score = 0;
        world_.lives = 3;
        world_.player = world_.previousPlayer = Vector2(Width / 2.0f, Height - 30.0f);
        world_.screen = Screen::Play;
        fallers_->Reset();
        // From the fade of the last game over, or from the title's playing track.
        MediaPlayer::CrossFade(world_.music, 1.0f);
        Log("demo: run started");
    }

    void Pause(bool pause) {
        world_.screen = pause ? Screen::Paused : Screen::Play;
        if (pause) MediaPlayer::Pause();
        else MediaPlayer::Resume();
        fallers_->SetEnabled(!pause);  // a disabled component gets no Update
    }

    void GlobalKeys() {
        if (Keyboard::WasPressed(Key::Escape) && world_.screen != Screen::NameEntry) {
            if (world_.screen == Screen::Title) Exit();
            else if (world_.screen == Screen::Play) Pause(true);
            else if (world_.screen == Screen::Paused) {
                Pause(false);
                world_.screen = Screen::Title;
                MediaPlayer::Play(world_.music);
            }
        }
        if (Keyboard::WasPressed(Key::F11)) ToggleFullscreen();
        if (Keyboard::WasPressed(Key::F3)) world_.debug = !world_.debug;
        if (Keyboard::WasPressed(Key::V) && world_.screen != Screen::NameEntry) {
            vsync_ = !vsync_;
            SetVSync(vsync_);
            world_.Say(vsync_ ? "vsync on" : "vsync off");
        }
        if (Keyboard::WasPressed(musicKey_) && world_.screen != Screen::NameEntry) {
            if (MediaPlayer::IsPlaying()) MediaPlayer::Stop();
            else MediaPlayer::Play(world_.music);
        }
        if (const float wheel = Mouse::ScrollWheelDelta(); wheel != 0.0f) {
            volume_ = MathHelper::Clamp(volume_ + wheel * 0.1f, 0.0f, 1.0f);
            SoundEffect::SetMasterVolume(volume_);
            MediaPlayer::SetVolume(volume_ * 0.5f);
            world_.Say("volume " + std::to_string(static_cast<int>(std::round(volume_ * 100))) + "%");
        }
        if (Keyboard::WasPressed(Key::F12)) Screenshot();
    }

    // The last finished frame, read back and written as a PNG beside the high scores.
    void Screenshot() {
        const int w = Graphics().BackBufferWidth(), h = Graphics().BackBufferHeight();
        std::vector<Color> pixels(static_cast<size_t>(w * h));
        if (!Graphics().GetBackBufferData(pixels.data())) return;
        const Texture2D shot = Texture2D::FromPixels(Graphics(), w, h, pixels.data());
        const std::string path = Storage::Folder() + "screenshot-" + std::to_string(++screenshots_) + ".png";
        if (shot.SaveAsPng(path)) world_.Say("saved screenshot-" + std::to_string(screenshots_) + ".png");
    }

    void MovePlayer(float dt) {
        world_.previousPlayer = world_.player;
        Vector2 move;
        const KeyboardState keys = Keyboard::GetState();
        if (keys.IsKeyDown(Key::Left) || keys.IsKeyDown(Key::A)) move.x -= 1;
        if (keys.IsKeyDown(Key::Right) || keys.IsKeyDown(Key::D)) move.x += 1;
        const GamePadState pad = GamePad::GetState(0);
        move.x += pad.leftStick.x;
        if (pad.IsButtonDown(Buttons::DPadLeft)) move.x -= 1;
        if (pad.IsButtonDown(Buttons::DPadRight)) move.x += 1;
        world_.player.x += MathHelper::Clamp(move.x, -1.0f, 1.0f) * 210.0f * dt;
        // The mouse is in the game's 640x360 pixels, whatever the window's size. Undo the
        // shake so the ship goes where the pointer is, not where the shaken screen shows it.
        const MouseState mouse = Mouse::GetState();
        if (mouse.left) {
            Matrix unshake;
            Vector2 target(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
            if (Matrix::Invert(world_.ShakeTransform(), unshake)) target = Transform(target, unshake);
            world_.player.x = MathHelper::Lerp(world_.player.x, target.x, std::min(1.0f, 12.0f * dt));
        }
        world_.player.x = MathHelper::Clamp(world_.player.x, 16.0f, Width - 16.0f);
    }

    // 64x64 of dim purple cloud that tiles: sums of sines with whole periods across it.
    Texture2D MakeNebula() {
        std::vector<Color> pixels(64 * 64);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                const float u = MathHelper::TwoPi * x / 64.0f, v = MathHelper::TwoPi * y / 64.0f;
                const float n = 0.5f + 0.25f * std::sin(u + 2 * std::sin(v)) + 0.25f * std::sin(2 * v + std::cos(u));
                const auto c = [n](float k) { return static_cast<std::uint8_t>(MathHelper::Clamp(n * k, 0.0f, 255.0f)); };
                pixels[static_cast<size_t>(y * 64 + x)] = Color(c(28), c(12), c(40));
            }
        }
        return Texture2D::FromPixels(Graphics(), 64, 64, pixels.data());
    }

    World world_;
    Fallers* fallers_ = nullptr;
    Screens* screens_ = nullptr;
    Key musicKey_ = Key::M;
    std::string windowTitle_;
    float volume_ = 1.0f;
    bool vsync_ = true;
    bool cursorShown_ = true;
    int screenshots_ = 0;
};

int main(int argc, char** argv) {
    const std::string profile = argc == 3 && std::string(argv[1]) == "--profile" ? argv[2] : "";
    Demo game(profile);
    return game.Run();
}
