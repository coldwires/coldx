#include "gpu_download.h"

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

} // namespace coldx::detail
