#pragma once

#include <coldx/core/texture2d.h>

struct SDL_GPUTexture;

namespace coldx::detail {

struct TextureAccess {
    // Null for an empty texture.
    static SDL_GPUTexture* Handle(const Texture2D& texture);
};

} // namespace coldx::detail
