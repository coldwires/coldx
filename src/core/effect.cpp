#include <coldx/core/effect.h>
#include <coldx/core/effect_format.h>

#include <cstring>
#include <unordered_set>
#include <vector>

#include <SDL3/SDL.h>

#include "effect_access.h"
#include "graphics_access.h"
#include "shader_blob.h"
#include "sprite_renderer.h"
#include "sprite_vert.h"
#include "texture_access.h"

namespace coldx {

struct Effect::Impl {
    std::string name;
    effect_format::Compiled compiled;
    std::vector<std::uint8_t> uniforms;
    std::vector<Texture2D> textures;  // by slot; slot 0, the sprite's, stays empty

    std::shared_ptr<detail::DeviceToken> token;
    SDL_GPUShader* vertex = nullptr;
    SDL_GPUShader* fragment = nullptr;
    SDL_GPUGraphicsPipeline* pipelines[detail::BlendStateCount] = {};
    bool failed = false;
    std::unordered_set<std::string> warned;

    ~Impl() { Release(); }

    SDL_GPUDevice* Device() const { return token ? token->device : nullptr; }

    void Release() {
        if (SDL_GPUDevice* device = Device()) {
            if (vertex) SDL_ReleaseGPUShader(device, vertex);
            if (fragment) SDL_ReleaseGPUShader(device, fragment);
            for (auto* p : pipelines)
                if (p) SDL_ReleaseGPUGraphicsPipeline(device, p);
        }
        vertex = fragment = nullptr;
        for (auto& p : pipelines) p = nullptr;
    }

    // Shaders for the device graphics has now; remade after a second Run makes a new one.
    bool Shaders(GraphicsDevice& graphics) {
        SDL_GPUDevice* device = detail::GraphicsAccess::Device(graphics);
        if (!device) return false;
        if (Device() != device) {
            Release();
            token = detail::GraphicsAccess::Token(graphics);
            failed = false;
        }
        if (failed) return false;
        if (!vertex) vertex = detail::CreateShader(device, shaders::sprite_vert, detail::ShaderStage::Vertex, "sprite.vert");
        if (!fragment) {
            const ShaderBlob blob{compiled.spirv.data(), compiled.spirv.size(),
                                  compiled.dxil.data(),  compiled.dxil.size(),
                                  compiled.msl.data(),   compiled.msl.size(),
                                  static_cast<unsigned>(compiled.textures.size()), 0, 0,
                                  compiled.uniformSize ? 1u : 0u};
            fragment = detail::CreateShader(device, blob, detail::ShaderStage::Fragment, name.c_str());
        }
        failed = !vertex || !fragment;
        return !failed;
    }

    void WarnOnce(const std::string& key, const char* format, const std::string& what) {
        if (warned.insert(key).second) SDL_Log(format, name.c_str(), what.c_str());
    }

    const effect_format::Parameter* Find(const std::string& parameter) {
        for (const effect_format::Parameter& p : compiled.parameters)
            if (p.name == parameter) return &p;
        WarnOnce("?" + parameter, "coldx.core: effect %s has no parameter %s; ignored", parameter);
        return nullptr;
    }

    // Writes size bytes for parameter if its type is one of the two given.
    void Write(const std::string& parameter, effect_format::ParameterType a, effect_format::ParameterType b,
               const void* value, std::size_t size) {
        const effect_format::Parameter* p = Find(parameter);
        if (!p) return;
        if (p->type != a && p->type != b) {
            WarnOnce("!" + parameter, "coldx.core: effect %s: parameter %s is set with a value of another type; ignored", parameter);
            return;
        }
        std::memcpy(uniforms.data() + p->offset, value, size);
    }
};

Effect::Effect() = default;
Effect::~Effect() = default;
Effect::Effect(const Effect&) = default;
Effect& Effect::operator=(const Effect&) = default;
Effect::Effect(Effect&&) noexcept = default;
Effect& Effect::operator=(Effect&&) noexcept = default;

Effect Effect::FromMemory(GraphicsDevice& graphics, const void* data, std::size_t size, const std::string& name) {
    auto impl = std::make_shared<Impl>();
    std::string error;
    if (!effect_format::Decode(data, size, impl->compiled, error)) {
        SDL_Log("coldx.core: effect %s: %s", name.c_str(), error.c_str());
        return {};
    }
    impl->name = name;
    // Rounded up to whole float4s, the unit every backend's uniform blocks come in.
    impl->uniforms.assign((impl->compiled.uniformSize + 15) / 16 * 16, 0);
    impl->textures.resize(impl->compiled.textures.size());
    // Made now when the device exists, so a shader the driver rejects fails at load.
    if (detail::GraphicsAccess::Device(graphics) && !impl->Shaders(graphics)) return {};
    Effect effect;
    effect.impl_ = std::move(impl);
    return effect;
}

bool Effect::Has(const std::string& name) const {
    if (!impl_) return false;
    for (const effect_format::Parameter& p : impl_->compiled.parameters)
        if (p.name == name) return true;
    for (const effect_format::Texture& t : impl_->compiled.textures)
        if (t.slot > 0 && t.name == name) return true;
    return false;
}

using Type = effect_format::ParameterType;

void Effect::Set(const std::string& name, float value) {
    if (impl_) impl_->Write(name, Type::Float, Type::Float, &value, sizeof(value));
}

void Effect::Set(const std::string& name, Vector2 value) {
    const float v[2] = {value.x, value.y};
    if (impl_) impl_->Write(name, Type::Float2, Type::Float2, v, sizeof(v));
}

void Effect::Set(const std::string& name, float x, float y, float z) {
    const float v[3] = {x, y, z};
    if (impl_) impl_->Write(name, Type::Float3, Type::Float3, v, sizeof(v));
}

void Effect::Set(const std::string& name, float x, float y, float z, float w) {
    const float v[4] = {x, y, z, w};
    if (impl_) impl_->Write(name, Type::Float4, Type::Float4, v, sizeof(v));
}

void Effect::Set(const std::string& name, Color value) {
    Set(name, value.r / 255.0f, value.g / 255.0f, value.b / 255.0f, value.a / 255.0f);
}

// Stored row-major as coldx keeps it; the shader reads the transpose, which is why
// mul(matrix, v) there is v * matrix here (the same as SpriteBatch's own transform).
void Effect::Set(const std::string& name, const Matrix& value) {
    if (impl_) impl_->Write(name, Type::Float4x4, Type::Float4x4, value.m, sizeof(value.m));
}

void Effect::Set(const std::string& name, int value) {
    if (impl_) impl_->Write(name, Type::Int, Type::UInt, &value, sizeof(value));
}

void Effect::Set(const std::string& name, bool value) {
    const std::uint32_t v = value ? 1 : 0;
    if (impl_) impl_->Write(name, Type::UInt, Type::Int, &v, sizeof(v));
}

void Effect::Set(const std::string& name, const Texture2D& texture) {
    if (!impl_) return;
    for (const effect_format::Texture& t : impl_->compiled.textures) {
        if (t.slot > 0 && t.name == name) {
            impl_->textures[t.slot] = texture;
            return;
        }
    }
    impl_->WarnOnce("?" + name, "coldx.core: effect %s has no texture %s at slot 1 or later; ignored", name);
}

const std::string& Effect::Name() const {
    static const std::string none;
    return impl_ ? impl_->name : none;
}

bool Effect::IsEmpty() const {
    return !impl_;
}

namespace detail {

SDL_GPUGraphicsPipeline* EffectAccess::Pipeline(const Effect& effect, GraphicsDevice& graphics, BlendState blend) {
    Effect::Impl& e = *effect.impl_;
    if (!e.Shaders(graphics)) return nullptr;
    SDL_GPUGraphicsPipeline*& pipeline = e.pipelines[static_cast<int>(blend)];
    if (!pipeline) {
        pipeline = CreateSpritePipeline(e.Device(), e.vertex, e.fragment, blend);
        e.failed = !pipeline;
    }
    return pipeline;
}

const std::vector<std::uint8_t>& EffectAccess::Uniforms(const Effect& effect) {
    return effect.impl_->uniforms;
}

std::size_t EffectAccess::TextureCount(const Effect& effect) {
    return effect.impl_->textures.size();
}

SDL_GPUTexture* EffectAccess::Texture(const Effect& effect, std::size_t slot, SDL_GPUTexture* fallback) {
    Effect::Impl& e = *effect.impl_;
    if (SDL_GPUTexture* set = TextureAccess::Handle(e.textures[slot])) return set;
    e.WarnOnce("#" + e.compiled.textures[slot].name, "coldx.core: effect %s: texture %s was never set; the sprite's own stands in",
               e.compiled.textures[slot].name);
    return fallback;
}

} // namespace detail
} // namespace coldx
