#pragma once

#include <memory>
#include <optional>

#include <coldx/core/color.h>
#include <coldx/core/rectangle.h>
#include <coldx/core/texture2d.h>
#include <coldx/core/vector2.h>

namespace coldx {

class GraphicsDevice;

// Collects sprites between Begin and End and draws them with as few GPU draws as the
// texture changes allow. Coordinates are back buffer pixels, (0, 0) at the top left.
//
// Can be constructed before Run, e.g. as a member of the game class; GPU resources are
// created on first use. Begin and End only work inside Draw.
class SpriteBatch {
public:
    explicit SpriteBatch(GraphicsDevice& graphics);
    ~SpriteBatch();
    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    // Alpha blending for premultiplied colors, linear filtering, clamped edges.
    void Begin();
    void End();

    // color tints the texture and is premultiplied like it. Color::White draws the texture
    // unchanged; Color::White * 0.5f draws it half transparent.
    void Draw(const Texture2D& texture, Vector2 position, Color color);
    void Draw(const Texture2D& texture, Vector2 position, std::optional<Rectangle> source, Color color);
    void Draw(const Texture2D& texture, Rectangle destination, Color color);
    void Draw(const Texture2D& texture, Rectangle destination, std::optional<Rectangle> source, Color color);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace coldx
