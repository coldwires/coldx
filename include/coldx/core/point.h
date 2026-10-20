#pragma once

namespace coldx {

// Integer position in pixels.
struct Point {
    int x = 0;
    int y = 0;

    constexpr Point() = default;
    constexpr Point(int x, int y) : x(x), y(y) {}

    constexpr Point operator+(Point o) const { return {x + o.x, y + o.y}; }
    constexpr Point operator-(Point o) const { return {x - o.x, y - o.y}; }
    friend constexpr bool operator==(Point, Point) = default;
};

} // namespace coldx
