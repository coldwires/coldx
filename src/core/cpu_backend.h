#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "render_backend.h"

struct SDL_Surface;

// The CPU backend: every pixel computed on the CPU, to the same rules the GPU path follows,
// so the two produce the same image. Only cpu_*.cpp include this.
namespace coldx::detail {

// Pixels in memory, RGBA8 premultiplied, rows top to bottom.
class CpuTexture final : public BackendTexture {
public:
    CpuTexture(int width, int height) : width_(width), height_(height), pixels_(static_cast<size_t>(width) * height) {}
    bool Upload(const Color* pixels) override;
    bool Download(const Rectangle& region, Color* out) const override;

    int Width() const { return width_; }
    int Height() const { return height_; }
    const Color* Pixels() const { return pixels_.data(); }

    // Called before every write. The first write in a frame keeps a copy of what was there,
    // so a Download inside the same frame sees what earlier frames drew, as on the GPU.
    Color* Write(std::uint64_t frame);
    // Frame numbers start at 1. 0 means outside a frame.
    void SetFrame(const std::uint64_t* frame) { currentFrame_ = frame; }

    // One byte per pixel, set where the pixel's value is near a rounding tie or was made
    // from one that is. Null unless the backend tracks ties (COLDX_CPU_TIES).
    void TrackTies() { ties_.assign(pixels_.size(), 0); }
    std::uint8_t* Ties() { return ties_.empty() ? nullptr : ties_.data(); }
    const std::uint8_t* Ties() const { return ties_.empty() ? nullptr : ties_.data(); }

private:
    int width_;
    int height_;
    std::vector<Color> pixels_;
    std::vector<std::uint8_t> ties_;
    std::vector<Color> previous_;  // as of the start of writtenIn_, valid during that frame
    std::uint64_t writtenIn_ = 0;
    const std::uint64_t* currentFrame_ = nullptr;
};

class CpuEffect final : public BackendEffect {
public:
    CpuEffect(const effect_format::Compiled& compiled, const std::string& name) : compiled_(compiled), name_(name) {}
    const effect_format::Compiled& Compiled() const { return compiled_; }
    const std::string& Name() const { return name_; }
    bool warned = false;

private:
    const effect_format::Compiled& compiled_;
    const std::string& name_;
};

class CpuBackend final : public RenderBackend {
public:
    ~CpuBackend() override { Close(); }

    bool Open(SDL_Window* window, bool vsync, int width, int height) override;
    void ReleaseWindow() override;
    void Close() override;
    bool HasDevice() const override { return open_; }
    void SetVSync(bool vsync) override { vsync_ = vsync; }
    void ResizeHeadless(int width, int height) override;
    void SetPresentation(const Presentation& presentation) override;
    bool BeginFrame() override;
    void EndFrame() override;
    bool InFrame() const override { return frame_ != 0; }
    void Clear(Color color) override;
    void SetTarget(const BackendTexture* target, int width, int height) override;
    int BackBufferWidth() const override { return backBuffer_ ? backBuffer_->Width() : 0; }
    int BackBufferHeight() const override { return backBuffer_ ? backBuffer_->Height() : 0; }
    Rectangle PresentedArea() const override { return presented_; }
    bool ReadBackBuffer(Color* pixels) const override;
    std::unique_ptr<BackendTexture> CreateTexture(int width, int height, bool renderTarget) override;
    std::unique_ptr<BackendEffect> CreateEffect(const effect_format::Compiled& compiled, const std::string& name) override;
    std::unique_ptr<SpriteRenderer> CreateSpriteRenderer() override;

    // Null outside a frame.
    CpuTexture* CurrentTarget() const { return frame_ ? target_ : nullptr; }
    std::uint64_t Frame() const { return frame_; }

private:
    void ResizeBackBuffer(int width, int height);
    void Present();
    void Pace();
    void WriteTies() const;

    bool open_ = false;
    SDL_Window* window_ = nullptr;
    bool vsync_ = true;
    std::unique_ptr<CpuTexture> backBuffer_;
    CpuTexture* target_ = nullptr;
    Presentation presentation_;
    Rectangle presented_;
    std::uint64_t frame_ = 0;      // the frame being drawn, 0 outside one
    std::uint64_t lastFrame_ = 0;  // the last one begun
    std::uint64_t nextPresent_ = 0;
    std::vector<Color> scaled_;    // the window's pixels when the back buffer is scaled into it
    std::string tiesPath_;         // COLDX_CPU_TIES, empty when not tracking
};

std::unique_ptr<SpriteRenderer> CreateCpuSpriteRenderer(CpuBackend& backend);

// Vertex positions snap to 1/256 of a pixel before rasterizing, Direct3D's 8 bits of
// subpixel precision.
inline constexpr int SubpixelBits = 8;

// One texel fetch with the sampler's filter and address mode, from UV, as the GPU's
// sampler does. Channels 0 to 1, premultiplied.
struct Float4 {
    float r, g, b, a;
};
// tie, when not null, is set if a texel read is marked as a tie.
Float4 Sample(const CpuTexture& texture, SamplerState sampler, float u, float v, bool* tie);

// How close to halfway between two 8-bit steps, in steps, a value counts as a tie. GPUs
// blend at slightly less than float precision and can round such a value the other way.
// Intel UHD 620 was seen off by up to 0.006 of a step.
inline constexpr float CpuNearTie = 0.01f;

// Fills a width x height rectangle of a window-sized RGBA image from source, scaled, as
// the GPU's blit does: nearest, or linear when smooth.
void ScaleInto(const CpuTexture& source, Color* out, int outWidth, const Rectangle& placed, bool smooth);

} // namespace coldx::detail
