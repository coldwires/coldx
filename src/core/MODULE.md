# core

**Owns:** the game loop, window, GPU device, textures, render targets, sprite drawing,
keyboard input, math types, logging and the debug options.
**Layer:** 0
**Depends on:** none
**Third-party:** SDL3
**Public headers:** `include/coldx/core/`, all included by `include/coldx/coldx.h`
**Library target:** `coldx::core`
**Size:** ~3.2k lines

## Public surface

- `Game`: subclass it and override `Initialize`, `LoadContent`, `UnloadContent`, `Update`,
  `Draw`, `OnResize`, `OnFocusChanged`, `OnExiting`. `Run` opens the window and loops.
  `Settings()` (`GameSettings`) is read when `Run` starts.
- `GameTime`: `Delta`, `Total`, `IsRunningSlowly`.
- `GraphicsDevice`: `Clear`, `SetRenderTarget`, back buffer size. Exists from `Game`
  construction; usable from `Initialize`.
- `Texture2D`: `FromFile` (PNG), `FromPixels`, `SetData`. Move-only, premultiplied alpha.
- `RenderTarget2D`: a `Texture2D` that can be drawn into. `Create`.
- `SpriteBatch`: `Begin(sortMode, blend, sampler, transform)`, `Draw` (position or
  destination, source, tint, rotation, origin, scale, flip, depth), `End`.
- `SpriteSortMode`, `SpriteEffects`, `BlendState`, `SamplerState`.
- `Keyboard`, `KeyboardState`, `Key`.
- `Color`, `Vector2`, `Point`, `Rectangle`, `Matrix`, `MathHelper`.
- `Log`: printf-style, to the same log as coldx's own lines.

## Inside

- `game.cpp`: `Run` and the loop. `debug_options.*`: `COLDX_*` variables, console, log file.
- `graphics_device.cpp` + `graphics_access.h`: the device, the internal back buffer, the
  current target, frame begin and end, capture. `GraphicsAccess` is how the rest of core
  reaches the device, command buffer and current target; `DeviceToken` lets resources
  outlive the device safely.
- `texture2d.cpp` + `texture_access.h`: textures and render targets.
- `sprite_batch.cpp`: the public API, sprite geometry (origin, scale, rotation, flip), the
  queue and sorting. `sprite_renderer.*`: the GPU side, one pipeline per blend state made
  on first use, one sampler per sampler state, buffers, and the draw.
- `shader_blob.*`: creates an SDL shader from a compiled header in the format the device takes.
  Built-in shader headers are in `shaders/compiled/`.
- `matrix.cpp`, `log.cpp`, `keyboard.cpp`.

## Invariants

- No SDL type appears in a public header.
- Drawing goes into the internal back buffer or a render target, never the swapchain.
- Every frame starts with the back buffer as the current target.
- The GPU device lives until `~Game`; `Run` only lets go of the window.
- GPU resources release only if their `DeviceToken` still holds a device.
- Unrotated sprites skip the trig, so their corners stay on whole pixels.
- Log lines start with `coldx.core:`.

## Gotchas

- A fragment shader's input must match its vertex output, `SV_Position` included. D3D12
  rejects the pipeline otherwise; Vulkan does not. Test drawing on both backends.
- `SpriteBatch::End`, `Clear` and `SetRenderTarget` do nothing outside `Draw`: there is no
  command buffer.
- Deferred sprites go to the target current at `End`, not at `Draw`. Immediate ones go to
  the target current at each `Draw`. `core.sprite` tests both.
- Textures cannot be created before `Run`; the device does not exist yet.
- Headless frame rates mean nothing: no present paces the loop, so it measures submission.
  Measure performance in a visible window with vsync off (the `stress` sample).

## Tests

`ctest -C Debug -R core` from the build directory. `core.math` checks the value types and
`Matrix`. `core.render.<backend>` and `core.sprite.<backend>` draw scenes headless and
compare pixels to computed values: tint, fade, rectangles, all sort modes, blend and
sampler states, transform, rotation, flips, origin and scale, render targets. Helpers:
`src/core/tests/headless.h` (run a scene, capture, compare) and `tests/check.h`.

## Open work

`docs/STATUS.md` lists what is not built yet.
