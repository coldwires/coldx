#include <coldx/coldx.h>
#include <coldx/core/cpu_math.h>

#include <cmath>
#include <cstring>

#include "exact_round.h"

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

static bool Near(float a, float b) {
    return a - b < 1e-4f && b - a < 1e-4f;
}

static bool NearMatrix(const Matrix& a, const Matrix& b) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (!Near(a.m[r][c], b.m[r][c])) return false;
    return true;
}

static void Matrices() {
    static_assert(Matrix::Identity() * Matrix::Identity() == Matrix::Identity());
    // Row vectors: S * T scales first, then translates.
    constexpr Matrix st = Matrix::CreateScale(2.0f) * Matrix::CreateTranslation(10.0f, 20.0f);
    static_assert(Transform(Vector2(1, 1), st) == Vector2(12, 22));
    constexpr Matrix ts = Matrix::CreateTranslation(10.0f, 20.0f) * Matrix::CreateScale(2.0f);
    static_assert(Transform(Vector2(1, 1), ts) == Vector2(22, 42));

    // Clockwise in screen space: +x turns toward +y.
    const Vector2 turned = Transform(Vector2(1, 0), Matrix::CreateRotationZ(MathHelper::PiOver2));
    CHECK(Near(turned.x, 0.0f) && Near(turned.y, 1.0f));

    // The sprite projection: corners of a 640x480 target land on clip-space corners.
    const Matrix ortho = Matrix::CreateOrthographicOffCenter(0, 640, 480, 0, 0, 1);
    CHECK(Transform(Vector2(0, 0), ortho) == Vector2(-1, 1));
    CHECK(Transform(Vector2(640, 480), ortho) == Vector2(1, -1));

    const Matrix camera = Matrix::CreateTranslation(-37.5f, 12.0f) * Matrix::CreateRotationZ(0.3f) *
                          Matrix::CreateScale(3.0f, 2.5f, 1.0f) * Matrix::CreateTranslation(320.0f, 240.0f);
    Matrix inverse;
    CHECK(Matrix::Invert(camera, inverse));
    CHECK(NearMatrix(camera * inverse, Matrix::Identity()));
    CHECK(NearMatrix(inverse * camera, Matrix::Identity()));
    const Vector2 world(123.0f, -45.0f);
    const Vector2 back = Transform(Transform(world, camera), inverse);
    CHECK(Near(back.x, world.x) && Near(back.y, world.y));

    Matrix untouched = Matrix::CreateTranslation(1, 2);
    CHECK(!Matrix::Invert(Matrix::CreateScale(0.0f), untouched));
    CHECK(untouched == Matrix::CreateTranslation(1, 2));
}

static void Helpers() {
    static_assert(Point(1, 2) + Point(3, 4) == Point(4, 6));
    static_assert(MathHelper::Clamp(5.0f, 0.0f, 1.0f) == 1.0f);
    static_assert(MathHelper::Clamp(-5.0f, 0.0f, 1.0f) == 0.0f);
    static_assert(MathHelper::Lerp(10.0f, 20.0f, 0.25f) == 12.5f);
    CHECK(Near(MathHelper::ToRadians(180.0f), MathHelper::Pi));
    CHECK(Near(MathHelper::ToDegrees(MathHelper::PiOver2), 90.0f));
}

// CpuMath against the double-precision library, over a range effects use.
void CpuSinCos() {
    double worst = 0.0;
    for (int i = -200000; i <= 200000; ++i) {
        const float x = static_cast<float>(i) * 0.0005f;  // -100 to 100 radians
        worst = std::fmax(worst, std::fabs(CpuMath::Sin(x) - std::sin(static_cast<double>(x))));
        worst = std::fmax(worst, std::fabs(CpuMath::Cos(x) - std::cos(static_cast<double>(x))));
    }
    std::printf("CpuMath Sin and Cos: worst error %.3g\n", worst);
    CHECK(worst < 3e-7);
    CHECK(CpuMath::Sin(0.0f) == 0.0f);
    CHECK(CpuMath::Cos(0.0f) == 1.0f);
    CHECK(CpuMath::Sin(-1.0f) == -CpuMath::Sin(1.0f));
}

// NearestEven and RoundAway against std::nearbyint and std::lround: every quarter from
// -2048 to 2048, halves included, and floats spread across the whole range used for pixels.
void ExactRounding() {
    int wrong = 0;
    auto check = [&](float x) {
        const float a = detail::NearestEven(x), b = std::nearbyint(x);
        if (std::memcmp(&a, &b, sizeof a) != 0 && !(a == 0.0f && b == 0.0f)) ++wrong;
        if (detail::RoundAway(x) != std::lround(x)) ++wrong;
        if (detail::RoundAway(static_cast<double>(x)) != std::lround(static_cast<double>(x))) ++wrong;
    };
    for (int i = -8192; i <= 8192; ++i) check(static_cast<float>(i) * 0.25f);
    for (std::uint32_t bits = 0x30000000u; bits < 0x4B000000u; bits += 977u) {
        float x;
        std::memcpy(&x, &bits, sizeof x);
        check(x);
        check(-x);
    }
    check(0.49999997f);
    check(-0.49999997f);
    check(8388607.5f);
    CHECK(wrong == 0);
}

int main() {
    CpuSinCos();
    ExactRounding();
    Vectors();
    Rectangles();
    Colors();
    Keys();
    Matrices();
    Helpers();
    return coldx_test::CheckResult();
}
