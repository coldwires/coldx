#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <coldx/core/texture2d.h>
#include <coldx/core/vector2.h>

namespace coldx {

class GraphicsDevice;

namespace detail {
struct FontAccess;
}

// A font baked from a TrueType file into a texture atlas at one pixel size: printable ASCII
// (32-126) and Latin-1 (160-255). Draw it with SpriteBatch::DrawString. Text is UTF-8.
// A handle: copies share one bake. Empty on a failed load.
//
// Glyphs land on whole pixels at scale 1, so text drawn at a whole-pixel position with
// SamplerState::PointClamp is exactly the rasterized font. A different size is a new bake,
// not a scale: scaling a bake blurs or blocks it.
class SpriteFont {
public:
    SpriteFont();
    ~SpriteFont();
    SpriteFont(const SpriteFont&);
    SpriteFont& operator=(const SpriteFont&);
    SpriteFont(SpriteFont&&) noexcept;
    SpriteFont& operator=(SpriteFont&&) noexcept;

    // pixelHeight is ascender to descender. A relative path is taken from the executable's
    // folder. Create from Initialize or LoadContent on.
    static SpriteFont FromFile(GraphicsDevice& graphics, const std::string& path, float pixelHeight);
    static SpriteFont FromMemory(GraphicsDevice& graphics, const void* data, std::size_t size, float pixelHeight);

    // Width of the widest line and height of all lines. Newlines start a line; '\r' is ignored.
    Vector2 MeasureString(std::string_view text) const;

    // Distance between baselines. Defaults to the font's ascender, descender and line gap.
    float LineSpacing() const;
    void SetLineSpacing(float spacing);

    // Spacing, line spacing and the default character belong to the bake, so a change
    // shows in every copy.
    // Extra space after every character. 0 by default.
    float Spacing() const;
    void SetSpacing(float spacing);

    // Drawn in place of a character the font does not have. '?' by default; nullopt skips
    // such characters instead.
    std::optional<char32_t> DefaultCharacter() const;
    void SetDefaultCharacter(std::optional<char32_t> character);

    bool HasCharacter(char32_t character) const;
    float PixelHeight() const;
    // The atlas the glyphs are drawn from.
    const Texture2D& Texture() const;
    bool IsEmpty() const;

private:
    friend struct detail::FontAccess;
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace coldx
