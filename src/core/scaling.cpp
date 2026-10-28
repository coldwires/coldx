#include "scaling.h"

#include <algorithm>
#include <cmath>

namespace coldx::detail {

Rectangle FitRectangle(int outWidth, int outHeight, int width, int height, ScaleMode mode) {
    if (outWidth <= 0 || outHeight <= 0 || width <= 0 || height <= 0) return {0, 0, std::max(0, outWidth), std::max(0, outHeight)};
    if (mode == ScaleMode::Stretch) return {0, 0, outWidth, outHeight};
    const double fit = std::min(static_cast<double>(outWidth) / width, static_cast<double>(outHeight) / height);
    // Integer scaling below 1x would be nothing at all. It shrinks as Fit does instead.
    const double scale = mode == ScaleMode::Integer && fit >= 1.0 ? std::floor(fit) : fit;
    const int w = std::clamp(static_cast<int>(std::lround(width * scale)), 1, outWidth);
    const int h = std::clamp(static_cast<int>(std::lround(height * scale)), 1, outHeight);
    return {(outWidth - w) / 2, (outHeight - h) / 2, w, h};
}

void OutputToImage(const Rectangle& placed, int width, int height, float x, float y, int& outX, int& outY) {
    if (placed.IsEmpty()) {
        outX = static_cast<int>(x);
        outY = static_cast<int>(y);
        return;
    }
    outX = static_cast<int>(std::floor((x - placed.x) * width / placed.width));
    outY = static_cast<int>(std::floor((y - placed.y) * height / placed.height));
}

void ImageToOutput(const Rectangle& placed, int width, int height, int x, int y, float& outX, float& outY) {
    if (width <= 0 || height <= 0) {
        outX = static_cast<float>(x);
        outY = static_cast<float>(y);
        return;
    }
    // The centre of the image pixel, so mapping back lands on the same pixel.
    outX = placed.x + (x + 0.5f) * placed.width / width;
    outY = placed.y + (y + 0.5f) * placed.height / height;
}

} // namespace coldx::detail
