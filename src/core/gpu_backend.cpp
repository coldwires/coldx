#include "gpu_backend.h"

#include <SDL3/SDL.h>

#include <coldx/core/effect_format.h>

#include "gpu_download.h"
#include "shader_blob.h"
#include "sprite_vert.h"

namespace coldx::detail {

static constexpr SDL_GPUTextureFormat BackBufferFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

std::unique_ptr<RenderBackend> CreateGpuBackend() {
    return std::make_unique<GpuBackend>();
}

// ---- Textures

GpuTexture::GpuTexture(std::shared_ptr<DeviceToken> token, SDL_GPUTexture* texture, int width, int height)
    : token_(std::move(token)), texture_(texture), width_(width), height_(height) {}

GpuTexture::~GpuTexture() {
    if (texture_ && token_ && token_->device) SDL_ReleaseGPUTexture(token_->device, texture_);
}

// Copies pixels on a command buffer of its own. Submitted at once, so it runs before
// anything drawn later, including in the frame being recorded if this is called from Draw.
bool GpuTexture::Upload(const Color* pixels) {
    SDL_GPUDevice* device = token_ ? token_->device : nullptr;
    if (!device) return false;
    const Uint32 size = static_cast<Uint32>(width_) * static_cast<Uint32>(height_) * 4;
    SDL_GPUTransferBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    bufferInfo.size = size;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &bufferInfo);
    if (!buffer) {
        SDL_Log("coldx.core: texture upload: SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }
    void* mapped = SDL_MapGPUTransferBuffer(device, buffer, false);
    if (!mapped) {
        SDL_Log("coldx.core: texture upload: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }
    SDL_memcpy(mapped, pixels, size);
    SDL_UnmapGPUTransferBuffer(device, buffer);

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("coldx.core: texture upload: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = buffer;
    source.pixels_per_row = static_cast<Uint32>(width_);
    source.rows_per_layer = static_cast<Uint32>(height_);
    SDL_GPUTextureRegion destination{};
    destination.texture = texture_;
    destination.w = static_cast<Uint32>(width_);
    destination.h = static_cast<Uint32>(height_);
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
    if (!submitted) SDL_Log("coldx.core: texture upload: submit failed: %s", SDL_GetError());
    // Released now, freed by SDL once the copy has run.
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return submitted;
}

bool GpuTexture::Download(const Rectangle& region, Color* out) const {
    SDL_GPUDevice* device = token_ ? token_->device : nullptr;
    if (!device) return false;
    return DownloadTexture(device, texture_, region, out);
}

std::unique_ptr<BackendTexture> GpuBackend::CreateTexture(int width, int height, bool renderTarget) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | (renderTarget ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0);
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device_, &info);
    if (!texture) {
        SDL_Log("coldx.core: could not create a %dx%d texture: %s", width, height, SDL_GetError());
        return nullptr;
    }
    auto result = std::make_unique<GpuTexture>(token_, texture, width, height);
    if (!renderTarget) return result;

    // GPU memory starts undefined. Clear once so an unused target draws as nothing.
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    if (!commands) {
        SDL_Log("coldx.core: RenderTarget2D: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return nullptr;
    }
    SDL_GPUColorTargetInfo target{};
    target.texture = texture;
    target.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_EndGPURenderPass(SDL_BeginGPURenderPass(commands, &target, 1, nullptr));
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        SDL_Log("coldx.core: RenderTarget2D: clear failed: %s", SDL_GetError());
        return nullptr;
    }
    return result;
}

// ---- Effects

void GpuEffect::Release() {
    if (SDL_GPUDevice* device = Device()) {
        if (vertex_) SDL_ReleaseGPUShader(device, vertex_);
        if (fragment_) SDL_ReleaseGPUShader(device, fragment_);
        for (auto* p : pipelines_)
            if (p) SDL_ReleaseGPUGraphicsPipeline(device, p);
    }
    vertex_ = fragment_ = nullptr;
    for (auto& p : pipelines_) p = nullptr;
}

bool GpuEffect::Shaders(GpuBackend& backend) {
    SDL_GPUDevice* device = backend.Device();
    if (!device) return false;
    if (Device() != device) {
        Release();
        token_ = backend.Token();
        failed_ = false;
    }
    if (failed_) return false;
    if (!vertex_) vertex_ = CreateShader(device, shaders::sprite_vert, ShaderStage::Vertex, "sprite.vert");
    if (!fragment_) {
        const ShaderBlob blob{compiled_.spirv.data(), compiled_.spirv.size(),
                              compiled_.dxil.data(),  compiled_.dxil.size(),
                              compiled_.msl.data(),   compiled_.msl.size(),
                              static_cast<unsigned>(compiled_.textures.size()), 0, 0,
                              compiled_.uniformSize ? 1u : 0u};
        fragment_ = CreateShader(device, blob, ShaderStage::Fragment, name_.c_str());
    }
    failed_ = !vertex_ || !fragment_;
    return !failed_;
}

SDL_GPUGraphicsPipeline* GpuEffect::Pipeline(GpuBackend& backend, BlendState blend) {
    if (!Shaders(backend)) return nullptr;
    SDL_GPUGraphicsPipeline*& pipeline = pipelines_[static_cast<int>(blend)];
    if (!pipeline) {
        pipeline = CreateSpritePipeline(Device(), vertex_, fragment_, blend);
        failed_ = !pipeline;
    }
    return pipeline;
}

std::unique_ptr<BackendEffect> GpuBackend::CreateEffect(const effect_format::Compiled& compiled, const std::string& name) {
    auto effect = std::make_unique<GpuEffect>(compiled, name);
    if (device_ && !effect->Shaders(*this)) return nullptr;
    return effect;
}

std::unique_ptr<SpriteRenderer> GpuBackend::CreateSpriteRenderer() {
    return CreateGpuSpriteRenderer(*this);
}

// ---- The device

void GpuBackend::WaitOldestFrame() {
    if (inFlightCount_ == 0) return;
    SDL_GPUFence* oldest = inFlight_[0];
    SDL_WaitForGPUFences(device_, true, &oldest, 1);
    SDL_ReleaseGPUFence(device_, oldest);
    for (int i = 1; i < inFlightCount_; ++i) inFlight_[static_cast<size_t>(i - 1)] = inFlight_[static_cast<size_t>(i)];
    inFlight_[static_cast<size_t>(--inFlightCount_)] = nullptr;
}

bool GpuBackend::ResizeBackBuffer(unsigned width, unsigned height) {
    if (backBuffer_ && width_ == width && height_ == height) return true;
    if (backBuffer_) SDL_ReleaseGPUTexture(device_, backBuffer_);

    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = BackBufferFormat;
    // SAMPLER because a blit reads its source by sampling.
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    backBuffer_ = SDL_CreateGPUTexture(device_, &info);
    if (!backBuffer_) {
        SDL_Log("coldx.core: could not create a %ux%u back buffer: %s", width, height, SDL_GetError());
        width_ = height_ = 0;
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

bool GpuBackend::Open(SDL_Window* window, bool vsync, int width, int height) {
#ifdef NDEBUG
    const bool debugMode = false;
#else
    const bool debugMode = true;
#endif
    // Every format a backend might ask for. Shaders ship precompiled to all three.
    const SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

    if (!device_) {
        device_ = SDL_CreateGPUDevice(formats, debugMode, nullptr);
        if (!device_) {
            SDL_Log("coldx.core: SDL_CreateGPUDevice failed: %s", SDL_GetError());
            return false;
        }
        token_ = std::make_shared<DeviceToken>(DeviceToken{device_});
        SDL_Log("coldx.core: GPU driver %s", SDL_GetGPUDeviceDriver(device_));
    }

    if (!window) {
        if (presentation_.Active()) {
            width = presentation_.width;
            height = presentation_.height;
        }
        return ResizeBackBuffer(static_cast<unsigned>(width), static_cast<unsigned>(height));
    }

    if (!SDL_ClaimWindowForGPUDevice(device_, window)) {
        SDL_Log("coldx.core: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return false;
    }
    window_ = window;

    SetVSync(vsync);

    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
    if (presentation_.Active()) {
        pixelWidth = presentation_.width;
        pixelHeight = presentation_.height;
    }
    return ResizeBackBuffer(static_cast<unsigned>(pixelWidth), static_cast<unsigned>(pixelHeight));
}

void GpuBackend::SetVSync(bool vsync) {
    if (!device_ || !window_) return;
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!vsync) {
        if (SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_IMMEDIATE))
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        else if (SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
        SDL_Log("coldx.core: SDL_SetGPUSwapchainParameters failed, keeping the old mode: %s", SDL_GetError());
}

void GpuBackend::ResizeHeadless(int width, int height) {
    if (device_ && !window_ && width > 0 && height > 0 && !presentation_.Active())
        ResizeBackBuffer(static_cast<unsigned>(width), static_cast<unsigned>(height));
}

void GpuBackend::SetPresentation(const Presentation& presentation) {
    presentation_ = presentation;
    if (device_ && !window_ && presentation.Active())
        ResizeBackBuffer(static_cast<unsigned>(presentation.width), static_cast<unsigned>(presentation.height));
}

void GpuBackend::ReleaseWindow() {
    if (!device_ || !window_) return;
    SDL_WaitForGPUIdle(device_);
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    window_ = nullptr;
}

void GpuBackend::Close() {
    if (!device_) return;
    ReleaseWindow();
    SDL_WaitForGPUIdle(device_);
    while (inFlightCount_ > 0) WaitOldestFrame();
    if (backBuffer_) SDL_ReleaseGPUTexture(device_, backBuffer_);
    token_->device = nullptr;
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
    commands_ = nullptr;
    swapchain_ = backBuffer_ = target_ = nullptr;
    width_ = height_ = targetWidth_ = targetHeight_ = 0;
    presentation_ = {};
    presented_ = {};
    inFrame_ = false;
    token_.reset();
}

bool GpuBackend::BeginFrame() {
    if (!window_ && inFlightCount_ == MaxFramesInFlight) WaitOldestFrame();
    commands_ = SDL_AcquireGPUCommandBuffer(device_);
    if (!commands_) {
        SDL_Log("coldx.core: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    if (window_) {
        Uint32 w = 0;
        Uint32 h = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands_, window_, &swapchain_, &w, &h)) {
            SDL_Log("coldx.core: SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
            swapchain_ = nullptr;
        }
        // No swapchain texture means nothing would be shown, so skip drawing.
        if (!swapchain_) return false;
        const Presentation& p = presentation_;
        if (p.Active()) {
            if (!ResizeBackBuffer(static_cast<unsigned>(p.width), static_cast<unsigned>(p.height))) return false;
            presented_ = FitRectangle(static_cast<int>(w), static_cast<int>(h), p.width, p.height, p.mode);
        } else {
            if (!ResizeBackBuffer(w, h)) return false;
            presented_ = Rectangle(0, 0, static_cast<int>(w), static_cast<int>(h));
        }
    }

    inFrame_ = true;
    target_ = backBuffer_;
    targetWidth_ = width_;
    targetHeight_ = height_;
    return true;
}

void GpuBackend::EndFrame() {
    if (inFrame_ && swapchain_) {
        SDL_GPUBlitInfo blit{};
        blit.source.texture = backBuffer_;
        blit.source.w = width_;
        blit.source.h = height_;
        blit.destination.texture = swapchain_;
        blit.destination.x = static_cast<Uint32>(presented_.x);
        blit.destination.y = static_cast<Uint32>(presented_.y);
        blit.destination.w = static_cast<Uint32>(presented_.width);
        blit.destination.h = static_cast<Uint32>(presented_.height);
        const Presentation& p = presentation_;
        // The bars around a scaled image are cleared. A 1:1 image covers the whole window.
        blit.load_op = p.Active() ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_DONT_CARE;
        blit.clear_color = {p.letterbox.r / 255.0f, p.letterbox.g / 255.0f, p.letterbox.b / 255.0f, 1.0f};
        blit.filter = p.Active() && p.smooth ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(commands_, &blit);
    }
    if (commands_ && !window_) {
        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands_);
        if (!fence) SDL_Log("coldx.core: SDL_SubmitGPUCommandBufferAndAcquireFence failed: %s", SDL_GetError());
        else inFlight_[static_cast<size_t>(inFlightCount_++)] = fence;
    } else if (commands_ && !SDL_SubmitGPUCommandBuffer(commands_)) {
        SDL_Log("coldx.core: SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    }
    commands_ = nullptr;
    swapchain_ = nullptr;
    inFrame_ = false;
    target_ = nullptr;
    targetWidth_ = targetHeight_ = 0;
}

void GpuBackend::Clear(Color color) {
    SDL_GPUColorTargetInfo target{};
    target.texture = target_;
    target.clear_color = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands_, &target, 1, nullptr);
    SDL_EndGPURenderPass(pass);
}

void GpuBackend::SetTarget(const BackendTexture* target, int width, int height) {
    if (target) {
        target_ = static_cast<const GpuTexture*>(target)->Handle();
        targetWidth_ = static_cast<unsigned>(width);
        targetHeight_ = static_cast<unsigned>(height);
    } else {
        target_ = backBuffer_;
        targetWidth_ = width_;
        targetHeight_ = height_;
    }
}

bool GpuBackend::ReadBackBuffer(Color* pixels) const {
    return DownloadTexture(device_, backBuffer_, Rectangle(0, 0, static_cast<int>(width_), static_cast<int>(height_)), pixels);
}

} // namespace coldx::detail
