#pragma once

#include <cstdint>
#include <memory>

#include <coldx/core/color.h>
#include <coldx/core/graphics_states.h>
#include <coldx/core/matrix.h>

struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUShader;
struct SDL_GPUTexture;

namespace coldx {

class Effect;
class GraphicsDevice;

namespace detail {

struct DeviceToken;

inline constexpr int BlendStateCount = 4;

struct SpriteVertex {
    float x, y;
    float u, v;
    Color color;
    // The source rectangle's UV limits (min u, min v, max u, max v). The fragment shader
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

// A pipeline drawing SpriteVertex quads into the back buffer's format with blend. The
// built-in shaders and every Effect share it. Null on failure, logged.
SDL_GPUGraphicsPipeline* CreateSpritePipeline(SDL_GPUDevice* device, SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                                              BlendState blend);

// The GPU half of SpriteBatch: pipelines, samplers, buffers, and the draw itself.
class SpriteRenderer {
public:
    SpriteRenderer();
    ~SpriteRenderer();
    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    // Draws quads in order into the current target, in the current frame. One GPU draw per
    // run of quads sharing a texture. With a non-empty effect, its fragment shader and its
    // parameters as they are now. An effect that cannot be used is logged and the built-in shader
    // draws instead. Logs and returns false outside a frame or on failure.
    bool Draw(GraphicsDevice& graphics, const SpriteQuad* quads, std::uint32_t count, BlendState blend,
              SamplerState sampler, const Effect& effect, const Matrix& transform);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace detail
} // namespace coldx
