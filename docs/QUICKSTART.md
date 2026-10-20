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
        // Textures are premultiplied. FromFile loads a PNG beside the executable.
        const Color white = Color::White;
        square = Texture2D::FromPixels(Graphics(), 1, 1, &white);
        font = SpriteFont::FromFile(Graphics(), "content/LiberationSans-Regular.ttf", 18.0f);
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

Its CMake file:

```cmake
add_subdirectory(coldx)                  # the coldx checkout
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE coldx::core)
coldx_game(my_game)                      # windowed program on Windows, main() kept
```

Content is loaded relative to the executable's folder; copy it there in a `POST_BUILD`
step, as `samples/quickstart/CMakeLists.txt` does for its font.

## The API

All in namespace `coldx`, all from `#include <coldx/coldx.h>`.

**Game.** Subclass `Game`; override any of `Initialize()`, `LoadContent()`,
`UnloadContent()`, `Update(const GameTime&)`, `Draw(const GameTime&)`,
`OnResize(int w, int h)`, `OnFocusChanged(bool)`, `OnExiting()`. Call `Run()` from `main`,
`Exit()` to quit. `Graphics()` is the `GraphicsDevice`. `Settings()` is read when `Run`
starts: `title`, `width`, `height`, `resizable`, `fullscreen`, `vsync`, `fixedTimeStep`
(default true), `targetElapsedSeconds` (1/60), `maxUpdatesPerFrame` (5), `console`
(`ConsoleMode::DebugBuilds`/`Always`/`Never`), `company`, `name`.

**Components.** `class Hud : public DrawableGameComponent` with `Initialize`,
`LoadContent`, `UnloadContent`, `Update`, `Draw`; construct with the game. Add with
`Components().Emplace<Hud>(*this)` (returns `Hud&`) or `Components().Add(std::make_unique<Hud>(*this))`;
`Components().Remove(hud)`. `SetUpdateOrder`, `SetDrawOrder` (higher draws on top),
`SetEnabled`, `SetVisible`, `GetGame()`, `Graphics()`. They run after the game's own
`Update` and `Draw`.

**GameTime.** `Delta()` seconds (float), `Total()` seconds (double), `IsRunningSlowly()`.

**GraphicsDevice.** `Clear(Color)`, `SetRenderTarget(const RenderTarget2D*)` (`nullptr` =
back buffer), `BackBufferWidth()`, `BackBufferHeight()`. Clear and render targets only
work inside `Draw`.

**Texture2D.** `Texture2D::FromFile(Graphics(), "content/x.png")`,
`Texture2D::FromPixels(Graphics(), w, h, const Color*)`, `SetData(const Color*)`,
`Width()`, `Height()`, `IsEmpty()`. Move-only. Create in `LoadContent` or later.

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
`LeftAlt`, and the rest in `keyboard.h`.

**Mouse.** `Mouse::GetState()` returns `MouseState` with `x`, `y`, `Position()`, `left`,
`right`, `middle`, `x1`, `x2`, `IsDown(MouseButton)`, `scrollWheel`,
`horizontalScrollWheel`. `Mouse::WasPressed(MouseButton)`, `WasReleased`,
`ScrollWheelDelta()`, `HorizontalScrollWheelDelta()`, `SetPosition(x, y)`,
`SetCursorVisible(bool)`.

**GamePad.** `GamePad::GetState(player, GamePadDeadZone = IndependentAxes)` returns
`GamePadState` with `isConnected`, `leftStick`, `rightStick` (Vector2), `leftTrigger`,
`rightTrigger` (0..1), `IsButtonDown(Buttons)`, `IsButtonUp`.
`GamePad::WasPressed(player, Buttons)`, `WasReleased`, `SetVibration(player, low, high)`.
`Buttons`: `A`, `B`, `X`, `Y`, `Back`, `Start`, `Guide`, `LeftShoulder`, `RightShoulder`,
`LeftStick`, `RightStick`, `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight`. Players 0 to 3.

**SoundEffect.** `SoundEffect::FromFile("content/x.wav")` (WAV or OGG),
`FromSamples(const float*, frames, channels, rate)`, `Play(volume = 1, pitch = 0, pan = 0)`
(pitch in octaves, -1..1; pan -1..1), `CreateInstance()`, `Duration()`,
`SoundEffect::SetMasterVolume`. Up to `SoundEffect::MaxVoices` (64) at once.

**SoundEffectInstance.** `Play()`, `Pause()`, `Resume()`, `Stop()`, `State()`
(`SoundState::Stopped`/`Playing`/`Paused`), `SetVolume`, `SetPitch`, `SetPan`,
`SetLooped`. Move-only; destroying it stops it.

**Music and MediaPlayer.** `Music::FromFile("content/x.ogg")`;
`MediaPlayer::Play(music, looped = true)`, `Stop()`, `Pause()`, `Resume()`, `IsPlaying()`,
`SetVolume`. One track at a time.

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
- Content loads by path relative to the executable, not through a ContentManager (yet).
- Music is `Music` plus `MediaPlayer`, like XNA's `Song`; `SoundEffect.Play` takes pitch in
  octaves as XNA does.
- Textures and fonts need the graphics device: create them in `LoadContent`, not the
  constructor. `SpriteBatch` is the exception.
- No exceptions: a failed load returns an empty object and logs why.
- Thumbstick y is up-positive, as in XNA; screen y grows downward.

## Running without a screen

    COLDX_HEADLESS=1 COLDX_FRAMES=60 COLDX_LOG=run.log COLDX_CAPTURE=frame.png ./my_game
    COLDX_HEADLESS=1 COLDX_INPUT=script.txt COLDX_CAPTURE=frame.png ./my_game

The first runs 60 frames with no window and saves the log and the last frame. The second
plays a script of input (`10 key down Left`, `30 mouse move 100 200`, `31 mouse down left`,
`60 exit`). `SDL_GPU_DRIVER=vulkan` or `direct3d12` picks the GPU backend. The README has
the full list, and how to write shaders.
