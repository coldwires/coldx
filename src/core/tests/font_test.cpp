// SpriteFont layout and drawing. Expected glyphs are rasterized here, straight from
// stb_truetype, and placed with the font's own metrics, so the test checks coldx's baking
// and layout against the font rather than against itself.

#include <cmath>
#include <fstream>
#include <iterator>

#include <stb/stb_truetype.h>

#include "headless.h"

using namespace coldx;
using namespace coldx_test;

namespace {

constexpr float Size = 32.0f;

// The font read directly, for reference values.
struct Reference {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    float scale = 0.0f;
    int baseline = 0;
    float lineSpacing = 0.0f;

    Reference() {
        std::ifstream file(COLDX_TEST_FONT, std::ios::binary);
        data.assign(std::istreambuf_iterator<char>(file), {});
        CHECK(!data.empty());
        CHECK(stbtt_InitFont(&info, data.data(), 0));
        scale = stbtt_ScaleForPixelHeight(&info, Size);
        int ascent = 0, descent = 0, gap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
        baseline = static_cast<int>(std::lround(ascent * scale));
        lineSpacing = std::round((ascent - descent + gap) * scale);
    }

    float Advance(int cp) const {
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&info, cp, &advance, &bearing);
        return advance * scale;
    }

    float Kern(int a, int b) const { return stbtt_GetCodepointKernAdvance(&info, a, b) * scale; }

    // Coverage of one line of ASCII text laid out from (0, 0), as a sparse map of pixels.
    struct Pixel {
        int x, y;
        std::uint8_t coverage;
    };
    std::vector<Pixel> Rasterize(const char* text) const {
        std::vector<Pixel> out;
        float pen = 0.0f;
        int previous = 0;
        for (const char* c = text; *c; ++c) {
            const int cp = static_cast<unsigned char>(*c);
            if (previous) pen += Kern(previous, cp);
            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&info, scale, scale, cp, &w, &h, &xoff, &yoff);
            const int left = static_cast<int>(std::round(pen)) + xoff;
            const int top = baseline + yoff;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) out.push_back({left + x, top + y, bitmap[y * w + x]});
            stbtt_FreeBitmap(bitmap, nullptr);
            pen += Advance(cp);
            previous = cp;
        }
        return out;
    }
};

bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-3f;
}

void Metrics(const Reference& ref) {
    Scene scene(16, 16);
    scene.load = [&](Scene& s) {
        SpriteFont font = SpriteFont::FromFile(s.Graphics(), COLDX_TEST_FONT, Size);
        CHECK(!font.IsEmpty());
        CHECK(font.PixelHeight() == Size);
        CHECK(font.LineSpacing() == ref.lineSpacing);

        CHECK(Near(font.MeasureString("H").x, ref.Advance('H')));
        CHECK(font.MeasureString("H").y == ref.lineSpacing);
        const float av = ref.Advance('A') + ref.Kern('A', 'V') + ref.Advance('V');
        CHECK(Near(font.MeasureString("AV").x, av));
        CHECK(ref.Kern('A', 'V') != 0.0f);  // so the line above really tests kerning

        const Vector2 lines = font.MeasureString("A\nAV\r\n");
        CHECK(Near(lines.x, av));
        CHECK(lines.y == 3 * ref.lineSpacing);
        CHECK(font.MeasureString("").y == 0.0f);

        // Latin-1 is baked. Beyond it the default character stands in, as does broken UTF-8.
        CHECK(font.HasCharacter(char32_t{0xE9}) && !font.HasCharacter(char32_t{0x100}));
        CHECK(Near(font.MeasureString("\xc3\xa9").x, ref.Advance(0xe9)));
        CHECK(Near(font.MeasureString("\xc4\x80").x, ref.Advance('?')));
        CHECK(Near(font.MeasureString("\xff").x, ref.Advance('?')));
        font.SetDefaultCharacter(std::nullopt);
        CHECK(font.MeasureString("\xc4\x80").x == 0.0f);

        font.SetSpacing(2.0f);
        CHECK(Near(font.MeasureString("HH").x, 2 * ref.Advance('H') + ref.Kern('H', 'H') + 4.0f));
        font.SetLineSpacing(50.0f);
        CHECK(font.MeasureString("H\nH").y == 100.0f);
    };
    RunAndCapture(scene, "font_metrics");
}

void Drawing(const Reference& ref) {
    Scene scene(256, 128);
    SpriteFont font;
    scene.load = [&](Scene& s) { font = SpriteFont::FromFile(s.Graphics(), COLDX_TEST_FONT, Size); };
    scene.draw = [&](Scene& s) {
        s.Graphics().Clear(Color::Black);
        s.batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        s.batch.DrawString(font, "Hg", Vector2(4, 4), Color::White);
        s.batch.DrawString(font, "Hg", Vector2(4, 44), Color::Red);
        s.batch.DrawString(font, "L", Vector2(64, 4), Color::White, 0.0f, Vector2(), 2.0f, SpriteEffects::None, 0.0f);
        s.batch.DrawString(font, "L", Vector2(160, 4), Color::White, 0.0f, Vector2(), 1.0f,
                           SpriteEffects::FlipHorizontally, 0.0f);
        s.batch.End();
    };
    const Frame f = RunAndCapture(scene, "font_drawing");
    // Release it while the scene, and so the GPU device, still exists.
    font = SpriteFont();

    const auto white = [](std::uint8_t c) { return Over(Color(c, c, c, c), Color::Black); };
    for (const auto& p : ref.Rasterize("Hg")) {
        CheckPixel(f, 4 + p.x, 4 + p.y, white(p.coverage));
        CheckPixel(f, 4 + p.x, 44 + p.y, Over(Tint(Color(p.coverage, p.coverage, p.coverage, p.coverage), Color::Red), Color::Black));
    }
    // Scale 2 with point sampling: each glyph pixel becomes a 2x2 block.
    for (const auto& p : ref.Rasterize("L"))
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx) CheckPixel(f, 64 + p.x * 2 + dx, 4 + p.y * 2 + dy, white(p.coverage));
    // Flipped: mirrored within the string's measured width, rounded to whole pixels.
    const int width = static_cast<int>(std::lround(ref.Advance('L')));
    for (const auto& p : ref.Rasterize("L")) CheckPixel(f, 160 + (width - 1 - p.x), 4 + p.y, white(p.coverage));
}

// FontSmoothing::Sharp: every pixel of drawn text is either the text colour or the
// background, never between. Smooth has in-between pixels at the edges.
void Sharpness() {
    auto count = [](FontSmoothing smoothing, const char* name) {
        SpriteFont font;
        Scene scene(160, 32);
        scene.load = [&](Scene& s) { font = SpriteFont::FromFile(s.Graphics(), COLDX_TEST_FONT, 16.0f, smoothing); };
        scene.draw = [&](Scene& s) {
            s.Graphics().Clear(Color::Black);
            s.batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
            s.batch.DrawString(font, "Sharp edges: Wg@&8", Vector2(2, 4), Color::White);
            s.batch.End();
        };
        const Frame f = RunAndCapture(scene, name);
        int full = 0, between = 0;
        for (int y = 0; y < f.Height(); ++y)
            for (int x = 0; x < f.Width(); ++x) {
                const int v = f.At(x, y).r;
                full += v == 255 ? 1 : 0;
                between += v > 0 && v < 255 ? 1 : 0;
            }
        std::printf("%s text: %d full pixels, %d in between\n", name, full, between);
        return std::pair{full, between};
    };
    const auto [sharpFull, sharpBetween] = count(FontSmoothing::Sharp, "font_sharp");
    const auto [smoothFull, smoothBetween] = count(FontSmoothing::Smooth, "font_smooth");
    CHECK(sharpFull > 100 && sharpBetween == 0);
    CHECK(smoothBetween > 50);
    (void)smoothFull;
}

} // namespace

int main() {
    const Reference ref;
    Metrics(ref);
    Drawing(ref);
    Sharpness();
    return CheckResult();
}
