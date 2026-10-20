#pragma once

#include <memory>

#include <coldx/core/color.h>

namespace coldx {

namespace detail {
struct GraphicsAccess;
}

class GraphicsDevice {
public:
    ~GraphicsDevice();
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    // Fills the back buffer. Only has an effect inside Draw.
    void Clear(Color color);

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
