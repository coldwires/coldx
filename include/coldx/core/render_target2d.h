#pragma once

#include <coldx/core/texture2d.h>

namespace coldx {

class GraphicsDevice;

// A texture that can also be drawn into. Pass it to GraphicsDevice::SetRenderTarget, draw,
// set the target back to nullptr, then draw it like any Texture2D. Starts out transparent.
class RenderTarget2D : public Texture2D {
public:
    RenderTarget2D() = default;

    // Empty on failure, with the reason logged. Create from Initialize or LoadContent on.
    static RenderTarget2D Create(GraphicsDevice& graphics, int width, int height);
};

} // namespace coldx
