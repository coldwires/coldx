#pragma once

#include <coldx/core/effect.h>
#include <coldx/core/graphics_states.h>

#include <cstddef>
#include <cstdint>
#include <vector>

struct SDL_GPUGraphicsPipeline;
struct SDL_GPUTexture;

namespace coldx::detail {

// What SpriteRenderer needs from an Effect to draw with it.
struct EffectAccess {
    // The pipeline for sprites drawn with effect in blend, made on first use with the
    // graphics device's current GPU device. Null on failure, logged once per effect.
    static SDL_GPUGraphicsPipeline* Pipeline(const Effect& effect, GraphicsDevice& graphics, BlendState blend);
    // The uniform block as last set. Empty when the shader has none.
    static const std::vector<std::uint8_t>& Uniforms(const Effect& effect);
    // Texture slots the shader samples, the sprite texture's (slot 0) included.
    static std::size_t TextureCount(const Effect& effect);
    // What was set for slot (1 or later), or fallback if nothing was, which is logged once.
    static SDL_GPUTexture* Texture(const Effect& effect, std::size_t slot, SDL_GPUTexture* fallback);
};

} // namespace coldx::detail
