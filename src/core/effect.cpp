#include <coldx/core/effect.h>
#include <coldx/core/effect_format.h>

#include <cstring>
#include <unordered_set>
#include <vector>

#include <SDL3/SDL.h>

#include "effect_access.h"
#include "graphics_access.h"
#include "render_backend.h"
#include "texture_access.h"

namespace coldx {

struct Effect::Impl {
    std::string name;
    effect_format::Compiled compiled;
    std::vector<std::uint8_t> uniforms;
    std::vector<Texture2D> textures;  // by slot, slot 0 (the sprite's) stays empty

    std::unordered_set<std::string> warned;
    // Last, so it goes first: it refers to compiled and name.
    std::unique_ptr<detail::BackendEffect> backend;

    void WarnOnce(const std::string& key, const char* format, const std::string& what) {
        if (warned.insert(key).second) SDL_Log(format, name.c_str(), what.c_str());
    }

    const effect_format::Parameter* Find(const std::string& parameter) {
        for (const effect_format::Parameter& p : compiled.parameters)
            if (p.name == parameter) return &p;
        WarnOnce("?" + parameter, "coldx.core: effect %s has no parameter %s, ignored", parameter);
        return nullptr;
    }

    // Writes size bytes for parameter if its type is one of the two given.
    void Write(const std::string& parameter, effect_format::ParameterType a, effect_format::ParameterType b,
               const void* value, std::size_t size) {
        const effect_format::Parameter* p = Find(parameter);
        if (!p) return;
        if (p->type != a && p->type != b) {
            WarnOnce("!" + parameter, "coldx.core: effect %s: parameter %s is set with a value of another type, ignored", parameter);
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
    impl->backend = detail::GraphicsAccess::Backend(graphics).CreateEffect(impl->compiled, impl->name);
    if (!impl->backend) return {};
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

// Stored row-major as coldx keeps it. The shader reads the transpose, which is why
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
    impl_->WarnOnce("?" + name, "coldx.core: effect %s has no texture %s at slot 1 or later, ignored", name);
}

const std::string& Effect::Name() const {
    static const std::string none;
    return impl_ ? impl_->name : none;
}

bool Effect::IsEmpty() const {
    return !impl_;
}

namespace detail {

BackendEffect& EffectAccess::Backend(const Effect& effect) {
    return *effect.impl_->backend;
}

const std::vector<std::uint8_t>& EffectAccess::Uniforms(const Effect& effect) {
    return effect.impl_->uniforms;
}

std::size_t EffectAccess::TextureCount(const Effect& effect) {
    return effect.impl_->textures.size();
}

const BackendTexture* EffectAccess::Texture(const Effect& effect, std::size_t slot, const BackendTexture* fallback) {
    Effect::Impl& e = *effect.impl_;
    if (const BackendTexture* set = TextureAccess::Handle(e.textures[slot])) return set;
    e.WarnOnce("#" + e.compiled.textures[slot].name, "coldx.core: effect %s: texture %s was never set. The sprite's own stands in",
               e.compiled.textures[slot].name);
    return fallback;
}

} // namespace detail
} // namespace coldx
