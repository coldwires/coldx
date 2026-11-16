#pragma once

#include <memory>
#include <string>

#include <coldx/core/graphics_device.h>

#include "scaling.h"

struct SDL_Window;

namespace coldx::detail {

class RenderBackend;

// The parts of GraphicsDevice that only coldx itself drives.
struct GraphicsAccess {
    static std::unique_ptr<GraphicsDevice> Create();

    // Creates the device on first call. Later calls reuse it. window is null in headless
    // mode, and the back buffer is then width x height with nothing presented.
    static bool Open(GraphicsDevice& graphics, SDL_Window* window, bool vsync, int width, int height);
    // Lets go of the window. The device stays until Close, so resources the game still
    // holds stay valid until the game object is destroyed.
    static void ReleaseWindow(GraphicsDevice& graphics);
    // Present with or without waiting for the display. No effect headless.
    static void SetVSync(GraphicsDevice& graphics, bool vsync);
    // Headless only: the back buffer size, which with a window follows the window.
    static void ResizeHeadless(GraphicsDevice& graphics, int width, int height);
    // A fixed back buffer size scaled into the window, or inactive for one that follows the
    // window. Takes effect at the next frame, or at once when headless.
    static void SetPresentation(GraphicsDevice& graphics, const Presentation& presentation);
    static void SetPixelArt(GraphicsDevice& graphics, bool pixelArt);
    static void Close(GraphicsDevice& graphics);

    // False when there is nothing to draw into this frame, e.g. the window is minimized.
    // EndFrame must still be called.
    static bool BeginFrame(GraphicsDevice& graphics);
    static void EndFrame(GraphicsDevice& graphics);

    // Writes the back buffer as it was after the last frame to a PNG. Waits for the GPU.
    static bool SaveBackBuffer(GraphicsDevice& graphics, const std::string& path);

    // What the device draws through. Exists from Create, before Open.
    static RenderBackend& Backend(GraphicsDevice& graphics);
};

} // namespace coldx::detail
