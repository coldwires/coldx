#pragma once

namespace coldx::MathHelper {

inline constexpr float Pi = 3.14159265358979323846f;
inline constexpr float TwoPi = Pi * 2.0f;
inline constexpr float PiOver2 = Pi / 2.0f;
inline constexpr float PiOver4 = Pi / 4.0f;

constexpr float ToRadians(float degrees) { return degrees * (Pi / 180.0f); }
constexpr float ToDegrees(float radians) { return radians * (180.0f / Pi); }

constexpr float Clamp(float value, float min, float max) {
    return value < min ? min : (value > max ? max : value);
}

constexpr float Lerp(float from, float to, float amount) { return from + (to - from) * amount; }

} // namespace coldx::MathHelper
