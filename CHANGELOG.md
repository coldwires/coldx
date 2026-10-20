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
- `hello_sprite` sample.
