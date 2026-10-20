# coldx

A C++20 framework for 2D games in the style of XNA, on SDL3 and its GPU API. A game
subclasses `Game` and overrides `Initialize`, `LoadContent`, `Update` and `Draw`.

```cpp
#include <coldx/coldx.h>

using namespace coldx;

class MyGame : public Game {
protected:
    void Update(const GameTime&) override {
        if (Keyboard::GetState().IsKeyDown(Key::Escape)) Exit();
    }
    void Draw(const GameTime&) override {
        Graphics().Clear(Color::CornflowerBlue);
    }
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

Together they run a game unattended and leave a log and a picture of its last frame:

    COLDX_HEADLESS=1 COLDX_FRAMES=60 COLDX_LOG=run.log COLDX_CAPTURE=frame.png ./hello_sprite

On exit the log records the frame count, update count, frames that needed catch-up updates,
and total game time.

## Layout

    include/coldx/       public headers
    src/                 implementation
    samples/             each sample is a separate game target
    third_party/         vendored dependencies, versions in VERSIONS.md

## Status

Early. The window, game loop, clear color, keyboard state and the debug options work.
Textures, SpriteBatch, fonts, audio, the other input devices and content loading are not
written yet.
Tested on Windows 11 with MSVC (Direct3D 12 backend). Linux and macOS have not been built.

## License

MIT. See `LICENSE`. SDL, under `third_party/SDL`, is under the zlib license.
