#include <coldx/core/matrix.h>

#include <cmath>
#include <utility>

namespace coldx {

Matrix Matrix::CreateRotationZ(float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Matrix r;
    r.m[0][0] = c;
    r.m[0][1] = s;
    r.m[1][0] = -s;
    r.m[1][1] = c;
    return r;
}

// Gauss-Jordan elimination with partial pivoting, in double so near-singular game
// transforms (a camera zoomed far out) still invert cleanly.
bool Matrix::Invert(const Matrix& matrix, Matrix& result) {
    double a[4][8];
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            a[row][col] = matrix.m[row][col];
            a[row][col + 4] = row == col ? 1.0 : 0.0;
        }
    }
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row)
            if (std::fabs(a[row][col]) > std::fabs(a[pivot][col])) pivot = row;
        if (std::fabs(a[pivot][col]) < 1e-12) return false;
        if (pivot != col)
            for (int k = 0; k < 8; ++k) std::swap(a[pivot][k], a[col][k]);
        const double scale = 1.0 / a[col][col];
        for (int k = 0; k < 8; ++k) a[col][k] *= scale;
        for (int row = 0; row < 4; ++row) {
            if (row == col) continue;
            const double factor = a[row][col];
            for (int k = 0; k < 8; ++k) a[row][k] -= factor * a[col][k];
        }
    }
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) result.m[row][col] = static_cast<float>(a[row][col + 4]);
    return true;
}

} // namespace coldx
