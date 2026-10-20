#include <coldx/core/sprite_batch.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include <SDL3/SDL.h>

#include "sprite_renderer.h"
#include "texture_access.h"

namespace coldx {

namespace {

struct QueuedSprite {
    detail::SpriteQuad quad;
    float depth;
};

} // namespace

struct SpriteBatch::Impl {
    GraphicsDevice& graphics;
    detail::SpriteRenderer renderer;

    std::vector<QueuedSprite> queue;
    std::vector<detail::SpriteQuad> ordered;
    bool begun = false;
    SpriteSortMode sortMode = SpriteSortMode::Deferred;
    BlendState blend = BlendState::AlphaBlend;
    SamplerState sampler = SamplerState::LinearClamp;
    Matrix transform;

    explicit Impl(GraphicsDevice& g) : graphics(g) {}

    // x, y: where the origin lands. w, h: drawn size. ox, oy: origin in drawn units.
    void Add(const Texture2D& texture, float x, float y, float w, float h, float ox, float oy,
             const Rectangle& source, Color color, float rotation, SpriteEffects effects, float depth);
    void Flush();
};

void SpriteBatch::Impl::Add(const Texture2D& texture, float x, float y, float w, float h, float ox, float oy,
                            const Rectangle& source, Color color, float rotation, SpriteEffects effects, float depth) {
    if (!begun) {
        SDL_Log("coldx.core: SpriteBatch::Draw called without Begin");
        return;
    }
    SDL_GPUTexture* handle = detail::TextureAccess::Handle(texture);
    if (!handle) return;

    const float tw = static_cast<float>(texture.Width());
    const float th = static_cast<float>(texture.Height());
    float u0 = source.x / tw;
    float v0 = source.y / th;
    float u1 = (source.x + source.width) / tw;
    float v1 = (source.y + source.height) / th;
    if (HasFlag(effects, SpriteEffects::FlipHorizontally)) std::swap(u0, u1);
    if (HasFlag(effects, SpriteEffects::FlipVertically)) std::swap(v0, v1);

    // Corners relative to the origin, then rotated about it. Skipping the trig when there is
    // no rotation keeps unrotated sprites exactly on whole pixels.
    const float lx[4] = {-ox, w - ox, -ox, w - ox};
    const float ly[4] = {-oy, -oy, h - oy, h - oy};
    const float us[4] = {u0, u1, u0, u1};
    const float vs[4] = {v0, v0, v1, v1};
    QueuedSprite sprite{};
    sprite.quad.texture = handle;
    sprite.depth = depth;
    const float c = rotation != 0.0f ? std::cos(rotation) : 1.0f;
    const float s = rotation != 0.0f ? std::sin(rotation) : 0.0f;
    for (int i = 0; i < 4; ++i) {
        sprite.quad.vertices[i] = {x + lx[i] * c - ly[i] * s, y + lx[i] * s + ly[i] * c, us[i], vs[i], color};
    }
    queue.push_back(sprite);
    if (sortMode == SpriteSortMode::Immediate) Flush();
}

void SpriteBatch::Impl::Flush() {
    if (queue.empty()) return;
    switch (sortMode) {
    case SpriteSortMode::Texture:
        std::stable_sort(queue.begin(), queue.end(),
                         [](const QueuedSprite& a, const QueuedSprite& b) { return a.quad.texture < b.quad.texture; });
        break;
    case SpriteSortMode::BackToFront:
        std::stable_sort(queue.begin(), queue.end(),
                         [](const QueuedSprite& a, const QueuedSprite& b) { return a.depth > b.depth; });
        break;
    case SpriteSortMode::FrontToBack:
        std::stable_sort(queue.begin(), queue.end(),
                         [](const QueuedSprite& a, const QueuedSprite& b) { return a.depth < b.depth; });
        break;
    case SpriteSortMode::Deferred:
    case SpriteSortMode::Immediate:
        break;
    }
    ordered.clear();
    ordered.reserve(queue.size());
    for (const QueuedSprite& q : queue) ordered.push_back(q.quad);
    renderer.Draw(graphics, ordered.data(), static_cast<std::uint32_t>(ordered.size()), blend, sampler, transform);
    queue.clear();
}

SpriteBatch::SpriteBatch(GraphicsDevice& graphics) : impl_(std::make_unique<Impl>(graphics)) {}

SpriteBatch::~SpriteBatch() = default;

void SpriteBatch::Begin(SpriteSortMode sortMode, BlendState blend, SamplerState sampler, const Matrix& transform) {
    Impl& b = *impl_;
    if (b.begun) SDL_Log("coldx.core: SpriteBatch::Begin called twice without End");
    b.begun = true;
    b.queue.clear();
    b.sortMode = sortMode;
    b.blend = blend;
    b.sampler = sampler;
    b.transform = transform;
}

void SpriteBatch::End() {
    Impl& b = *impl_;
    if (!b.begun) {
        SDL_Log("coldx.core: SpriteBatch::End called without Begin");
        return;
    }
    b.Flush();
    b.begun = false;
}

static Rectangle SourceOrWhole(const Texture2D& texture, std::optional<Rectangle> source) {
    return source ? *source : Rectangle(0, 0, texture.Width(), texture.Height());
}

void SpriteBatch::Draw(const Texture2D& texture, Vector2 position, Color color) {
    Draw(texture, position, std::nullopt, color, 0.0f, Vector2(), Vector2::One(), SpriteEffects::None, 0.0f);
}

void SpriteBatch::Draw(const Texture2D& texture, Vector2 position, std::optional<Rectangle> source, Color color) {
    Draw(texture, position, source, color, 0.0f, Vector2(), Vector2::One(), SpriteEffects::None, 0.0f);
}

void SpriteBatch::Draw(const Texture2D& texture, Vector2 position, std::optional<Rectangle> source, Color color,
                       float rotation, Vector2 origin, float scale, SpriteEffects effects, float layerDepth) {
    Draw(texture, position, source, color, rotation, origin, Vector2(scale, scale), effects, layerDepth);
}

void SpriteBatch::Draw(const Texture2D& texture, Vector2 position, std::optional<Rectangle> source, Color color,
                       float rotation, Vector2 origin, Vector2 scale, SpriteEffects effects, float layerDepth) {
    const Rectangle src = SourceOrWhole(texture, source);
    impl_->Add(texture, position.x, position.y, src.width * scale.x, src.height * scale.y, origin.x * scale.x,
               origin.y * scale.y, src, color, rotation, effects, layerDepth);
}

void SpriteBatch::Draw(const Texture2D& texture, Rectangle destination, Color color) {
    Draw(texture, destination, std::nullopt, color, 0.0f, Vector2(), SpriteEffects::None, 0.0f);
}

void SpriteBatch::Draw(const Texture2D& texture, Rectangle destination, std::optional<Rectangle> source, Color color) {
    Draw(texture, destination, source, color, 0.0f, Vector2(), SpriteEffects::None, 0.0f);
}

void SpriteBatch::Draw(const Texture2D& texture, Rectangle destination, std::optional<Rectangle> source, Color color,
                       float rotation, Vector2 origin, SpriteEffects effects, float layerDepth) {
    const Rectangle src = SourceOrWhole(texture, source);
    if (src.width == 0 || src.height == 0) return;
    const float sx = static_cast<float>(destination.width) / src.width;
    const float sy = static_cast<float>(destination.height) / src.height;
    impl_->Add(texture, static_cast<float>(destination.x), static_cast<float>(destination.y),
               static_cast<float>(destination.width), static_cast<float>(destination.height), origin.x * sx,
               origin.y * sy, src, color, rotation, effects, layerDepth);
}

} // namespace coldx
