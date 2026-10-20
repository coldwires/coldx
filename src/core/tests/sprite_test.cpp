// SpriteBatch states and geometry, and render targets. Every expected pixel is computed
// from the texels and the blend equation; opaque results must match exactly.

#include "headless.h"

using namespace coldx;
using namespace coldx_test;

namespace {

const Color R(255, 0, 0);
const Color G(0, 255, 0);
const Color B(0, 0, 255);
const Color W(255, 255, 255);
const Color Black = Color::Black;

std::vector<Color> Solid(Color c, int count) {
    return std::vector<Color>(static_cast<size_t>(count), c);
}

void Block(const Frame& f, int x, int y, int w, int h, Color c) {
    for (int py = y; py < y + h; ++py)
        for (int px = x; px < x + w; ++px) CheckPixel(f, px, py, c);
}

void Geometry() {
    Scene scene(64, 16);
    scene.load = [](Scene& s) {
        s.AddTexture(2, 2, {R, G, B, W});  // 0
        s.AddTexture(2, 1, {R, G});        // 1
    };
    scene.draw = [](Scene& s) {
        const Texture2D& quad = s.textures[0];
        const Texture2D& pair = s.textures[1];
        SpriteBatch& b = s.batch;
        s.Graphics().Clear(Black);

        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        b.Draw(quad, Rectangle(0, 0, 8, 8), Color::White);
        b.Draw(quad, Vector2(20, 0), Rectangle(0, 0, 4, 2), Color::White);  // past the edge: clamped
        b.End();

        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::LinearClamp);
        b.Draw(quad, Rectangle(10, 0, 8, 8), Color::White);
        b.End();

        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointWrap);
        b.Draw(quad, Vector2(20, 4), Rectangle(0, 0, 4, 2), Color::White);
        b.End();
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::LinearWrap);
        b.Draw(quad, Vector2(26, 4), Rectangle(0, 0, 4, 2), Color::White);
        b.End();

        // Scale by 2, then move right 32: the pair lands on x 32..35, y 0..1.
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp,
                Matrix::CreateScale(2.0f) * Matrix::CreateTranslation(32.0f, 0.0f));
        b.Draw(pair, Vector2(0, 0), Color::White);
        b.End();

        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        // A quarter turn clockwise about the top-left corner: x runs down from (40, 8).
        b.Draw(pair, Vector2(41, 8), std::nullopt, Color::White, MathHelper::PiOver2, Vector2(), 1.0f,
               SpriteEffects::None, 0.0f);
        b.Draw(pair, Vector2(44, 0), std::nullopt, Color::White, 0.0f, Vector2(), 1.0f,
               SpriteEffects::FlipHorizontally, 0.0f);
        b.Draw(quad, Vector2(48, 0), std::nullopt, Color::White, 0.0f, Vector2(), 1.0f,
               SpriteEffects::FlipVertically, 0.0f);
        b.Draw(quad, Vector2(52, 0), std::nullopt, Color::White, 0.0f, Vector2(), 1.0f,
               SpriteEffects::FlipHorizontally | SpriteEffects::FlipVertically, 0.0f);
        // Origin (1, 0) at (58, 12), scale 2: spans x 56..59, y 12..13.
        b.Draw(pair, Vector2(58, 12), std::nullopt, Color::White, 0.0f, Vector2(1, 0), 2.0f,
               SpriteEffects::None, 0.0f);
        b.End();
    };
    const Frame f = RunAndCapture(scene, "sprite_geometry");

    // PointClamp at 4x: four solid 4x4 blocks.
    Block(f, 0, 0, 4, 4, R);
    Block(f, 4, 0, 4, 4, G);
    Block(f, 0, 4, 4, 4, B);
    Block(f, 4, 4, 4, 4, W);
    // Clamp repeats the edge texels past the texture: columns 2 and 3 copy column 1.
    CheckPixel(f, 22, 0, G);
    CheckPixel(f, 23, 1, W);

    // LinearClamp: corners clamp to the texel, the middle is a blend of all four.
    CheckPixel(f, 10, 0, R);
    CheckPixel(f, 17, 7, W);
    const Color mid = f.At(13, 3);
    CHECK(mid.r > 0 && mid.g > 0 && mid.b > 0 && !(mid == R) && !(mid == W));

    // Wrap repeats: columns 2 and 3 copy columns 0 and 1. 1:1 linear sampling hits texel
    // centers, so LinearWrap is exact too.
    for (int x0 : {20, 26}) {
        CheckPixel(f, x0 + 0, 4, R);
        CheckPixel(f, x0 + 1, 4, G);
        CheckPixel(f, x0 + 2, 4, R);
        CheckPixel(f, x0 + 3, 5, W);
    }

    Block(f, 32, 0, 2, 2, R);
    Block(f, 34, 0, 2, 2, G);
    CheckPixel(f, 36, 0, Black);

    // Rotation: texel 0 at (40, 8), texel 1 at (40, 9); nothing right of the position.
    CheckPixel(f, 40, 8, R);
    CheckPixel(f, 40, 9, G);
    CheckPixel(f, 41, 8, Black);

    CheckPixel(f, 44, 0, G);
    CheckPixel(f, 45, 0, R);
    CheckPixel(f, 48, 0, B);
    CheckPixel(f, 48, 1, R);
    CheckPixel(f, 49, 0, W);
    CheckPixel(f, 52, 0, W);
    CheckPixel(f, 53, 0, B);
    CheckPixel(f, 52, 1, G);
    CheckPixel(f, 53, 1, R);

    Block(f, 56, 12, 2, 2, R);
    Block(f, 58, 12, 2, 2, G);
    CheckPixel(f, 55, 12, Black);
    CheckPixel(f, 60, 12, Black);
}

// Each blend state's equation, applied to one texel over a blue background.
void Blends() {
    const Color background(0, 0, 100);
    const Color premultiplied(128, 0, 0, 128);
    const Color straight(255, 0, 0, 128);

    Scene scene(16, 4);
    scene.load = [&](Scene& s) {
        s.AddTexture(1, 1, {premultiplied});
        s.AddTexture(1, 1, {straight});
    };
    scene.draw = [&](Scene& s) {
        s.Graphics().Clear(background);
        const BlendState states[] = {BlendState::AlphaBlend, BlendState::Additive, BlendState::NonPremultiplied,
                                     BlendState::Opaque};
        for (int i = 0; i < 4; ++i) {
            s.batch.Begin(SpriteSortMode::Deferred, states[i], SamplerState::PointClamp);
            const Texture2D& t = states[i] == BlendState::NonPremultiplied ? s.textures[1] : s.textures[0];
            s.batch.Draw(t, Vector2(static_cast<float>(i * 2), 0), Color::White);
            s.batch.End();
        }
    };
    const Frame f = RunAndCapture(scene, "sprite_blend");

    auto blend = [](Color src, Color dst, bool srcTimesAlpha, bool dstTimesInvAlpha) {
        const float a = src.a / 255.0f;
        auto channel = [&](std::uint8_t s, std::uint8_t d) {
            const float v = s * (srcTimesAlpha ? a : 1.0f) + d * (dstTimesInvAlpha ? 1.0f - a : 1.0f) + 0.5f;
            return static_cast<std::uint8_t>(v > 255.0f ? 255.0f : v);
        };
        return Color(channel(src.r, dst.r), channel(src.g, dst.g), channel(src.b, dst.b), channel(src.a, dst.a));
    };
    CheckPixel(f, 0, 0, blend(premultiplied, background, false, true), 1);  // AlphaBlend
    CheckPixel(f, 2, 0, blend(premultiplied, background, true, false), 1);  // Additive
    CheckPixel(f, 4, 0, blend(straight, background, true, true), 1);       // NonPremultiplied
    CheckPixel(f, 6, 0, premultiplied);                                     // Opaque
    CheckPixel(f, 8, 0, background);
}

void SortModes() {
    Scene scene(64, 16);
    scene.load = [](Scene& s) {
        s.AddTexture(2, 2, Solid(R, 4));
        s.AddTexture(2, 2, Solid(G, 4));
        s.AddTexture(2, 2, Solid(B, 4));
        s.targets.push_back(RenderTarget2D::Create(s.Graphics(), 4, 4));
        s.targets.push_back(RenderTarget2D::Create(s.Graphics(), 8, 16));
    };
    scene.draw = [](Scene& s) {
        const Texture2D& red = s.textures[0];
        const Texture2D& green = s.textures[1];
        const Texture2D& blue = s.textures[2];
        SpriteBatch& b = s.batch;
        GraphicsDevice& g = s.Graphics();
        auto draw = [&](const Texture2D& t, float x, float y, float depth) {
            b.Draw(t, Vector2(x, y), std::nullopt, Color::White, 0.0f, Vector2(), 1.0f, SpriteEffects::None, depth);
        };
        g.SetRenderTarget(&s.targets[0]);
        g.Clear(Color::Transparent);
        g.SetRenderTarget(&s.targets[1]);
        g.Clear(Color::Transparent);
        g.SetRenderTarget(nullptr);
        g.Clear(Black);

        const std::pair<SpriteSortMode, float> layered[] = {
            {SpriteSortMode::Deferred, 0.0f},
            {SpriteSortMode::BackToFront, 4.0f},
            {SpriteSortMode::FrontToBack, 8.0f},
        };
        for (auto [mode, x] : layered) {
            b.Begin(mode);
            draw(red, x, 0, 0.0f);
            draw(green, x, 0, 1.0f);
            draw(blue, x, 0, 0.5f);
            b.End();
        }
        b.Begin(SpriteSortMode::Texture);
        draw(red, 12, 0, 0);
        draw(green, 14, 0, 0);
        draw(red, 16, 0, 0);
        draw(blue, 18, 0, 0);
        b.End();

        // Immediate draws at each call, so switching targets mid-batch splits the sprites.
        b.Begin(SpriteSortMode::Immediate);
        draw(red, 0, 8, 0);
        g.SetRenderTarget(&s.targets[0]);
        draw(green, 0, 0, 0);
        b.End();
        g.SetRenderTarget(nullptr);

        // Deferred draws at End, into whatever target is current then.
        b.Begin(SpriteSortMode::Deferred);
        draw(blue, 4, 8, 0);
        g.SetRenderTarget(&s.targets[1]);
        b.End();
        g.SetRenderTarget(nullptr);

        b.Begin();
        b.Draw(s.targets[0], Vector2(20, 8), Color::White);
        b.Draw(s.targets[1], Vector2(32, 0), Color::White);
        b.End();
    };
    const Frame f = RunAndCapture(scene, "sprite_sort");

    Block(f, 0, 0, 2, 2, B);  // Deferred: last drawn on top
    Block(f, 4, 0, 2, 2, R);  // BackToFront: depth 0 last
    Block(f, 8, 0, 2, 2, G);  // FrontToBack: depth 1 last
    Block(f, 12, 0, 2, 2, R);  // Texture: grouping loses nothing
    Block(f, 14, 0, 2, 2, G);
    Block(f, 16, 0, 2, 2, R);
    Block(f, 18, 0, 2, 2, B);

    Block(f, 0, 8, 2, 2, R);      // Immediate: drawn before the switch
    Block(f, 20, 8, 2, 2, G);     // ... and after it, into the target
    Block(f, 4, 8, 2, 2, Black);  // Deferred: not in the back buffer
    Block(f, 36, 8, 2, 2, B);     // ... but in the target current at End
}

void RenderTargets() {
    Scene scene(32, 8);
    scene.load = [](Scene& s) {
        s.AddTexture(2, 2, Solid(R, 4));
        s.targets.push_back(RenderTarget2D::Create(s.Graphics(), 4, 4));
        s.targets.push_back(RenderTarget2D::Create(s.Graphics(), 4, 4));  // never drawn into
        CHECK(s.targets[0].Width() == 4 && s.targets[0].Height() == 4);
    };
    scene.draw = [](Scene& s) {
        GraphicsDevice& g = s.Graphics();
        g.Clear(Black);
        g.SetRenderTarget(&s.targets[0]);
        g.Clear(G);
        s.batch.Begin();
        s.batch.Draw(s.textures[0], Vector2(1, 1), Color::White);
        s.batch.End();
        g.SetRenderTarget(nullptr);

        s.batch.Begin();
        s.batch.Draw(s.targets[0], Vector2(8, 0), Color::White);
        s.batch.Draw(s.targets[1], Vector2(16, 0), Color::White);
        s.batch.End();
    };
    const Frame f = RunAndCapture(scene, "render_target");

    CheckPixel(f, 8, 0, G);
    Block(f, 9, 1, 2, 2, R);
    CheckPixel(f, 11, 3, G);
    Block(f, 0, 0, 4, 4, Black);   // drawing into the target left the back buffer alone
    Block(f, 16, 0, 4, 4, Black);  // a fresh target is transparent
}

} // namespace

int main() {
    Geometry();
    Blends();
    SortModes();
    RenderTargets();
    return CheckResult();
}
