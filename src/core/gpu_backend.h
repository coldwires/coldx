#pragma once

#include <array>
#include <memory>
#include <string>

#include "render_backend.h"

struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUFence;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUShader;
struct SDL_GPUTexture;

// The SDL_GPU backend. Only gpu_*.cpp include this.
namespace coldx::detail {

// Shared by the device and every GPU resource made from it. The device clears it when it
// is destroyed, so a resource that outlives the device skips releasing instead of using a
// dead pointer.
struct DeviceToken {
    SDL_GPUDevice* device = nullptr;
};

inline constexpr int BlendStateCount = 4;

class GpuTexture final : public BackendTexture {
public:
    GpuTexture(std::shared_ptr<DeviceToken> token, SDL_GPUTexture* texture, int width, int height);
    ~GpuTexture() override;
    bool Upload(const Color* pixels) override;
    bool Download(const Rectangle& region, Color* out) const override;
    SDL_GPUTexture* Handle() const { return texture_; }

private:
    std::shared_ptr<DeviceToken> token_;
    SDL_GPUTexture* texture_;
    int width_;
    int height_;
};

class GpuBackend;

class GpuEffect final : public BackendEffect {
public:
    GpuEffect(const effect_format::Compiled& compiled, const std::string& name) : compiled_(compiled), name_(name) {}
    ~GpuEffect() override { Release(); }
    // Shaders for the device backend has now. Remade after a second Run makes a new one.
    bool Shaders(GpuBackend& backend);
    // The pipeline for sprites drawn in blend, made on first use. Null on failure, logged
    // once.
    SDL_GPUGraphicsPipeline* Pipeline(GpuBackend& backend, BlendState blend);

private:
    void Release();
    SDL_GPUDevice* Device() const { return token_ ? token_->device : nullptr; }

    const effect_format::Compiled& compiled_;
    const std::string& name_;
    std::shared_ptr<DeviceToken> token_;
    SDL_GPUShader* vertex_ = nullptr;
    SDL_GPUShader* fragment_ = nullptr;
    SDL_GPUGraphicsPipeline* pipelines_[BlendStateCount] = {};
    bool failed_ = false;
};

// Drawing goes into backBuffer, which is copied to the window's swapchain texture at the end
// of the frame. The swapchain texture cannot be read back and does not exist without a visible
// window, so this is what lets a frame be captured and lets headless mode draw at all.
class GpuBackend final : public RenderBackend {
public:
    ~GpuBackend() override { Close(); }

    bool Open(SDL_Window* window, bool vsync, int width, int height) override;
    void ReleaseWindow() override;
    void Close() override;
    bool HasDevice() const override { return device_ != nullptr; }
    void SetVSync(bool vsync) override;
    void ResizeHeadless(int width, int height) override;
    void SetPresentation(const Presentation& presentation) override;
    bool BeginFrame() override;
    void EndFrame() override;
    bool InFrame() const override { return inFrame_; }
    void Clear(Color color) override;
    void SetTarget(const BackendTexture* target, int width, int height) override;
    int BackBufferWidth() const override { return static_cast<int>(width_); }
    int BackBufferHeight() const override { return static_cast<int>(height_); }
    Rectangle PresentedArea() const override { return presented_; }
    bool ReadBackBuffer(Color* pixels) const override;
    std::unique_ptr<BackendTexture> CreateTexture(int width, int height, bool renderTarget) override;
    std::unique_ptr<BackendEffect> CreateEffect(const effect_format::Compiled& compiled, const std::string& name) override;
    std::unique_ptr<SpriteRenderer> CreateSpriteRenderer() override;

    // Null before Open and after Close.
    SDL_GPUDevice* Device() const { return device_; }
    const std::shared_ptr<DeviceToken>& Token() const { return token_; }
    // All null or zero outside a frame. The current target is the back buffer unless the
    // game set a render target.
    SDL_GPUCommandBuffer* Commands() const { return inFrame_ ? commands_ : nullptr; }
    SDL_GPUTexture* CurrentTarget() const { return inFrame_ ? target_ : nullptr; }
    int CurrentTargetWidth() const { return static_cast<int>(targetWidth_); }
    int CurrentTargetHeight() const { return static_cast<int>(targetHeight_); }

private:
    bool ResizeBackBuffer(unsigned width, unsigned height);
    // Waits for, and releases, the oldest frame in flight.
    void WaitOldestFrame();

    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUCommandBuffer* commands_ = nullptr;
    SDL_GPUTexture* swapchain_ = nullptr;
    SDL_GPUTexture* backBuffer_ = nullptr;
    unsigned width_ = 0;
    unsigned height_ = 0;
    Presentation presentation_;
    Rectangle presented_;  // where the back buffer went in the swapchain, last frame
    bool inFrame_ = false;
    SDL_GPUTexture* target_ = nullptr;
    unsigned targetWidth_ = 0;
    unsigned targetHeight_ = 0;
    std::shared_ptr<DeviceToken> token_;
    // With no window there is no swapchain to make the CPU wait for the GPU, so frames
    // submitted faster than the GPU finishes them would queue without end (a software
    // driver, headless, at thousands of frames a second). The fences of the frames in
    // flight, oldest first. BeginFrame waits for the oldest once there are this many.
    static constexpr int MaxFramesInFlight = 2;
    std::array<SDL_GPUFence*, MaxFramesInFlight> inFlight_{};
    int inFlightCount_ = 0;
};

// A pipeline drawing SpriteVertex quads into the back buffer's format with blend. The
// built-in shaders and every Effect share it. Null on failure, logged.
SDL_GPUGraphicsPipeline* CreateSpritePipeline(SDL_GPUDevice* device, SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                                              BlendState blend);

std::unique_ptr<SpriteRenderer> CreateGpuSpriteRenderer(GpuBackend& backend);

} // namespace coldx::detail
