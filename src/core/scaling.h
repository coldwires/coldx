#pragma once

#include <coldx/core/color.h>
#include <coldx/core/graphics_device.h>
#include <coldx/core/rectangle.h>

namespace coldx::detail {

// A fixed virtual resolution and how it is shown. Inactive (0 x 0): the back buffer is the
// window's pixel size and is shown 1:1.
struct Presentation {
    int width = 0;
    int height = 0;
    ScaleMode mode = ScaleMode::Fit;
    bool smooth = false;
    Color letterbox = Color::Black;

    bool Active() const { return width > 0 && height > 0; }
};

// Where a width x height image lands in an output of outWidth x outHeight pixels. Whole
// pixels, centred, so nearest filtering gives the same image every frame.
Rectangle FitRectangle(int outWidth, int outHeight, int width, int height, ScaleMode mode);

// An output pixel position to the image's pixels, through FitRectangle. Positions in the bars
// map outside the image (negative, or past its size).
void OutputToImage(const Rectangle& placed, int width, int height, float x, float y, int& outX, int& outY);
// The reverse: an image pixel to the output.
void ImageToOutput(const Rectangle& placed, int width, int height, int x, int y, float& outX, float& outY);

} // namespace coldx::detail
