// Sine and cosine from float arithmetic only, in a fixed order: reduce to an octant with
// pi/4 in three parts, then a short polynomial on [-pi/4, pi/4]. The method and the
// coefficients are Cephes' sinf and cosf. No call reaches the platform's maths library, and
// the core library is compiled without fused multiply-add contraction, so every machine
// computes the same bits.

#include <coldx/core/cpu_math.h>

#include <cstdint>

namespace coldx::CpuMath {

namespace {

constexpr float FourOverPi = 1.27323954473516f;
// pi/4 as three floats whose sum is pi/4 to far beyond float precision.
constexpr float Dp1 = 0.78515625f;
constexpr float Dp2 = 2.4187564849853515625e-4f;
constexpr float Dp3 = 3.77489497744594108e-8f;

float SinPoly(float x) {
    const float z = x * x;
    return ((-1.9515295891e-4f * z + 8.3321608736e-3f) * z - 1.6666654611e-1f) * z * x + x;
}

float CosPoly(float x) {
    const float z = x * x;
    return ((2.443315711809948e-5f * z - 1.388731625493765e-3f) * z + 4.166664568298827e-2f) * z * z - 0.5f * z + 1.0f;
}

// x reduced into [-pi/4, pi/4], and which octant it came from (0 to 7, for x >= 0).
float Reduce(float x, std::uint32_t& octant) {
    std::uint32_t j = static_cast<std::uint32_t>(x * FourOverPi);
    float y = static_cast<float>(j);
    // Odd octants round up, so the remainder lands in [-pi/4, pi/4].
    if (j & 1u) {
        j += 1u;
        y += 1.0f;
    }
    octant = j & 7u;
    return ((x - y * Dp1) - y * Dp2) - y * Dp3;
}

} // namespace

float Sin(float x) {
    float sign = 1.0f;
    if (x < 0.0f) {
        x = -x;
        sign = -1.0f;
    }
    std::uint32_t octant = 0;
    const float r = Reduce(x, octant);
    if (octant > 3u) {
        sign = -sign;
        octant -= 4u;
    }
    const float v = (octant == 1u || octant == 2u) ? CosPoly(r) : SinPoly(r);
    return sign * v;
}

float Cos(float x) {
    if (x < 0.0f) x = -x;
    std::uint32_t octant = 0;
    const float r = Reduce(x, octant);
    float sign = 1.0f;
    if (octant > 3u) {
        octant -= 4u;
        sign = -sign;
    }
    if (octant > 1u) sign = -sign;
    const float v = (octant == 1u || octant == 2u) ? SinPoly(r) : CosPoly(r);
    return sign * v;
}

} // namespace coldx::CpuMath
