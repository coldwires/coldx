#pragma once

#include <functional>
#include <string_view>

#include <coldx/core/rectangle.h>
#include <coldx/core/sprite_font.h>

namespace coldx::detail {

struct FontAccess {
    // Calls draw for every visible glyph of text, in order, with its rectangle in the atlas
    // and the top-left corner it is drawn at, relative to the text's top-left, unscaled.
    // Spaces and other empty glyphs are skipped.
    static void ForEachGlyph(const SpriteFont& font, std::string_view text,
                             const std::function<void(const Rectangle& source, float x, float y)>& draw);
};

} // namespace coldx::detail
