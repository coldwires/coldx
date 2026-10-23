// Effect: custom fragment shaders in SpriteBatch. The example effects and a test effect
// that uses every parameter type, each checked against pixels computed here.

#include <coldx/core/effect_format.h>

#include <cmath>
#include <fstream>

#include "headless.h"

#include "compiled/flash.h"
#include "compiled/grayscale.h"
#include "compiled/wave.h"
#include "effects/compiled/params.h"

using namespace coldx;
using namespace coldx_test;

namespace {

const Color R(255, 0, 0);
const Color G(0, 255, 0);
const Color B(0, 0, 255);
const Color W(255, 255, 255);
const Color Black = Color::Black;

float K(int n) {
    return n / 255.0f;
}

template <std::size_t N>
Effect Load(Scene& s, const unsigned char (&bytes)[N], const char* name) {
    Effect e = Effect::FromMemory(s.Graphics(), bytes, N, name);
    CHECK(!e.IsEmpty());
    return e;
}

std::uint8_t Grey(Color c) {
    return static_cast<std::uint8_t>(std::lround(c.r * 0.299 + c.g * 0.587 + c.b * 0.114));
}

void Block(const Frame& f, int x, int y, int w, int h, Color c, int tolerance = 0) {
    for (int py = y; py < y + h; ++py)
        for (int px = x; px < x + w; ++px) CheckPixel(f, px, py, c, tolerance);
}

// Grayscale at 1 and 0, the built-in shader after it in the same frame, and when values
// are read: at End for Deferred, at each Draw for Immediate.
void Grayscale() {
    const Color texels[3] = {Color(200, 40, 10), Color(30, 180, 90), Color(10, 20, 250)};
    Scene scene(64, 16);
    Effect gray;  // after the scene: released before the game's device goes
    scene.load = [&](Scene& s) {
        s.AddTexture(3, 1, {texels[0], texels[1], texels[2]});
        gray = Load(s, effects::grayscale, "grayscale");
        CHECK(gray.Has("Amount"));
        CHECK(!gray.Has("Nope"));
    };
    scene.draw = [&](Scene& s) {
        SpriteBatch& b = s.batch;
        const Texture2D& t = s.textures[0];
        s.Graphics().Clear(Black);
        auto at = [&](float x, float y) {
            b.Draw(t, Vector2(x, y), std::nullopt, Color::White, 0.0f, Vector2(), 4.0f, SpriteEffects::None, 0);
        };
        gray.Set("Amount", 1.0f);
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, gray);
        at(0, 0);
        b.End();
        gray.Set("Amount", 0.0f);
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, gray);
        at(16, 0);
        b.End();
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        at(32, 0);
        b.End();

        // Deferred: both sprites drawn at End, with the value set last.
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, gray);
        gray.Set("Amount", 0.0f);
        at(0, 8);
        gray.Set("Amount", 1.0f);
        at(16, 8);
        b.End();
        // Immediate: each sprite with the value it was drawn with.
        b.Begin(SpriteSortMode::Immediate, BlendState::AlphaBlend, SamplerState::PointClamp, gray);
        gray.Set("Amount", 1.0f);
        at(32, 8);
        gray.Set("Amount", 0.0f);
        at(48, 8);
        b.End();
    };
    const Frame f = RunAndCapture(scene, "effect_grayscale");
    for (int i = 0; i < 3; ++i) {
        const std::uint8_t g = Grey(texels[i]);
        const Color grey(g, g, g);
        Block(f, i * 4, 0, 4, 4, grey, 1);
        Block(f, 16 + i * 4, 0, 4, 4, texels[i]);
        Block(f, 32 + i * 4, 0, 4, 4, texels[i]);
        Block(f, i * 4, 8, 4, 4, grey, 1);
        Block(f, 16 + i * 4, 8, 4, 4, grey, 1);
        Block(f, 32 + i * 4, 8, 4, 4, grey, 1);
        Block(f, 48 + i * 4, 8, 4, 4, texels[i]);
    }
}

// Every parameter type lands where the shader reads it, on every backend, and a second
// texture is sampled. Opaque blending, so each pixel is exactly what the shader returned.
void Parameters() {
    Scene scene(80, 8);
    Effect fx, unset;
    scene.load = [&](Scene& s) {
        s.AddTexture(2, 2, {W, W, W, W});  // 0: the sprite
        s.AddTexture(2, 2, {R, G, B, W});  // 1: the mask
        fx = Load(s, effects::params, "params");
        unset = Load(s, effects::params, "params, no mask");
        for (const char* name : {"A", "B", "C", "D", "E", "M", "Mode", "Invert", "Mask"}) CHECK(fx.Has(name));
        fx.Set("A", K(51));
        fx.Set("B", Vector2(K(10), K(102)));
        fx.Set("C", K(20), K(30), K(153));
        fx.Set("D", 1.0f);
        fx.Set("E", Color(70, 80, 90, 255));
        Matrix m = Matrix::CreateTranslation(K(40), K(50), K(60));
        fx.Set("M", m);
        fx.Set("Mask", s.textures[1]);
        // Wrong names and types are ignored, leaving what was set.
        fx.Set("Nope", 1.0f);
        fx.Set("A", Vector2(1, 1));
        fx.Set("Mode", 0.5f);
    };
    scene.draw = [&](Scene& s) {
        SpriteBatch& b = s.batch;
        const Texture2D& sprite = s.textures[0];
        s.Graphics().Clear(Black);
        auto at = [&](float x) {
            b.Draw(sprite, Vector2(x, 0), std::nullopt, Color::White, 0.0f, Vector2(), 4.0f, SpriteEffects::None, 0);
        };
        b.Begin(SpriteSortMode::Immediate, BlendState::Opaque, SamplerState::PointClamp, fx);
        for (int mode = 0; mode <= 4; ++mode) {
            fx.Set("Mode", mode);
            at(mode * 8.0f);
        }
        fx.Set("Mode", 0);
        fx.Set("Invert", true);
        at(48);
        fx.Set("Invert", false);
        b.End();
        // No mask set: the sprite's own (white) texture stands in.
        unset.Set("Mode", 3);
        b.Begin(SpriteSortMode::Deferred, BlendState::Opaque, SamplerState::PointClamp, unset);
        at(64);
        b.End();
    };
    const Frame f = RunAndCapture(scene, "effect_parameters");
    Block(f, 0, 0, 8, 8, Color(51, 102, 153, 255));
    Block(f, 8, 0, 8, 8, Color(70, 80, 90, 255));
    Block(f, 16, 0, 8, 8, Color(40, 50, 60, 255));
    Block(f, 24, 0, 4, 4, R);
    Block(f, 28, 0, 4, 4, G);
    Block(f, 24, 4, 4, 4, B);
    Block(f, 28, 4, 4, 4, W);
    Block(f, 32, 0, 8, 8, Color(10, 20, 30, 255));
    Block(f, 48, 0, 8, 8, Color(204, 153, 102, 255));
    Block(f, 64, 0, 8, 8, W);
}

// Flash washes towards its colour and keeps premultiplied edges.
void Flash() {
    const Color half(100, 0, 0, 128);  // premultiplied
    Scene scene(32, 8);
    Effect flash;
    scene.load = [&](Scene& s) {
        s.AddTexture(1, 1, {R});
        s.AddTexture(1, 1, {half});
        flash = Load(s, effects::flash, "flash");
        flash.Set("Color", Color::White);
    };
    scene.draw = [&](Scene& s) {
        SpriteBatch& b = s.batch;
        s.Graphics().Clear(Black);
        flash.Set("Amount", 0.5f);
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, flash);
        b.Draw(s.textures[0], Rectangle(0, 0, 8, 8), Color::White);
        b.End();
        flash.Set("Amount", 1.0f);
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, flash);
        b.Draw(s.textures[1], Rectangle(16, 0, 8, 8), Color::White);
        b.End();
    };
    const Frame f = RunAndCapture(scene, "effect_flash");
    Block(f, 0, 0, 8, 8, Color(255, 128, 128, 255), 1);
    Block(f, 16, 0, 8, 8, Over(Color(128, 128, 128, 128), Black), 1);
}

// Wave moves pixels, and never past the sprite's source rectangle: a red tile beside a
// green one in the same texture shows no green, however far the wave reaches.
void Wave() {
    Scene scene(48, 16);
    Effect wave;
    scene.load = [&](Scene& s) {
        s.AddTexture(4, 2, {R, R, G, G, R, R, G, G});
        wave = Load(s, effects::wave, "wave");
    };
    scene.draw = [&](Scene& s) {
        SpriteBatch& b = s.batch;
        const Texture2D& t = s.textures[0];
        s.Graphics().Clear(Black);
        wave.Set("Time", 0.1f);
        wave.Set("Frequency", 0.7f);
        wave.Set("Amplitude", 3.0f);
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::LinearClamp, wave);
        b.Draw(t, Vector2(0, 0), Rectangle(0, 0, 2, 2), Color::White, 0.0f, Vector2(), 8.0f, SpriteEffects::None, 0);
        b.End();
        wave.Set("Amplitude", 1.0f);
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, wave);
        b.Draw(t, Vector2(24, 0), std::nullopt, Color::White, 0.0f, Vector2(), 4.0f, SpriteEffects::None, 0);
        b.End();
    };
    const Frame f = RunAndCapture(scene, "effect_wave");
    Block(f, 0, 0, 16, 16, R);
    // The whole texture, 16 pixels wide: where red turns green differs from row to row.
    int first = -1, last = -1;
    bool moved = false;
    for (int y = 0; y < 8; ++y) {
        int edge = 0;
        while (edge < 16 && f.At(24 + edge, y).r > 128) ++edge;
        if (first < 0) first = edge;
        if (edge != first) moved = true;
        last = edge;
    }
    std::printf("wave: red/green edge at %d in row 0, %d in row 7\n", first, last);
    CHECK(moved);
}

// Bytes that are not an effect give an empty Effect, and an empty Effect draws as the
// built-in shader.
void BadData() {
    Scene scene(16, 8);
    scene.load = [](Scene& s) {
        s.AddTexture(1, 1, {G});
        const char junk[] = "not an effect at all, just some words";
        CHECK(Effect::FromMemory(s.Graphics(), junk, sizeof(junk), "junk").IsEmpty());
        CHECK(Effect::FromMemory(s.Graphics(), effects::grayscale, sizeof(effects::grayscale) - 10, "short").IsEmpty());
        CHECK(Effect::FromMemory(s.Graphics(), nullptr, 0, "null").IsEmpty());
        // Every truncation is refused, and damage to the tables is refused or stays in
        // bounds (ASan runs this too).
        std::vector<std::uint8_t> bytes(std::begin(effects::params), std::end(effects::params));
        effect_format::Compiled c;
        std::string error;
        CHECK(effect_format::Decode(bytes.data(), bytes.size(), c, error) && c.parameters.size() == 8 && c.textures.size() == 2);
        CHECK(effect_format::Encode(c) == bytes);
        int refused = 0;
        for (std::size_t n = 0; n < bytes.size(); ++n) refused += effect_format::Decode(bytes.data(), n, c, error) ? 0 : 1;
        CHECK(refused == static_cast<int>(bytes.size()));
        for (std::size_t at = 0; at < 160; ++at) {
            for (int v : {0x00, 0x01, 0x7f, 0xff}) {
                std::vector<std::uint8_t> damaged = bytes;
                damaged[at] = static_cast<std::uint8_t>(v);
                if (effect_format::Decode(damaged.data(), damaged.size(), c, error))
                    for (const auto& p : c.parameters) CHECK(p.offset + effect_format::SizeOf(p.type) <= c.uniformSize);
            }
        }
        Effect none;
        none.Set("Amount", 1.0f);  // ignored, no crash
        CHECK(!none.Has("Amount"));
    };
    scene.draw = [](Scene& s) {
        SpriteBatch& b = s.batch;
        s.Graphics().Clear(Black);
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, Effect());
        b.Draw(s.textures[0], Rectangle(0, 0, 8, 8), Color::White);
        b.End();
    };
    const Frame f = RunAndCapture(scene, "effect_bad");
    Block(f, 0, 0, 8, 8, G);
}

// Content().Load<Effect> finds a compiled effect in a package, by name without ".fx".
void FromPackage() {
    const std::string path = (std::filesystem::temp_directory_path() / "coldx_effect_test.rime").string();
    {
        rime::PackageWriter writer(rime::HashName(0, "effect test"));
        CHECK(writer.Add("fx/gray.fx", rime::AssetType::Effect,
                         std::vector<std::uint8_t>(std::begin(effects::grayscale), std::end(effects::grayscale))));
        const std::vector<std::uint8_t> bytes = writer.Finish();
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    {
        Scene scene(16, 8);
        Effect gray;
        scene.load = [&](Scene& s) {
            s.AddTexture(1, 1, {R});
            CHECK(s.Content().Mount(path));
            gray = s.Content().Load<Effect>("fx/gray");
            CHECK(!gray.IsEmpty());
            CHECK(gray.Has("Amount"));
            gray.Set("Amount", 1.0f);
        };
        scene.draw = [&](Scene& s) {
            SpriteBatch& b = s.batch;
            s.Graphics().Clear(Black);
            b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, gray);
            b.Draw(s.textures[0], Rectangle(0, 0, 8, 8), Color::White);
            b.End();
        };
        const Frame f = RunAndCapture(scene, "effect_package");
        Block(f, 0, 0, 8, 8, Color(Grey(R), Grey(R), Grey(R)), 1);
    }
    // The scene, which has the package open, is gone first.
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace

int main() {
    Grayscale();
    Parameters();
    Flash();
    Wave();
    BadData();
    FromPackage();
    return CheckResult();
}
