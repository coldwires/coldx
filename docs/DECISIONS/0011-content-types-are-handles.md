# 0011: Textures and fonts are shared handles, not move-only owners

**Context.** `Texture2D` and `SpriteFont` started move-only, one owner per GPU resource.
A ContentManager caches what it loads, and the game holds what it uses: two owners. XNA
code reads `player = Content.Load<Texture2D>("player")`, which a move-only type cannot
support without handing out references into the cache that dangle after `Unload`.

**Decision.** `Texture2D`, `RenderTarget2D` and `SpriteFont` hold their implementation by
`shared_ptr`. Copying one shares the resource; it is released when the last copy goes,
still guarded by the device token (ADR 0007). `SoundEffect` and `Music` already worked
this way. `SoundEffectInstance` stays move-only: it is one playback, not shared data.

**Consequences.** `Load<T>` returns `T` by value, as in XNA, and `Unload` cannot leave the
game with a dangling texture. `SetData` and font settings such as `SetSpacing` change
every copy, since they change the shared resource; the headers say so. Loaded types from
games must be copyable, and the README advises making them handles too.
