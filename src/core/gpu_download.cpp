#include "gpu_download.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

namespace coldx::detail {

bool DownloadTexture(SDL_GPUDevice* device, SDL_GPUTexture* texture, const Rectangle& region, Color* out) {
    if (!device || !texture || region.IsEmpty() || !out) return false;
    const Uint32 w = static_cast<Uint32>(region.width), h = static_cast<Uint32>(region.height);
    SDL_GPUTransferBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    bufferInfo.size = w * h * 4;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &bufferInfo);
    if (!buffer) {
        SDL_Log("coldx.core: read back: SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("coldx.core: read back: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion source{};
    source.texture = texture;
    source.x = static_cast<Uint32>(region.x);
    source.y = static_cast<Uint32>(region.y);
    source.w = w;
    source.h = h;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = buffer;
    destination.pixels_per_row = w;
    destination.rows_per_layer = h;
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);

    bool ok = false;
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence) {
        SDL_WaitForGPUFences(device, true, &fence, 1);
        SDL_ReleaseGPUFence(device, fence);
        if (const void* pixels = SDL_MapGPUTransferBuffer(device, buffer, false)) {
            SDL_memcpy(out, pixels, static_cast<size_t>(w) * h * 4);
            SDL_UnmapGPUTransferBuffer(device, buffer);
            ok = true;
        } else {
            SDL_Log("coldx.core: read back: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        }
    } else {
        SDL_Log("coldx.core: read back: submit failed: %s", SDL_GetError());
    }
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return ok;
}

bool WritePng(const char* path, int width, int height, const Color* pixels, bool straightAlpha) {
    std::vector<Color> copy;
    if (straightAlpha) {
        copy.assign(pixels, pixels + static_cast<size_t>(width) * height);
        for (Color& c : copy) {
            if (c.a == 0 || c.a == 255) continue;
            auto undo = [&](std::uint8_t v) { return static_cast<std::uint8_t>(std::min(255, (v * 255 + c.a / 2) / c.a)); };
            c = Color(undo(c.r), undo(c.g), undo(c.b), c.a);
        }
        pixels = copy.data();
    }
    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, const_cast<Color*>(pixels), width * 4);
    const bool saved = surface && SDL_SavePNG(surface, path);
    if (!saved) SDL_Log("coldx.core: could not write %s: %s", path, SDL_GetError());
    SDL_DestroySurface(surface);
    return saved;
}

} // namespace coldx::detail
