#include <coldx/core/graphics_device.h>
#include <coldx/core/render_target2d.h>

#include <SDL3/SDL.h>

#include "gpu_download.h"
#include "graphics_access.h"
#include "texture_access.h"

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
    detail::Presentation presentation;
    bool pixelArt = false;
    Rectangle presented;  // where the back buffer went in the swapchain, last frame
    bool inFrame = false;
    SDL_GPUTexture* target = nullptr;
    Uint32 targetWidth = 0;
    Uint32 targetHeight = 0;
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
    target.texture = g.target;
    target.clear_color = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(g.commands, &target, 1, nullptr);
    SDL_EndGPURenderPass(pass);
}

void GraphicsDevice::SetRenderTarget(const RenderTarget2D* target) {
    Impl& g = *impl_;
    if (!g.inFrame) {
        SDL_Log("coldx.core: SetRenderTarget called outside Draw; ignored");
        return;
    }
    SDL_GPUTexture* handle = target ? detail::TextureAccess::Handle(*target) : nullptr;
    if (handle) {
        g.target = handle;
        g.targetWidth = static_cast<Uint32>(target->Width());
        g.targetHeight = static_cast<Uint32>(target->Height());
    } else {
        g.target = g.backBuffer;
        g.targetWidth = g.width;
        g.targetHeight = g.height;
    }
}

int GraphicsDevice::BackBufferWidth() const {
    return static_cast<int>(impl_->width);
}

int GraphicsDevice::BackBufferHeight() const {
    return static_cast<int>(impl_->height);
}

bool GraphicsDevice::PixelArt() const {
    return impl_->pixelArt;
}

Rectangle GraphicsDevice::PresentedArea() const {
    return impl_->presented;
}

bool GraphicsDevice::GetBackBufferData(Color* pixels) const {
    const Impl& g = *impl_;
    return detail::DownloadTexture(g.device, g.backBuffer, Rectangle(0, 0, static_cast<int>(g.width), static_cast<int>(g.height)),
                                   pixels);
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
        SDL_Log("coldx.core: could not create a %ux%u back buffer: %s", width, height, SDL_GetError());
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
            SDL_Log("coldx.core: SDL_CreateGPUDevice failed: %s", SDL_GetError());
            return false;
        }
        g.token = std::make_shared<DeviceToken>(DeviceToken{g.device});
        SDL_Log("coldx.core: GPU driver %s", SDL_GetGPUDeviceDriver(g.device));
    }

    if (!window) {
        if (g.presentation.Active()) {
            width = g.presentation.width;
            height = g.presentation.height;
        }
        if (!ResizeBackBuffer(graphics, static_cast<unsigned>(width), static_cast<unsigned>(height))) {
            Close(graphics);
            return false;
        }
        return true;
    }

    if (!SDL_ClaimWindowForGPUDevice(g.device, window)) {
        SDL_Log("coldx.core: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        Close(graphics);
        return false;
    }
    g.window = window;

    SetVSync(graphics, vsync);

    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
    if (g.presentation.Active()) {
        pixelWidth = g.presentation.width;
        pixelHeight = g.presentation.height;
    }
    if (!ResizeBackBuffer(graphics, static_cast<unsigned>(pixelWidth), static_cast<unsigned>(pixelHeight))) {
        Close(graphics);
        return false;
    }
    return true;
}

void GraphicsAccess::SetVSync(GraphicsDevice& graphics, bool vsync) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    if (!g.device || !g.window) return;
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!vsync) {
        if (SDL_WindowSupportsGPUPresentMode(g.device, g.window, SDL_GPU_PRESENTMODE_IMMEDIATE))
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        else if (SDL_WindowSupportsGPUPresentMode(g.device, g.window, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    if (!SDL_SetGPUSwapchainParameters(g.device, g.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
        SDL_Log("coldx.core: SDL_SetGPUSwapchainParameters failed, keeping the old mode: %s", SDL_GetError());
}

void GraphicsAccess::ResizeHeadless(GraphicsDevice& graphics, int width, int height) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    if (g.device && !g.window && width > 0 && height > 0 && !g.presentation.Active())
        ResizeBackBuffer(graphics, static_cast<unsigned>(width), static_cast<unsigned>(height));
}

void GraphicsAccess::SetPixelArt(GraphicsDevice& graphics, bool pixelArt) {
    graphics.impl_->pixelArt = pixelArt;
}

void GraphicsAccess::SetPresentation(GraphicsDevice& graphics, const Presentation& presentation) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    g.presentation = presentation;
    if (g.device && !g.window && presentation.Active())
        ResizeBackBuffer(graphics, static_cast<unsigned>(presentation.width), static_cast<unsigned>(presentation.height));
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
        SDL_Log("coldx.core: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    if (g.window) {
        Uint32 w = 0;
        Uint32 h = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(g.commands, g.window, &g.swapchain, &w, &h)) {
            SDL_Log("coldx.core: SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
            g.swapchain = nullptr;
        }
        // No swapchain texture means nothing would be shown, so skip drawing.
        if (!g.swapchain) return false;
        const detail::Presentation& p = g.presentation;
        if (p.Active()) {
            if (!ResizeBackBuffer(graphics, static_cast<unsigned>(p.width), static_cast<unsigned>(p.height))) return false;
            g.presented = FitRectangle(static_cast<int>(w), static_cast<int>(h), p.width, p.height, p.mode);
        } else {
            if (!ResizeBackBuffer(graphics, w, h)) return false;
            g.presented = Rectangle(0, 0, static_cast<int>(w), static_cast<int>(h));
        }
    }

    g.inFrame = true;
    g.target = g.backBuffer;
    g.targetWidth = g.width;
    g.targetHeight = g.height;
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
        blit.destination.x = static_cast<Uint32>(g.presented.x);
        blit.destination.y = static_cast<Uint32>(g.presented.y);
        blit.destination.w = static_cast<Uint32>(g.presented.width);
        blit.destination.h = static_cast<Uint32>(g.presented.height);
        const detail::Presentation& p = g.presentation;
        // The bars around a scaled image are cleared; a 1:1 image covers the whole window.
        blit.load_op = p.Active() ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_DONT_CARE;
        blit.clear_color = {p.letterbox.r / 255.0f, p.letterbox.g / 255.0f, p.letterbox.b / 255.0f, 1.0f};
        blit.filter = p.Active() && p.smooth ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(g.commands, &blit);
    }
    if (g.commands && !SDL_SubmitGPUCommandBuffer(g.commands))
        SDL_Log("coldx.core: SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    g.commands = nullptr;
    g.swapchain = nullptr;
    g.inFrame = false;
    g.target = nullptr;
    g.targetWidth = g.targetHeight = 0;
}

bool GraphicsAccess::SaveBackBuffer(GraphicsDevice& graphics, const std::string& path) {
    GraphicsDevice::Impl& g = *graphics.impl_;
    if (!g.backBuffer) return false;
    // As drawn, premultiplied: tests compare these bytes with what the GPU should produce.
    std::unique_ptr<Color[]> pixels(new Color[static_cast<size_t>(g.width) * g.height]);
    return graphics.GetBackBufferData(pixels.get()) &&
           WritePng(path.c_str(), static_cast<int>(g.width), static_cast<int>(g.height), pixels.get(), false);
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

SDL_GPUTexture* GraphicsAccess::CurrentTarget(GraphicsDevice& graphics) {
    return graphics.impl_->inFrame ? graphics.impl_->target : nullptr;
}

int GraphicsAccess::CurrentTargetWidth(GraphicsDevice& graphics) {
    return static_cast<int>(graphics.impl_->targetWidth);
}

int GraphicsAccess::CurrentTargetHeight(GraphicsDevice& graphics) {
    return static_cast<int>(graphics.impl_->targetHeight);
}

} // namespace detail
} // namespace coldx
