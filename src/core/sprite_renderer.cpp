#include "sprite_renderer.h"

#include <SDL3/SDL.h>

#include "graphics_access.h"
#include "shader_blob.h"
#include "sprite_frag.h"
#include "sprite_vert.h"

namespace coldx::detail {

namespace {

constexpr Uint32 InitialCapacity = 2048;
constexpr int BlendCount = 4;
constexpr int SamplerCount = 4;

SDL_GPUColorTargetBlendState BlendFor(BlendState blend) {
    SDL_GPUColorTargetBlendState b{};
    b.enable_blend = true;
    b.color_blend_op = SDL_GPU_BLENDOP_ADD;
    b.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    switch (blend) {
    case BlendState::AlphaBlend:
        b.src_color_blendfactor = b.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        b.dst_color_blendfactor = b.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case BlendState::Additive:
        b.src_color_blendfactor = b.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        b.dst_color_blendfactor = b.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        break;
    case BlendState::NonPremultiplied:
        b.src_color_blendfactor = b.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        b.dst_color_blendfactor = b.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case BlendState::Opaque:
        b.enable_blend = false;
        break;
    }
    return b;
}

SDL_GPUSamplerCreateInfo SamplerFor(SamplerState sampler) {
    const bool point = sampler == SamplerState::PointClamp || sampler == SamplerState::PointWrap;
    const bool wrap = sampler == SamplerState::PointWrap || sampler == SamplerState::LinearWrap;
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = info.mag_filter = point ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = info.address_mode_v = info.address_mode_w =
        wrap ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    return info;
}

} // namespace

struct SpriteRenderer::Impl {
    std::shared_ptr<DeviceToken> token;
    SDL_GPUShader* vertexShader = nullptr;
    SDL_GPUShader* fragmentShader = nullptr;
    SDL_GPUGraphicsPipeline* pipelines[BlendCount] = {};
    SDL_GPUSampler* samplers[SamplerCount] = {};
    SDL_GPUBuffer* vertexBuffer = nullptr;
    SDL_GPUBuffer* indexBuffer = nullptr;
    SDL_GPUTransferBuffer* transfer = nullptr;
    Uint32 capacity = 0;
    bool failed = false;

    ~Impl() { Release(); }

    SDL_GPUDevice* Device() const { return token ? token->device : nullptr; }

    void Release() {
        if (SDL_GPUDevice* device = Device()) {
            if (vertexShader) SDL_ReleaseGPUShader(device, vertexShader);
            if (fragmentShader) SDL_ReleaseGPUShader(device, fragmentShader);
            for (auto* p : pipelines)
                if (p) SDL_ReleaseGPUGraphicsPipeline(device, p);
            for (auto* s : samplers)
                if (s) SDL_ReleaseGPUSampler(device, s);
            if (vertexBuffer) SDL_ReleaseGPUBuffer(device, vertexBuffer);
            if (indexBuffer) SDL_ReleaseGPUBuffer(device, indexBuffer);
            if (transfer) SDL_ReleaseGPUTransferBuffer(device, transfer);
        }
        vertexShader = fragmentShader = nullptr;
        for (auto& p : pipelines) p = nullptr;
        for (auto& s : samplers) s = nullptr;
        vertexBuffer = indexBuffer = nullptr;
        transfer = nullptr;
        capacity = 0;
    }

    SDL_GPUGraphicsPipeline* Pipeline(BlendState blend);
    SDL_GPUSampler* Sampler(SamplerState sampler);
    bool EnsureCapacity(Uint32 quadCount);
};

// Pipelines differ only in blend state, so they are made on first use of each.
SDL_GPUGraphicsPipeline* SpriteRenderer::Impl::Pipeline(BlendState blend) {
    const int index = static_cast<int>(blend);
    if (pipelines[index]) return pipelines[index];
    SDL_GPUDevice* device = Device();

    if (!vertexShader) vertexShader = CreateShader(device, shaders::sprite_vert, ShaderStage::Vertex, "sprite.vert");
    if (!fragmentShader)
        fragmentShader = CreateShader(device, shaders::sprite_frag, ShaderStage::Fragment, "sprite.frag");
    if (!vertexShader || !fragmentShader) return nullptr;

    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.pitch = sizeof(SpriteVertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attributes[4]{};
    attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(SpriteVertex, x)};
    attributes[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(SpriteVertex, u)};
    attributes[2] = {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(SpriteVertex, color)};
    attributes[3] = {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(SpriteVertex, bounds)};

    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    target.blend_state = BlendFor(blend);

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertexShader;
    info.fragment_shader = fragmentShader;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 4;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;

    pipelines[index] = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!pipelines[index]) SDL_Log("coldx.core: SpriteBatch: SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
    return pipelines[index];
}

SDL_GPUSampler* SpriteRenderer::Impl::Sampler(SamplerState sampler) {
    const int index = static_cast<int>(sampler);
    if (!samplers[index]) {
        const SDL_GPUSamplerCreateInfo info = SamplerFor(sampler);
        samplers[index] = SDL_CreateGPUSampler(Device(), &info);
        if (!samplers[index]) SDL_Log("coldx.core: SpriteBatch: SDL_CreateGPUSampler failed: %s", SDL_GetError());
    }
    return samplers[index];
}

// Grows the buffers to hold quadCount quads. The index buffer never changes after it is
// filled, so it is uploaded here on a command buffer of its own.
bool SpriteRenderer::Impl::EnsureCapacity(Uint32 quadCount) {
    if (quadCount <= capacity) return true;
    SDL_GPUDevice* device = Device();
    Uint32 newCapacity = capacity ? capacity : InitialCapacity;
    while (newCapacity < quadCount) newCapacity *= 2;

    if (vertexBuffer) SDL_ReleaseGPUBuffer(device, vertexBuffer);
    if (indexBuffer) SDL_ReleaseGPUBuffer(device, indexBuffer);
    if (transfer) SDL_ReleaseGPUTransferBuffer(device, transfer);
    vertexBuffer = indexBuffer = nullptr;
    transfer = nullptr;
    capacity = 0;

    const Uint32 vertexBytes = newCapacity * 4 * sizeof(SpriteVertex);
    const Uint32 indexBytes = newCapacity * 6 * sizeof(Uint32);
    SDL_GPUBufferCreateInfo vertexInfo{};
    vertexInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertexInfo.size = vertexBytes;
    vertexBuffer = SDL_CreateGPUBuffer(device, &vertexInfo);
    SDL_GPUBufferCreateInfo indexInfo{};
    indexInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    indexInfo.size = indexBytes;
    indexBuffer = SDL_CreateGPUBuffer(device, &indexInfo);
    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = vertexBytes > indexBytes ? vertexBytes : indexBytes;
    transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    if (!vertexBuffer || !indexBuffer || !transfer) {
        SDL_Log("coldx.core: SpriteBatch: could not create buffers for %u sprites: %s", newCapacity, SDL_GetError());
        return false;
    }

    auto* indices = static_cast<Uint32*>(SDL_MapGPUTransferBuffer(device, transfer, false));
    if (!indices) {
        SDL_Log("coldx.core: SpriteBatch: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }
    for (Uint32 i = 0; i < newCapacity; ++i) {
        const Uint32 v = i * 4;
        Uint32* q = indices + i * 6;
        q[0] = v; q[1] = v + 1; q[2] = v + 2;
        q[3] = v + 1; q[4] = v + 3; q[5] = v + 2;
    }
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("coldx.core: SpriteBatch: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTransferBufferLocation source{transfer, 0};
    SDL_GPUBufferRegion destination{indexBuffer, 0, indexBytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        SDL_Log("coldx.core: SpriteBatch: index upload failed: %s", SDL_GetError());
        return false;
    }
    capacity = newCapacity;
    return true;
}

SpriteRenderer::SpriteRenderer() : impl_(std::make_unique<Impl>()) {}

SpriteRenderer::~SpriteRenderer() = default;

bool SpriteRenderer::Draw(GraphicsDevice& graphics, const SpriteQuad* quads, std::uint32_t count, BlendState blend,
                          SamplerState samplerState, const Matrix& transform) {
    Impl& r = *impl_;
    if (count == 0) return true;

    SDL_GPUCommandBuffer* commands = GraphicsAccess::Commands(graphics);
    SDL_GPUTexture* target = GraphicsAccess::CurrentTarget(graphics);
    if (!commands || !target) {
        SDL_Log("coldx.core: SpriteBatch drawn outside Draw; nothing drawn");
        return false;
    }

    // A new device (a second Run) invalidates everything made on the old one.
    SDL_GPUDevice* device = GraphicsAccess::Device(graphics);
    if (r.Device() != device) {
        r.Release();
        r.token = GraphicsAccess::Token(graphics);
        r.failed = false;
    }
    if (r.failed) return false;

    SDL_GPUGraphicsPipeline* pipeline = r.Pipeline(blend);
    SDL_GPUSampler* sampler = r.Sampler(samplerState);
    if (!pipeline || !sampler || !r.EnsureCapacity(count)) {
        // Logged already. Stop retrying every frame.
        r.failed = true;
        return false;
    }

    // cycle = true: if the GPU still reads this buffer from an earlier draw this frame, SDL
    // hands out a fresh one instead of overwriting it.
    auto* vertices = static_cast<SpriteVertex*>(SDL_MapGPUTransferBuffer(device, r.transfer, true));
    if (!vertices) {
        SDL_Log("coldx.core: SpriteBatch: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }
    for (std::uint32_t i = 0; i < count; ++i) SDL_memcpy(vertices + i * 4, quads[i].vertices, sizeof(SpriteVertex) * 4);
    SDL_UnmapGPUTransferBuffer(device, r.transfer);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTransferBufferLocation source{r.transfer, 0};
    SDL_GPUBufferRegion destination{r.vertexBuffer, 0, count * 4 * static_cast<Uint32>(sizeof(SpriteVertex))};
    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    SDL_EndGPUCopyPass(copy);

    // The game's transform, then pixels of the current target to clip space.
    const float w = static_cast<float>(GraphicsAccess::CurrentTargetWidth(graphics));
    const float h = static_cast<float>(GraphicsAccess::CurrentTargetHeight(graphics));
    const Matrix toClip = transform * Matrix::CreateOrthographicOffCenter(0.0f, w, h, 0.0f, 0.0f, 1.0f);
    SDL_PushGPUVertexUniformData(commands, 0, toClip.m, sizeof(toClip.m));

    SDL_GPUColorTargetInfo colorTarget{};
    colorTarget.texture = target;
    colorTarget.load_op = SDL_GPU_LOADOP_LOAD;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    SDL_GPUBufferBinding vertexBinding{r.vertexBuffer, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
    SDL_GPUBufferBinding indexBinding{r.indexBuffer, 0};
    SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    std::uint32_t first = 0;
    while (first < count) {
        SDL_GPUTexture* texture = quads[first].texture;
        std::uint32_t end = first + 1;
        while (end < count && quads[end].texture == texture) ++end;
        SDL_GPUTextureSamplerBinding binding{texture, sampler};
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
        SDL_DrawGPUIndexedPrimitives(pass, (end - first) * 6, 1, first * 6, 0, 0);
        first = end;
    }
    SDL_EndGPURenderPass(pass);
    return true;
}

} // namespace coldx::detail
