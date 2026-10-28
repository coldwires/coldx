#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include <coldx/core/color.h>
#include <coldx/core/matrix.h>
#include <coldx/core/texture2d.h>
#include <coldx/core/vector2.h>

namespace coldx {

class GraphicsDevice;

namespace detail {
struct EffectAccess;
}

// A custom fragment shader for SpriteBatch, with named parameters. Pass it to Begin and
// every sprite in that batch is drawn with it. Written in HLSL against coldx.hlsli and
// compiled by coldxc, into a package ([effect] in a content project, then
// Content().Load<Effect>("name")) or into a C++ header (coldx_compile_effects, then
// FromMemory). The vertex stage stays SpriteBatch's own.
//
// Parameters are the members of the shader's CX_FRAGMENT_UNIFORMS block, set by name.
// Their values are read when the batch draws: at End, or at each Draw in Immediate mode.
// Set twice inside one batch and the whole batch gets the second value, as in XNA. A name
// the shader does not have, or a value of the wrong type, is logged once and ignored.
//
// A handle: copies share one effect, parameters included. An empty Effect draws with
// SpriteBatch's built-in shader.
class Effect {
public:
    Effect();
    ~Effect();
    Effect(const Effect&);
    Effect& operator=(const Effect&);
    Effect(Effect&&) noexcept;
    Effect& operator=(Effect&&) noexcept;

    // A compiled effect (coldx/core/effect_format.h), as coldxc wrote it. name is for log
    // lines. Empty on failure, with the reason logged. Copies the bytes.
    static Effect FromMemory(GraphicsDevice& graphics, const void* data, std::size_t size, const std::string& name = "memory");

    // Whether the shader has a parameter, or an extra texture, called name.
    bool Has(const std::string& name) const;

    // float, float2, float3, float4.
    void Set(const std::string& name, float value);
    void Set(const std::string& name, Vector2 value);
    void Set(const std::string& name, float x, float y, float z);
    void Set(const std::string& name, float x, float y, float z, float w);
    // A float4, each channel 0 to 1. Colors are premultiplied. Color::White * 0.5f is half
    // transparent white.
    void Set(const std::string& name, Color value);
    // A float4x4, in the same row-vector convention as the rest of coldx. In the shader,
    // mul(matrix, v) is v transformed by it.
    void Set(const std::string& name, const Matrix& value);
    // int, uint or bool.
    void Set(const std::string& name, int value);
    void Set(const std::string& name, bool value);
    // A texture the shader declares at slot 1 or later with CX_FRAGMENT_TEXTURE, sampled
    // with the batch's sampler. Until one is set, the sprite's own texture stands in.
    void Set(const std::string& name, const Texture2D& texture);

    const std::string& Name() const;
    bool IsEmpty() const;
    explicit operator bool() const { return !IsEmpty(); }

private:
    friend struct detail::EffectAccess;
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace coldx
