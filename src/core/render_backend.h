#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <coldx/core/color.h>
#include <coldx/core/graphics_states.h>
#include <coldx/core/matrix.h>
#include <coldx/core/rectangle.h>

#include "scaling.h"

struct SDL_Window;

namespace coldx {

class Effect;

namespace effect_format {
struct Compiled;
}

namespace detail {

// What GraphicsDevice, Texture2D, Effect and SpriteBatch draw through. One backend per
// GraphicsDevice, chosen when it is made. Nothing above this header knows which it is.
// Every object a backend hands out is only ever passed back to that same backend.

// A texture's pixels, RGBA8 premultiplied, in whatever form the backend keeps them.
// Destroyed with the Texture2D that owns it. One that outlives its device does nothing.
class BackendTexture {
public:
    virtual ~BackendTexture() = default;
    // Replaces every pixel, width * height of them, rows top to bottom.
    virtual bool Upload(const Color* pixels) = 0;
    // Copies region out, rows top to bottom. Sees what earlier frames drew, not one still
    // being recorded. False on failure, logged, or silently once the device is gone.
    virtual bool Download(const Rectangle& region, Color* out) const = 0;
};

// What a backend keeps for one Effect. Made with the effect, destroyed with it.
class BackendEffect {
public:
    virtual ~BackendEffect() = default;
};

struct SpriteVertex {
    float x, y;
    float u, v;
    Color color;
    // The source rectangle's UV limits (min u, min v, max u, max v). The fragment stage
    // clamps sampling to them so atlas neighbours never bleed in.
    float bounds[4];
};
static_assert(sizeof(SpriteVertex) == 36);

// One sprite: four corners, top-left, top-right, bottom-left, bottom-right.
struct SpriteQuad {
    const BackendTexture* texture;
    SpriteVertex vertices[4];
};

// The drawing half of SpriteBatch.
class SpriteRenderer {
public:
    virtual ~SpriteRenderer() = default;
    // Draws quads in order into the current target, in the current frame. With a non-empty
    // effect, its fragment stage and its parameters as they are now. An effect that cannot
    // be used is logged and the built-in shader draws instead. Logs and returns false
    // outside a frame or on failure.
    virtual bool Draw(const SpriteQuad* quads, std::uint32_t count, BlendState blend, SamplerState sampler,
                      const Effect& effect, const Matrix& transform) = 0;
};

class RenderBackend {
public:
    virtual ~RenderBackend() = default;

    // Creates the device on first call. Later calls reuse it. window is null headless, and
    // the back buffer is then width x height with nothing presented.
    virtual bool Open(SDL_Window* window, bool vsync, int width, int height) = 0;
    // Lets go of the window. The device stays until Close.
    virtual void ReleaseWindow() = 0;
    virtual void Close() = 0;
    // Whether Open has made a device that Close has not destroyed.
    virtual bool HasDevice() const = 0;

    virtual void SetVSync(bool vsync) = 0;
    virtual void ResizeHeadless(int width, int height) = 0;
    virtual void SetPresentation(const Presentation& presentation) = 0;

    // False when there is nothing to draw into this frame. EndFrame must still be called.
    virtual bool BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual bool InFrame() const = 0;

    // Fills the current target. Only inside a frame.
    virtual void Clear(Color color) = 0;
    // Null means the back buffer. Only inside a frame.
    virtual void SetTarget(const BackendTexture* target, int width, int height) = 0;

    virtual int BackBufferWidth() const = 0;
    virtual int BackBufferHeight() const = 0;
    virtual Rectangle PresentedArea() const = 0;
    // The back buffer as the last finished frame left it, premultiplied.
    virtual bool ReadBackBuffer(Color* pixels) const = 0;

    // Null on failure, logged. Needs the device. A render target starts transparent.
    virtual std::unique_ptr<BackendTexture> CreateTexture(int width, int height, bool renderTarget) = 0;
    // compiled and name must outlive what is returned. Built now when the device exists, so
    // a shader the driver rejects fails at load: null then, logged.
    virtual std::unique_ptr<BackendEffect> CreateEffect(const effect_format::Compiled& compiled, const std::string& name) = 0;
    // Never null. Usable before Open: it makes what it needs on first draw.
    virtual std::unique_ptr<SpriteRenderer> CreateSpriteRenderer() = 0;
};

// SDL_GPU: Direct3D 12, Vulkan or Metal, as SDL picks or SDL_GPU_DRIVER says.
std::unique_ptr<RenderBackend> CreateGpuBackend();

} // namespace detail
} // namespace coldx
