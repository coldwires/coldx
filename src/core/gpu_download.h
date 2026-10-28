#pragma once

#include <coldx/core/color.h>
#include <coldx/core/rectangle.h>

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace coldx::detail {

// Copies region of an R8G8B8A8 texture into out (region.width * region.height pixels, rows
// top to bottom) on a command buffer of its own, and waits for it. Work submitted before it
// is included. A frame still being recorded is not. False, logged, on failure.
bool DownloadTexture(SDL_GPUDevice* device, SDL_GPUTexture* texture, const Rectangle& region, Color* out);

// Writes width x height pixels to a PNG. straightAlpha undoes premultiplying first, so the
// file loads back through Texture2D::FromFile as the same image. False, logged, on failure.
bool WritePng(const char* path, int width, int height, const Color* pixels, bool straightAlpha);

} // namespace coldx::detail
