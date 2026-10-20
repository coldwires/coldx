#include <coldx/coldx.h>

#include "check.h"

using namespace coldx;

static void Vectors() {
    constexpr Vector2 a(1.0f, 2.0f);
    constexpr Vector2 b(3.0f, -4.0f);
    static_assert(a + b == Vector2(4.0f, -2.0f));
    static_assert(b - a == Vector2(2.0f, -6.0f));
    static_assert(a * 2.0f == Vector2(2.0f, 4.0f));
    static_assert(2.0f * a == a * 2.0f);
    static_assert(-a == Vector2(-1.0f, -2.0f));
    Vector2 c = a;
    c += b;
    c *= 0.5f;
    CHECK(c == Vector2(2.0f, -1.0f));
}

static void Rectangles() {
    constexpr Rectangle r(10, 20, 30, 40);
    static_assert(r.Right() == 40 && r.Bottom() == 60);
    static_assert(r.Contains(10, 20) && r.Contains(39, 59));
    static_assert(!r.Contains(40, 20) && !r.Contains(10, 60));
    static_assert(r.Intersects(Rectangle(39, 59, 5, 5)));
    static_assert(!r.Intersects(Rectangle(40, 20, 5, 5)));
    static_assert(Rectangle(0, 0, 0, 5).IsEmpty());
    CHECK(!r.IsEmpty());
}

static void Colors() {
    static_assert(sizeof(Color) == 4);
    static_assert(Color::CornflowerBlue == Color(100, 149, 237, 255));
    static_assert(Color::White * 0.5f == Color(128, 128, 128, 128));
    static_assert(Color::White * 0.0f == Color::Transparent);
    // Clamps rather than wrapping.
    static_assert(Color(200, 10, 0, 255) * 2.0f == Color(255, 20, 0, 255));
    static_assert(Color(200, 10, 0, 255) * -1.0f == Color::Transparent);
    CHECK(Color::Red * 1.0f == Color::Red);
}

static void Keys() {
    KeyboardState state;
    CHECK(state.IsKeyUp(Key::A));
    state.SetKey(Key::A, true);
    state.SetKey(Key::RightSuper, true);
    CHECK(state.IsKeyDown(Key::A) && state.IsKeyDown(Key::RightSuper));
    CHECK(state.IsKeyUp(Key::B));
    state.SetKey(Key::A, false);
    CHECK(state.IsKeyUp(Key::A));
    // Out of range is ignored, not a crash.
    state.SetKey(static_cast<Key>(9999), true);
    CHECK(state.IsKeyUp(static_cast<Key>(9999)));
}

int main() {
    Vectors();
    Rectangles();
    Colors();
    Keys();
    return coldx_test::CheckResult();
}
