#include <coldx/core/texture2d.h>

#include <SDL3/SDL.h>

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
        SDL_Log("coldx: texture upload: SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }
    void* mapped = SDL_MapGPUTransferBuffer(device, buffer, false);
    if (!mapped) {
        SDL_Log("coldx: texture upload: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }
    SDL_memcpy(mapped, pixels, size);
    SDL_UnmapGPUTransferBuffer(device, buffer);

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("coldx: texture upload: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
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
    if (!submitted) SDL_Log("coldx: texture upload: submit failed: %s", SDL_GetError());
    // Released now, freed by SDL once the copy has run.
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return submitted;
}

Texture2D::Texture2D() = default;
Texture2D::~Texture2D() = default;
Texture2D::Texture2D(Texture2D&& other) noexcept = default;
Texture2D& Texture2D::operator=(Texture2D&& other) noexcept = default;

Texture2D Texture2D::FromPixels(GraphicsDevice& graphics, int width, int height, const Color* pixels) {
    Texture2D result;
    SDL_GPUDevice* device = detail::GraphicsAccess::Device(graphics);
    if (!device) {
        SDL_Log("coldx: Texture2D created before the graphics device exists; create it in LoadContent");
        return result;
    }
    if (width <= 0 || height <= 0 || !pixels) {
        SDL_Log("coldx: Texture2D::FromPixels: invalid size %dx%d or no pixels", width, height);
        return result;
    }

    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);
    if (!texture) {
        SDL_Log("coldx: could not create a %dx%d texture: %s", width, height, SDL_GetError());
        return result;
    }

    result.impl_ = std::make_unique<Impl>();
    result.impl_->token = detail::GraphicsAccess::Token(graphics);
    result.impl_->texture = texture;
    result.impl_->width = width;
    result.impl_->height = height;
    if (!Upload(device, texture, width, height, pixels)) result.impl_.reset();
    return result;
}

Texture2D Texture2D::FromFile(GraphicsDevice& graphics, const std::string& path) {
    std::string full = path;
    const bool absolute = !path.empty() && (path[0] == '/' || path[0] == '\\' || (path.size() > 1 && path[1] == ':'));
    if (!absolute) {
        const char* base = SDL_GetBasePath();
        if (base) full = std::string(base) + path;
    }

    SDL_Surface* loaded = SDL_LoadPNG(full.c_str());
    if (!loaded) {
        SDL_Log("coldx: could not load %s: %s", full.c_str(), SDL_GetError());
        return {};
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!rgba) {
        SDL_Log("coldx: could not convert %s to RGBA: %s", full.c_str(), SDL_GetError());
        return {};
    }
    if (!SDL_PremultiplySurfaceAlpha(rgba, false)) {
        SDL_Log("coldx: could not premultiply %s: %s", full.c_str(), SDL_GetError());
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
    Texture2D result = FromPixels(graphics, rgba->w, rgba->h, pixels.get());
    SDL_DestroySurface(rgba);
    return result;
}

void Texture2D::SetData(const Color* pixels) {
    if (!impl_ || !pixels || !impl_->token->device) return;
    Upload(impl_->token->device, impl_->texture, impl_->width, impl_->height, pixels);
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
