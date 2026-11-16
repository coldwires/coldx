#pragma once

// CPU versions of the example effects, for the CPU renderer (COLDX_RENDERER=cpu). Each one
// follows its .hlsl line for line. Call RegisterExampleCpuEffects() once before the first
// frame, then load the effects as usual. The GPU path never uses these.

#include <cstddef>

#include <coldx/core/cpu_effect.h>
#include <coldx/core/cpu_math.h>

namespace coldx::examples {

// lerp(a, b, t) as HLSL defines it.
inline float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// grayscale.hlsl
struct GrayscaleParams {
    float Amount;
};

inline Float4 Grayscale(const CpuPixel& in, const GrayscaleParams& p) {
    const Float4 c = in.Sample() * in.color;
    const float grey = c.r * 0.299f + c.g * 0.587f + c.b * 0.114f;
    return {Lerp(c.r, grey, p.Amount), Lerp(c.g, grey, p.Amount), Lerp(c.b, grey, p.Amount), c.a};
}

// flash.hlsl
struct FlashParams {
    Float4 Color;
    float Amount;
};

inline Float4 Flash(const CpuPixel& in, const FlashParams& p) {
    const Float4 c = in.Sample() * in.color;
    return {Lerp(c.r, p.Color.r * c.a, p.Amount), Lerp(c.g, p.Color.g * c.a, p.Amount), Lerp(c.b, p.Color.b * c.a, p.Amount),
            c.a};
}

// wave.hlsl
struct WaveParams {
    float Time;
    float Amplitude;
    float Frequency;
};

inline Float4 Wave(const CpuPixel& in, const WaveParams& p) {
    const float width = static_cast<float>(in.TextureWidth());
    const float shift = p.Amplitude * CpuMath::Sin(in.position.y * p.Frequency + p.Time * 6.2831853f);
    return in.SampleAt(Vector2(in.uv.x + shift / width, in.uv.y)) * in.color;
}

inline void RegisterExampleCpuEffects() {
    CpuEffects::Register<GrayscaleParams>("grayscale", Grayscale, {{"Amount", offsetof(GrayscaleParams, Amount)}});
    CpuEffects::Register<FlashParams>("flash", Flash,
                                      {{"Color", offsetof(FlashParams, Color)}, {"Amount", offsetof(FlashParams, Amount)}});
    CpuEffects::Register<WaveParams>("wave", Wave,
                                     {{"Time", offsetof(WaveParams, Time)},
                                      {"Amplitude", offsetof(WaveParams, Amplitude)},
                                      {"Frequency", offsetof(WaveParams, Frequency)}});
}

} // namespace coldx::examples
