#pragma once

#include <memory>
#include <string>

#include <coldx/core/graphics_device.h>

struct SDL_Window;

namespace coldx::detail {

// The parts of GraphicsDevice that only Game drives.
struct GraphicsAccess {
    static std::unique_ptr<GraphicsDevice> Create();

    // window is null in headless mode. The back buffer is then width x height and nothing
    // is presented.
    static bool Open(GraphicsDevice& graphics, SDL_Window* window, bool vsync, int width, int height);
    static void Close(GraphicsDevice& graphics);

    // False when there is nothing to draw into this frame, e.g. the window is minimized.
    // EndFrame must still be called.
    static bool BeginFrame(GraphicsDevice& graphics);
    static void EndFrame(GraphicsDevice& graphics);

    // Writes the back buffer as it was after the last frame to a PNG. Waits for the GPU.
    static bool SaveBackBuffer(GraphicsDevice& graphics, const std::string& path);

private:
    static bool ResizeBackBuffer(GraphicsDevice& graphics, unsigned width, unsigned height);
};

} // namespace coldx::detail
