# coldx

A C++20 framework for 2D games in the style of XNA, on SDL3 and its GPU API. A game
subclasses `Game` and overrides `Initialize`, `LoadContent`, `Update` and `Draw`.

`docs/QUICKSTART.md` has a complete game, the whole API on one page, and what differs
from XNA. Start there. `samples/demo` is a small game that uses most of coldx at once.

```cpp
#include <coldx/coldx.h>

using namespace coldx;

class MyGame : public Game {
protected:
    void LoadContent() override {
        player = Texture2D::FromFile(Graphics(), "content/player.png");
    }
    void Update(const GameTime&) override {
        if (Keyboard::GetState().IsKeyDown(Key::Escape)) Exit();
    }
    void Draw(const GameTime&) override {
        Graphics().Clear(Color::CornflowerBlue);
        batch.Begin();
        batch.Draw(player, Vector2(100, 100), Color::White);
        batch.End();
    }

private:
    SpriteBatch batch{Graphics()};
    Texture2D player;
};

int main() {
    MyGame game;
    return game.Run();
}
```

## Building

Needs CMake 3.24 or newer and a C++20 compiler. All dependencies are in `third_party/`, so
the build works offline.

    cmake -S . -B build
    cmake --build build --config Debug

The samples build when coldx is the top-level project. Set `COLDX_BUILD_SAMPLES` to
override that.

## Using it from a game

Add coldx as a subdirectory and link the library:

```cmake
add_subdirectory(coldx)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE coldx::core)
coldx_game(my_game)
```

`coldx_game` makes the executable a windowed program on Windows, so no console window
opens with it, while keeping `main` as the entry point.

Games include headers from `include/coldx/` only. Everything under `src/` is private.

## Drawing sprites

`SpriteBatch::Begin` takes a sort mode, a blend state, a sampler state and a transform, all
optional:

```cpp
batch.Begin(SpriteSortMode::BackToFront, BlendState::AlphaBlend, SamplerState::PointClamp,
            Matrix::CreateTranslation(-cameraX, -cameraY));
batch.Draw(texture, position, source, Color::White, rotation, origin, scale,
           SpriteEffects::FlipHorizontally, layerDepth);
batch.End();
```

`SamplerState::PointClamp` keeps pixel art sharp when scaled; the default, `LinearClamp`,
smooths it. Rotation is in radians, clockwise on screen. Deferred sprites are drawn at
`End`, into whatever render target is current then; `Immediate` draws at each call.

`RenderTarget2D` is a texture that can be drawn into:

```cpp
Graphics().SetRenderTarget(&target);
// draw
Graphics().SetRenderTarget(nullptr);
batch.Begin();
batch.Draw(target, Vector2(0, 0), Color::White);
batch.End();
```

## Text

```cpp
font = SpriteFont::FromFile(Graphics(), "content/LiberationSans-Regular.ttf", 18.0f);
// ...
batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
batch.DrawString(font, "Hello", Vector2(40, 40), Color::White);
batch.End();
```

A `SpriteFont` is a TrueType font baked at one pixel height into an atlas: printable ASCII
and Latin-1. Text is UTF-8. Glyphs land on whole pixels, so at a whole-pixel position with
`PointClamp` the text is exactly the rasterized font. For another size, bake another font;
scaling a bake blurs it. Characters the font lacks draw as `DefaultCharacter`, `?` by
default. `MeasureString` gives the size `DrawString` will cover.

`samples/hello_text` shows it at 18 px; Up and Down change the size.

`Log` writes a printf-style line to the same log as coldx's own messages.

## Components

```cpp
class Hud : public DrawableGameComponent {
public:
    using DrawableGameComponent::DrawableGameComponent;
    void Draw(const GameTime&) override { /* ... */ }
};
// in the game's constructor:
Components().Emplace<Hud>(*this).SetDrawOrder(10);
```

A `GameComponent` has its own `Initialize` and `Update`; a `DrawableGameComponent` adds
`LoadContent`, `UnloadContent` and `Draw`. The game owns them. They run after the game's
own methods, in `UpdateOrder` and `DrawOrder`. Adding, removing or reordering in the middle
of an update or draw takes effect after it.

## Input

```cpp
if (Keyboard::WasPressed(Key::Space)) Jump();
const MouseState mouse = Mouse::GetState();
const GamePadState pad = GamePad::GetState(0);
position += pad.leftStick * speed * time.Delta();
```

`Keyboard`, `Mouse` and `GamePad` return the state as of the start of the current
`Update`, so it does not change halfway through one. `WasPressed` and `WasReleased` compare
it with the previous `Update`. Keys are physical positions: `Key::W` is the same key on
QWERTY and AZERTY. Mouse positions are back buffer pixels; the wheel counts notches, up
positive. Up to four pads, by player index, connect and disconnect while the game runs;
sticks are -1 to 1 with up positive, after a dead zone (`GamePadDeadZone`).

## Sound and music

```cpp
jump = SoundEffect::FromFile("content/jump.wav");   // WAV or OGG, decoded into memory
jump.Play();                                       // fire and forget
engine = jump.CreateInstance();                    // one playback you control
engine.SetLooped(true);
engine.Play();
engine.SetPitch(0.5f);                             // octaves: half an octave up

theme = Music::FromFile("content/theme.ogg");     // OGG, decoded as it plays
MediaPlayer::Play(theme);                          // loops by default
```

Up to 64 sounds play at once, plus one music track. Sounds load any time; playing needs
`Run` started. Headless runs use SDL's silent dummy audio driver. `samples/hello_audio`
shows all of it.

## Saving settings and games

```cpp
Settings().company = "my studio";       // in the game's constructor
Storage::WriteFile("settings.txt", text);
if (auto saved = Storage::ReadFile("settings.txt")) Apply(*saved);
```

`Storage` keeps files in a per-user folder the game can always write to, named by
`GameSettings::company` and `name` (the title by default): on Windows
`%APPDATA%\<company>\<name>`. A write replaces the whole file through a temporary, so a
crash never leaves half a file.

## Content

```cpp
player = Content().Load<Texture2D>("sprites/player");     // content/sprites/player.png
jump = Content().Load<SoundEffect>("sounds/jump");        // .wav or .ogg
font = Content().LoadFont("fonts/main", 18.0f);           // .ttf, baked at 18 px
```

The `ContentManager` finds the extension itself and caches by name. It reads a
`content.rime` package beside the executable if there is one, then loose files from
`content/`. Games teach it their own types with `RegisterDataLoader`. `Texture2D`,
`SpriteFont` and the audio types are handles: a copy shares the loaded data, which is
freed when the last copy and the cache let go.

### Packing content

A content project lists what goes in the package:

```
# content/content.coldx
seed = my game
[texture] sprites/*.png
[sound]   sounds/**/*.wav
[music]   music/*.ogg
[font]    fonts/*.ttf
[data]    levels/*.lvl
```

```cmake
coldx_add_content(my_game PROJECT content/content.coldx)
```

`coldxc` builds `content.rime` beside the game whenever a file in the folder changes, and
leaves it alone when nothing did. Entries that compress (WAV, fonts, data) are stored with
LZ4; `format = raw` under a `[texture]` section stores decoded pixels, which load without
a PNG decode for a little more space. Names in the package are hashed, never stored. The same
`Load` calls work packed and loose; release builds stop reading loose files once a
package is mounted. A game adds its own asset types with a `coldx::content::Processor`
and a tool of its own that calls `coldx::content::RunTool` with it (see
`include/coldx/content/build.h`).

## Textures and colors

Textures hold premultiplied alpha. `Texture2D::FromFile` loads a PNG and premultiplies it;
`FromPixels` and `SetData` expect pixels that already are. A relative path in `FromFile` is
taken from the executable's folder. Tints are premultiplied too: `Color::White * 0.5f` is
white at half opacity.

## Writing shaders

Shaders are HLSL, compiled ahead of time to SPIR-V (Vulkan), DXIL (Direct3D 12) and MSL
(Metal). The game never compiles a shader at runtime.

Compiling needs `dxc` and `spirv-cross`, both in the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
for Windows, Linux and macOS. CMake finds them through the `VULKAN_SDK` environment variable
the installer sets. Only shader authors need it: compiled shaders are committed as headers, so
building coldx does not.

Include `coldx.hlsli` from `shaders/include/`. Its macros declare textures and uniform
blocks in the registers SDL_GPU expects on each backend, so a shader names a slot number
and nothing else:

```hlsl
#include "coldx.hlsli"

CX_FRAGMENT_TEXTURE(image, imageSampler, 0);
CX_FRAGMENT_UNIFORMS(Params, 0) { float4 tint; };

struct Input {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};

float4 main(Input input) : SV_Target0 {
    return image.Sample(imageSampler, input.uv) * input.color * tint;
}
```

Name files `*.vert.hlsl` or `*.frag.hlsl`; the entry point is `main`. A fragment shader's
input must list the same members as its vertex shader's output, in order, `SV_Position`
included. The comments at the top of `coldx.hlsli` give the rest of the rules.

`coldx_compile_shaders` turns shaders into headers:

```cmake
coldx_compile_shaders(my_shaders
    OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled
    SOURCES shaders/glow.frag.hlsl)
```

Building `my_shaders` writes `glow_frag.h`. Rebuilding with unchanged sources gives
byte-identical headers, so committed ones only change when a shader does. coldx's own
shaders are regenerated with `cmake --build build --target coldx_builtin_shaders`.

Using a custom shader when drawing is not available yet. `SpriteBatch` uses its built-in pair.

## Release builds

```
cmake -S . -B build -DCOLDX_HARDEN=ON
```

```cmake
coldx_harden(my_game)
```

`COLDX_HARDEN` builds coldx and its samples in Release without RTTI and with whole-program
optimization, and leaves out what identifies the build machine: tool versions, the build
time, folder paths. `coldx_harden` does the same for a game's own executable; the game
then cannot use `dynamic_cast` or `typeid` on its own types. Symbols go to a `.pdb` beside
the executable: keep it for crash reports, do not ship it. The flags are listed in
`cmake/coldx_harden.cmake`.

```
coldxc --new-key game.key          # once; keep it out of public repositories
```

```cmake
coldx_add_content(my_game PROJECT content/content.coldx
    KEY_FILE game.key              # encrypt the package; the game gets the key itself
    BUILD_ID "beta-tester-07")     # fingerprint the package and the game
```

```
coldxc --project content/content.coldx --identify suspicious.exe
```

`COLDX_HIDDEN("text")` (link `coldx::protect`) keeps a string out of the executable as
text. None of this stops a determined reader of the game: the key and the strings are in
the client. It stops casual extraction and makes leaked builds traceable.

## Console window

On Windows, debug builds open a console window for log output and release builds do not.
`GameSettings::console` changes that per game: `ConsoleMode::DebugBuilds` (the default),
`Always` or `Never`. No console opens when stdout and stderr are already redirected to a file
or pipe; output goes there instead. Other platforms log to the terminal that launched the game.

## Debug options

Environment variables, read when `Run` starts. They work on any game without changes to it.

| Variable | Effect |
|---|---|
| `COLDX_CONSOLE=1` or `0` | Show or hide the console window, overriding `GameSettings::console` |
| `COLDX_LOG=<path>` | Also write every log line to this file, with a timestamp, flushed per line |
| `COLDX_HEADLESS=1` | Keep the window hidden and present nothing. Drawing still happens, into a back buffer of `GameSettings` width x height. Error dialogs are skipped. |
| `COLDX_FRAMES=<n>` | Exit after drawing n frames |
| `COLDX_CAPTURE=<path.png>` | On exit, save the last drawn frame as a PNG |
| `COLDX_INPUT=<path>` | Scripted input: lines like `10 key down Up`, `20 mouse move 100 200`, `21 mouse down left`, `30 wheel 1`, `35 text hello` (while the game has `TextInput` on), `40 exit`, applied before the numbered Update |

SDL's own `SDL_GPU_DRIVER=vulkan` (or `direct3d12`, `metal`) picks the GPU backend.

Together they run a game unattended and leave a log and a picture of its last frame:

    COLDX_HEADLESS=1 COLDX_FRAMES=60 COLDX_LOG=run.log COLDX_CAPTURE=frame.png ./hello_sprite

On exit the log records the frame count, update count, frames that needed catch-up updates,
and total game time.

## Tests

    cmake --build build --config Debug
    cd build && ctest -C Debug --output-on-failure

`coldx_framework_tests` (in `tests/framework/`) stress-tests the framework the way a large
game would: resizing and fullscreen, dozens of cached render targets, sprite throughput,
timestep accuracy and stalls, sound bursts, loop seams, gamepads through SDL's virtual pads,
save locations. `--auto` runs everything that can be measured and writes a report; with no
arguments it opens a menu of the checks a person has to judge (sharpness, hearing, real
controllers), each with on-screen instructions. It opens real windows and goes fullscreen.

`samples/stress` draws 10,000 moving sprites with vsync off and logs frames per second; pass
a count as its first argument for more.

`check` runs `tools/check.py`, which enforces the module boundaries in
`docs/ARCHITECTURE.md`; it needs Python 3 and is skipped without it. `core.math` covers the
value types and `Matrix`. `core.render.<backend>` and `core.sprite.<backend>` draw scenes
headless and compare every pixel that matters to a value computed from the same inputs:
every sort mode, blend and sampler state, transforms, rotation, flips and render targets.
They run once per GPU backend
(`COLDX_TEST_GPU_DRIVERS`: Direct3D 12 and Vulkan on Windows by default).

## Layout

    include/coldx/       public headers
    src/<module>/        implementation, MODULE.md, tests/
    shaders/             HLSL sources, the shared include, compiled headers
    cmake/               shader compilation, coldx_add_content
    samples/             each sample is a separate game target
    tests/               shared test helpers, the framework stress tests
    tools/               coldxc, check.py
    docs/                architecture, status, flows, decisions
    third_party/         vendored dependencies, versions in VERSIONS.md

## Status

Early. `docs/STATUS.md` lists what is done and what is not started. Tested on Windows 11
with MSVC, on the Direct3D 12 and Vulkan backends; Linux and macOS have not been built.

## License

MIT. See `LICENSE`. SDL, under `third_party/SDL`, is under the zlib license.
