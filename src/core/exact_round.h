#pragma once

#include <cmath>
#include <cstdint>

// Rounding to whole numbers from floor, trunc and subtraction only. Those are exactly
// specified, so every C library gives the same bits for them. nearbyint and lround are
// missing from some small libraries, and these replace them in coldx's drawing path.
namespace coldx::detail {

// The nearest whole number, halves to the even one, as std::nearbyint in the default
// rounding mode. x - floor(x) is exact for every float.
inline float NearestEven(float x) {
    const float below = std::floor(x);
    const float fraction = x - below;
    if (fraction > 0.5f) return below + 1.0f;
    if (fraction < 0.5f) return below;
    return (static_cast<std::int64_t>(below) & 1) == 0 ? below : below + 1.0f;
}

// The nearest whole number, halves away from zero, as std::lround. x - trunc(x) is exact.
inline long RoundAway(double x) {
    const double whole = std::trunc(x);
    const double fraction = x - whole;
    if (fraction >= 0.5) return static_cast<long>(whole) + 1;
    if (fraction <= -0.5) return static_cast<long>(whole) - 1;
    return static_cast<long>(whole);
}

inline long RoundAway(float x) {
    const float whole = std::trunc(x);
    const float fraction = x - whole;
    if (fraction >= 0.5f) return static_cast<long>(whole) + 1;
    if (fraction <= -0.5f) return static_cast<long>(whole) - 1;
    return static_cast<long>(whole);
}

} // namespace coldx::detail
