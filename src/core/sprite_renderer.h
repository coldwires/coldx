#pragma once

#include <cstdint>
#include <memory>

#include <coldx/core/color.h>
#include <coldx/core/graphics_states.h>
#include <coldx/core/matrix.h>

struct SDL_GPUTexture;

namespace coldx {

class GraphicsDevice;

namespace detail {

struct DeviceToken;

struct SpriteVertex {
    float x, y;
    float u, v;
    Color color;
    // The source rectangle's UV limits (min u, min v, max u, max v); the fragment shader
    // clamps sampling to them so atlas neighbours never bleed in.
    float bounds[4];
};
static_assert(sizeof(SpriteVertex) == 36);

// One sprite as the GPU sees it: four corners, top-left, top-right, bottom-left,
// bottom-right.
struct SpriteQuad {
    SDL_GPUTexture* texture;
    SpriteVertex vertices[4];
};

// The GPU half of SpriteBatch: pipelines, samplers, buffers, and the draw itself.
class SpriteRenderer {
public:
    SpriteRenderer();
    ~SpriteRenderer();
    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    // Draws quads in order into the current target, in the current frame. One GPU draw per
    // run of quads sharing a texture. Logs and returns false outside a frame or on failure.
    bool Draw(GraphicsDevice& graphics, const SpriteQuad* quads, std::uint32_t count, BlendState blend,
              SamplerState sampler, const Matrix& transform);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace detail
} // namespace coldx
