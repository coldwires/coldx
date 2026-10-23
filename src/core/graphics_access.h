#pragma once

#include <memory>
#include <string>

#include <coldx/core/graphics_device.h>

#include "scaling.h"

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPUTexture;

namespace coldx::detail {

// Shared by the device and every GPU resource made from it. The device clears it when it
// is destroyed, so a resource that outlives the device skips releasing instead of using a
// dead pointer.
struct DeviceToken {
    SDL_GPUDevice* device = nullptr;
};

// The parts of GraphicsDevice that only coldx itself drives.
struct GraphicsAccess {
    static std::unique_ptr<GraphicsDevice> Create();

    // Creates the GPU device on first call; later calls reuse it. window is null in headless
    // mode, and the back buffer is then width x height with nothing presented.
    static bool Open(GraphicsDevice& graphics, SDL_Window* window, bool vsync, int width, int height);
    // Lets go of the window. The device stays until Close, so GPU resources the game still
    // holds stay valid until the game object is destroyed.
    static void ReleaseWindow(GraphicsDevice& graphics);
    // Present with or without waiting for the display. No effect headless.
    static void SetVSync(GraphicsDevice& graphics, bool vsync);
    // Headless only: the back buffer size, which with a window follows the window.
    static void ResizeHeadless(GraphicsDevice& graphics, int width, int height);
    // A fixed back buffer size scaled into the window, or inactive for one that follows the
    // window. Takes effect at the next frame; headless, at once.
    static void SetPresentation(GraphicsDevice& graphics, const Presentation& presentation);
    static void SetPixelArt(GraphicsDevice& graphics, bool pixelArt);
    static void Close(GraphicsDevice& graphics);

    // False when there is nothing to draw into this frame, e.g. the window is minimized.
    // EndFrame must still be called.
    static bool BeginFrame(GraphicsDevice& graphics);
    static void EndFrame(GraphicsDevice& graphics);

    // Writes the back buffer as it was after the last frame to a PNG. Waits for the GPU.
    static bool SaveBackBuffer(GraphicsDevice& graphics, const std::string& path);

    // Null before Open and after Close.
    static SDL_GPUDevice* Device(GraphicsDevice& graphics);
    static std::shared_ptr<DeviceToken> Token(GraphicsDevice& graphics);
    // All null or zero outside a frame. The current target is the back buffer unless the
    // game set a render target.
    static SDL_GPUCommandBuffer* Commands(GraphicsDevice& graphics);
    static SDL_GPUTexture* CurrentTarget(GraphicsDevice& graphics);
    static int CurrentTargetWidth(GraphicsDevice& graphics);
    static int CurrentTargetHeight(GraphicsDevice& graphics);

private:
    static bool ResizeBackBuffer(GraphicsDevice& graphics, unsigned width, unsigned height);
};

} // namespace coldx::detail
