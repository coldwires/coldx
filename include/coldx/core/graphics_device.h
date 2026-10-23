#pragma once

#include <memory>

#include <coldx/core/color.h>
#include <coldx/core/rectangle.h>

namespace coldx {

class RenderTarget2D;

// How a fixed virtual resolution (GameSettings::virtualWidth and virtualHeight) fills the
// window.
enum class ScaleMode {
    Fit,      // as large as fits with the aspect ratio kept; bars on two sides
    Integer,  // whole multiples only (1x, 2x, 3x), for pixel art; below 1x it shrinks as Fit
    Stretch,  // fills the window; the aspect ratio is not kept
};

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

    // Size of the back buffer in pixels, as of the current frame. With a virtual resolution
    // set, that size, whatever the window's.
    int BackBufferWidth() const;
    int BackBufferHeight() const;

    // Whether the game set GameSettings::pixelArt: SpriteBatch's defaults follow it.
    bool PixelArt() const;

    // Where the back buffer was shown in the window last frame, in window pixels. The whole
    // window unless a virtual resolution is set; empty headless.
    Rectangle PresentedArea() const;

    // The back buffer as the last finished frame left it, BackBufferWidth() *
    // BackBufferHeight() pixels, premultiplied. Waits for the GPU, as Texture2D::GetData.
    bool GetBackBufferData(Color* pixels) const;

private:
    friend struct detail::GraphicsAccess;
    struct Impl;

    GraphicsDevice();
    std::unique_ptr<Impl> impl_;
};

} // namespace coldx
