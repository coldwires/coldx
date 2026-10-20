# core

**Owns:** the game loop, window, GPU device, textures, sprite drawing, keyboard input,
math types, logging and the debug options.
**Layer:** 0
**Depends on:** none
**Third-party:** SDL3
**Public headers:** `include/coldx/core/`, all included by `include/coldx/coldx.h`
**Library target:** `coldx::core`
**Size:** ~1.9k lines

## Public surface

- `Game`: subclass it and override `Initialize`, `LoadContent`, `UnloadContent`, `Update`,
  `Draw`, `OnResize`, `OnFocusChanged`, `OnExiting`. `Run` opens the window and loops.
  `Settings()` (`GameSettings`) is read when `Run` starts.
- `GameTime`: `Delta`, `Total`, `IsRunningSlowly`.
- `GraphicsDevice`: `Clear`, back buffer size. Exists from `Game` construction; usable from
  `Initialize`.
- `Texture2D`: `FromFile` (PNG), `FromPixels`, `SetData`. Move-only, premultiplied alpha.
- `SpriteBatch`: `Begin`, `Draw` (position, destination, source, tint), `End`.
- `Keyboard`, `KeyboardState`, `Key`.
- `Color`, `Vector2`, `Rectangle`.

## Inside

- `game.cpp`: `Run` and the loop. `debug_options.*`: `COLDX_*` variables, console, log file.
- `graphics_device.cpp` + `graphics_access.h`: the device, the internal back buffer, frame
  begin and end, capture. `GraphicsAccess` is how the rest of core reaches the device,
  command buffer and back buffer; `DeviceToken` lets resources outlive the device safely.
- `texture2d.cpp` + `texture_access.h`, `sprite_batch.cpp`, `keyboard.cpp`.
- `shader_blob.*`: creates an SDL shader from a compiled header in the format the device takes.
  Built-in shader headers are in `shaders/compiled/`.

## Invariants

- No SDL type appears in a public header.
- Drawing goes into the internal back buffer, never straight into the swapchain.
- The GPU device lives until `~Game`; `Run` only lets go of the window.
- GPU resources release only if their `DeviceToken` still holds a device.
- Log lines start with `coldx.core:`.

## Gotchas

- A fragment shader's input must match its vertex output, `SV_Position` included. D3D12
  rejects the pipeline otherwise; Vulkan does not. Test drawing on both backends.
- `SpriteBatch::End` and `Clear` do nothing outside `Draw`: there is no command buffer.
- Textures cannot be created before `Run`; the device does not exist yet.

## Tests

`ctest -C Debug -R core` from the build directory. `core.math` checks the value types;
`core.render.<backend>` draws a scene headless and compares pixels to computed values, on
each backend in `COLDX_TEST_GPU_DRIVERS`.

## Open work

`docs/STATUS.md` lists what is not built yet.
