# Status

What exists today. `done` is built and tested; `stubbed` exists with placeholder
behaviour; `not started` has no code.

## core

| # | Feature | State |
|---|---|---|
| 1 | Game loop: fixed and variable timestep, catch-up cap, lifecycle hooks | done |
| 2 | Window settings: title, size, resizable, fullscreen at start, vsync | done |
| 3 | Fullscreen toggle and resize while running | not started |
| 4 | Console window on Windows, `COLDX_*` debug options | done |
| 5 | `GraphicsDevice`: clear, internal back buffer, capture | done |
| 6 | `RenderTarget2D` | not started |
| 7 | `Texture2D`: PNG, raw pixels, `SetData` | done |
| 8 | `SpriteBatch`: position, destination, source, tint; deferred, alpha blend, linear clamp | done |
| 9 | `SpriteBatch`: sort modes, blend and sampler states, transform, rotation, origin, scale, flip, depth | not started |
| 10 | `SpriteFont` | not started |
| 11 | Keyboard | done |
| 12 | Mouse, gamepad | not started |
| 13 | Audio | not started |
| 14 | `GameComponent`, `DrawableGameComponent` | not started |
| 15 | `ContentManager` | not started |
| 16 | Math: `Vector2`, `Rectangle`, `Color` | done |
| 17 | Math: `Point`, `Matrix`, `MathHelper` | not started |
| 18 | Custom shaders when drawing (`Effect`) | not started |

## Shaders

| # | Feature | State |
|---|---|---|
| 19 | `coldx.hlsli`, `coldx_compile_shaders`: HLSL to SPIR-V, DXIL, MSL | done |

## Other modules

| # | Module | State |
|---|---|---|
| 20 | `content` and the `coldxc` tool | not started |
| 21 | `physics` | not started |
| 22 | `net` | not started |
| 23 | `protect` | not started |

## Platforms

| Platform | State |
|---|---|
| Windows, MSVC, Direct3D 12 and Vulkan | built and tested |
| Linux | not built |
| macOS, Metal | not built; the MSL output has never run |
