#include "cpu_backend.h"

#include <algorithm>
#include <cstring>

#include <SDL3/SDL.h>

namespace coldx::detail {

std::unique_ptr<RenderBackend> CreateCpuBackend() {
    return std::make_unique<CpuBackend>();
}

// ---- Textures

// The GPU runs an upload at once, ahead of the frame being recorded, so a Download later in
// the same frame sees it. Drawing into the texture earlier in that frame and then uploading
// over it is the one case left different: the GPU would draw over the upload.
bool CpuTexture::Upload(const Color* pixels) {
    const size_t bytes = pixels_.size() * sizeof(Color);
    std::memcpy(pixels_.data(), pixels, bytes);
    std::fill(ties_.begin(), ties_.end(), std::uint8_t{0});
    if (currentFrame_ && *currentFrame_ != 0 && writtenIn_ == *currentFrame_) std::memcpy(previous_.data(), pixels, bytes);
    return true;
}

bool CpuTexture::Download(const Rectangle& region, Color* out) const {
    if (region.IsEmpty() || !out) return false;
    const bool inFrame = currentFrame_ && *currentFrame_ != 0 && writtenIn_ == *currentFrame_;
    const Color* from = inFrame ? previous_.data() : pixels_.data();
    for (int y = 0; y < region.height; ++y)
        std::memcpy(out + static_cast<size_t>(y) * region.width,
                    from + static_cast<size_t>(region.y + y) * width_ + region.x, static_cast<size_t>(region.width) * sizeof(Color));
    return true;
}

Color* CpuTexture::Write(std::uint64_t frame) {
    if (frame != 0 && writtenIn_ != frame) {
        previous_ = pixels_;
        writtenIn_ = frame;
    }
    return pixels_.data();
}

std::unique_ptr<BackendTexture> CpuBackend::CreateTexture(int width, int height, bool) {
    // Zeroed, so a render target starts transparent as the GPU's is cleared to.
    auto texture = std::make_unique<CpuTexture>(width, height);
    texture->SetFrame(&frame_);
    if (!tiesPath_.empty()) texture->TrackTies();
    return texture;
}

std::unique_ptr<BackendEffect> CpuBackend::CreateEffect(const effect_format::Compiled& compiled, const std::string& name) {
    return std::make_unique<CpuEffect>(compiled, name);
}

std::unique_ptr<SpriteRenderer> CpuBackend::CreateSpriteRenderer() {
    return CreateCpuSpriteRenderer(*this);
}

// ---- The device

void CpuBackend::ResizeBackBuffer(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (backBuffer_ && backBuffer_->Width() == width && backBuffer_->Height() == height) return;
    backBuffer_ = std::make_unique<CpuTexture>(width, height);
    backBuffer_->SetFrame(&frame_);
    if (!tiesPath_.empty()) backBuffer_->TrackTies();
}

bool CpuBackend::Open(SDL_Window* window, bool vsync, int width, int height) {
    if (!open_) {
        open_ = true;
        SDL_Log("coldx.core: renderer cpu");
        if (const char* ties = SDL_getenv("COLDX_CPU_TIES")) tiesPath_ = ties;
    }
    vsync_ = vsync;
    if (window) {
        window_ = window;
        SDL_GetWindowSizeInPixels(window, &width, &height);
    }
    if (presentation_.Active()) {
        width = presentation_.width;
        height = presentation_.height;
    }
    ResizeBackBuffer(width, height);
    if (!backBuffer_) {
        SDL_Log("coldx.core: could not create a %dx%d back buffer", width, height);
        return false;
    }
    return true;
}

void CpuBackend::ReleaseWindow() {
    if (window_) SDL_DestroyWindowSurface(window_);
    window_ = nullptr;
}

void CpuBackend::Close() {
    if (!open_) return;
    ReleaseWindow();
    open_ = false;
    backBuffer_.reset();
    target_ = nullptr;
    presentation_ = {};
    presented_ = {};
    frame_ = 0;
    scaled_.clear();
    tiesPath_.clear();
    pool_.reset();
}

void CpuBackend::ResizeHeadless(int width, int height) {
    if (open_ && !window_ && !presentation_.Active()) ResizeBackBuffer(width, height);
}

void CpuBackend::SetPresentation(const Presentation& presentation) {
    presentation_ = presentation;
    if (open_ && !window_ && presentation.Active()) ResizeBackBuffer(presentation.width, presentation.height);
}

bool CpuBackend::BeginFrame() {
    if (window_) {
        // Nothing would be shown, as the GPU path has no swapchain texture then.
        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) return false;
        int w = 0;
        int h = 0;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        if (w <= 0 || h <= 0) return false;
        if (presentation_.Active()) {
            ResizeBackBuffer(presentation_.width, presentation_.height);
            presented_ = FitRectangle(w, h, presentation_.width, presentation_.height, presentation_.mode);
        } else {
            ResizeBackBuffer(w, h);
            presented_ = Rectangle(0, 0, w, h);
        }
    }
    if (!backBuffer_) return false;
    frame_ = ++lastFrame_;
    target_ = backBuffer_.get();
    return true;
}

void CpuBackend::EndFrame() {
    if (frame_ && window_) {
        Present();
        Pace();
    }
    if (frame_ && !tiesPath_.empty()) WriteTies();
    frame_ = 0;
    target_ = nullptr;
}

// Copies the back buffer into the window's surface, scaled into the presented rectangle
// with the bars filled, as the GPU path's blit.
void CpuBackend::Present() {
    SDL_Surface* surface = SDL_GetWindowSurface(window_);
    if (!surface) {
        SDL_Log("coldx.core: SDL_GetWindowSurface failed: %s", SDL_GetError());
        return;
    }
    const CpuTexture& back = *backBuffer_;
    const Color* image = back.Pixels();
    int w = back.Width();
    int h = back.Height();
    if (presentation_.Active() || surface->w != w || surface->h != h) {
        w = surface->w;
        h = surface->h;
        const Color bar(presentation_.letterbox.r, presentation_.letterbox.g, presentation_.letterbox.b, 255);
        scaled_.assign(static_cast<size_t>(w) * h, bar);
        ScaleInto(back, scaled_.data(), w, presented_, presentation_.Active() && presentation_.smooth);
        image = scaled_.data();
    }
    if (!SDL_ConvertPixels(w, h, SDL_PIXELFORMAT_RGBA32, image, w * 4, surface->format, surface->pixels, surface->pitch))
        SDL_Log("coldx.core: SDL_ConvertPixels failed: %s", SDL_GetError());
    if (!SDL_UpdateWindowSurface(window_)) SDL_Log("coldx.core: SDL_UpdateWindowSurface failed: %s", SDL_GetError());
}

// A window surface has no vertical blank to wait on. With vsync on, frames are spaced at
// the display's refresh rate instead.
void CpuBackend::Pace() {
    if (!vsync_) return;
    float hz = 60.0f;
    if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_)))
        if (mode->refresh_rate > 0.0f) hz = mode->refresh_rate;
    const auto period = static_cast<std::uint64_t>(1e9 / hz);
    const std::uint64_t now = SDL_GetTicksNS();
    if (nextPresent_ == 0 || now > nextPresent_ + period) nextPresent_ = now;
    else if (now < nextPresent_) SDL_DelayPrecise(nextPresent_ - now);
    nextPresent_ += period;
}

WorkerPool& CpuBackend::Pool() {
    if (!pool_) {
        int count = SDL_GetNumLogicalCPUCores();
        if (const char* wanted = SDL_getenv("COLDX_CPU_THREADS")) count = SDL_atoi(wanted);
        count = std::clamp(count, 1, 16);
        pool_ = std::make_unique<WorkerPool>(count);
        SDL_Log("coldx.core: cpu renderer on %d thread%s", pool_->Count(), pool_->Count() == 1 ? "" : "s");
    }
    return *pool_;
}

void CpuBackend::Clear(Color color) {
    const size_t count = static_cast<size_t>(target_->Width()) * target_->Height();
    Color* p = target_->Write(frame_);
    std::fill(p, p + count, color);
    if (std::uint8_t* ties = target_->Ties()) std::fill(ties, ties + count, std::uint8_t{0});
}

// The back buffer's tie pixels as "x y" lines, replacing what the last frame wrote.
void CpuBackend::WriteTies() const {
    SDL_IOStream* io = SDL_IOFromFile(tiesPath_.c_str(), "w");
    if (!io) {
        SDL_Log("coldx.core: could not write %s: %s", tiesPath_.c_str(), SDL_GetError());
        return;
    }
    const std::uint8_t* ties = backBuffer_->Ties();
    const int w = backBuffer_->Width();
    for (int i = 0; i < w * backBuffer_->Height(); ++i)
        if (ties[i]) SDL_IOprintf(io, "%d %d\n", i % w, i / w);
    SDL_CloseIO(io);
}

void CpuBackend::SetTarget(const BackendTexture* target, int, int) {
    // Targets are textures this backend made, and drawing into one is what they are for.
    target_ = target ? const_cast<CpuTexture*>(static_cast<const CpuTexture*>(target)) : backBuffer_.get();
}

bool CpuBackend::ReadBackBuffer(Color* pixels) const {
    if (!backBuffer_) return false;
    return backBuffer_->Download(Rectangle(0, 0, backBuffer_->Width(), backBuffer_->Height()), pixels);
}

} // namespace coldx::detail
