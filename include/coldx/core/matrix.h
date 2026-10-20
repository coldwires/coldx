#pragma once

#include <coldx/core/vector2.h>

namespace coldx {

// 4x4 matrix in XNA's convention: row vectors, so a point is transformed as v * M, and
// A * B applies A first. m[row][col]; the translation is m[3][0..2].
struct Matrix {
    float m[4][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };

    static constexpr Matrix Identity() { return {}; }

    static constexpr Matrix CreateTranslation(float x, float y, float z = 0.0f) {
        Matrix r;
        r.m[3][0] = x;
        r.m[3][1] = y;
        r.m[3][2] = z;
        return r;
    }

    static constexpr Matrix CreateScale(float x, float y, float z = 1.0f) {
        Matrix r;
        r.m[0][0] = x;
        r.m[1][1] = y;
        r.m[2][2] = z;
        return r;
    }

    static constexpr Matrix CreateScale(float s) { return CreateScale(s, s, s); }

    // Rotation about the z axis. With y pointing down, as in screen space, positive radians
    // turn clockwise.
    static Matrix CreateRotationZ(float radians);

    // Maps the box to clip space: x from left..right to -1..1, y from bottom..top to -1..1,
    // z from near..far to 0..1.
    static constexpr Matrix CreateOrthographicOffCenter(float left, float right, float bottom, float top,
                                                        float zNear, float zFar) {
        Matrix r;
        r.m[0][0] = 2.0f / (right - left);
        r.m[1][1] = 2.0f / (top - bottom);
        r.m[2][2] = 1.0f / (zNear - zFar);
        r.m[3][0] = (left + right) / (left - right);
        r.m[3][1] = (top + bottom) / (bottom - top);
        r.m[3][2] = zNear / (zNear - zFar);
        return r;
    }

    // False, leaving result unchanged, when the matrix has no inverse.
    static bool Invert(const Matrix& matrix, Matrix& result);

    friend constexpr Matrix operator*(const Matrix& a, const Matrix& b) {
        Matrix r;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                r.m[row][col] = a.m[row][0] * b.m[0][col] + a.m[row][1] * b.m[1][col] +
                                a.m[row][2] * b.m[2][col] + a.m[row][3] * b.m[3][col];
            }
        }
        return r;
    }

    friend constexpr bool operator==(const Matrix& a, const Matrix& b) {
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                if (a.m[row][col] != b.m[row][col]) return false;
        return true;
    }
};

// v * M for a point (w = 1).
constexpr Vector2 Transform(Vector2 v, const Matrix& m) {
    return {v.x * m.m[0][0] + v.y * m.m[1][0] + m.m[3][0], v.x * m.m[0][1] + v.y * m.m[1][1] + m.m[3][1]};
}

} // namespace coldx
