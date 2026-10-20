#include <coldx/core/sprite_batch.h>

#include <vector>

#include <SDL3/SDL.h>

#include "graphics_access.h"
#include "shader_blob.h"
#include "sprite_frag.h"
#include "sprite_vert.h"
#include "texture_access.h"

namespace coldx {

namespace {

struct Vertex {
    float x, y;
    float u, v;
    Color color;
};
static_assert(sizeof(Vertex) == 20);

struct Sprite {
    SDL_GPUTexture* texture;
    Vertex vertices[4];
};

constexpr Uint32 InitialCapacity = 2048;

} // namespace

struct SpriteBatch::Impl {
    GraphicsDevice& graphics;
    std::shared_ptr<detail::DeviceToken> token;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    SDL_GPUSampler* sampler = nullptr;
    SDL_GPUBuffer* vertexBuffer = nullptr;
    SDL_GPUBuffer* indexBuffer = nullptr;
    SDL_GPUTransferBuffer* transfer = nullptr;
    Uint32 capacity = 0;

    std::vector<Sprite> sprites;
    bool begun = false;
    bool failed = false;

    explicit Impl(GraphicsDevice& g) : graphics(g) {}
    ~Impl() { Release(); }

    void Release() {
        SDL_GPUDevice* device = token ? token->device : nullptr;
        if (device) {
            if (pipeline) SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            if (sampler) SDL_ReleaseGPUSampler(device, sampler);
            if (vertexBuffer) SDL_ReleaseGPUBuffer(device, vertexBuffer);
            if (indexBuffer) SDL_ReleaseGPUBuffer(device, indexBuffer);
            if (transfer) SDL_ReleaseGPUTransferBuffer(device, transfer);
        }
        pipeline = nullptr;
        sampler = nullptr;
        vertexBuffer = nullptr;
        indexBuffer = nullptr;
        transfer = nullptr;
        capacity = 0;
    }

    bool CreatePipeline(SDL_GPUDevice* device);
    bool EnsureCapacity(SDL_GPUDevice* device, Uint32 spriteCount);
    void Add(const Texture2D& texture, float x, float y, float w, float h, std::optional<Rectangle> source, Color color);
};

bool SpriteBatch::Impl::CreatePipeline(SDL_GPUDevice* device) {
    SDL_GPUShader* vertexShader = detail::CreateShader(device, shaders::sprite_vert, detail::ShaderStage::Vertex, "sprite.vert");
    SDL_GPUShader* fragmentShader = detail::CreateShader(device, shaders::sprite_frag, detail::ShaderStage::Fragment, "sprite.frag");
    if (!vertexShader || !fragmentShader) {
        if (vertexShader) SDL_ReleaseGPUShader(device, vertexShader);
        if (fragmentShader) SDL_ReleaseGPUShader(device, fragmentShader);
        return false;
    }

    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.pitch = sizeof(Vertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attributes[3]{};
    attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, x)};
    attributes[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, u)};
    attributes[2] = {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(Vertex, color)};

    // Premultiplied alpha: source already carries its alpha in rgb.
    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertexShader;
    info.fragment_shader = fragmentShader;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 3;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);
    if (!pipeline) {
        SDL_Log("coldx: SpriteBatch: SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
        return false;
    }

    SDL_GPUSamplerCreateInfo samplerInfo{};
    samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler = SDL_CreateGPUSampler(device, &samplerInfo);
    if (!sampler) {
        SDL_Log("coldx: SpriteBatch: SDL_CreateGPUSampler failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

// Grows the buffers to hold spriteCount sprites. The index buffer never changes after it is
// filled, so it is uploaded here on a command buffer of its own.
bool SpriteBatch::Impl::EnsureCapacity(SDL_GPUDevice* device, Uint32 spriteCount) {
    if (spriteCount <= capacity) return true;
    Uint32 newCapacity = capacity ? capacity : InitialCapacity;
    while (newCapacity < spriteCount) newCapacity *= 2;

    if (vertexBuffer) SDL_ReleaseGPUBuffer(device, vertexBuffer);
    if (indexBuffer) SDL_ReleaseGPUBuffer(device, indexBuffer);
    if (transfer) SDL_ReleaseGPUTransferBuffer(device, transfer);
    vertexBuffer = nullptr;
    indexBuffer = nullptr;
    transfer = nullptr;
    capacity = 0;

    const Uint32 vertexBytes = newCapacity * 4 * sizeof(Vertex);
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
        SDL_Log("coldx: SpriteBatch: could not create buffers for %u sprites: %s", newCapacity, SDL_GetError());
        return false;
    }

    auto* indices = static_cast<Uint32*>(SDL_MapGPUTransferBuffer(device, transfer, false));
    if (!indices) {
        SDL_Log("coldx: SpriteBatch: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }
    // Vertices per sprite: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right.
    for (Uint32 i = 0; i < newCapacity; ++i) {
        const Uint32 v = i * 4;
        Uint32* q = indices + i * 6;
        q[0] = v; q[1] = v + 1; q[2] = v + 2;
        q[3] = v + 1; q[4] = v + 3; q[5] = v + 2;
    }
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("coldx: SpriteBatch: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTransferBufferLocation source{transfer, 0};
    SDL_GPUBufferRegion destination{indexBuffer, 0, indexBytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        SDL_Log("coldx: SpriteBatch: index upload failed: %s", SDL_GetError());
        return false;
    }
    capacity = newCapacity;
    return true;
}

void SpriteBatch::Impl::Add(const Texture2D& texture, float x, float y, float w, float h,
                            std::optional<Rectangle> source, Color color) {
    if (!begun) {
        SDL_Log("coldx: SpriteBatch::Draw called without Begin");
        return;
    }
    SDL_GPUTexture* handle = detail::TextureAccess::Handle(texture);
    if (!handle) return;

    const float tw = static_cast<float>(texture.Width());
    const float th = static_cast<float>(texture.Height());
    const Rectangle src = source ? *source : Rectangle(0, 0, texture.Width(), texture.Height());
    const float u0 = src.x / tw;
    const float v0 = src.y / th;
    const float u1 = (src.x + src.width) / tw;
    const float v1 = (src.y + src.height) / th;

    Sprite sprite{handle, {}};
    sprite.vertices[0] = {x, y, u0, v0, color};
    sprite.vertices[1] = {x + w, y, u1, v0, color};
    sprite.vertices[2] = {x, y + h, u0, v1, color};
    sprite.vertices[3] = {x + w, y + h, u1, v1, color};
    sprites.push_back(sprite);
}

SpriteBatch::SpriteBatch(GraphicsDevice& graphics) : impl_(std::make_unique<Impl>(graphics)) {}

SpriteBatch::~SpriteBatch() = default;

void SpriteBatch::Begin() {
    if (impl_->begun) SDL_Log("coldx: SpriteBatch::Begin called twice without End");
    impl_->begun = true;
    impl_->sprites.clear();
}

void SpriteBatch::Draw(const Texture2D& texture, Vector2 position, Color color) {
    Draw(texture, position, std::nullopt, color);
}

void SpriteBatch::Draw(const Texture2D& texture, Vector2 position, std::optional<Rectangle> source, Color color) {
    const float w = static_cast<float>(source ? source->width : texture.Width());
    const float h = static_cast<float>(source ? source->height : texture.Height());
    impl_->Add(texture, position.x, position.y, w, h, source, color);
}

void SpriteBatch::Draw(const Texture2D& texture, Rectangle destination, Color color) {
    Draw(texture, destination, std::nullopt, color);
}

void SpriteBatch::Draw(const Texture2D& texture, Rectangle destination, std::optional<Rectangle> source, Color color) {
    impl_->Add(texture, static_cast<float>(destination.x), static_cast<float>(destination.y),
               static_cast<float>(destination.width), static_cast<float>(destination.height), source, color);
}

void SpriteBatch::End() {
    Impl& b = *impl_;
    if (!b.begun) {
        SDL_Log("coldx: SpriteBatch::End called without Begin");
        return;
    }
    b.begun = false;
    if (b.sprites.empty()) return;

    SDL_GPUCommandBuffer* commands = detail::GraphicsAccess::Commands(b.graphics);
    SDL_GPUTexture* target = detail::GraphicsAccess::BackBuffer(b.graphics);
    if (!commands || !target) {
        SDL_Log("coldx: SpriteBatch::End called outside Draw; nothing drawn");
        b.sprites.clear();
        return;
    }

    SDL_GPUDevice* device = detail::GraphicsAccess::Device(b.graphics);
    if (!b.token || b.token->device != device) {
        b.Release();
        b.token = detail::GraphicsAccess::Token(b.graphics);
        b.failed = false;
    }
    if (b.failed) {
        b.sprites.clear();
        return;
    }
    const Uint32 count = static_cast<Uint32>(b.sprites.size());
    if ((!b.pipeline && !b.CreatePipeline(device)) || !b.EnsureCapacity(device, count)) {
        // Logged already. Stop retrying every frame.
        b.failed = true;
        b.sprites.clear();
        return;
    }

    // cycle = true: if the GPU is still reading this buffer from an earlier End, SDL hands
    // out a fresh one instead of overwriting it.
    auto* vertices = static_cast<Vertex*>(SDL_MapGPUTransferBuffer(device, b.transfer, true));
    if (!vertices) {
        SDL_Log("coldx: SpriteBatch: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        b.sprites.clear();
        return;
    }
    for (Uint32 i = 0; i < count; ++i) SDL_memcpy(vertices + i * 4, b.sprites[i].vertices, sizeof(Vertex) * 4);
    SDL_UnmapGPUTransferBuffer(device, b.transfer);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTransferBufferLocation source{b.transfer, 0};
    SDL_GPUBufferRegion destination{b.vertexBuffer, 0, count * 4 * static_cast<Uint32>(sizeof(Vertex))};
    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    SDL_EndGPUCopyPass(copy);

    // Pixels to clip space, (0, 0) top left. Row-vector convention, stored row-major.
    const float w = static_cast<float>(b.graphics.BackBufferWidth());
    const float h = static_cast<float>(b.graphics.BackBufferHeight());
    const float transform[16] = {
        2.0f / w, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / h, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,
    };
    SDL_PushGPUVertexUniformData(commands, 0, transform, sizeof(transform));

    SDL_GPUColorTargetInfo colorTarget{};
    colorTarget.texture = target;
    colorTarget.load_op = SDL_GPU_LOADOP_LOAD;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, b.pipeline);
    SDL_GPUBufferBinding vertexBinding{b.vertexBuffer, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
    SDL_GPUBufferBinding indexBinding{b.indexBuffer, 0};
    SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    // One draw per run of sprites sharing a texture.
    Uint32 first = 0;
    while (first < count) {
        SDL_GPUTexture* texture = b.sprites[first].texture;
        Uint32 end = first + 1;
        while (end < count && b.sprites[end].texture == texture) ++end;
        SDL_GPUTextureSamplerBinding binding{texture, b.sampler};
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
        SDL_DrawGPUIndexedPrimitives(pass, (end - first) * 6, 1, first * 6, 0, 0);
        first = end;
    }
    SDL_EndGPURenderPass(pass);
    b.sprites.clear();
}

} // namespace coldx
