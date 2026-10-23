# 0014: Effects are fragment shaders, compiled by coldxc

**Context.** Games need their own shaders when drawing sprites: flashes, tints, waves,
dissolves. XNA's `Effect` is a whole technique built by its content pipeline from `.fx`
files; coldx has no effect framework and compiles HLSL ahead of time (ADR 0006). The
backends disagree on uniform layout, and Direct3D 12 rejects a fragment shader whose
input does not match the vertex output exactly.

**Decision.**
- An `Effect` replaces SpriteBatch's fragment shader only. The vertex shader, and so the
  input every effect takes, stays SpriteBatch's: `CxSpriteInput` from
  `CX_SPRITE_EFFECT()` in `coldx.hlsli`, which the built-in fragment shader uses too.
- Parameters are the members of one uniform block, named by spirv-cross reflection at
  build time. dxc compiles SPIR-V with `-fvk-use-dx-layout`, so one table of offsets holds
  on every backend. Values are copied into the command buffer when the batch draws.
- One compiled form (`coldx/core/effect_format.h`), written by one compiler in the content
  module, reaches a game two ways: a `.fx` entry packed by `[effect]`, or a byte array in a
  header from `coldxc --compile-effect` (`coldx_compile_effects`). coldxc carries
  `coldx.hlsli` inside it.
- `SpriteBatch::Begin` takes the effect before the transform, in XNA's order.
- Shaders SpriteBatch cannot feed (arrays, structs, other resources, gaps in texture
  slots, a different input) fail the content build, not the game.

**Consequences.** A vertex-stage effect needs a later, separate API. Building content with
effects needs the Vulkan SDK; the committed-header route keeps a clone buildable without
it. The effect bytes depend on the dxc version as well as the source, so two machines with
different SDKs can pack different bytes for the same project. Every existing `Begin` call
with a transform gained an `Effect()` argument.
