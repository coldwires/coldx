#pragma once

#include <memory>

#include <coldx/core/color.h>

namespace coldx {

class RenderTarget2D;

namespace detail {
struct GraphicsAccess;
}

class GraphicsDevice {
public:
    ~GraphicsDevice();
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    // Fills the current target. Only has an effect inside Draw.
    void Clear(Color color);

    // Where Clear and SpriteBatch draw from now on. nullptr, or an empty target, means the
    // back buffer. Only has an effect inside Draw; every frame starts on the back buffer.
    // Do not draw a target into itself.
    void SetRenderTarget(const RenderTarget2D* target);

    // Size of the back buffer in pixels, as of the current frame.
    int BackBufferWidth() const;
    int BackBufferHeight() const;

private:
    friend struct detail::GraphicsAccess;
    struct Impl;

    GraphicsDevice();
    std::unique_ptr<Impl> impl_;
};

} // namespace coldx
