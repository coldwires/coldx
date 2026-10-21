# coldx quickstart

Everything needed to write a game, on one page. The API follows XNA closely; where it
differs, "Different from XNA" says how.

## A complete game

`samples/quickstart/main.cpp`, built and run by CI:

<!-- sample: samples/quickstart/main.cpp -->
```cpp
#include <coldx/coldx.h>

using namespace coldx;

class MyGame : public Game {
public:
    MyGame() {
        Settings().title = "quickstart";
        Settings().company = "my studio";  // names the Storage folder
    }

protected:
    void LoadContent() override {
        // Content loads by name from the content folder beside the executable:
        // Content().Load<Texture2D>("player") reads content/player.png. Fonts load per size.
        font = Content().LoadFont("LiberationSans-Regular", 18.0f);
        // Textures can also be made in code. Colors are premultiplied.
        const Color white = Color::White;
        square = Texture2D::FromPixels(Graphics(), 1, 1, &white);
    }

    void Update(const GameTime& time) override {
        if (Keyboard::WasPressed(Key::Escape)) Exit();

        Vector2 move;
        const KeyboardState keys = Keyboard::GetState();
        if (keys.IsKeyDown(Key::Left)) move.x -= 1;
        if (keys.IsKeyDown(Key::Right)) move.x += 1;
        if (keys.IsKeyDown(Key::Up)) move.y -= 1;
        if (keys.IsKeyDown(Key::Down)) move.y += 1;
        // Sticks are up-positive; screen y grows downward.
        const Vector2 stick = GamePad::GetState(0).leftStick;
        move += Vector2(stick.x, -stick.y);
        position += move * 300.0f * time.Delta();

        if (Mouse::WasPressed(MouseButton::Left)) {
            const MouseState mouse = Mouse::GetState();
            position = Vector2(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
        }
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color::CornflowerBlue);
        batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        batch.Draw(square, Rectangle(static_cast<int>(position.x), static_cast<int>(position.y), 32, 32),
                   Color::Yellow);
        batch.DrawString(font, "Arrows or a stick move it. Click to place it.", Vector2(20, 20), Color::White);
        batch.End();
    }

private:
    SpriteBatch batch{Graphics()};  // fine before Run: GPU objects are made on first use
    Texture2D square;
    SpriteFont font;
    Vector2 position{100, 100};
};

int main() {
    MyGame game;
    return game.Run();
}
```

`samples/demo` goes further: components, content by name, sound, music, a sprite
sheet drawn by tile name, screen shake through the batch transform, and a saved best score.

Its CMake file:

```cmake
add_subdirectory(coldx)                  # the coldx checkout
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE coldx::core)
coldx_game(my_game)                      # windowed program on Windows, main() kept
```

Content is loaded from a `content` folder beside the executable; copy it there in a
`POST_BUILD` step, as `samples/quickstart/CMakeLists.txt` does for its font.

## The API

All in namespace `coldx`, all from `#include <coldx/coldx.h>`.

**Game.** Subclass `Game`; override any of `Initialize()`, `LoadContent()`,
`UnloadContent()`, `Update(const GameTime&)`, `Draw(const GameTime&)`,
`OnResize(int w, int h)`, `OnFocusChanged(bool)`, `OnExiting()`. Call `Run()` from `main`,
`Exit()` to quit. `Graphics()` is the `GraphicsDevice`. `Settings()` is read when `Run`
starts: `title`, `width`, `height`, `resizable`, `fullscreen`, `vsync`, `fixedTimeStep`
(default true), `targetElapsedSeconds` (1/60), `maxUpdatesPerFrame` (5), `console`
(`ConsoleMode::DebugBuilds`/`Always`/`Never`), `company`, `name`. While running:
`SetFullscreen(bool)`, `ToggleFullscreen()`, `IsFullscreen()`, `SetWindowSize(w, h)`,
`SetWindowTitle`, `SetVSync`. Fullscreen is borderless at the desktop's resolution unless
`fullscreenMode = FullscreenMode::Exclusive` (with `fullscreenWidth`, `fullscreenHeight`,
`fullscreenRefreshRate`, 0 = the desktop's) or, while running,
`SetFullscreenMode(FullscreenMode::Exclusive, 1920, 1080)`. `DisplayModes()` lists what the
display offers (`DisplayMode`: `width`, `height`, `refreshRate`), `DesktopDisplayMode()`.

**Virtual resolution.** `Settings().virtualWidth = 320; virtualHeight = 180;` draws at a
fixed size scaled into the window: the back buffer is 320x180 whatever the window, and
mouse positions are in its pixels. `scaleMode`: `ScaleMode::Fit` (default, bars),
`Integer` (whole multiples, for pixel art), `Stretch`. `smoothScaling` (false: sharp),
`letterboxColor` (black). While running: `SetVirtualResolution(w, h)` (0, 0 to follow the
window again), `SetScaleMode(mode, smooth)`. Window resizes do not call `OnResize` then;
`Graphics().PresentedArea()` is where the image sits in the window.

**Components.** `class Hud : public DrawableGameComponent` with `Initialize`,
`LoadContent`, `UnloadContent`, `Update`, `Draw`; construct with the game. Add with
`Components().Emplace<Hud>(*this)` (returns `Hud&`) or `Components().Add(std::make_unique<Hud>(*this))`;
`Components().Remove(hud)`. `SetUpdateOrder`, `SetDrawOrder` (higher draws on top),
`SetEnabled`, `SetVisible`, `GetGame()`, `Graphics()`. They run after the game's own
`Update` and `Draw`.

**GameTime.** `Delta()` seconds (float), `Total()` seconds (double), `IsRunningSlowly()`,
`Alpha()`. By default a fixed-timestep game draws once per update, so a 144 Hz display
shows 60 frames. `Settings().drawBetweenUpdates = true` draws every display frame;
`Alpha()` (0 to just under 1, in `Draw`) is how far the frame is toward the next update:
keep each object's previous and current position and draw
`previous + (current - previous) * time.Alpha()`.

**GraphicsDevice.** `Clear(Color)`, `SetRenderTarget(const RenderTarget2D*)` (`nullptr` =
back buffer), `BackBufferWidth()`, `BackBufferHeight()`, `PresentedArea()`,
`GetBackBufferData(Color*)` (the last finished frame). Clear and render targets only work
inside `Draw`.

**ContentManager.** `Content().Load<Texture2D>("sprites/player")` reads
`content/sprites/player.png`; also `Load<SoundEffect>` (.wav, .ogg), `Load<Music>` (.ogg),
`LoadFont("name", pixelHeight)` (.ttf). Cached by name; `Unload()` drops the cache.
`SetRootDirectory`. Your own types: `Content().RegisterLoader<Level>({".lvl"}, [](const std::string& path) { ... return Level{...}; });`
then `Content().Load<Level>("levels/one")`.

**Texture2D.** `Texture2D::FromFile(Graphics(), "content/x.png")`,
`Texture2D::FromPixels(Graphics(), w, h, const Color*)`, `SetData(const Color*)`,
`GetData(Color*)`, `GetData(Rectangle, Color*)`, `SaveAsPng(path)`, `Width()`, `Height()`,
`IsEmpty()`. A handle: copies share one texture. Create in `LoadContent` or later.
`GetData` waits for the GPU (fine for screenshots and collision masks, not every frame);
inside `Draw` it does not see that frame's drawing yet. A screenshot: draw into a
`RenderTarget2D`, then `SaveAsPng` it from `Update`.

**RenderTarget2D.** A `Texture2D` you can draw into: `RenderTarget2D::Create(Graphics(), w, h)`.

**SpriteBatch.** `SpriteBatch batch{Graphics()};` as a member.
`Begin(SpriteSortMode, BlendState, SamplerState, Matrix transform)`, all defaulted;
`End()`.
`Draw(texture, Vector2 position, Color)`;
`Draw(texture, Vector2 position, std::optional<Rectangle> source, Color)`;
`Draw(texture, Vector2 position, source, Color, float rotation, Vector2 origin, float or Vector2 scale, SpriteEffects, float layerDepth)`;
`Draw(texture, Rectangle destination, Color)`;
`Draw(texture, Rectangle destination, source, Color)`;
`Draw(texture, Rectangle destination, source, Color, float rotation, Vector2 origin, SpriteEffects, float layerDepth)`;
`DrawString(font, text, Vector2 position, Color)`;
`DrawString(font, text, position, Color, rotation, origin, float or Vector2 scale, SpriteEffects, layerDepth)`.
- `SpriteSortMode`: `Deferred` (default), `Immediate`, `Texture`, `BackToFront`, `FrontToBack`.
- `BlendState`: `AlphaBlend` (default), `Additive`, `NonPremultiplied`, `Opaque`.
- `SamplerState`: `PointClamp`, `LinearClamp` (default), `PointWrap`, `LinearWrap`.
- `SpriteEffects`: `None`, `FlipHorizontally`, `FlipVertically`, combine with `|`.

**SpriteFont.** `SpriteFont::FromFile(Graphics(), "content/font.ttf", pixelHeight)`,
`FromMemory(Graphics(), data, size, pixelHeight)`, `MeasureString(text)` returns `Vector2`,
`LineSpacing()`/`SetLineSpacing`, `Spacing()`/`SetSpacing`,
`DefaultCharacter()`/`SetDefaultCharacter` (`?` by default, `std::nullopt` to skip),
`HasCharacter(char32_t)`. Text is UTF-8; ASCII and Latin-1 are baked.

**Keyboard.** `Keyboard::GetState()` returns `KeyboardState` with `IsKeyDown(Key)`,
`IsKeyUp(Key)`. `Keyboard::WasPressed(Key)`, `WasReleased(Key)`. Keys: `Key::A`..`Key::Z`,
`Key::D0`..`Key::D9`, `Key::F1`..`Key::F12`, `Up`, `Down`, `Left`, `Right`, `Space`,
`Enter`, `Escape`, `Tab`, `Backspace`, `LeftShift`, `RightShift`, `LeftControl`,
`LeftAlt`, and the rest in `keyboard.h`. Keys are physical positions (`Key::W` is the
same key on AZERTY). `Keyboard::KeyName(Key)` is what it is called on the current layout,
for showing bindings; `Keyboard::KeyForCharacter('w')` is the key that types w.

**TextInput.** For typed text (names, chat): `TextInput::Start()` (from `Initialize` on),
`Stop()`, `IsActive()`. `TextInput::GetText()` is the UTF-8 typed since the previous
Update, accents and IME text included; append it to your string. Read Backspace and Enter
from `Keyboard`. With an IME: `Composition()`, `CompositionCursor()`, and
`SetCaretArea(Rectangle)` to place its candidate list. `Clipboard::GetText()`,
`SetText(s)`, `HasText()`.

**Mouse.** `Mouse::GetState()` returns `MouseState` with `x`, `y`, `Position()`, `left`,
`right`, `middle`, `x1`, `x2`, `IsDown(MouseButton)`, `scrollWheel`,
`horizontalScrollWheel`. `Mouse::WasPressed(MouseButton)`, `WasReleased`,
`ScrollWheelDelta()`, `HorizontalScrollWheelDelta()`, `SetPosition(x, y)`,
`SetCursorVisible(bool)`.

**GamePad info.** `GamePad::GetCapabilities(player)`: `isConnected`, `name`, `type`
(`GamePadType::XboxOne`, `PS5`, `SwitchPro`, ...), `hasRumble`, `vendorId`, `productId`.
`GamePad::FaceButtonLabel(player, Buttons::A)` is what the pad prints there
(`ButtonLabel::A`, `Cross`, or `B` on Nintendo pads), for button prompts.

**GamePad.** `GamePad::GetState(player, GamePadDeadZone = IndependentAxes)` returns
`GamePadState` with `isConnected`, `leftStick`, `rightStick` (Vector2), `leftTrigger`,
`rightTrigger` (0..1), `IsButtonDown(Buttons)`, `IsButtonUp`.
`GamePad::WasPressed(player, Buttons)`, `WasReleased`, `SetVibration(player, low, high)`.
`Buttons`: `A`, `B`, `X`, `Y`, `Back`, `Start`, `Guide`, `LeftShoulder`, `RightShoulder`,
`LeftStick`, `RightStick`, `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight`. Players 0 to 3.

**SoundEffect.** `SoundEffect::FromFile("content/x.wav")` (WAV or OGG),
`FromSamples(const float*, frames, channels, rate)`, `Play(volume = 1, pitch = 0, pan = 0)`
(pitch in octaves, -1..1; pan -1..1), `CreateInstance()`, `Duration()`,
`SoundEffect::SetMasterVolume`. Up to `SoundEffect::MaxVoices` (256) at once; past that
`Play` returns false. Volume and pan can change every frame: they ramp, so no clicks.

**SoundEffectInstance.** `Play()`, `Pause()`, `Resume()`, `Stop()`, `State()`
(`SoundState::Stopped`/`Playing`/`Paused`), `SetVolume`, `SetPitch`, `SetPan`,
`SetLooped`. Move-only; destroying it stops it.

**Music and MediaPlayer.** `Music::FromFile("content/x.ogg")`;
`MediaPlayer::Play(music, looped = true)`, `CrossFade(music, seconds, looped = true)`,
`Stop()`, `FadeOut(seconds)`, `Pause()`, `Resume()`, `IsPlaying()`, `SetVolume`. One track
at a time; during a crossfade the old one fades out under it.

**Storage.** `Storage::WriteFile("name.txt", contents)`, `ReadFile("name.txt")` returns
`std::optional<std::string>`, `Exists`, `Remove`, `Folder()`. Plain file names only.

**Math.** `Vector2` (`x`, `y`, arithmetic, `Zero()`, `One()`), `Point`, `Rectangle`
(`x`, `y`, `width`, `height`, `Contains`, `Intersects`, `Left/Right/Top/Bottom`),
`Color` (`r`, `g`, `b`, `a`; `Color::White`, `Black`, `Red`, `Green`, `Blue`, `Yellow`,
`Cyan`, `Magenta`, `Gray`, `Transparent`, `CornflowerBlue`; `color * 0.5f` fades),
`Matrix` (`Identity()`, `CreateTranslation`, `CreateScale`, `CreateRotationZ`,
`CreateOrthographicOffCenter`, `Invert`, `*`; `Transform(Vector2, Matrix)`),
`MathHelper::Pi`, `TwoPi`, `PiOver2`, `ToRadians`, `ToDegrees`, `Clamp`, `Lerp`.

**Log.** `Log("score %d", score);` printf-style, to the console and the log file.

## Different from XNA

- C++: value types are structs with lowercase fields (`v.x`, `rect.width`, `color.a`);
  static members are functions (`Vector2::Zero()`, `Matrix::Identity()`); `Color::White`
  is a constant.
- Colors are premultiplied everywhere. Half-transparent white is `Color::White * 0.5f`,
  not `Color(255, 255, 255, 128)`. Use `BlendState::NonPremultiplied` for straight-alpha art.
- `SpriteBatch::Begin` takes sort, blend, sampler, then the transform. There is no
  `Effect` parameter yet, and no depth-stencil or rasterizer state.
- A `SpriteFont` is a TTF baked at one pixel size, loaded at runtime. For another size,
  bake another font; scaling one blurs it.
- `Keyboard::WasPressed` and friends are built in: no need to keep last frame's state.
- `GetState` returns the state as of the start of the current `Update`, not live.
- Content loads loose files by name (`Content().Load<Texture2D>("player")`); packed
  content files are not written yet. Fonts load with `LoadFont(name, size)`.
- `Texture2D`, `SpriteFont` and the audio types are handles, like XNA's reference types:
  a copy shares the resource.
- Music is `Music` plus `MediaPlayer`, like XNA's `Song`; `SoundEffect.Play` takes pitch in
  octaves as XNA does.
- Textures and fonts need the graphics device: create them in `LoadContent`, not the
  constructor. `SpriteBatch` is the exception.
- No exceptions: a failed load returns an empty object and logs why.
- Thumbstick y is up-positive, as in XNA; screen y grows downward.
- On Windows, `<windows.h>` declares a function `Rectangle`, so with `using namespace
  coldx` the name is ambiguous. Define `NOGDI` before including it, or write
  `coldx::Rectangle`.

## Running without a screen

    COLDX_HEADLESS=1 COLDX_FRAMES=60 COLDX_LOG=run.log COLDX_CAPTURE=frame.png ./my_game
    COLDX_HEADLESS=1 COLDX_INPUT=script.txt COLDX_CAPTURE=frame.png ./my_game

The first runs 60 frames with no window and saves the log and the last frame. The second
plays a script of input (`10 key down Left`, `30 mouse move 100 200`, `31 mouse down left`,
`40 text hello`, `60 exit`). `SDL_GPU_DRIVER=vulkan` or `direct3d12` picks the GPU backend. The README has
the full list, and how to write shaders.
