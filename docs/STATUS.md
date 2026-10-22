# Status

What exists today. `done` is built and tested; `stubbed` exists with placeholder
behaviour; `not started` has no code.

## core

| # | Feature | State |
|---|---|---|
| 1 | Game loop: fixed and variable timestep, catch-up cap, lifecycle hooks, optional drawing between updates (`GameTime::Alpha`) | done |
| 2 | Window settings: title, size, resizable, fullscreen at start, vsync | done |
| 3 | Fullscreen toggle (borderless or exclusive at a chosen mode), display modes, resize, title and vsync while running | done |
| 4 | Console window on Windows, `COLDX_*` debug options | done |
| 5 | `GraphicsDevice`: clear, internal back buffer, capture | done |
| 6 | `RenderTarget2D`, `GraphicsDevice::SetRenderTarget` | done |
| 7 | `Texture2D`: PNG, raw pixels, `SetData`, `GetData`, `SaveAsPng`; `GetBackBufferData` | done |
| 8 | `SpriteBatch`: position, destination, source, tint | done |
| 9 | `SpriteBatch`: sort modes, blend and sampler states, transform, rotation, origin, scale, flip, depth | done |
| 10 | `SpriteFont` from a TTF at runtime; `DrawString`, `MeasureString` | done |
| 25 | `SpriteFont` from packed content: the TTF is packed and baked at load, at any size (ADR 0012) | done |
| 11 | Keyboard, with `WasPressed`/`WasReleased` | done |
| 12 | Mouse, gamepad (4 players, hot-plug, dead zones, rumble, name, type, face labels) | done |
| 13 | Audio: `SoundEffect`, `SoundEffectInstance`, streaming `Music`, `MediaPlayer` with crossfade and fade out | done |
| 14 | `GameComponent`, `DrawableGameComponent` | done |
| 15 | `ContentManager`: loose files, cache, `RegisterLoader`, `RegisterDataLoader`, `ReadBytes` | done |
| 28 | `ContentManager` reading packed `.rime` content: mount, default package, checks, LZ4 entries, raw textures | done |
| 16 | Math: `Vector2`, `Rectangle`, `Color` | done |
| 17 | Math: `Point`, `Matrix`, `MathHelper` | done |
| 18 | Custom shaders when drawing (`Effect`); `SpriteBatch::Begin` will take one | not started |
| 24 | `Log` for games | done |
| 29 | `demo` sample: components, content, input, audio, text, many sprites | done |
| 26 | `Storage`: per-user folder, atomic writes | done |
| 27 | Typed text input (UTF-8, IME composition), clipboard, key names on the current layout | done |
| 31 | Virtual resolution: fixed back buffer scaled into the window (Fit, Integer, Stretch), mouse mapped | done |
| 30 | `tests/framework`: stress and edge-case runner, automatic checks plus guided ones for a person | done |

## Release builds

| # | Feature | State |
|---|---|---|
| 32 | `COLDX_HARDEN`, `coldx_harden`: no RTTI, whole-program optimization, no build-machine details (MSVC) | done |

## Shaders

| # | Feature | State |
|---|---|---|
| 19 | `coldx.hlsli`, `coldx_compile_shaders`: HLSL to SPIR-V, DXIL, MSL | done |

## Other modules

| # | Module | State |
|---|---|---|
| 20 | `content` and the `coldxc` tool: projects, built-in and custom processors, cache, `coldx_add_content` | done |
| 21 | `physics` | not planned in coldx: games build it above coldx |
| 22 | `net` | not planned in coldx: games build it above coldx |
| 23 | `protect` | not started |

## Platforms

| Platform | State |
|---|---|
| Windows, MSVC, Direct3D 12 and Vulkan | built and tested; 10,000 sprites at over 1,000 fps in Release on the test machine |
| Linux | not built |
| macOS, Metal | not built; the MSL output has never run |
