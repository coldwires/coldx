// GFX-07 texture edge cases and GFX-08 sprite throughput.

#include "framework_test.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include <SDL3/SDL.h>

using namespace coldx;

namespace ft {

namespace {

bool WritePng(const std::string& path, int w, int h, const std::function<Color(int, int)>& pixel) {
    SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!s) return false;
    for (int y = 0; y < h; ++y) {
        auto* row = static_cast<std::uint8_t*>(s->pixels) + y * s->pitch;
        for (int x = 0; x < w; ++x) {
            const Color c = pixel(x, y);
            row[x * 4] = c.r;
            row[x * 4 + 1] = c.g;
            row[x * 4 + 2] = c.b;
            row[x * 4 + 3] = c.a;
        }
    }
    const bool ok = SDL_SavePNG(s, path.c_str());
    SDL_DestroySurface(s);
    return ok;
}

// GFX-07: odd sizes from PNG, big atlases, a size past the hardware limit, and sampling
// next to an atlas cell's edge.
Result TextureEdges() {
    Result r{"GFX-07", "P1"};
    const std::string npot = TempPath("gfx07_npot.png"), tiny = TempPath("gfx07_1x1.png"), clear = TempPath("gfx07_clear.png");
    WritePng(npot, 37, 23, [](int x, int y) { return Color(static_cast<std::uint8_t>(x * 6), static_cast<std::uint8_t>(y * 11), 90); });
    WritePng(tiny, 1, 1, [](int, int) { return Color(255, 0, 255); });
    WritePng(clear, 8, 8, [](int, int) { return Color(0, 0, 0, 0); });
    const std::string capture = TempPath("gfx07.png");
    std::filesystem::remove(capture);
    SetEnv("COLDX_CAPTURE", capture.c_str());
    SetEnv("COLDX_HEADLESS", "1");

    Probe game;
    game.Settings().width = 320;
    game.Settings().height = 160;
    std::string notes, problems;
    Texture2D tNpot, tTiny, tClear, atlas, big4k, big8k;
    double tooBigSeconds = 0;
    game.load = [&] {
        GraphicsDevice& g = game.Graphics();
        tNpot = Texture2D::FromFile(g, npot);
        tTiny = Texture2D::FromFile(g, tiny);
        tClear = Texture2D::FromFile(g, clear);
        if (tNpot.Width() != 37 || tNpot.Height() != 23) problems += " 37x23 PNG loaded wrong;";
        if (tTiny.Width() != 1) problems += " 1x1 PNG failed;";
        if (tClear.Width() != 8) problems += " transparent PNG failed;";

        // Atlas: two 16x16 cells, red then blue.
        std::vector<Color> cells(32 * 16);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 32; ++x) cells[y * 32 + x] = x < 16 ? Color::Red : Color::Blue;
        atlas = Texture2D::FromPixels(g, 32, 16, cells.data());

        for (int size : {4096, 8192}) {
            std::vector<Color> px(static_cast<size_t>(size) * size, Color(10, 200, 30));
            px.back() = Color::Yellow;  // bottom-right texel, sampled below
            const double t0 = Now();
            Texture2D t = Texture2D::FromPixels(g, size, size, px.data());
            notes += Format("%dx%d atlas %s in %.0f ms; ", size, size, t.IsEmpty() ? "FAILED" : "loaded", (Now() - t0) * 1000);
            if (t.IsEmpty()) problems += Format(" %d atlas failed;", size);
            (size == 4096 ? big4k : big8k) = t;
        }
        // Past any GPU's limit: must come back empty with a log line, not crash.
        const double t0 = Now();
        RenderTarget2D huge = RenderTarget2D::Create(g, 32768, 32768);
        tooBigSeconds = Now() - t0;
        if (!huge.IsEmpty()) problems += " a 32768x32768 target was accepted;";
        notes += huge.IsEmpty() ? "32768x32768 refused cleanly (logged); " : "";
    };
    game.update = [](Probe& p, const GameTime&) {
        if (p.updates == 3) p.Exit();
    };
    game.draw = [&](Probe&, SpriteBatch& b, const GameTime&) {
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        b.Draw(tNpot, Vector2(0, 0), Color::White);
        b.Draw(tTiny, Rectangle(40, 0, 4, 4), Color::White);
        b.Draw(tClear, Vector2(50, 0), Color::White);
        // The last texel of each big atlas, through a 1x1 source rectangle at the edge.
        b.Draw(big4k, Rectangle(60, 0, 4, 4), Rectangle(4095, 4095, 1, 1), Color::White);
        b.Draw(big8k, Rectangle(70, 0, 4, 4), Rectangle(8191, 8191, 1, 1), Color::White);
        // The red cell, 4x, at a whole pixel: point sampling.
        b.Draw(atlas, Vector2(0, 40), Rectangle(0, 0, 16, 16), Color::White, 0, Vector2(), 4.0f, SpriteEffects::None, 0);
        b.End();
        // The same, at a half pixel with linear filtering: where bleeding shows if it does.
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::LinearClamp);
        b.Draw(atlas, Vector2(100.5f, 40), Rectangle(0, 0, 16, 16), Color::White, 0, Vector2(), 4.0f, SpriteEffects::None, 0);
        b.End();
    };
    const int rc = game.Run();
    SetEnv("COLDX_CAPTURE", nullptr);
    SetEnv("COLDX_HEADLESS", nullptr);

    Capture c;
    int pointBleed = 0, linearBleed = 0;
    if (c.Load(capture)) {
        if (!Near(c.At(10, 10), Color(60, 110, 90), 0)) problems += " NPOT texel wrong;";
        if (!Near(c.At(41, 1), Color(255, 0, 255), 0)) problems += " 1x1 texel wrong;";
        if (!Near(c.At(61, 1), Color::Yellow, 0) || !Near(c.At(71, 1), Color::Yellow, 0)) problems += " big atlas edge texel wrong;";
        for (int y = 40; y < 104; ++y) {
            pointBleed += c.At(63, y).b > 10 ? 1 : 0;
            linearBleed += c.At(163, y).b > 10 ? 1 : 0;
        }
    } else {
        problems += " no capture;";
    }
    if (pointBleed) problems += Format(" %d bleeding pixels with point sampling;", pointBleed);
    r.status = rc == 0 && problems.empty() ? Status::Pass : Status::Fail;
    r.notes = notes + Format("refusal took %.0f ms. Atlas cell drawn 4x: point sampling at whole pixels, %d bled pixels; "
                             "linear filtering at a half pixel, %d edge pixels pick up the neighbouring cell. That is "
                             "how XNA and FNA behave too: pad atlas cells or use PointClamp.",
                             tooBigSeconds * 1000, pointBleed, linearBleed) +
              (problems.empty() ? "" : " Problems:" + problems);
    for (const std::string& f : {npot, tiny, clear, capture}) std::filesystem::remove(f);
    return r;
}

// GFX-08: 10k, 50k and 100k sprites a frame, vsync off, in a visible window; frame times,
// and memory over a repeated 50k run to see whether anything grows per frame.
Result Throughput() {
    Result r{"GFX-08", "P0"};
    struct Level {
        int count;
        double fps = 0, worstMs = 0, memStart = 0, memEnd = 0;
    };
    std::vector<Level> levels = {{10000}, {50000}, {100000}, {50000}};
    Probe game;
    game.Settings().vsync = false;
    game.Settings().fixedTimeStep = false;
    Texture2D atlas;
    size_t level = 0;
    double levelStart = 0, last = 0;
    int frames = 0;
    bool measuring = false;
    game.load = [&] {
        std::vector<Color> px(32 * 32);
        for (int i = 0; i < 32 * 32; ++i) px[i] = Color(static_cast<std::uint8_t>(i % 256), 120, 200);
        atlas = Texture2D::FromPixels(game.Graphics(), 32, 32, px.data());
        levelStart = last = Now();
    };
    game.update = [&](Probe& p, const GameTime&) {
        const double now = Now();
        Level& lv = levels[level];
        if (!measuring && now - levelStart > 1.0) {  // a second to warm up
            measuring = true;
            frames = 0;
            levelStart = now;
            lv.memStart = ResidentMegabytes();
        } else if (measuring) {
            ++frames;
            lv.worstMs = std::max(lv.worstMs, (now - last) * 1000);
            if (now - levelStart > 4.0) {
                lv.fps = frames / (now - levelStart);
                lv.memEnd = ResidentMegabytes();
                measuring = false;
                levelStart = now;
                if (++level == levels.size()) p.Exit();
            }
        }
        last = now;
    };
    game.draw = [&](Probe& p, SpriteBatch& b, const GameTime& time) {
        if (level >= levels.size()) return;
        const int n = levels[level].count;
        const float w = static_cast<float>(p.Graphics().BackBufferWidth()), h = static_cast<float>(p.Graphics().BackBufferHeight());
        const float t = static_cast<float>(time.Total());
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        for (int i = 0; i < n; ++i) {
            const float x = std::fmod(i * 37.0f + t * 50, w), y = std::fmod(i * 13.0f + (i % 7) * t * 20, h);
            b.Draw(atlas, Vector2(x, y), Rectangle((i % 4) * 8, 0, 8, 8), Color::White, t + i, Vector2(4, 4), 1.0f,
                   SpriteEffects::None, 0);
        }
        b.End();
    };
    game.Run();

    std::string notes;
    for (const Level& lv : levels)
        notes += Format("%dk: %.0f fps, worst frame %.1f ms, memory %.1f -> %.1f MB; ", lv.count / 1000, lv.fps, lv.worstMs,
                        lv.memStart, lv.memEnd);
    // Per-frame growth shows within a level. Between levels memory follows the largest batch
    // so far: buffers grown for 100k sprites are kept for reuse, which is not a leak.
    double worstGrowth = 0;
    for (const Level& lv : levels) worstGrowth = std::max(worstGrowth, lv.memEnd - lv.memStart);
    const bool fast = levels[1].fps >= 60;
    const bool stable = worstGrowth < 2.0;
    r.status = fast && stable ? Status::Pass : Status::Fail;
    r.notes = notes + Format("largest growth within a level, over 4 s of frames: %+.1f MB (memory between levels follows the "
                             "biggest batch drawn so far, since buffers are kept for reuse). Machine: see report header. "
                             "Low-end hardware (PLAT-03) needs a person.",
                             worstGrowth);
    return r;
}

} // namespace

void RegisterGraphicsMore() {
    AutoTests().push_back({"GFX-07", "P1", "Texture loading edge cases", TextureEdges});
    AutoTests().push_back({"GFX-08", "P0", "Sprite throughput", Throughput});
}

} // namespace ft
