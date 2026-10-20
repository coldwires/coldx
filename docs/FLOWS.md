# Flows

How data moves through coldx for the main cases. Each step is a file and what it does.

## A frame

1. `src/core/game.cpp` `Game::Run` - polls SDL events: quit, resize (`OnResize`), focus.
2. `game.cpp` - adds elapsed time to the accumulator, capped at `maxUpdatesPerFrame` steps;
   calls `Update` once per step (or once, with the real delta, for variable timestep).
3. `graphics_device.cpp` `GraphicsAccess::BeginFrame` - acquires a command buffer and the
   swapchain texture; resizes the internal back buffer to match. No swapchain texture
   (minimized) means no `Draw` this frame.
4. The game's `Draw` - `GraphicsDevice::Clear` records a clearing render pass on the
   current target, which is the back buffer until `SetRenderTarget` changes it.
   `SpriteBatch::Draw` turns each sprite into four corners (origin, scale, rotation, flip)
   and appends them to a queue; in Immediate mode it flushes at once.
5. `sprite_batch.cpp` `SpriteBatch::End` - sorts the queue by the sort mode and hands it to
   `sprite_renderer.cpp` `SpriteRenderer::Draw`, which copies the vertices to the GPU,
   pushes the game's transform times the target's pixel-to-clip matrix, opens a render pass
   that keeps the target's contents, and issues one indexed draw per run of sprites sharing
   a texture. The pipeline is picked by blend state, the sampler by sampler state.
6. `GraphicsAccess::EndFrame` - blits the back buffer to the swapchain texture and submits.

Covered by `core.render.*` and `core.sprite.*` for steps 3 to 6.

## Drawing text

1. `src/core/sprite_font.cpp` `SpriteFont::FromFile` - loads the TTF, packs printable ASCII
   and Latin-1 into one atlas at the requested pixel height with stb_truetype, and keeps
   the font data for kerning.
2. `sprite_batch.cpp` `SpriteBatch::DrawString` - walks the text through
   `FontAccess::ForEachGlyph`, which decodes UTF-8 and lays it out (pen rounded per glyph,
   kerning, newlines, default character). Each glyph becomes one sprite from the atlas,
   with the string's origin offset by the glyph's place, so rotation and scale apply to
   the whole string.
3. From there it is a frame like any other sprite batch.

Covered by `core.font.*`.

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
