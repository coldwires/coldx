// The CPU renderer against the GPU, byte for byte. Each case is drawn on every GPU driver
// in COLDX_TEST_GPU_DRIVERS and on the CPU, and the captures must be identical, except that
// a pixel the CPU reports as near a rounding tie (COLDX_CPU_TIES) may differ by one step per
// channel: GPUs blend at slightly less than float precision. Cases go where the two could
// part: fractional positions, scaling, rotation, linear filtering, tints, every blend state
// over noise, render targets, transforms and text.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "headless.h"

using namespace coldx;
using namespace coldx_test;

namespace {

constexpr int W = 160;
constexpr int H = 120;

// 16x16, every texel different, alpha falling to the right, premultiplied.
std::vector<Color> Gradient() {
    std::vector<Color> p;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const int a = 255 - x * 13;
            auto pm = [a](int v) { return static_cast<std::uint8_t>((v * a + 127) / 255); };
            p.push_back(Color(pm(x * 17), pm(y * 17), pm(255 - (x + y) * 7), static_cast<std::uint8_t>(a)));
        }
    return p;
}

// 8x8 checker of four opaque colours, for point sampling edges.
std::vector<Color> Checker() {
    const Color c[4] = {Color(255, 0, 0), Color(0, 255, 0), Color(0, 0, 255), Color(255, 255, 255)};
    std::vector<Color> p;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) p.push_back(c[(x / 2 + y / 2) % 4]);
    return p;
}

class DiffScene : public Scene {
public:
    using Scene::Scene;
    SpriteFont font;
};

struct Case {
    const char* name;
    std::function<void(DiffScene&)> draw;
};

std::uint8_t Hash(int x, int y, std::uint32_t seed) {
    const std::uint32_t h = (static_cast<std::uint32_t>(x) * 73856093u) ^ (static_cast<std::uint32_t>(y) * 19349663u) ^
                            (seed * 83492791u);
    return static_cast<std::uint8_t>(h % 256u);
}

// W x H of unrelated values in every channel, alpha included, so each blend state meets
// thousands of combinations of source, alpha and destination.
std::vector<Color> Noise(std::uint32_t seed) {
    std::vector<Color> p;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            p.push_back(Color(Hash(x, y, seed), Hash(x, y, seed + 1), Hash(x, y, seed + 2), Hash(x, y, seed + 3)));
    return p;
}

std::vector<Case> Cases() {
    const Color back(40, 80, 120, 255);
    auto begin = [](Scene& s, BlendState blend, SamplerState sampler, const Matrix& m = Matrix()) {
        s.batch.Begin(SpriteSortMode::Deferred, blend, sampler, Effect(), m);
    };
    // Noise drawn opaque, then a second noise blended over it.
    auto noise = [](BlendState blend, Color tint) {
        return [blend, tint](DiffScene& s) {
            s.batch.Begin(SpriteSortMode::Deferred, BlendState::Opaque, SamplerState::PointClamp);
            s.batch.Draw(s.textures[3], Vector2(0, 0), Color::White);
            s.batch.End();
            s.batch.Begin(SpriteSortMode::Deferred, blend, SamplerState::PointClamp);
            s.batch.Draw(s.textures[2], Vector2(0, 0), tint);
            s.batch.End();
        };
    };
    const Color tint(200, 100, 50, 180);
    return {
        {"noise_alpha", noise(BlendState::AlphaBlend, Color::White)},
        {"noise_additive", noise(BlendState::Additive, Color::White)},
        {"noise_nonpremultiplied", noise(BlendState::NonPremultiplied, Color::White)},
        {"noise_tint", noise(BlendState::Opaque, tint)},
        {"noise_tint_alpha", noise(BlendState::AlphaBlend, tint)},
        {"noise_tint_additive", noise(BlendState::Additive, tint)},
        {"noise_tint_nonpremultiplied", noise(BlendState::NonPremultiplied, tint)},
        {"noise_over_clear", [=](DiffScene& s) {
             s.Graphics().Clear(back);
             s.batch.Begin(SpriteSortMode::Deferred, BlendState::NonPremultiplied, SamplerState::PointClamp);
             s.batch.Draw(s.textures[2], Vector2(0, 0), Color::White);
             s.batch.End();
         }},
        {"blends", [=](DiffScene& s) {
             s.Graphics().Clear(back);
             const BlendState blends[4] = {BlendState::AlphaBlend, BlendState::Additive, BlendState::NonPremultiplied, BlendState::Opaque};
             for (int i = 0; i < 4; ++i) {
                 begin(s, blends[i], SamplerState::PointClamp);
                 s.batch.Draw(s.textures[0], Vector2(4.0f + i * 38.0f, 4.0f), Color::White);
                 s.batch.Draw(s.textures[0], Vector2(4.0f + i * 38.0f, 24.0f), Color(200, 100, 50, 180));
                 s.batch.Draw(s.textures[0], Rectangle(4 + i * 38, 44, 32, 32), Color(90, 200, 255, 128));
                 s.batch.End();
             }
         }},
        {"fractional", [=](DiffScene& s) {
             s.Graphics().Clear(back);
             for (SamplerState sampler : {SamplerState::PointClamp, SamplerState::LinearClamp}) {
                 const float y0 = sampler == SamplerState::PointClamp ? 2.0f : 60.0f;
                 begin(s, BlendState::AlphaBlend, sampler);
                 for (int i = 0; i < 8; ++i) {
                     const float f = i / 8.0f + 0.03f;
                     s.batch.Draw(s.textures[1], Vector2(2.0f + i * 19.0f + f, y0 + f * 0.7f), Color::White);
                     s.batch.Draw(s.textures[0], Vector2(2.0f + i * 19.0f + f * 0.5f, y0 + 20.0f + f), Color::White);
                 }
                 s.batch.End();
             }
         }},
        {"scaled", [=](DiffScene& s) {
             s.Graphics().Clear(back);
             const SamplerState samplers[4] = {SamplerState::PointClamp, SamplerState::LinearClamp, SamplerState::PointWrap,
                                               SamplerState::LinearWrap};
             for (int i = 0; i < 4; ++i) {
                 begin(s, BlendState::AlphaBlend, samplers[i]);
                 s.batch.Draw(s.textures[0], Rectangle(2 + i * 40, 2, 37, 41), Color::White);
                 s.batch.Draw(s.textures[0], Rectangle(2 + i * 40, 46, 9, 7), Color::White);
                 s.batch.Draw(s.textures[1], Vector2(2.0f + i * 40, 60.0f), Rectangle(-3, 2, 13, 9), Color::White, 0.0f,
                              Vector2(), Vector2(2.3f, 2.7f), SpriteEffects::None, 0.0f);
                 s.batch.End();
             }
         }},
        {"rotated", [=](DiffScene& s) {
             s.Graphics().Clear(back);
             for (SamplerState sampler : {SamplerState::PointClamp, SamplerState::LinearClamp}) {
                 const float y = sampler == SamplerState::PointClamp ? 30.0f : 90.0f;
                 begin(s, BlendState::AlphaBlend, sampler);
                 const float angles[5] = {0.3f, 1.0f, 2.5f, -0.7f, MathHelper::PiOver2};
                 for (int i = 0; i < 5; ++i)
                     s.batch.Draw(s.textures[i % 2], Vector2(18.0f + i * 31.0f, y), std::nullopt, Color(255, 230, 200, 230),
                                  angles[i], Vector2(8.0f, 8.0f), 1.4f, i == 2 ? SpriteEffects::FlipHorizontally : SpriteEffects::None,
                                  0.0f);
                 s.batch.End();
             }
         }},
        {"transform", [=](DiffScene& s) {
             s.Graphics().Clear(back);
             const Matrix m = Matrix::CreateRotationZ(0.2f) * Matrix::CreateScale(1.7f) * Matrix::CreateTranslation(30.5f, 10.25f, 0.0f);
             begin(s, BlendState::AlphaBlend, SamplerState::LinearClamp, m);
             s.batch.Draw(s.textures[0], Vector2(0, 0), Color::White);
             s.batch.Draw(s.textures[1], Vector2(20, 5), std::nullopt, Color::White, 0.0f, Vector2(), 1.0f,
                          SpriteEffects::FlipVertically, 0.0f);
             s.batch.End();
         }},
        {"target", [=](DiffScene& s) {
             s.Graphics().SetRenderTarget(&s.targets[0]);
             s.Graphics().Clear(Color(0, 0, 0, 0));
             begin(s, BlendState::AlphaBlend, SamplerState::LinearClamp);
             s.batch.Draw(s.textures[0], Rectangle(3, 2, 27, 23), Color::White);
             s.batch.Draw(s.textures[1], Vector2(10.5f, 12.25f), Color(255, 255, 255, 150));
             s.batch.End();
             s.Graphics().SetRenderTarget(nullptr);
             s.Graphics().Clear(back);
             begin(s, BlendState::AlphaBlend, SamplerState::LinearClamp);
             s.batch.Draw(s.targets[0], Vector2(80, 60), std::nullopt, Color::White, 0.6f, Vector2(16, 16), 2.2f,
                          SpriteEffects::None, 0.0f);
             s.batch.End();
             begin(s, BlendState::Additive, SamplerState::PointClamp);
             s.batch.Draw(s.targets[0], Vector2(2, 2), Color(128, 128, 128, 128));
             s.batch.End();
         }},
        {"slivers", [=](DiffScene& s) {
             s.Graphics().Clear(back);
             begin(s, BlendState::AlphaBlend, SamplerState::PointClamp);
             for (int i = 0; i < 40; ++i) {
                 const float x = 2.0f + i * 3.9f;
                 const float wdt = 0.1f + i * 0.07f;
                 s.batch.Draw(s.textures[1], Vector2(x, 3.0f + i * 0.13f), Rectangle(0, 0, 1, 1), Color::White, 0.0f, Vector2(),
                              Vector2(wdt, 30.0f + i * 0.5f), SpriteEffects::None, 0.0f);
                 s.batch.Draw(s.textures[1], Vector2(x + 0.5f, 70.0f), Rectangle(2, 0, 1, 1), Color::White, 0.05f * i,
                              Vector2(0.5f, 0.5f), Vector2(1.0f, 40.0f), SpriteEffects::None, 0.0f);
             }
             s.batch.End();
         }},
        {"text", [=](DiffScene& s) {
             s.Graphics().Clear(back);
             begin(s, BlendState::AlphaBlend, SamplerState::LinearClamp);
             s.batch.DrawString(s.font, "Hg@&8 Wave", Vector2(3, 3), Color::White);
             s.batch.DrawString(s.font, "scaled 1.5x", Vector2(3.5f, 30.25f), Color(255, 220, 120, 255), 0.0f, Vector2(), 1.5f,
                                SpriteEffects::None, 0.0f);
             s.batch.DrawString(s.font, "turned", Vector2(80, 70), Color(120, 255, 160, 200), 0.4f, Vector2(), 1.0f,
                                SpriteEffects::None, 0.0f);
             s.batch.End();
         }},
    };
}

using Ties = std::set<std::pair<int, int>>;

// The pixels the CPU reported as near a rounding tie, from the file COLDX_CPU_TIES named.
Ties ReadTies(const std::string& path) {
    Ties ties;
    std::ifstream in(path);
    int x = 0, y = 0;
    while (in >> x >> y) ties.insert({x, y});
    return ties;
}

const std::string& TiesPath() {
    static const std::string path = (std::filesystem::temp_directory_path() / "coldx_diff_ties.txt").string();
    return path;
}

bool Capture(const Case& c, const std::string& backend, std::vector<Color>& out) {
    SDL_Environment* env = SDL_GetEnvironment();
    if (backend == "cpu") {
        SDL_SetEnvironmentVariable(env, "COLDX_RENDERER", "cpu", true);
        SDL_SetEnvironmentVariable(env, "COLDX_CPU_TIES", TiesPath().c_str(), true);
        SDL_UnsetEnvironmentVariable(env, "SDL_GPU_DRIVER");
        std::error_code ignored;
        std::filesystem::remove(TiesPath(), ignored);
    } else {
        SDL_SetEnvironmentVariable(env, "COLDX_RENDERER", "gpu", true);
        SDL_SetEnvironmentVariable(env, "SDL_GPU_DRIVER", backend.c_str(), true);
    }
    DiffScene scene(W, H);
    scene.load = [](Scene& base) {
        auto& s = static_cast<DiffScene&>(base);
        s.AddTexture(16, 16, Gradient());
        s.AddTexture(8, 8, Checker());
        s.AddTexture(W, H, Noise(1));
        s.AddTexture(W, H, Noise(11));
        s.targets.push_back(RenderTarget2D::Create(s.Graphics(), 32, 32));
        s.font = SpriteFont::FromFile(s.Graphics(), COLDX_TEST_FONT, 15.0f);
        CHECK(!s.font.IsEmpty());
    };
    scene.draw = [&c](Scene& s) { c.draw(static_cast<DiffScene&>(s)); };
    const Frame frame = RunAndCapture(scene, std::string("diff_") + c.name);
    if (!frame.Valid()) return false;
    out.resize(static_cast<size_t>(W) * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) out[static_cast<size_t>(y) * W + x] = frame.At(x, y);
    return true;
}

// Prints how the GPU's capture differs from the CPU's and returns whether every difference
// is one step at a pixel the CPU reported as a tie.
bool Matches(const char* name, const std::vector<Color>& cpu, const Ties& ties, const std::string& gpu,
             const std::vector<Color>& g) {
    int atTies = 0;
    int wrong = 0;
    for (int i = 0; i < W * H; ++i) {
        const Color p = cpu[static_cast<size_t>(i)], q = g[static_cast<size_t>(i)];
        const int d = std::max({std::abs(p.r - q.r), std::abs(p.g - q.g), std::abs(p.b - q.b), std::abs(p.a - q.a)});
        if (d == 0) continue;
        if (d == 1 && ties.count({i % W, i / W})) {
            ++atTies;
            continue;
        }
        if (wrong < 6)
            std::printf("  %s (%d, %d): cpu %d,%d,%d,%d  %s %d,%d,%d,%d\n", name, i % W, i / W, p.r, p.g, p.b, p.a, gpu.c_str(),
                        q.r, q.g, q.b, q.a);
        ++wrong;
    }
    std::printf("%s: cpu vs %s: %d differ one step at ties (%zu ties), %d differ otherwise\n", name, gpu.c_str(), atTies,
                ties.size(), wrong);
    return wrong == 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> gpus = {COLDX_TEST_DRIVERS};
    const std::string only = argc > 1 ? argv[1] : "";
    for (const Case& c : Cases()) {
        if (!only.empty() && only != c.name) continue;
        std::vector<Color> cpu;
        if (!Capture(c, "cpu", cpu)) continue;
        const Ties ties = ReadTies(TiesPath());
        for (const std::string& gpu : gpus) {
            std::vector<Color> g;
            if (!Capture(c, gpu, g)) continue;
            CHECK(Matches(c.name, cpu, ties, gpu, g));
        }
    }
    return CheckResult();
}
