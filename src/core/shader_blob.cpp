#include "shader_blob.h"

#include <SDL3/SDL.h>

namespace coldx::detail {

SDL_GPUShader* CreateShader(SDL_GPUDevice* device, const ShaderBlob& blob, ShaderStage stage, const char* name) {
    SDL_GPUShaderCreateInfo info{};
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = blob.spirv;
        info.code_size = blob.spirvSize;
        info.entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = blob.dxil;
        info.code_size = blob.dxilSize;
        info.entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.code = blob.msl;
        info.code_size = blob.mslSize;
        info.entrypoint = "main0";
    } else {
        SDL_Log("coldx.core: shader %s: the GPU device takes none of SPIR-V, DXIL or MSL", name);
        return nullptr;
    }
    info.stage = stage == ShaderStage::Vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = blob.samplers;
    info.num_storage_textures = blob.storageTextures;
    info.num_storage_buffers = blob.storageBuffers;
    info.num_uniform_buffers = blob.uniformBuffers;

    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    if (!shader) SDL_Log("coldx.core: shader %s: SDL_CreateGPUShader failed: %s", name, SDL_GetError());
    return shader;
}

} // namespace coldx::detail
