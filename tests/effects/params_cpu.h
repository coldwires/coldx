#pragma once

// The CPU version of params.hlsl, for the CPU renderer's run of effect_test.cpp.

#include <cstddef>
#include <cstdint>

#include <coldx/core/cpu_effect.h>

namespace coldx::test_effects {

// Laid out as HLSL packs the block: C may not straddle a 16-byte row, so it starts the
// second one, after a gap at 12.
struct ParamsParams {
    float A;
    float B[2];
    float gap;
    float C[3];
    float D;
    float E[4];
    float M[16];  // as Effect::Set(Matrix) wrote it, row-major
    std::int32_t Mode;
    std::uint32_t Invert;
};

inline Float4 Params(const CpuPixel& in, const ParamsParams& p) {
    const Float4 texel = in.Sample();
    Float4 c;
    if (p.Mode == 0) c = {p.A, p.B[1], p.C[2], p.D};
    else if (p.Mode == 1) c = {p.E[0], p.E[1], p.E[2], p.E[3]};
    // mul(M, float4(0, 0, 0, 1)): the shader reads the row-major block as its transpose, so
    // this is the last row as written.
    else if (p.Mode == 2) c = {p.M[12], p.M[13], p.M[14], p.M[15]};
    else if (p.Mode == 3) c = in.SampleTexture(1, in.uv);
    else c = {p.B[0], p.C[0], p.C[1], 1.0f};
    if (p.Invert) c = {1.0f - c.r, 1.0f - c.g, 1.0f - c.b, c.a};
    return c * texel.a;
}

inline void RegisterParamsCpuEffect() {
    CpuEffects::Register<ParamsParams>("params", Params,
                                       {{"A", offsetof(ParamsParams, A)},
                                        {"B", offsetof(ParamsParams, B)},
                                        {"C", offsetof(ParamsParams, C)},
                                        {"D", offsetof(ParamsParams, D)},
                                        {"E", offsetof(ParamsParams, E)},
                                        {"M", offsetof(ParamsParams, M)},
                                        {"Mode", offsetof(ParamsParams, Mode)},
                                        {"Invert", offsetof(ParamsParams, Invert)}});
}

} // namespace coldx::test_effects
