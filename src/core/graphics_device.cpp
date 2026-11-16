#include <coldx/core/graphics_device.h>
#include <coldx/core/render_target2d.h>

#include <SDL3/SDL.h>

#include "graphics_access.h"
#include "png_file.h"
#include "render_backend.h"
#include "texture_access.h"

namespace coldx {

struct GraphicsDevice::Impl {
    std::unique_ptr<detail::RenderBackend> backend;
    bool pixelArt = false;
};

GraphicsDevice::GraphicsDevice() : impl_(std::make_unique<Impl>()) {
    impl_->backend = detail::CreateGpuBackend();
}

GraphicsDevice::~GraphicsDevice() {
    detail::GraphicsAccess::Close(*this);
}

void GraphicsDevice::Clear(Color color) {
    if (impl_->backend->InFrame()) impl_->backend->Clear(color);
}

void GraphicsDevice::SetRenderTarget(const RenderTarget2D* target) {
    detail::RenderBackend& b = *impl_->backend;
    if (!b.InFrame()) {
        SDL_Log("coldx.core: SetRenderTarget called outside Draw, ignored");
        return;
    }
    const detail::BackendTexture* handle = target ? detail::TextureAccess::Handle(*target) : nullptr;
    if (handle) b.SetTarget(handle, target->Width(), target->Height());
    else b.SetTarget(nullptr, 0, 0);
}

int GraphicsDevice::BackBufferWidth() const {
    return impl_->backend->BackBufferWidth();
}

int GraphicsDevice::BackBufferHeight() const {
    return impl_->backend->BackBufferHeight();
}

bool GraphicsDevice::PixelArt() const {
    return impl_->pixelArt;
}

Rectangle GraphicsDevice::PresentedArea() const {
    return impl_->backend->PresentedArea();
}

bool GraphicsDevice::GetBackBufferData(Color* pixels) const {
    return impl_->backend->ReadBackBuffer(pixels);
}

namespace detail {

std::unique_ptr<GraphicsDevice> GraphicsAccess::Create() {
    return std::unique_ptr<GraphicsDevice>(new GraphicsDevice());
}

bool GraphicsAccess::Open(GraphicsDevice& graphics, SDL_Window* window, bool vsync, int width, int height) {
    if (graphics.impl_->backend->Open(window, vsync, width, height)) return true;
    Close(graphics);
    return false;
}

void GraphicsAccess::SetVSync(GraphicsDevice& graphics, bool vsync) {
    graphics.impl_->backend->SetVSync(vsync);
}

void GraphicsAccess::ResizeHeadless(GraphicsDevice& graphics, int width, int height) {
    graphics.impl_->backend->ResizeHeadless(width, height);
}

void GraphicsAccess::SetPixelArt(GraphicsDevice& graphics, bool pixelArt) {
    graphics.impl_->pixelArt = pixelArt;
}

void GraphicsAccess::SetPresentation(GraphicsDevice& graphics, const Presentation& presentation) {
    graphics.impl_->backend->SetPresentation(presentation);
}

void GraphicsAccess::ReleaseWindow(GraphicsDevice& graphics) {
    graphics.impl_->backend->ReleaseWindow();
}

void GraphicsAccess::Close(GraphicsDevice& graphics) {
    if (!graphics.impl_->backend->HasDevice()) return;
    graphics.impl_->backend->Close();
    graphics.impl_->pixelArt = false;
}

bool GraphicsAccess::BeginFrame(GraphicsDevice& graphics) {
    return graphics.impl_->backend->BeginFrame();
}

void GraphicsAccess::EndFrame(GraphicsDevice& graphics) {
    graphics.impl_->backend->EndFrame();
}

bool GraphicsAccess::SaveBackBuffer(GraphicsDevice& graphics, const std::string& path) {
    const int w = graphics.BackBufferWidth();
    const int h = graphics.BackBufferHeight();
    if (w <= 0 || h <= 0) return false;
    // As drawn, premultiplied: tests compare these bytes with what the GPU should produce.
    std::unique_ptr<Color[]> pixels(new Color[static_cast<size_t>(w) * static_cast<size_t>(h)]);
    return graphics.GetBackBufferData(pixels.get()) && WritePng(path.c_str(), w, h, pixels.get(), false);
}

RenderBackend& GraphicsAccess::Backend(GraphicsDevice& graphics) {
    return *graphics.impl_->backend;
}

} // namespace detail
} // namespace coldx
