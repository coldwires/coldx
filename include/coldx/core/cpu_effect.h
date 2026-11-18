#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <string>

#include <coldx/core/vector2.h>

namespace coldx {

// Four floats, as a shader's float4. As a colour: channels 0 to 1, premultiplied.
struct Float4 {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;

    constexpr Float4() = default;
    constexpr Float4(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_) {}

    constexpr Float4 operator+(const Float4& o) const { return {r + o.r, g + o.g, b + o.b, a + o.a}; }
    constexpr Float4 operator-(const Float4& o) const { return {r - o.r, g - o.g, b - o.b, a - o.a}; }
    constexpr Float4 operator*(const Float4& o) const { return {r * o.r, g * o.g, b * o.b, a * o.a}; }
    constexpr Float4 operator*(float s) const { return {r * s, g * s, b * s, a * s}; }
};

namespace detail {
struct CpuPixelSource;
}

// One pixel as an effect's CPU version sees it: what CxSpriteInput holds in the shader, and
// the textures. Sampling uses the batch's sampler and the same rules as the GPU.
class CpuPixel {
public:
    Vector2 position;  // the pixel's centre in the current target, the shader's input.position.xy
    Vector2 uv;        // input.uv
    Float4 color;      // input.color: the Draw call's tint, premultiplied

    // CX_SAMPLE: the sprite's texture at uv, held inside the sprite's source rectangle.
    Float4 Sample() const;
    // CX_SAMPLE_AT: the sprite's texture at any UV, held inside the source rectangle.
    Float4 SampleAt(Vector2 at) const;
    // texture.Sample(sampler, at) for the texture at slot (0 is the sprite's), not held
    // inside anything. A slot the effect does not have reads as transparent black.
    Float4 SampleTexture(int slot, Vector2 at) const;
    // GetDimensions on the texture at slot.
    int TextureWidth(int slot = 0) const;
    int TextureHeight(int slot = 0) const;

private:
    friend struct detail::CpuPixelSource;
    const detail::CpuPixelSource* source_ = nullptr;
};

// A member of the shader's CX_FRAGMENT_UNIFORMS block, as the CPU version's struct holds it.
struct CpuParameter {
    const char* name;
    std::size_t offset;
};

// The CPU renderer (COLDX_RENDERER=cpu) runs an effect by calling a C++ version of its
// shader, written once by hand and registered here under the effect's name. The name is
// compared without a folder or extension, so "wave" serves an effect loaded as
// "effects/wave". Register before the first frame that draws with the effect. The function
// is called on several threads at once (COLDX_CPU_THREADS), so it must work from its
// arguments only, keeping and writing nothing.
//
// Parameters arrive as a struct laid out as the uniform block is. HLSL packs it in 16-byte
// rows that a member may not straddle, so a float3 after a float2 starts a new row and the
// struct needs padding there. List every member by name and offsetof: a version whose
// names or offsets differ from the compiled effect's is refused, logged, and the built-in
// shader draws instead. The GPU path never calls these.
namespace CpuEffects {

using Function = std::function<Float4(const CpuPixel& pixel, const void* parameters)>;

void Register(const std::string& name, std::size_t parametersSize, std::initializer_list<CpuParameter> parameters,
              Function function);

template <class Parameters>
void Register(const std::string& name, Float4 (*function)(const CpuPixel&, const Parameters&),
              std::initializer_list<CpuParameter> parameters) {
    Register(name, sizeof(Parameters), parameters, [function](const CpuPixel& pixel, const void* values) {
        return function(pixel, *static_cast<const Parameters*>(values));
    });
}

// An effect with no parameters.
void Register(const std::string& name, Float4 (*function)(const CpuPixel&));

// Whether a CPU version is registered under name.
bool Has(const std::string& name);

} // namespace CpuEffects
} // namespace coldx
