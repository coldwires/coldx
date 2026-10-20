#pragma once

#include <memory>
#include <optional>

#include <coldx/core/color.h>
#include <coldx/core/graphics_states.h>
#include <coldx/core/matrix.h>
#include <coldx/core/rectangle.h>
#include <coldx/core/texture2d.h>
#include <coldx/core/vector2.h>

namespace coldx {

class GraphicsDevice;

enum class SpriteSortMode {
    Deferred,     // drawn at End, in the order Draw was called
    Immediate,    // drawn at each Draw call
    Texture,      // drawn at End, grouped by texture: fewest GPU draws
    BackToFront,  // drawn at End, layerDepth 1 first, 0 last
    FrontToBack,  // drawn at End, layerDepth 0 first, 1 last
};

enum class SpriteEffects : unsigned {
    None = 0,
    FlipHorizontally = 1,
    FlipVertically = 2,
};

constexpr SpriteEffects operator|(SpriteEffects a, SpriteEffects b) {
    return static_cast<SpriteEffects>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr bool HasFlag(SpriteEffects value, SpriteEffects flag) {
    return (static_cast<unsigned>(value) & static_cast<unsigned>(flag)) != 0;
}

// Collects sprites between Begin and End and draws them with as few GPU draws as texture
// changes allow. Coordinates are pixels of the current target, (0, 0) top left, y down,
// then put through the transform given to Begin.
//
// Can be constructed before Run, e.g. as a member of the game class; GPU resources are
// created on first use. Begin and End only work inside Draw.
class SpriteBatch {
public:
    explicit SpriteBatch(GraphicsDevice& graphics);
    ~SpriteBatch();
    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    void Begin(SpriteSortMode sortMode = SpriteSortMode::Deferred, BlendState blend = BlendState::AlphaBlend,
               SamplerState sampler = SamplerState::LinearClamp, const Matrix& transform = Matrix::Identity());
    void End();

    // color tints the texture and is premultiplied like it. Color::White draws the texture
    // unchanged; Color::White * 0.5f draws it half transparent.
    //
    // source picks part of the texture; nullopt is all of it. origin is the point, in source
    // pixels, that sits at position and that rotation (radians, clockwise) turns about.
    // Flipping mirrors the texture within the sprite; it does not move the origin.
    // layerDepth, 0 to 1, only matters to the BackToFront and FrontToBack sort modes.
    void Draw(const Texture2D& texture, Vector2 position, Color color);
    void Draw(const Texture2D& texture, Vector2 position, std::optional<Rectangle> source, Color color);
    void Draw(const Texture2D& texture, Vector2 position, std::optional<Rectangle> source, Color color,
              float rotation, Vector2 origin, float scale, SpriteEffects effects, float layerDepth);
    void Draw(const Texture2D& texture, Vector2 position, std::optional<Rectangle> source, Color color,
              float rotation, Vector2 origin, Vector2 scale, SpriteEffects effects, float layerDepth);

    // The sprite is stretched to destination. origin is in source pixels and scales with it.
    void Draw(const Texture2D& texture, Rectangle destination, Color color);
    void Draw(const Texture2D& texture, Rectangle destination, std::optional<Rectangle> source, Color color);
    void Draw(const Texture2D& texture, Rectangle destination, std::optional<Rectangle> source, Color color,
              float rotation, Vector2 origin, SpriteEffects effects, float layerDepth);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace coldx
