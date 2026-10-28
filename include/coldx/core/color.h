#pragma once

#include <cstdint>

namespace coldx {

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr Color() = default;
    constexpr Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    friend constexpr bool operator==(Color, Color) = default;

    // Scales all four channels, which for a premultiplied color fades it. White * 0.5f is
    // white at half opacity.
    friend constexpr Color operator*(Color c, float s) {
        auto scale = [s](std::uint8_t v) {
            const float f = v * s + 0.5f;
            return static_cast<std::uint8_t>(f < 0.0f ? 0.0f : (f > 255.0f ? 255.0f : f));
        };
        return {scale(c.r), scale(c.g), scale(c.b), scale(c.a)};
    }

    static const Color Transparent;
    static const Color Black;
    static const Color White;
    static const Color Gray;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Cyan;
    static const Color Magenta;
    static const Color CornflowerBlue;
};

inline constexpr Color Color::Transparent{0, 0, 0, 0};
inline constexpr Color Color::Black{0, 0, 0};
inline constexpr Color Color::White{255, 255, 255};
inline constexpr Color Color::Gray{128, 128, 128};
inline constexpr Color Color::Red{255, 0, 0};
inline constexpr Color Color::Green{0, 128, 0};
inline constexpr Color Color::Blue{0, 0, 255};
inline constexpr Color Color::Yellow{255, 255, 0};
inline constexpr Color Color::Cyan{0, 255, 255};
inline constexpr Color Color::Magenta{255, 0, 255};
inline constexpr Color Color::CornflowerBlue{100, 149, 237};

} // namespace coldx
