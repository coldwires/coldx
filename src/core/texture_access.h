#pragma once

#include <coldx/core/texture2d.h>

namespace coldx::detail {

class BackendTexture;

struct TextureAccess {
    // Null for an empty texture.
    static const BackendTexture* Handle(const Texture2D& texture);
};

} // namespace coldx::detail
