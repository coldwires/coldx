# coldx quickstart

Everything needed to write a game, on one page: a game can be written from this alone.
Pointers to headers and samples are for extras, not needed reading. The API follows XNA
closely; where it differs, "Different from XNA" says how. Then "Packing content" for a
game's content folder, "Shipping a release" for what a release build adds, and "Running
without a screen" for testing a game unattended.

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

`samples/demo` goes further, and is pixel-perfect: components, packed content, sound,
music, a sprite sheet drawn by tile name, screen shake, and a saved best score.

A game's CMake file:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_game LANGUAGES C CXX)
# The coldx checkout. Inside the game's folder, add_subdirectory(coldx) is enough; outside
# it, give a build folder: add_subdirectory(C:/src/coldx ${CMAKE_BINARY_DIR}/coldx).
add_subdirectory(coldx)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE coldx::core)   # brings C++20 with it
coldx_game(my_game)                      # windowed program on Windows, main() kept
coldx_add_content(my_game PROJECT content/content.coldx)   # packs content/ beside the exe
```

Configure with `-DCOLDX_BUILD_TESTS=OFF -DCOLDX_BUILD_SAMPLES=OFF` to build only what the
game needs.

Content lives in a `content` folder; `coldx_add_content` packs it into `content.rime`
beside the executable on every build ("Packing content"). A game can also just copy its
`content` folder beside the executable and load loose files, as
`samples/quickstart/CMakeLists.txt` does for its font.

## Pixel-perfect 2D

coldx games are expected to be pixel-perfect: every texel of the art lands on whole
screen pixels, the same size everywhere, and nothing blurs or shimmers. The recipe, all
of which `samples/demo` uses:

```cpp
Settings().virtualWidth = 640;                 // draw at a fixed size...
Settings().virtualHeight = 360;
Settings().scaleMode = ScaleMode::Integer;     // ...scaled by whole numbers into the window
batch.SetPixelSnap(true);                      // sprites and text on whole pixels
batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
batch.Draw(sheet, position, tile, Color::White, 0.0f, Vector2(16, 16), 1.0f,   // scale 1, 2, 3...
           SpriteEffects::None, 0.0f);
font = Content().LoadFont("myfont", 12, FontSmoothing::Sharp);   // no grey edges
```

- Draw at whole scales (1, 2, 3); 1.5x makes some texels one pixel wide and others two.
- `SamplerState::PointClamp` everywhere; the default `LinearClamp` blurs.
- Rotation other than by flips makes pixel art jagged; draw rotated frames instead.
- Keep a camera or shake transform on whole pixels: `Matrix::CreateTranslation(std::round(x),
  std::round(y))`. Snapping happens before the transform.
- Atlases need no padding: a sprite never samples outside its source rectangle.
- A pixel font (designed on a pixel grid) at its design size, or a multiple, looks best
  with `FontSmoothing::Sharp`; an ordinary font made sharp is legible but rough.

## The API

All in namespace `coldx`, all from `#include <coldx/coldx.h>`.

**Game.** Subclass `Game`; override any of `Initialize()`, `LoadContent()`,
`UnloadContent()`, `Update(const GameTime&)`, `Draw(const GameTime&)`,
`OnResize(int w, int h)`, `OnFocusChanged(bool)`, `OnExiting()`. Call `Run()` from `main`,
`Exit()` to quit. `Graphics()` is the `GraphicsDevice`. `Settings()` is read when `Run`
starts: `title`, `width` and `height` (1280x720), `resizable`, `fullscreen`, `vsync`,
`fixedTimeStep` (default true), `targetElapsedSeconds` (1/60), `maxUpdatesPerFrame` (5),
`console` (`ConsoleMode::DebugBuilds`/`Always`/`Never`), `company`, `name` (see Storage).
While running:
`SetFullscreen(bool)`, `ToggleFullscreen()`, `IsFullscreen()`, `SetWindowSize(w, h)`,
`SetWindowTitle`, `SetVSync`. Fullscreen is borderless at the desktop's resolution unless
`fullscreenMode = FullscreenMode::Exclusive` (with `fullscreenWidth`, `fullscreenHeight`,
`fullscreenRefreshRate`, 0 = the desktop's) or, while running,
`SetFullscreenMode(FullscreenMode::Exclusive, 1920, 1080)`. `DisplayModes()` lists what the
display offers (`DisplayMode`: `width`, `height`, `refreshRate`), `DesktopDisplayMode()`.

**Virtual resolution.** `Settings().virtualWidth = 320; Settings().virtualHeight = 180;`
draws at a fixed size scaled into the window: the back buffer is 320x180 whatever the
window, and mouse positions are in its pixels. Also in `Settings()`: `scaleMode`
(`ScaleMode::Fit`, the default, with bars; `Integer`, whole multiples, for pixel art;
`Stretch`), `smoothScaling` (false: sharp), `letterboxColor` (black). While running: `SetVirtualResolution(w, h)` (0, 0 to follow the
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

**ContentManager.** `Content().Load<Texture2D>("sprites/player")` finds
`sprites/player.png`; also `Load<SoundEffect>` (.wav, .ogg), `Load<Music>` (.ogg),
`LoadFont("name", pixelHeight)` (.ttf, any size; `LoadFont(name, size,
FontSmoothing::Sharp)` for crisp edges). A name is the file's path inside the
content folder without its extension: `content/fonts/main.ttf` is `LoadFont("fonts/main",
18)`, `content/coin.png` is `Load<Texture2D>("coin")`. A texture and a sound may share a
name (`coin.png`, `coin.wav`): each type looks for its own extensions. Cached by name;
`Unload()` drops the cache. It looks in `content.rime` beside the executable first (mounted on the first load),
then in loose files under `content/` (`SetRootDirectory` to change it). `Mount("dlc.rime")`
adds a package, searched before the ones mounted earlier. Release builds read loose files
only while no package is mounted; `SetLooseFiles(LooseFiles::Always)` or `Never` to choose.
`ReadBytes("levels/one.lvl")` gives a file's bytes, extension included. Your own types:
`Content().RegisterDataLoader<Level>({".lvl"}, [](const std::vector<std::uint8_t>& bytes, const std::string& name) { ... return Level{...}; });`
then `Content().Load<Level>("levels/one")`; it works packed and loose. (`RegisterLoader`,
which takes a file path, works for loose files only.)

**Texture2D.** PNGs are premultiplied as they load, so ordinary PNG art with transparency
draws correctly with the default `BlendState::AlphaBlend`.
`Texture2D::FromFile(Graphics(), "content/x.png")`,
`Texture2D::FromPixels(Graphics(), w, h, const Color*)`, `SetData(const Color*)`,
`GetData(Color*)`, `GetData(Rectangle, Color*)`, `SaveAsPng(path)`, `Width()`, `Height()`,
`IsEmpty()`. A handle: copies share one texture. Create in `LoadContent` or later.
`GetData` waits for the GPU (fine for screenshots and collision masks, not every frame);
inside `Draw` it does not see that frame's drawing yet. A screenshot: draw into a
`RenderTarget2D`, then `SaveAsPng` it from `Update`.

**RenderTarget2D.** A `Texture2D` you can draw into: `RenderTarget2D::Create(Graphics(), w, h)`.

**SpriteBatch.** `SpriteBatch batch{Graphics()};` as a member.
`Begin(SpriteSortMode, BlendState, SamplerState, Matrix transform)`, all defaulted;
`End()`. `SetPixelSnap(true)` puts unrotated sprites and text on whole pixels (lasts
until changed).
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
`FromMemory(Graphics(), data, size, pixelHeight)`, both with an optional
`FontSmoothing::Smooth` (default) or `Sharp` (every pixel fully on or off); `MeasureString(text)` returns `Vector2`,
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

**GamePad.** `GamePad::GetState(player, GamePadDeadZone = IndependentAxes)` returns
`GamePadState` with `isConnected`, `leftStick`, `rightStick` (Vector2), `leftTrigger`,
`rightTrigger` (0..1), `IsButtonDown(Buttons)`, `IsButtonUp`.
`GamePad::WasPressed(player, Buttons)`, `WasReleased`, `SetVibration(player, low, high)`.
`Buttons`: `A`, `B`, `X`, `Y`, `Back`, `Start`, `Guide`, `LeftShoulder`, `RightShoulder`,
`LeftStick`, `RightStick`, `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight`. Players 0 to 3.

**GamePad info.** `GamePad::GetCapabilities(player)`: `isConnected`, `name`, `type`
(`GamePadType::XboxOne`, `PS5`, `SwitchPro`, ...), `hasRumble`, `vendorId`, `productId`.
`GamePad::FaceButtonLabel(player, Buttons::A)` is what the pad prints there
(`ButtonLabel::A`, `Cross`, or `B` on Nintendo pads), for button prompts.

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
`std::optional<std::string>`, `Exists`, `Remove`, `Folder()`. Plain file names only. The
folder is the per-user one named by `Settings().company` and `Settings().name` (the title
if unset): `%APPDATA%\company\name\` on Windows, `~/.local/share/...` on Linux.

**Math.** `Vector2` (`x`, `y`, arithmetic, `Zero()`, `One()`), `Point`, `Rectangle`
(`x`, `y`, `width`, `height`, `Contains`, `Intersects`, `Left/Right/Top/Bottom`),
`Color` (`r`, `g`, `b`, `a`; `Color::White`, `Black`, `Red`, `Green`, `Blue`, `Yellow`,
`Cyan`, `Magenta`, `Gray`, `Transparent`, `CornflowerBlue`; `color * 0.5f` fades),
`Matrix` (`Identity()`, `CreateTranslation`, `CreateScale`, `CreateRotationZ`,
`CreateOrthographicOffCenter`, `Invert`, `*`; `Transform(Vector2, Matrix)`),
`MathHelper::Pi`, `TwoPi`, `PiOver2`, `ToRadians`, `ToDegrees`, `Clamp`, `Lerp`.

**Log.** `Log("score %d", score);` printf-style, to the console and the log file.

**Not in coldx.** Random numbers (use `<random>`), physics, networking, and custom
shaders while drawing (not yet). These are the game's own, or come from libraries.

## Different from XNA

- C++: value types are structs with lowercase fields (`v.x`, `rect.width`, `color.a`);
  static members are functions (`Vector2::Zero()`, `Matrix::Identity()`); `Color::White`
  is a constant.
- Colors are premultiplied everywhere. Half-transparent white is `Color::White * 0.5f`,
  not `Color(255, 255, 255, 128)`. PNGs are premultiplied on load; use
  `BlendState::NonPremultiplied` only for pixels you made straight-alpha yourself.
- A sprite drawn from part of a texture never shows the texels around that part, with
  `PointClamp` or `LinearClamp`, at any scale: atlases need no padding (XNA and FNA bleed
  neighbouring texels in here).
- `SpriteBatch::Begin` takes sort, blend, sampler, then the transform. There is no
  `Effect` parameter yet, and no depth-stencil or rasterizer state.
- A `SpriteFont` is a TTF baked at one pixel size, loaded at runtime. For another size,
  bake another font; scaling one blurs it.
- `Keyboard::WasPressed` and friends are built in: no need to keep last frame's state.
- `GetState` returns the state as of the start of the current `Update`, not live.
- Content loads by name (`Content().Load<Texture2D>("player")`) from a `.rime` package
  coldx's own tool builds, or from loose files; there is no XNB step. Fonts load with
  `LoadFont(name, size)`.
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

## Packing content

A content project, `content/content.coldx`, lists what goes in the package; names inside
it are paths relative to that folder, the same names `Load` uses:

```
seed = my game                 # makes this game's name hashes its own
[texture] sprites/*.png        # * within a folder, ** across folders
[sound]   sounds/**/*.wav sounds/**/*.ogg
[music]   music/*.ogg
[font]    fonts/*.ttf
[data]    levels/*.lvl         # anything else, as bytes: ReadBytes or RegisterDataLoader
[texture] shared/*.png
from = ../../shared            # a section can take files from another folder
```

`coldx_add_content(my_game PROJECT content/content.coldx)` runs `coldxc` whenever a file
there changes and copies `content.rime` beside the game; unchanged content is not
repacked. Under a `[texture]` section, `format = raw` stores decoded pixels (no PNG decode
at load, a little larger). Entries are compressed where that helps. A file matched by two
sections, an unknown setting or a broken PNG fails the build, naming the file and line.
Put `content/** -text` in the game's `.gitattributes` so line endings never change what is
packed. A game's own asset types can get a processor of their own: see
`include/coldx/content/build.h`.

## Shipping a release

All optional; each is one line.

- **Release build flags.** `coldx_harden(my_game)`, and configure coldx with
  `-DCOLDX_HARDEN=ON`: no RTTI (`dynamic_cast` and `typeid` stop working on the game's own
  types), whole-program optimization, and nothing in the executable naming the build
  machine. Keep the `.pdb` beside the build for crash reports; do not ship it.
- **Encrypted content.** `coldxc --new-key game.key` once (keep the file out of public
  repositories), then `coldx_add_content(my_game PROJECT ... KEY_FILE game.key)`. The game
  gets the key by itself; no code. Stops casual extraction, not a determined reader.
- **Watermarks.** Under a section of the content project: `watermark = alpha` for pixel
  art (alpha 254 on scattered opaque pixels: invisible, palette exact), `watermark = rgb`
  for painted art, `watermark = on` for `[sound]` (WAV only). Marks are the project's own,
  or a build's with `BUILD_ID "name"`. `coldxc --project content/content.coldx --identify
  ripped.png` says whether a file carries the mark, with the odds of chance; a single
  24x24 sprite cut from a sheet is enough. They survive a re-save, cropping and trimming;
  not resizing or lossy re-encoding.
- **Build fingerprints.** `BUILD_ID "beta-tester-07"` (or `-DCOLDX_BUILD_ID=...`) marks the
  package and the executable; `coldxc --project ... --identify game.exe` reads it back.
- **Hidden strings.** `#include <coldx/protect/hidden.h>`, link `coldx::protect`, and
  `COLDX_HIDDEN("text")` gives a `std::string` whose text is not in the executable.

## Running without a screen

    COLDX_HEADLESS=1 COLDX_FRAMES=60 COLDX_LOG=run.log COLDX_CAPTURE=frame.png ./my_game
    COLDX_HEADLESS=1 COLDX_INPUT=script.txt COLDX_LOG=run.log COLDX_CAPTURE=frame.png ./my_game

The first runs 60 frames with no window and saves the log (also printed to stdout) and the
last frame, at the back buffer's size (the virtual resolution, if set). The second plays
a script of input, one command a line, each applied just before the numbered Update (1 is
the first):

    10 key down Left       # held until a key up; SDL key names: Left, Space, Escape, A...
    40 key up Left
    50 mouse move 100 200  # back buffer pixels
    51 mouse down left
    52 mouse up left
    60 text hello          # typed text, if the game has TextInput on
    90 exit                # stops before Update 90 runs

`SDL_GPU_DRIVER=vulkan` or `direct3d12` picks the GPU backend. Headless audio is silent,
so check sounds through the game's own log lines. Read the log and look at the captured PNG
to check a change. The README has the full list, and how to write shaders.
