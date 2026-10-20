#pragma once

#include <cstddef>

struct SDL_GPUDevice;
struct SDL_GPUShader;

namespace coldx {

// One shader stage compiled for every backend, as written by cmake/compile_shader.cmake.
struct ShaderBlob {
    const unsigned char* spirv;
    std::size_t spirvSize;
    const unsigned char* dxil;
    std::size_t dxilSize;
    // Metal source text. spirv-cross renames the entry point to main0.
    const unsigned char* msl;
    std::size_t mslSize;
    unsigned samplers;
    unsigned storageTextures;
    unsigned storageBuffers;
    unsigned uniformBuffers;
};

namespace detail {

enum class ShaderStage { Vertex, Fragment };

// Picks the format the device takes. Null on failure, with the reason logged.
SDL_GPUShader* CreateShader(SDL_GPUDevice* device, const ShaderBlob& blob, ShaderStage stage, const char* name);

} // namespace detail
} // namespace coldx
