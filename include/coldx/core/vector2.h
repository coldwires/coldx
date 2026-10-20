#pragma once

namespace coldx {

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vector2() = default;
    constexpr Vector2(float x, float y) : x(x), y(y) {}

    constexpr Vector2 operator+(Vector2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vector2 operator-(Vector2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vector2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vector2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vector2 operator-() const { return {-x, -y}; }
    constexpr Vector2& operator+=(Vector2 o) { x += o.x; y += o.y; return *this; }
    constexpr Vector2& operator-=(Vector2 o) { x -= o.x; y -= o.y; return *this; }
    constexpr Vector2& operator*=(float s) { x *= s; y *= s; return *this; }
    friend constexpr bool operator==(Vector2, Vector2) = default;

    static constexpr Vector2 Zero() { return {0.0f, 0.0f}; }
    static constexpr Vector2 One() { return {1.0f, 1.0f}; }
};

constexpr Vector2 operator*(float s, Vector2 v) { return v * s; }

} // namespace coldx
