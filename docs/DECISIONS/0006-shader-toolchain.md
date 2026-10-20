# 0006: Shaders are HLSL, compiled ahead of time with dxc and spirv-cross

**Context.** SDL_GPU takes SPIR-V on Vulkan, DXIL on Direct3D 12 and MSL on Metal, with a
fixed register layout per stage. Games must never compile shaders at runtime, and writing
a shader for a game should be easy. SDL_shadercross was the first plan; building it means
building DXC from source.

**Decision.** Shaders are HLSL. `cmake/compile_shader.cmake` runs `dxc` (to SPIR-V and to
DXIL) and `spirv-cross` (SPIR-V to MSL, and reflection for resource counts), both from the
Vulkan SDK, and writes one header per stage holding all three formats and the counts.
`coldx_compile_shaders` wraps it for CMake. `shaders/include/coldx.hlsli` provides macros
that place textures and uniform blocks where SDL_GPU expects them on every backend.
Compiled headers are committed.

Rejected: vendoring DXC binaries (large binaries in git), and the Windows SDK's `dxc`
(no SPIR-V output, no MSL path).

**Consequences.** Only someone editing a shader needs the Vulkan SDK; building coldx does
not. Output is deterministic, so committed headers change only with their source. DXC 1.9
signs DXIL itself, so `dxil.dll` is not needed. Tool versions that produced the committed
headers are in `third_party/VERSIONS.md`.
