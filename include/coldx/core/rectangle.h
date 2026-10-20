#pragma once

namespace coldx {

// Integer rectangle in pixels, top-left origin, y down.
struct Rectangle {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    constexpr Rectangle() = default;
    constexpr Rectangle(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {}

    constexpr int Left() const { return x; }
    constexpr int Top() const { return y; }
    constexpr int Right() const { return x + width; }
    constexpr int Bottom() const { return y + height; }
    constexpr bool IsEmpty() const { return width <= 0 || height <= 0; }

    constexpr bool Contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
    constexpr bool Intersects(const Rectangle& o) const {
        return o.x < Right() && x < o.Right() && o.y < Bottom() && y < o.Bottom();
    }

    friend constexpr bool operator==(const Rectangle&, const Rectangle&) = default;
};

} // namespace coldx
