#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include <coldx/core/color.h>
#include <coldx/core/effect.h>
#include <coldx/core/graphics_states.h>
#include <coldx/core/matrix.h>
#include <coldx/core/rectangle.h>
#include <coldx/core/sprite_font.h>
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
// With the clamp samplers a sprite samples only its own source rectangle, at any scale or
// position: atlas tiles need no padding against their neighbours. The wrap samplers tile
// past the rectangle, as they should.
//
// Can be constructed before Run, e.g. as a member of the game class. GPU resources are
// created on first use. Begin and End only work inside Draw.
class SpriteBatch {
public:
    explicit SpriteBatch(GraphicsDevice& graphics);
    ~SpriteBatch();
    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    // SamplerState::Default is PointClamp in a pixel-art game (GameSettings::pixelArt),
    // LinearClamp otherwise. An empty effect draws with the built-in shader. A custom one
    // reads its parameters when the batch draws (see Effect). XNA's order: the effect comes
    // before the transform.
    void Begin(SpriteSortMode sortMode = SpriteSortMode::Deferred, BlendState blend = BlendState::AlphaBlend,
               SamplerState sampler = SamplerState::Default, const Effect& effect = Effect(),
               const Matrix& transform = Matrix::Identity());
    void End();

    // Pixel art: with snapping on, every unrotated sprite and every glyph is moved to the
    // nearest whole pixel before drawing, so a sprite moving at fractional speeds steps
    // cleanly instead of shimmering, and its texels map 1:1 (or 2:1, 3:1) onto pixels.
    // Positions are snapped in sprite coordinates, before the Begin transform. Keep a camera
    // transform on whole pixels too. Rotated sprites are left alone. Off by default, as in
    // XNA, on in a pixel-art game (GameSettings::pixelArt). Once set it lasts until changed.
    void SetPixelSnap(bool snap);
    bool PixelSnap() const;

    // color tints the texture and is premultiplied like it. Color::White draws the texture
    // unchanged. Color::White * 0.5f draws it half transparent.
    //
    // source picks part of the texture, and nullopt is all of it. origin is the point, in source
    // pixels, that sits at position and that rotation (radians, clockwise) turns about.
    // Flipping mirrors the texture within the sprite. It does not move the origin.
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

    // UTF-8 text with its top-left at position. origin, rotation, scale and depth work as for
    // Draw, applied to the whole string. Flipping mirrors the whole string.
    void DrawString(const SpriteFont& font, std::string_view text, Vector2 position, Color color);
    void DrawString(const SpriteFont& font, std::string_view text, Vector2 position, Color color, float rotation,
                    Vector2 origin, float scale, SpriteEffects effects, float layerDepth);
    void DrawString(const SpriteFont& font, std::string_view text, Vector2 position, Color color, float rotation,
                    Vector2 origin, Vector2 scale, SpriteEffects effects, float layerDepth);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace coldx
