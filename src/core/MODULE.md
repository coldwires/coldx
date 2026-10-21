# core

**Owns:** the game loop, window, GPU device, textures, render targets, sprite and text
drawing, keyboard, mouse and gamepad input, audio, per-user storage, math types, logging and
the debug options.
**Layer:** 0
**Depends on:** none
**Third-party:** SDL3, stb, dr_libs
**Public headers:** `include/coldx/core/`, all included by `include/coldx/coldx.h`
**Library target:** `coldx::core`
**Size:** ~7.2k lines

## Public surface

- `Game`: subclass it and override `Initialize`, `LoadContent`, `UnloadContent`, `Update`,
  `Draw`, `OnResize`, `OnFocusChanged`, `OnExiting`. `Run` opens the window and loops.
  `Settings()` (`GameSettings`) is read when `Run` starts. While running: `SetFullscreen`,
  `ToggleFullscreen`, `IsFullscreen`, `SetWindowSize`, `SetWindowTitle`, `SetVSync`,
  `SetVirtualResolution`, `SetScaleMode` (`ScaleMode`), `SetFullscreenMode`
  (`FullscreenMode` borderless or exclusive), `DisplayModes`, `DesktopDisplayMode`.
- `GameTime`: `Delta`, `Total`, `IsRunningSlowly`.
- `GameComponent`, `DrawableGameComponent`, `GameComponentCollection` (`Game::Components()`:
  `Add`, `Emplace`, `Remove`, `Count`). Update and draw order, `Enabled`, `Visible`.
- `GraphicsDevice`: `Clear`, `SetRenderTarget`, back buffer size, `PresentedArea`. Exists from `Game`
  construction; usable from `Initialize`.
- `Texture2D`: `FromFile` (PNG), `FromPixels`, `SetData`, `GetData`, `SaveAsPng`. A shared
  handle; premultiplied. `GraphicsDevice::GetBackBufferData`.
- `RenderTarget2D`: a `Texture2D` that can be drawn into. `Create`.
- `SpriteBatch`: `Begin(sortMode, blend, sampler, transform)`, `Draw` (position or
  destination, source, tint, rotation, origin, scale, flip, depth), `End`.
- `SpriteBatch::DrawString`: UTF-8 text, with the same origin, rotation, scale, flip and depth.
- `SpriteFont`: `FromFile`, `FromMemory` (TTF, baked at one pixel height), `MeasureString`,
  `LineSpacing`, `Spacing`, `DefaultCharacter`, `HasCharacter`.
- `SpriteSortMode`, `SpriteEffects`, `BlendState`, `SamplerState`.
- `Keyboard`, `KeyboardState`, `Key`; `Mouse`, `MouseState`, `MouseButton`; `GamePad`,
  `GamePadState`, `Buttons`, `GamePadDeadZone`. Each has `GetState` (as of the start of the
  current Update) and `WasPressed`/`WasReleased` (since the previous Update).
- `SoundEffect` (`FromFile` WAV/OGG, `FromSamples`, `Play`, `CreateInstance`,
  `MasterVolume`), `SoundEffectInstance` (play, pause, resume, stop, volume, pitch, pan,
  loop, `State`), `Music` (`FromFile` OGG), `MediaPlayer` (one track: play, stop, pause,
  resume, volume, `IsPlaying`).
- `ContentManager` (`Game::Content()`): `Load<T>(name)` with a cache, `LoadFont(name, size)`,
  `RegisterLoader<T>`, `Unload`, `SetRootDirectory`.
- `Storage`: per-user folder named by `GameSettings::company`/`name`; whole-file
  `ReadFile`, atomic `WriteFile`, `Exists`, `Remove`.
- `Color`, `Vector2`, `Point`, `Rectangle`, `Matrix`, `MathHelper`.
- `Log`: printf-style, to the same log as coldx's own lines.

## Inside

- `game_component.cpp`: the collection. Changes during a pass wait for the pass to end.
- `game.cpp`: `Run` and the loop. `debug_options.*`: `COLDX_*` variables, console, log file.
- `graphics_device.cpp` + `graphics_access.h`: the device, the internal back buffer, the
  current target, frame begin and end, capture. `GraphicsAccess` is how the rest of core
  reaches the device, command buffer and current target; `DeviceToken` lets resources
  outlive the device safely.
- `gpu_download.*`: reading a texture back (a copy pass and a fence) and writing PNGs;
  used by `GetData`, `GetBackBufferData` and `COLDX_CAPTURE`.
- `scaling.*`: a virtual resolution's placement in the window (`FitRectangle`) and the
  mapping between window and back buffer pixels, shared by the final blit and the mouse.
- `texture2d.cpp` + `texture_access.h`: textures and render targets.
- `sprite_batch.cpp`: the public API, sprite geometry (origin, scale, rotation, flip), the
  queue and sorting. `sprite_renderer.*`: the GPU side, one pipeline per blend state made
  on first use, one sampler per sampler state, buffers, and the draw.
- `shader_blob.*`: creates an SDL shader from a compiled header in the format the device takes.
  Built-in shader headers are in `shaders/compiled/`.
- `sprite_font.cpp` + `font_access.h`: baking (stb_truetype packs ASCII and Latin-1 into one
  atlas), UTF-8 decoding, and the one layout routine both measuring and drawing use.
  `stb_truetype_impl.cpp` compiles stb's implementation, warnings silenced there only.
- `content_manager.cpp`: finding files by name and extension, built-in loaders, the cache.
  `Load<T>` and `RegisterLoader<T>` are templates in the header, type-erased into it.
- `content_path.*`: relative content paths resolve against the executable's folder.
- `input.*`: the input hub. Live state follows SDL events; `BeginUpdate` copies it to
  current and previous before each Update, and polls the pads. `keyboard.cpp`,
  `mouse.cpp`, `gamepad.cpp` are the public classes over it.
- `input_script.*`: `COLDX_INPUT`, scripted input applied before numbered Updates.
- Audio: `mixer.*` mixes up to 256 voices and one music stream into stereo float at
  48 kHz (linear resampling, pitch, balance pan, gains ramped per block, clamp); it is pure arithmetic, tested
  without a device. `spsc_queue.h` carries commands to it. `audio.*` is the game-thread
  side: the SDL device stream whose callback runs the mixer, the voice slots, and freeing
  what the mixer hands back. `sound_effect.cpp`, `music.cpp` are the public classes;
  `audio_codecs.cpp` decodes; `audio_codecs_impl.cpp` compiles dr_wav and stb_vorbis.
- `storage.cpp` + `storage_names.h`: the Storage folder and file access.
- `matrix.cpp`, `log.cpp`.

## Invariants

- No SDL type appears in a public header.
- Drawing goes into the internal back buffer or a render target, never the swapchain.
- Every frame starts with the back buffer as the current target.
- The GPU device lives until `~Game`; `Run` only lets go of the window. Components, then
  cached content, are destroyed before it.
- GPU resources release only if their `DeviceToken` still holds a device.
- Unrotated sprites skip the trig, so their corners stay on whole pixels.
- Glyphs land on whole pixels at scale 1: the pen is rounded per glyph, and a flipped
  string mirrors across its rounded width.
- `MeasureString` and `DrawString` share one layout; they cannot disagree.
- Input is event-driven, never polled from SDL's key array: injected and scripted input
  behave exactly like real input. Losing focus releases every key and mouse button.
- The audio thread never locks, allocates or frees. Commands reach it through a
  lock-free queue; voice ends come back as per-slot generation numbers; music streams
  come back through a second queue for the game thread to free. Sound data stays alive
  until its slot's generation is reported ended.
- Components: game first, then components, for Initialize, Update and Draw; drawable
  components unload before the game's UnloadContent and are destroyed before the device.
- Every Update sees one input snapshot; edges compare it with the previous Update's.
- Storage never writes outside its folder: names with separators or dots-only are refused.
- `OnResize` fires only when the back buffer size really changes. With a virtual
  resolution that is never a window resize.
- The final blit and the mouse use the same `FitRectangle`, so a click lands on the pixel
  drawn under it.
- Log lines start with `coldx.core:`.

## Gotchas

- A fragment shader's input must match its vertex output, `SV_Position` included. D3D12
  rejects the pipeline otherwise; Vulkan does not. Test drawing on both backends.
- `SpriteBatch::End`, `Clear` and `SetRenderTarget` do nothing outside `Draw`: there is no
  command buffer.
- Deferred sprites go to the target current at `End`, not at `Draw`. Immediate ones go to
  the target current at each `Draw`. `core.sprite` tests both.
- Textures and fonts cannot be created before `Run`; the device does not exist yet.
- A font size is a bake. Scaling a `SpriteFont` blurs (linear) or blocks (point) it; bake
  the size you draw at.
- Source stays ASCII. Write code points as numbers (`char32_t{0xE9}`) and UTF-8 text as
  hex byte escapes in string literals; editors and tools that turn escapes into literal
  characters break the build on MSVC and fail `check.py`.
- SDL ignores pad button presses while the window has no focus. A headless window never
  has focus, so tests that press virtual pad buttons set
  `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS`.
- Headless runs use SDL's dummy audio driver: real-time and silent. Tests that play
  sound need nothing set.
- A slot frees itself once its voice ends, even if an instance still holds it. Ended()
  compares with >= so that instance still reads Stopped after the slot is reused.
- Mixer bugs tend to hang rather than crash (a refill that never advances). Every test
  has a CTest timeout for that reason.
- SDL ends a rumble after 65 seconds at most; the hub re-sends a held vibration every 30.
- Tests write to the real per-user folder; `core.input` uses company `coldx-test` and
  deletes it afterwards.
- Headless frame rates mean nothing: no present paces the loop, so it measures submission.
  Measure performance in a visible window with vsync off (the `stress` sample).

## Tests

`ctest -C Debug -R core` from the build directory. `core.math` checks the value types and
`Matrix`. `core.render.<backend>` and `core.sprite.<backend>` draw scenes headless and
compare pixels to computed values: tint, fade, rectangles, all sort modes, blend and
sampler states, transform, rotation, flips, origin and scale, render targets.
`core.font.<backend>` checks metrics, kerning, Latin-1, the default character and drawn
glyphs against glyphs rasterized directly with stb_truetype. `core.input` drives the
keyboard and mouse with SDL events and pads with SDL virtual gamepads, and covers edges,
focus loss, wheel direction, dead zones, hot-plug order, rumble, `COLDX_INPUT` and Storage.
`core.audio` checks the mixer sample by sample (rate, pitch, pan, loop seam, pause, stale
commands, clamp, music across chunk boundaries), decoding, and the public classes end to
end on the dummy driver, including the 64-voice limit. Helpers:
`src/core/tests/headless.h` (run a scene, capture, compare) and `tests/check.h`.

## Open work

`docs/STATUS.md` lists what is not built yet.
