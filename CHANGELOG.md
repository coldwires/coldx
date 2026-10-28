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
- `Mouse` and `GamePad`. `WasPressed`/`WasReleased` on all three input devices. Input is
  event-driven and snapshotted per Update.
- `Storage` for settings and saves, and `GameSettings::company`/`name`.
- `COLDX_INPUT` scripted input.
- `GameComponent`, `DrawableGameComponent` and `Game::Components()`.
- Window control while running: fullscreen toggle, size, title, vsync. `OnResize`
  fires only on a real change.
- `ContentManager` (`Game::Content()`): loose files by name, a cache, `LoadFont`,
  `RegisterLoader` for game types.
- `Texture2D`, `RenderTarget2D` and `SpriteFont` are copyable handles. Copies share.
- Audio: `SoundEffect` (WAV, OGG), `SoundEffectInstance`, streaming `Music` and
  `MediaPlayer`, on a lock-free software mixer. Headless runs use a silent driver.
- `hello_audio` sample. Every test has a CTest timeout.
- `demo` sample: a small game using components, ContentManager, input, sound, music,
  text, a named-tile sprite sheet and Storage.
- `coldx_framework_tests`: automated and human-judged stress tests with a report.
- Fixes from the stress tests. Volume, pan and music volume ramp instead of stepping. 256
  voices, and skipped sounds are logged once a second. Stereo OGG sound effects keep their
  last frame.
- Virtual resolution: a fixed back buffer scaled into the window (`ScaleMode` Fit, Integer,
  Stretch), with the mouse mapped into it.
- `Texture2D::GetData`, `SaveAsPng` and `GraphicsDevice::GetBackBufferData`.
- Exclusive fullscreen at a chosen mode, with `DisplayModes` and `DesktopDisplayMode`.
- `GameSettings::drawBetweenUpdates` and `GameTime::Alpha` for interpolated drawing.
- `TextInput` (typed UTF-8, IME composition), `Clipboard`, `Keyboard::KeyName` and
  `KeyForCharacter`.
- `GamePad::GetCapabilities` (name, type, rumble) and `FaceButtonLabel`.
- `MediaPlayer::CrossFade` and `FadeOut`.
- `.rime` packages: `coldx/core/rime.h`, `ContentManager::Mount`, the default
  `content.rime`, `LooseFiles`, `RegisterDataLoader`, `ReadBytes`. `FromMemory` for
  textures, sounds and music.
- The `content` module and `coldxc`: content projects, processors (built-in and a game's
  own), a build cache, `coldx_add_content`. The `demo` sample loads from a package.
- The `demo` sample uses the whole library: title menu, pause, high score name entry with
  `TextInput`, credits from packed text, a render target shown through the grayscale
  effect, a flash on hits and a wave on the title, pixel-exact collisions, a radar
  texture rewritten each frame, an engine hum made with `FromSamples`, gamepad prompts
  and rumble, screenshots, and encrypted, fingerprinted, watermarked content. A scripted
  run through every screen is the `samples.demo` test.
- `Effect`: custom fragment shaders for `SpriteBatch` with named parameters and extra
  textures. `SpriteBatch::Begin` takes one before the transform (calls that passed a
  transform now pass `Effect()` first). `[effect]` in content projects,
  `coldxc --compile-effect` and `coldx_compile_effects`. `CX_SPRITE_EFFECT`, `CX_SAMPLE`
  and `CX_SAMPLE_AT` in `coldx.hlsli`. Grayscale, flash and wave in `shaders/examples/`.
- `SpriteBatch::SetPixelSnap` and `FontSmoothing::Sharp` for pixel-perfect games, and
  `GameSettings::pixelArt` to turn on the whole recipe at once (`SamplerState::Default`).
  The `demo` sample is pixel-perfect (640x360, integer scaling) with the Silkscreen pixel
  font.
- Sprites sample only their own source rectangle (with the clamp samplers), so atlas
  neighbours never bleed in, through filtering or rounding on shrunk sprites.
- LZ4 compression of package entries where it helps, and `[texture] format = raw`.
- Package encryption (ChaCha20) with `coldxc --key-file`, `--new-key` and
  `coldx_add_content(... KEY_FILE)`. `ContentManager::SetKey`, `SetDefaultKey`.
- Build fingerprints: `--build-id`, `BUILD_ID`, `COLDX_BUILD_ID`, `coldxc --identify`.
- The `protect` module: `COLDX_HIDDEN`.
- Watermarks: `[texture] watermark = rgb | alpha`, `[sound] watermark = on`, read by
  `coldxc --identify`.
- `COLDX_HARDEN` and `coldx_harden` for release builds, and coldx no longer uses RTTI.
  `ContentManager::IsLoaded` is now `IsLoaded<T>(name)`.
- `hello_sprite`, `hello_text`, `quickstart` and `stress` samples. `hello_text` saves its
  text size.
- `docs/QUICKSTART.md`: a complete game, the API on one page, differences from XNA. Its
  example is `samples/quickstart`, kept identical by `tools/check.py`.
- CTest suite: math, headless render tests per GPU backend, and `tools/check.py` for
  module boundaries, file size and ASCII.
- Log lines name their module: `coldx.core:`.
