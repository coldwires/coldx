#pragma once

#include <coldx/core/effect.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coldx::detail {

class BackendEffect;
class BackendTexture;

// What a SpriteRenderer needs from an Effect to draw with it.
struct EffectAccess {
    // What the graphics device's backend made for the effect when it was loaded.
    static BackendEffect& Backend(const Effect& effect);
    // The uniform block as last set. Empty when the shader has none.
    static const std::vector<std::uint8_t>& Uniforms(const Effect& effect);
    // Texture slots the shader samples, the sprite texture's (slot 0) included.
    static std::size_t TextureCount(const Effect& effect);
    // What was set for slot (1 or later), or fallback if nothing was, which is logged once.
    static const BackendTexture* Texture(const Effect& effect, std::size_t slot, const BackendTexture* fallback);
};

} // namespace coldx::detail
