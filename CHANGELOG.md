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
- `hello_sprite`, `hello_text` and `stress` samples. `hello_text` saves its text size.
- CTest suite: math, headless render tests per GPU backend, and `tools/check.py` for
  module boundaries, file size and ASCII.
- Log lines name their module: `coldx.core:`.
