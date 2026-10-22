#include <coldx/core/render_target2d.h>
#include <coldx/core/texture2d.h>

#include <SDL3/SDL.h>

#include <coldx/core/rime.h>

#include <algorithm>

#include "content_path.h"
#include "rime_bytes.h"
#include "gpu_download.h"
#include "graphics_access.h"
#include "texture_access.h"

namespace coldx {

// Color arrays are uploaded as R8G8B8A8 without conversion.
static_assert(sizeof(Color) == 4);

struct Texture2D::Impl {
    std::shared_ptr<detail::DeviceToken> token;
    SDL_GPUTexture* texture = nullptr;
    int width = 0;
    int height = 0;

    ~Impl() {
        if (texture && token && token->device) SDL_ReleaseGPUTexture(token->device, texture);
    }
};

// Copies pixels into texture on a command buffer of its own. Submitted at once, so it runs
// before anything drawn later, including in the frame being recorded if this is called
// from Draw.
static bool Upload(SDL_GPUDevice* device, SDL_GPUTexture* texture, int width, int height, const Color* pixels) {
    const Uint32 size = static_cast<Uint32>(width) * static_cast<Uint32>(height) * 4;
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
    source.pixels_per_row = static_cast<Uint32>(width);
    source.rows_per_layer = static_cast<Uint32>(height);
    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = static_cast<Uint32>(width);
    destination.h = static_cast<Uint32>(height);
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
    if (!submitted) SDL_Log("coldx.core: texture upload: submit failed: %s", SDL_GetError());
    // Released now, freed by SDL once the copy has run.
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return submitted;
}

Texture2D::Texture2D() = default;
Texture2D::~Texture2D() = default;
Texture2D::Texture2D(const Texture2D&) = default;
Texture2D& Texture2D::operator=(const Texture2D&) = default;
Texture2D::Texture2D(Texture2D&&) noexcept = default;
Texture2D& Texture2D::operator=(Texture2D&&) noexcept = default;

std::shared_ptr<Texture2D::Impl> Texture2D::Make(GraphicsDevice& graphics, int width, int height, bool renderTarget) {
    SDL_GPUDevice* device = detail::GraphicsAccess::Device(graphics);
    if (!device) {
        SDL_Log("coldx.core: texture created before the graphics device exists; create it in LoadContent");
        return nullptr;
    }
    if (width <= 0 || height <= 0) {
        SDL_Log("coldx.core: invalid texture size %dx%d", width, height);
        return nullptr;
    }

    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | (renderTarget ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0);
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);
    if (!texture) {
        SDL_Log("coldx.core: could not create a %dx%d texture: %s", width, height, SDL_GetError());
        return nullptr;
    }

    auto impl = std::make_shared<Impl>();
    impl->token = detail::GraphicsAccess::Token(graphics);
    impl->texture = texture;
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
    if (result.impl_ && !Upload(result.impl_->token->device, result.impl_->texture, width, height, pixels))
        result.impl_.reset();
    return result;
}

RenderTarget2D RenderTarget2D::Create(GraphicsDevice& graphics, int width, int height) {
    RenderTarget2D result;
    result.impl_ = Make(graphics, width, height, true);
    if (!result.impl_) return result;

    // GPU memory starts undefined; clear once so an unused target draws as nothing.
    SDL_GPUDevice* device = result.impl_->token->device;
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("coldx.core: RenderTarget2D: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        result.impl_.reset();
        return result;
    }
    SDL_GPUColorTargetInfo target{};
    target.texture = result.impl_->texture;
    target.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_EndGPURenderPass(SDL_BeginGPURenderPass(commands, &target, 1, nullptr));
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        SDL_Log("coldx.core: RenderTarget2D: clear failed: %s", SDL_GetError());
        result.impl_.reset();
    }
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

    // Rows can be padded; FromPixels wants them packed.
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
        // Pixels are not aligned for Color here; copy them out.
        std::vector<Color> pixels(std::size_t{w} * h);
        SDL_memcpy(pixels.data(), bytes + rime::RawTextureHeader, pixels.size() * 4);
        return FromPixels(graphics, static_cast<int>(w), static_cast<int>(h), pixels.data());
    }
    SDL_IOStream* io = png ? SDL_IOFromConstMem(png, size) : nullptr;
    return FromSurface(graphics, io ? SDL_LoadPNG_IO(io, true) : nullptr, name);
}

void Texture2D::SetData(const Color* pixels) {
    if (!impl_ || !pixels || !impl_->token->device) return;
    Upload(impl_->token->device, impl_->texture, impl_->width, impl_->height, pixels);
}

bool Texture2D::GetData(Color* pixels) const {
    return GetData(Rectangle(0, 0, Width(), Height()), pixels);
}

bool Texture2D::GetData(const Rectangle& region, Color* pixels) const {
    if (!impl_ || !impl_->token->device || !pixels) return false;
    if (region.IsEmpty() || region.x < 0 || region.y < 0 || region.Right() > impl_->width || region.Bottom() > impl_->height) {
        SDL_Log("coldx.core: Texture2D::GetData: region %d,%d %dx%d is not inside the %dx%d texture", region.x, region.y,
                region.width, region.height, impl_->width, impl_->height);
        return false;
    }
    return detail::DownloadTexture(impl_->token->device, impl_->texture, region, pixels);
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

SDL_GPUTexture* TextureAccess::Handle(const Texture2D& texture) {
    return texture.impl_ ? texture.impl_->texture : nullptr;
}

} // namespace detail
} // namespace coldx
