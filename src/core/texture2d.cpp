#include <coldx/core/render_target2d.h>
#include <coldx/core/texture2d.h>

#include <SDL3/SDL.h>

#include <coldx/core/rime.h>

#include <algorithm>

#include "content_path.h"
#include "rime_bytes.h"
#include "graphics_access.h"
#include "png_file.h"
#include "render_backend.h"
#include "texture_access.h"

namespace coldx {

// Color arrays are uploaded as R8G8B8A8 without conversion.
static_assert(sizeof(Color) == 4);

struct Texture2D::Impl {
    std::unique_ptr<detail::BackendTexture> texture;
    int width = 0;
    int height = 0;
};

Texture2D::Texture2D() = default;
Texture2D::~Texture2D() = default;
Texture2D::Texture2D(const Texture2D&) = default;
Texture2D& Texture2D::operator=(const Texture2D&) = default;
Texture2D::Texture2D(Texture2D&&) noexcept = default;
Texture2D& Texture2D::operator=(Texture2D&&) noexcept = default;

std::shared_ptr<Texture2D::Impl> Texture2D::Make(GraphicsDevice& graphics, int width, int height, bool renderTarget) {
    detail::RenderBackend& backend = detail::GraphicsAccess::Backend(graphics);
    if (!backend.HasDevice()) {
        SDL_Log("coldx.core: texture created before the graphics device exists. Create it in LoadContent");
        return nullptr;
    }
    if (width <= 0 || height <= 0) {
        SDL_Log("coldx.core: invalid texture size %dx%d", width, height);
        return nullptr;
    }
    std::unique_ptr<detail::BackendTexture> texture = backend.CreateTexture(width, height, renderTarget);
    if (!texture) return nullptr;

    auto impl = std::make_shared<Impl>();
    impl->texture = std::move(texture);
    impl->width = width;
    impl->height = height;
    return impl;
}

Texture2D Texture2D::FromPixels(GraphicsDevice& graphics, int width, int height, const Color* pixels) {
    Texture2D result;
    if (!pixels) {
        SDL_Log("coldx.core: Texture2D::FromPixels: no pixels");
        return result;
    }
    result.impl_ = Make(graphics, width, height, false);
    if (result.impl_ && !result.impl_->texture->Upload(pixels)) result.impl_.reset();
    return result;
}

RenderTarget2D RenderTarget2D::Create(GraphicsDevice& graphics, int width, int height) {
    RenderTarget2D result;
    result.impl_ = Make(graphics, width, height, true);
    return result;
}

// Takes ownership of loaded, converts it to premultiplied RGBA and uploads it.
static Texture2D FromSurface(GraphicsDevice& graphics, SDL_Surface* loaded, const std::string& name) {
    if (!loaded) {
        SDL_Log("coldx.core: could not load %s: %s", name.c_str(), SDL_GetError());
        return {};
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!rgba) {
        SDL_Log("coldx.core: could not convert %s to RGBA: %s", name.c_str(), SDL_GetError());
        return {};
    }
    if (!SDL_PremultiplySurfaceAlpha(rgba, false)) {
        SDL_Log("coldx.core: could not premultiply %s: %s", name.c_str(), SDL_GetError());
        SDL_DestroySurface(rgba);
        return {};
    }

    // Rows can be padded. FromPixels wants them packed.
    std::unique_ptr<Color[]> pixels(new Color[static_cast<size_t>(rgba->w) * rgba->h]);
    for (int y = 0; y < rgba->h; ++y) {
        SDL_memcpy(&pixels[static_cast<size_t>(y) * rgba->w],
                   static_cast<const Uint8*>(rgba->pixels) + static_cast<size_t>(y) * rgba->pitch,
                   static_cast<size_t>(rgba->w) * 4);
    }
    Texture2D result = Texture2D::FromPixels(graphics, rgba->w, rgba->h, pixels.get());
    SDL_DestroySurface(rgba);
    return result;
}

Texture2D Texture2D::FromFile(GraphicsDevice& graphics, const std::string& path) {
    const std::string full = detail::ResolveContentPath(path);
    return FromSurface(graphics, SDL_LoadPNG(full.c_str()), full);
}

Texture2D Texture2D::FromMemory(GraphicsDevice& graphics, const void* png, std::size_t size, const std::string& name) {
    const auto* bytes = static_cast<const std::uint8_t*>(png);
    if (bytes && size >= rime::RawTextureHeader && std::equal(std::begin(rime::RawTextureTag), std::end(rime::RawTextureTag), bytes)) {
        const std::uint32_t w = detail::Get32(bytes + 4), h = detail::Get32(bytes + 8);
        if (w == 0 || h == 0 || w > 16384 || h > 16384 || size - rime::RawTextureHeader != std::size_t{w} * h * 4) {
            SDL_Log("coldx.core: %s: a stored texture with the wrong size", name.c_str());
            return {};
        }
        // Pixels are not aligned for Color here. Copy them out.
        std::vector<Color> pixels(std::size_t{w} * h);
        SDL_memcpy(pixels.data(), bytes + rime::RawTextureHeader, pixels.size() * 4);
        return FromPixels(graphics, static_cast<int>(w), static_cast<int>(h), pixels.data());
    }
    SDL_IOStream* io = png ? SDL_IOFromConstMem(png, size) : nullptr;
    return FromSurface(graphics, io ? SDL_LoadPNG_IO(io, true) : nullptr, name);
}

void Texture2D::SetData(const Color* pixels) {
    if (!impl_ || !pixels) return;
    impl_->texture->Upload(pixels);
}

bool Texture2D::GetData(Color* pixels) const {
    return GetData(Rectangle(0, 0, Width(), Height()), pixels);
}

bool Texture2D::GetData(const Rectangle& region, Color* pixels) const {
    if (!impl_ || !pixels) return false;
    if (region.IsEmpty() || region.x < 0 || region.y < 0 || region.Right() > impl_->width || region.Bottom() > impl_->height) {
        SDL_Log("coldx.core: Texture2D::GetData: region %d,%d %dx%d is not inside the %dx%d texture", region.x, region.y,
                region.width, region.height, impl_->width, impl_->height);
        return false;
    }
    return impl_->texture->Download(region, pixels);
}

bool Texture2D::SaveAsPng(const std::string& path) const {
    if (!impl_) return false;
    std::unique_ptr<Color[]> pixels(new Color[static_cast<size_t>(impl_->width) * impl_->height]);
    return GetData(pixels.get()) && detail::WritePng(path.c_str(), impl_->width, impl_->height, pixels.get(), true);
}

int Texture2D::Width() const {
    return impl_ ? impl_->width : 0;
}

int Texture2D::Height() const {
    return impl_ ? impl_->height : 0;
}

bool Texture2D::IsEmpty() const {
    return !impl_;
}

namespace detail {

const BackendTexture* TextureAccess::Handle(const Texture2D& texture) {
    return texture.impl_ ? texture.impl_->texture.get() : nullptr;
}

} // namespace detail
} // namespace coldx
