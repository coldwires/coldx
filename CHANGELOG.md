# Changelog

## Unreleased

- `Game` with a fixed or variable timestep loop, window settings, and resize, focus and exit hooks.
- `GraphicsDevice::Clear` on SDL_GPU.
- `Keyboard::GetState` with `Key` values for the common keys.
- `Color` with a few named colors.
- Console window in debug builds only on Windows, set by `GameSettings::console`.
- `COLDX_*` debug options: log file, headless runs, frame limit, PNG capture of the last frame.
- `coldx_game` CMake function.
- `Texture2D`: PNG loading with premultiplied alpha, raw pixels, `SetData`.
- `SpriteBatch` with position, destination and source rectangles, and tint.
- `Vector2`, `Rectangle`, `Color * float`.
- HLSL shader pipeline: `coldx.hlsli` and `coldx_compile_shaders`, compiling to SPIR-V, DXIL
  and MSL with resource counts, into committed headers.
- `SpriteBatch`: sort modes, blend and sampler states, transform matrix, rotation, origin,
  scale, flips and layer depth.
- `RenderTarget2D` and `GraphicsDevice::SetRenderTarget`.
- `Matrix`, `Point`, `MathHelper`.
- `Log` for games.
- `COLDX_LOG` files can be read while the game runs.
- `SpriteFont` baked from a TTF, `SpriteBatch::DrawString`, `MeasureString`: UTF-8,
  kerning, newlines, Latin-1, a default character.
- `Mouse` and `GamePad`; `WasPressed`/`WasReleased` on all three input devices. Input is
  event-driven and snapshotted per Update.
- `Storage` for settings and saves, and `GameSettings::company`/`name`.
- `COLDX_INPUT` scripted input.
- `GameComponent`, `DrawableGameComponent` and `Game::Components()`.
- Window control while running: fullscreen toggle, size, title, vsync. `OnResize`
  fires only on a real change.
- `ContentManager` (`Game::Content()`): loose files by name, a cache, `LoadFont`,
  `RegisterLoader` for game types.
- `Texture2D`, `RenderTarget2D` and `SpriteFont` are copyable handles: copies share.
- Audio: `SoundEffect` (WAV, OGG), `SoundEffectInstance`, streaming `Music` and
  `MediaPlayer`, on a lock-free software mixer. Headless runs use a silent driver.
- `hello_audio` sample. Every test has a CTest timeout.
- `demo` sample: a small game using components, ContentManager, input, sound, music,
  text, a named-tile sprite sheet and Storage.
- `coldx_framework_tests`: automated and human-judged stress tests with a report.
- `hello_sprite`, `hello_text`, `quickstart` and `stress` samples. `hello_text` saves its
  text size.
- `docs/QUICKSTART.md`: a complete game, the API on one page, differences from XNA. Its
  example is `samples/quickstart`, kept identical by `tools/check.py`.
- CTest suite: math, headless render tests per GPU backend, and `tools/check.py` for
  module boundaries, file size and ASCII.
- Log lines name their module: `coldx.core:`.
