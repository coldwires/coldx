#pragma once

#include <coldx/core/color.h>

namespace coldx::detail {

// Writes width x height pixels to a PNG. straightAlpha undoes premultiplying first, so the
// file loads back through Texture2D::FromFile as the same image. False, logged, on failure.
bool WritePng(const char* path, int width, int height, const Color* pixels, bool straightAlpha);

} // namespace coldx::detail
