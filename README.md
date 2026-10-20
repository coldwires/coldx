# coldx

A C++20 framework for 2D games in the style of XNA, on SDL3 and its GPU API. A game
subclasses `Game` and overrides `Initialize`, `LoadContent`, `Update` and `Draw`.

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

SDL's own `SDL_GPU_DRIVER=vulkan` (or `direct3d12`, `metal`) picks the GPU backend.

Together they run a game unattended and leave a log and a picture of its last frame:

    COLDX_HEADLESS=1 COLDX_FRAMES=60 COLDX_LOG=run.log COLDX_CAPTURE=frame.png ./hello_sprite

On exit the log records the frame count, update count, frames that needed catch-up updates,
and total game time.

## Layout

    include/coldx/       public headers
    src/                 implementation
    shaders/             HLSL sources, the shared include, compiled headers
    cmake/               shader compilation
    samples/             each sample is a separate game target
    third_party/         vendored dependencies, versions in VERSIONS.md

## Status

Early. The window, game loop, clear color, keyboard state, the debug options, textures and
a basic `SpriteBatch` work. Sort modes, blend and sampler choices, rotation and scale,
render targets, fonts, audio, the other input devices and content loading are not written yet.

Tested on Windows 11 with MSVC, on the Direct3D 12 and Vulkan backends. Linux and macOS have
not been built, so the MSL output has never run.

## License

MIT. See `LICENSE`. SDL, under `third_party/SDL`, is under the zlib license.
