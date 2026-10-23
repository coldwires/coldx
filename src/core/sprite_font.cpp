#include <coldx/core/sprite_font.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>
#include <stb/stb_truetype.h>

#include "content_path.h"
#include "font_access.h"

namespace coldx {

namespace {

// Baked ranges: printable ASCII and Latin-1. Everything baked is below 256, so glyphs are a
// flat table indexed by code point.
struct Range {
    int first;
    int count;
};
constexpr Range BakedRanges[] = {{32, 95}, {160, 96}};
constexpr int TableSize = 256;
constexpr int MaxAtlasSize = 4096;

struct Glyph {
    Rectangle source;  // in the atlas; empty for a space
    int xOffset;       // from the pen to the left edge
    int yOffset;       // from the baseline to the top edge, negative above it
    float advance;
};

// Next code point of UTF-8 text; malformed input gives U+FFFD and moves one byte on.
char32_t NextCodePoint(std::string_view text, size_t& i) {
    const auto byte = [&](size_t k) { return static_cast<unsigned char>(text[k]); };
    const unsigned char lead = byte(i);
    if (lead < 0x80) {
        ++i;
        return lead;
    }
    int length = 0;
    char32_t cp = 0;
    if ((lead & 0xE0) == 0xC0) { length = 2; cp = lead & 0x1F; }
    else if ((lead & 0xF0) == 0xE0) { length = 3; cp = lead & 0x0F; }
    else if ((lead & 0xF8) == 0xF0) { length = 4; cp = lead & 0x07; }
    if (length == 0 || i + length > text.size()) {
        ++i;
        return 0xFFFD;
    }
    for (int k = 1; k < length; ++k) {
        const unsigned char next = byte(i + k);
        if ((next & 0xC0) != 0x80) {
            ++i;
            return 0xFFFD;
        }
        cp = (cp << 6) | (next & 0x3F);
    }
    i += length;
    return cp;
}

} // namespace

struct SpriteFont::Impl {
    std::vector<unsigned char> fontData;  // stbtt_fontinfo points into this; kept for kerning
    stbtt_fontinfo info{};
    float scale = 0.0f;
    float pixelHeight = 0.0f;
    int baseline = 0;
    float lineSpacing = 0.0f;
    float spacing = 0.0f;
    std::optional<char32_t> defaultCharacter = U'?';
    std::array<std::optional<Glyph>, TableSize> glyphs;
    Texture2D atlas;

    const Glyph* Find(char32_t cp) const {
        return cp < TableSize && glyphs[cp] ? &*glyphs[cp] : nullptr;
    }

    // Lays out text, calling visit for every character drawn with its glyph and top-left
    // corner. Returns the size MeasureString reports.
    template <class Visit>
    Vector2 Layout(std::string_view text, Visit&& visit) const {
        float penX = 0.0f;
        float lineY = 0.0f;
        float widest = 0.0f;
        int lines = text.empty() ? 0 : 1;
        char32_t previous = 0;
        for (size_t i = 0; i < text.size();) {
            char32_t cp = NextCodePoint(text, i);
            if (cp == U'\r') continue;
            if (cp == U'\n') {
                penX = 0.0f;
                lineY += lineSpacing;
                ++lines;
                previous = 0;
                continue;
            }
            const Glyph* glyph = Find(cp);
            if (!glyph && defaultCharacter) {
                cp = *defaultCharacter;
                glyph = Find(cp);
            }
            if (!glyph) continue;
            if (previous) penX += stbtt_GetCodepointKernAdvance(&info, static_cast<int>(previous), static_cast<int>(cp)) * scale;
            visit(*glyph, std::round(penX) + glyph->xOffset, lineY + baseline + glyph->yOffset);
            penX += glyph->advance + spacing;
            if (penX > widest) widest = penX;
            previous = cp;
        }
        return {widest, lines * lineSpacing};
    }
};

SpriteFont::SpriteFont() = default;
SpriteFont::~SpriteFont() = default;
SpriteFont::SpriteFont(const SpriteFont&) = default;
SpriteFont& SpriteFont::operator=(const SpriteFont&) = default;
SpriteFont::SpriteFont(SpriteFont&&) noexcept = default;
SpriteFont& SpriteFont::operator=(SpriteFont&&) noexcept = default;

SpriteFont SpriteFont::FromFile(GraphicsDevice& graphics, const std::string& path, float pixelHeight, FontSmoothing smoothing) {
    const std::string full = detail::ResolveContentPath(path);
    size_t size = 0;
    void* data = SDL_LoadFile(full.c_str(), &size);
    if (!data) {
        SDL_Log("coldx.core: could not load font %s: %s", full.c_str(), SDL_GetError());
        return {};
    }
    SpriteFont font = FromMemory(graphics, data, size, pixelHeight, smoothing);
    SDL_free(data);
    if (font.IsEmpty()) SDL_Log("coldx.core: font %s was not usable", full.c_str());
    return font;
}

SpriteFont SpriteFont::FromMemory(GraphicsDevice& graphics, const void* data, std::size_t size, float pixelHeight,
                                  FontSmoothing smoothing) {
    if (!data || size == 0 || pixelHeight <= 0.0f) {
        SDL_Log("coldx.core: SpriteFont: no data, or pixel height %.1f", pixelHeight);
        return {};
    }
    auto impl = std::make_shared<Impl>();
    const auto* bytes = static_cast<const unsigned char*>(data);
    impl->fontData.assign(bytes, bytes + size);
    const unsigned char* fontData = impl->fontData.data();
    if (!stbtt_InitFont(&impl->info, fontData, stbtt_GetFontOffsetForIndex(fontData, 0))) {
        SDL_Log("coldx.core: SpriteFont: not a TrueType font");
        return {};
    }
    impl->pixelHeight = pixelHeight;
    impl->scale = stbtt_ScaleForPixelHeight(&impl->info, pixelHeight);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&impl->info, &ascent, &descent, &lineGap);
    impl->baseline = static_cast<int>(std::lround(ascent * impl->scale));
    impl->lineSpacing = std::round((ascent - descent + lineGap) * impl->scale);

    // Pack into the smallest square atlas that holds every glyph.
    std::vector<stbtt_packedchar> packed[std::size(BakedRanges)];
    std::vector<unsigned char> coverage;
    int atlasSize = 128;
    for (;; atlasSize *= 2) {
        if (atlasSize > MaxAtlasSize) {
            SDL_Log("coldx.core: SpriteFont: %.0f px does not fit a %d px atlas", pixelHeight, MaxAtlasSize);
            return {};
        }
        coverage.assign(static_cast<size_t>(atlasSize) * atlasSize, 0);
        stbtt_pack_context context;
        if (!stbtt_PackBegin(&context, coverage.data(), atlasSize, atlasSize, 0, 1, nullptr)) return {};
        stbtt_pack_range ranges[std::size(BakedRanges)]{};
        for (size_t r = 0; r < std::size(BakedRanges); ++r) {
            packed[r].assign(static_cast<size_t>(BakedRanges[r].count), {});
            ranges[r].font_size = pixelHeight;
            ranges[r].first_unicode_codepoint_in_range = BakedRanges[r].first;
            ranges[r].num_chars = BakedRanges[r].count;
            ranges[r].chardata_for_range = packed[r].data();
        }
        const int fitted = stbtt_PackFontRanges(&context, fontData, 0, ranges, static_cast<int>(std::size(ranges)));
        stbtt_PackEnd(&context);
        if (fitted) break;
    }

    for (size_t r = 0; r < std::size(BakedRanges); ++r) {
        for (int k = 0; k < BakedRanges[r].count; ++k) {
            const int cp = BakedRanges[r].first + k;
            // Code points the font lacks still pack, as glyph 0; leave them out so the default
            // character stands in.
            if (stbtt_FindGlyphIndex(&impl->info, cp) == 0) continue;
            const stbtt_packedchar& p = packed[r][static_cast<size_t>(k)];
            Glyph g;
            g.source = Rectangle(p.x0, p.y0, p.x1 - p.x0, p.y1 - p.y0);
            g.xOffset = static_cast<int>(std::lround(p.xoff));
            g.yOffset = static_cast<int>(std::lround(p.yoff));
            g.advance = p.xadvance;
            impl->glyphs[static_cast<size_t>(cp)] = g;
        }
    }

    // White, with coverage as alpha: premultiplied, so tinting multiplies cleanly. Sharp
    // turns each pixel fully on or off at half coverage.
    std::vector<Color> pixels(coverage.size());
    for (size_t i = 0; i < coverage.size(); ++i) {
        std::uint8_t c = coverage[i];
        if (smoothing == FontSmoothing::Sharp) c = c >= 128 ? 255 : 0;
        pixels[i] = Color(c, c, c, c);
    }
    impl->atlas = Texture2D::FromPixels(graphics, atlasSize, atlasSize, pixels.data());
    if (impl->atlas.IsEmpty()) return {};

    SpriteFont font;
    font.impl_ = std::move(impl);
    return font;
}

Vector2 SpriteFont::MeasureString(std::string_view text) const {
    if (!impl_) return {};
    return impl_->Layout(text, [](const Glyph&, float, float) {});
}

float SpriteFont::LineSpacing() const {
    return impl_ ? impl_->lineSpacing : 0.0f;
}

void SpriteFont::SetLineSpacing(float spacing) {
    if (impl_) impl_->lineSpacing = spacing;
}

float SpriteFont::Spacing() const {
    return impl_ ? impl_->spacing : 0.0f;
}

void SpriteFont::SetSpacing(float spacing) {
    if (impl_) impl_->spacing = spacing;
}

std::optional<char32_t> SpriteFont::DefaultCharacter() const {
    return impl_ ? impl_->defaultCharacter : std::nullopt;
}

void SpriteFont::SetDefaultCharacter(std::optional<char32_t> character) {
    if (impl_) impl_->defaultCharacter = character;
}

bool SpriteFont::HasCharacter(char32_t character) const {
    return impl_ && impl_->Find(character);
}

float SpriteFont::PixelHeight() const {
    return impl_ ? impl_->pixelHeight : 0.0f;
}

const Texture2D& SpriteFont::Texture() const {
    static const Texture2D empty;
    return impl_ ? impl_->atlas : empty;
}

bool SpriteFont::IsEmpty() const {
    return !impl_;
}

namespace detail {

void FontAccess::ForEachGlyph(const SpriteFont& font, std::string_view text,
                              const std::function<void(const Rectangle& source, float x, float y)>& draw) {
    if (!font.impl_) return;
    font.impl_->Layout(text, [&](const Glyph& glyph, float x, float y) {
        if (!glyph.source.IsEmpty()) draw(glyph.source, x, y);
    });
}

} // namespace detail
} // namespace coldx
