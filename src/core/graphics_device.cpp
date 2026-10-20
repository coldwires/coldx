#include <coldx/core/graphics_device.h>

#include <SDL3/SDL.h>

#include "graphics_access.h"

namespace coldx {

// Drawing goes into backBuffer, which is copied to the window's swapchain texture at the end
// of the frame. The swapchain texture cannot be read back and does not exist without a visible
// window, so this is what lets a frame be captured and lets headless mode draw at all.
struct GraphicsDevice::Impl {
    SDL_Window* window = nullptr;
    SDL_GPUDevice* device = nullptr;
    SDL_GPUCommandBuffer* commands = nullptr;
    SDL_GPUTexture* swapchain = nullptr;
    SDL_GPUTexture* backBuffer = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    bool inFrame = false;
    std::shared_ptr<detail::DeviceToken> token;
};

static constexpr SDL_GPUTextureFormat BackBufferFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

GraphicsDevice::GraphicsDevice() : impl_(std::make_unique<Impl>()) {}

GraphicsDevice::~GraphicsDevice() {
    detail::GraphicsAccess::Close(*this);
}

void GraphicsDevice::Clear(Color color) {
    Impl& g = *impl_;
    if (!g.inFrame) return;

    SDL_GPUColorTargetInfo target{};
    target.texture = g.backBuffer;
    target.clear_color = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(g.commands, &target, 1, nullptr);
    SDL_EndGPURenderPass(pass);
}

int GraphicsDevice::BackBufferWidth() const {
    return static_cast<int>(impl_->width);
}

int GraphicsDevice::BackBufferHeight() const {
    return static_cast<int>(impl_->height);
}

namespace detail {

bool GraphicsAccess::ResizeBackBuffer(GraphicsDevice& graphics, unsigned width, unsigned height) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    if (g.backBuffer && g.width == width && g.height == height) return true;
    if (g.backBuffer) SDL_ReleaseGPUTexture(g.device, g.backBuffer);

    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = BackBufferFormat;
    // SAMPLER because a blit reads its source by sampling.
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    g.backBuffer = SDL_CreateGPUTexture(g.device, &info);
    if (!g.backBuffer) {
        SDL_Log("coldx: could not create a %ux%u back buffer: %s", width, height, SDL_GetError());
        g.width = g.height = 0;
        return false;
    }
    g.width = width;
    g.height = height;
    return true;
}

std::unique_ptr<GraphicsDevice> GraphicsAccess::Create() {
    return std::unique_ptr<GraphicsDevice>(new GraphicsDevice());
}

bool GraphicsAccess::Open(GraphicsDevice& graphics, SDL_Window* window, bool vsync, int width, int height) {
    GraphicsDevice::Impl& g = *graphics.impl_;

#ifdef NDEBUG
    const bool debugMode = false;
#else
    const bool debugMode = true;
#endif
    // Every format a backend might ask for. Shaders ship precompiled to all three.
    const SDL_GPUShaderFormat formats =
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

    if (!g.device) {
        g.device = SDL_CreateGPUDevice(formats, debugMode, nullptr);
        if (!g.device) {
            SDL_Log("coldx: SDL_CreateGPUDevice failed: %s", SDL_GetError());
            return false;
        }
        g.token = std::make_shared<DeviceToken>(DeviceToken{g.device});
        SDL_Log("coldx: GPU driver %s", SDL_GetGPUDeviceDriver(g.device));
    }

    if (!window) {
        if (!ResizeBackBuffer(graphics, static_cast<unsigned>(width), static_cast<unsigned>(height))) {
            Close(graphics);
            return false;
        }
        return true;
    }

    if (!SDL_ClaimWindowForGPUDevice(g.device, window)) {
        SDL_Log("coldx: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        Close(graphics);
        return false;
    }
    g.window = window;

    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!vsync) {
        if (SDL_WindowSupportsGPUPresentMode(g.device, window, SDL_GPU_PRESENTMODE_IMMEDIATE))
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        else if (SDL_WindowSupportsGPUPresentMode(g.device, window, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    if (!SDL_SetGPUSwapchainParameters(g.device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
        SDL_Log("coldx: SDL_SetGPUSwapchainParameters failed, keeping defaults: %s", SDL_GetError());

    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
    if (!ResizeBackBuffer(graphics, static_cast<unsigned>(pixelWidth), static_cast<unsigned>(pixelHeight))) {
        Close(graphics);
        return false;
    }
    return true;
}

void GraphicsAccess::ReleaseWindow(GraphicsDevice& graphics) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    if (!g.device || !g.window) return;
    SDL_WaitForGPUIdle(g.device);
    SDL_ReleaseWindowFromGPUDevice(g.device, g.window);
    g.window = nullptr;
}

void GraphicsAccess::Close(GraphicsDevice& graphics) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    if (!g.device) return;
    ReleaseWindow(graphics);
    SDL_WaitForGPUIdle(g.device);
    if (g.backBuffer) SDL_ReleaseGPUTexture(g.device, g.backBuffer);
    g.token->device = nullptr;
    SDL_DestroyGPUDevice(g.device);
    g = {};
}

bool GraphicsAccess::BeginFrame(GraphicsDevice& graphics) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    g.commands = SDL_AcquireGPUCommandBuffer(g.device);
    if (!g.commands) {
        SDL_Log("coldx: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    if (g.window) {
        Uint32 w = 0;
        Uint32 h = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(g.commands, g.window, &g.swapchain, &w, &h)) {
            SDL_Log("coldx: SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
            g.swapchain = nullptr;
        }
        // No swapchain texture means nothing would be shown, so skip drawing.
        if (!g.swapchain) return false;
        if (!ResizeBackBuffer(graphics, w, h)) return false;
    }

    g.inFrame = true;
    return true;
}

void GraphicsAccess::EndFrame(GraphicsDevice& graphics) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    if (g.inFrame && g.swapchain) {
        SDL_GPUBlitInfo blit{};
        blit.source.texture = g.backBuffer;
        blit.source.w = g.width;
        blit.source.h = g.height;
        blit.destination.texture = g.swapchain;
        blit.destination.w = g.width;
        blit.destination.h = g.height;
        blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(g.commands, &blit);
    }
    if (g.commands && !SDL_SubmitGPUCommandBuffer(g.commands))
        SDL_Log("coldx: SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    g.commands = nullptr;
    g.swapchain = nullptr;
    g.inFrame = false;
}

bool GraphicsAccess::SaveBackBuffer(GraphicsDevice& graphics, const std::string& path) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    if (!g.backBuffer) return false;

    const Uint32 size = g.width * g.height * 4;
    SDL_GPUTransferBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    bufferInfo.size = size;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(g.device, &bufferInfo);
    if (!buffer) {
        SDL_Log("coldx: capture: SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(g.device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion source{};
    source.texture = g.backBuffer;
    source.w = g.width;
    source.h = g.height;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = buffer;
    destination.pixels_per_row = g.width;
    destination.rows_per_layer = g.height;
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    bool saved = false;
    if (fence) {
        SDL_WaitForGPUFences(g.device, true, &fence, 1);
        SDL_ReleaseGPUFence(g.device, fence);

        void* pixels = SDL_MapGPUTransferBuffer(g.device, buffer, false);
        SDL_Surface* surface = SDL_CreateSurfaceFrom(static_cast<int>(g.width), static_cast<int>(g.height),
                                                     SDL_PIXELFORMAT_RGBA32, pixels, static_cast<int>(g.width * 4));
        saved = surface && SDL_SavePNG(surface, path.c_str());
        if (!saved) SDL_Log("coldx: capture: could not write %s: %s", path.c_str(), SDL_GetError());
        SDL_DestroySurface(surface);
        SDL_UnmapGPUTransferBuffer(g.device, buffer);
    } else {
        SDL_Log("coldx: capture: submit failed: %s", SDL_GetError());
    }
    SDL_ReleaseGPUTransferBuffer(g.device, buffer);
    return saved;
}

SDL_GPUDevice* GraphicsAccess::Device(GraphicsDevice& graphics) {
    return graphics.impl_->device;
}

std::shared_ptr<DeviceToken> GraphicsAccess::Token(GraphicsDevice& graphics) {
    return graphics.impl_->token;
}

SDL_GPUCommandBuffer* GraphicsAccess::Commands(GraphicsDevice& graphics) {
    return graphics.impl_->inFrame ? graphics.impl_->commands : nullptr;
}

SDL_GPUTexture* GraphicsAccess::BackBuffer(GraphicsDevice& graphics) {
    return graphics.impl_->inFrame ? graphics.impl_->backBuffer : nullptr;
}

} // namespace detail
} // namespace coldx
