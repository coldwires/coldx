# Flows

How data moves through coldx for the main cases. Each step is a file and what it does.

## A frame

1. `src/core/game.cpp` `Game::Run` - polls SDL events: quit, resize (`OnResize`), focus.
2. `game.cpp` - adds elapsed time to the accumulator, capped at `maxUpdatesPerFrame` steps;
   calls `Update` once per step (or once, with the real delta, for variable timestep).
3. `graphics_device.cpp` `GraphicsAccess::BeginFrame` - acquires a command buffer and the
   swapchain texture; resizes the internal back buffer to match. No swapchain texture
   (minimized) means no `Draw` this frame.
4. The game's `Draw` - `GraphicsDevice::Clear` records a clearing render pass on the back
   buffer. `SpriteBatch::Draw` only appends quads to a list.
5. `sprite_batch.cpp` `SpriteBatch::End` - copies the quads' vertices to the GPU, pushes the
   pixel-to-clip transform, opens a render pass that keeps the back buffer's contents, and
   issues one indexed draw per run of sprites sharing a texture.
6. `GraphicsAccess::EndFrame` - blits the back buffer to the swapchain texture and submits.

Covered by `core.render.*` for steps 3 to 6.

## Loading a texture

1. `src/core/texture2d.cpp` `Texture2D::FromFile` - prefixes a relative path with the
   executable's folder; `SDL_LoadPNG`; converts to RGBA; premultiplies alpha; packs rows.
2. `Texture2D::FromPixels` - creates the GPU texture and holds the device's `DeviceToken`.
3. `Upload` - copies through a transfer buffer on a command buffer of its own, submitted at
   once, so it runs before any frame recorded later.

## Building a shader

1. `cmake/coldx_shaders.cmake` `coldx_compile_shaders` - one custom command per `.hlsl`.
2. `cmake/compile_shader.cmake` - `dxc` to SPIR-V and to DXIL; `spirv-cross` to MSL and to
   reflection JSON, from which it counts samplers, storage textures and buffers, and
   uniform buffers. Writes one header with all three and the counts; leaves it untouched
   if the bytes did not change.
3. At runtime, `src/core/shader_blob.cpp` `CreateShader` - picks the format the device
   takes (SPIR-V, DXIL, then MSL with entry point `main0`).

## Shutdown

1. `Game::Run` - loop ends; logs the frame summary; saves `COLDX_CAPTURE` if set;
   `OnExiting`, `UnloadContent`; releases the window from the device; destroys the window.
2. The derived game's destructor - its textures and batches release their GPU objects; the
   device still exists.
3. `~Game` - destroys the device, which clears `DeviceToken`; then `SDL_Quit`. Anything
   still holding a token skips its release.

Covered by `core.render.*`, which keeps a texture alive past the `Game` and destroys it after.
