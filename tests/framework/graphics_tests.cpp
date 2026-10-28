// GFX-01 to GFX-09. Automated where the result can be measured. Each automated test runs
// a real game and checks its numbers or its exit capture.

#include "framework_test.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <SDL3/SDL.h>

using namespace coldx;

namespace ft {

namespace {

// A render target filled with one color and a marker square in another.
RenderTarget2D MarkedTarget(GraphicsDevice& g, SpriteBatch& batch, const Texture2D& white, int size, Color fill, Color mark) {
    RenderTarget2D rt = RenderTarget2D::Create(g, size, size);
    g.SetRenderTarget(&rt);
    g.Clear(fill);
    batch.Begin();
    batch.Draw(white, Rectangle(size / 4, size / 4, size / 4, size / 4), mark);
    batch.End();
    g.SetRenderTarget(nullptr);
    return rt;
}

// GFX-01: the back buffer follows every size, tiny and huge, and a render target still
// draws at the end.
Result WindowResize() {
    Result r{"GFX-01", "P0"};
    const std::string capture = TempPath("gfx01.png");
    std::filesystem::remove(capture);
    SetEnv("COLDX_CAPTURE", capture.c_str());

    const std::pair<int, int> sizes[] = {{1280, 720}, {320, 180}, {1, 1}, {64, 64}, {2400, 1300}, {1920, 400}, {800, 600}};
    Probe game;
    game.Settings().resizable = true;
    Texture2D white;
    RenderTarget2D target;
    std::string mismatches;
    int step = 0;
    game.load = [&] { white = Solid(game.Graphics(), Color::White); };
    game.update = [&](Probe& p, const GameTime&) {
        if (p.updates % 10 != 1) return;
        if (step > 0) {
            int pw = 0, ph = 0;
            SDL_GetWindowSizeInPixels(OnlyWindow(), &pw, &ph);
            const int bw = p.Graphics().BackBufferWidth(), bh = p.Graphics().BackBufferHeight();
            if (bw != pw || bh != ph)
                mismatches += Format(" asked %dx%d: window %dx%d px, back buffer %dx%d.", sizes[step - 1].first,
                                     sizes[step - 1].second, pw, ph, bw, bh);
        }
        if (step == static_cast<int>(std::size(sizes))) {
            p.Exit();
            return;
        }
        p.SetWindowSize(sizes[step].first, sizes[step].second);
        ++step;
    };
    game.draw = [&](Probe& p, SpriteBatch& b, const GameTime&) {
        if (target.IsEmpty()) target = MarkedTarget(p.Graphics(), b, white, 64, Color::Green, Color::Red);
        const int w = p.Graphics().BackBufferWidth(), h = p.Graphics().BackBufferHeight();
        b.Begin();
        for (auto [x, y] : {std::pair{0, 0}, {w - 16, 0}, {0, h - 16}, {w - 16, h - 16}})
            b.Draw(white, Rectangle(x, y, 16, 16), Color::White);
        b.Draw(target, Vector2(static_cast<float>(w / 2 - 32), static_cast<float>(h / 2 - 32)), Color::White);
        b.End();
    };
    const int rc = game.Run();
    SetEnv("COLDX_CAPTURE", nullptr);

    Capture c;
    const bool loaded = c.Load(capture);
    bool pixels = loaded && c.width == 800 && c.height == 600;
    if (pixels) {
        pixels = Near(c.At(2, 2), Color::White, 0) && Near(c.At(797, 597), Color::White, 0) &&
                 Near(c.At(400 - 32 + 2, 300 - 32 + 2), Color::Green, 0) && Near(c.At(400 - 32 + 20, 300 - 32 + 20), Color::Red, 0);
    }
    const bool ok = rc == 0 && mismatches.empty() && pixels;
    r.status = ok ? Status::NeedsHuman : Status::Fail;
    r.notes = ok ? "Programmatic resizes to 1x1 .. 2400x1300 keep the back buffer equal to the window's pixel size. "
                   "Corners and render target correct at the end. Dragging the edge by hand still needs a person."
                 : "Problems:" + mismatches + (pixels ? "" : " final frame wrong or capture missing.");
    std::filesystem::remove(capture);
    return r;
}

// GFX-02: fullscreen on and off repeatedly. The back buffer follows, and a render target
// made before keeps its contents.
Result Fullscreen() {
    Result r{"GFX-02", "P0"};
    const std::string capture = TempPath("gfx02.png");
    std::filesystem::remove(capture);
    SetEnv("COLDX_CAPTURE", capture.c_str());
    Probe game;
    Texture2D white;
    RenderTarget2D target;
    std::string problems;
    int toggles = 0;
    game.load = [&] { white = Solid(game.Graphics(), Color::White); };
    game.update = [&](Probe& p, const GameTime&) {
        if (p.updates % 30 != 0) return;
        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(OnlyWindow(), &pw, &ph);
        if (p.Graphics().BackBufferWidth() != pw || p.Graphics().BackBufferHeight() != ph)
            problems += Format(" back buffer %dx%d vs window %dx%d.", p.Graphics().BackBufferWidth(),
                               p.Graphics().BackBufferHeight(), pw, ph);
        if (toggles == 6) {
            if (p.IsFullscreen()) problems += " still fullscreen after an even number of toggles.";
            p.Exit();
            return;
        }
        p.ToggleFullscreen();
        ++toggles;
    };
    game.draw = [&](Probe& p, SpriteBatch& b, const GameTime&) {
        if (target.IsEmpty()) target = MarkedTarget(p.Graphics(), b, white, 64, Color::Blue, Color::Yellow);
        b.Begin();
        b.Draw(target, Vector2(100, 100), Color::White);
        b.End();
    };
    const int rc = game.Run();
    SetEnv("COLDX_CAPTURE", nullptr);
    Capture c;
    const bool pixels = c.Load(capture) && Near(c.At(102, 102), Color::Blue, 0) && Near(c.At(100 + 20, 100 + 20), Color::Yellow, 0);
    const bool ok = rc == 0 && problems.empty() && pixels;
    r.status = ok ? Status::NeedsHuman : Status::Fail;
    r.notes = ok ? "6 toggles: back buffer always matched the window (desktop resolution when fullscreen). A render "
                   "target made before the toggles kept its contents. These toggles are borderless. Exclusive mode "
                   "is FS-01. Alt-tab and moving between monitors need a person."
                 : "Problems:" + problems + (pixels ? "" : " render target contents lost or capture missing.");
    std::filesystem::remove(capture);
    return r;
}

// GFX-03: on this display's scaling, back buffer equals window pixels, and a mouse position
// set in back buffer pixels reads back the same.
Result HighDpi() {
    Result r{"GFX-03", "P0"};
    Probe game;
    float scale = 0, density = 0;
    int pw = 0, ph = 0;
    bool mouseOk = false;
    std::string mouseNote;
    game.update = [&](Probe& p, const GameTime&) {
        if (p.updates == 5) {
            SDL_Window* w = OnlyWindow();
            scale = SDL_GetWindowDisplayScale(w);
            density = SDL_GetWindowPixelDensity(w);
            SDL_GetWindowSizeInPixels(w, &pw, &ph);
            Mouse::SetPosition(300, 200);
        }
        if (p.updates == 12) {
            const MouseState m = Mouse::GetState();
            mouseOk = m.x == 300 && m.y == 200;
            mouseNote = Format("set (300, 200), read (%d, %d)", m.x, m.y);
            p.Exit();
        }
    };
    const int rc = game.Run();
    const bool sizeOk = pw == game.Settings().width * density && ph > 0;
    r.status = rc == 0 && mouseOk ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("Display scale %.2f, pixel density %.2f, window %dx%d px for %dx%d asked. Mouse %s. ", scale, density,
                     pw, ph, game.Settings().width, game.Settings().height, mouseNote.c_str()) +
              (sizeOk ? "" : "Window pixel size is not width x density. ") +
              "Windows sizes the window in pixels at any scaling, so at 125% it is smaller on screen than the "
              "same size at 100%. Output is drawn 1:1, not upscaled. Sharpness and macOS Retina need a person.";
    return r;
}

// GFX-04: the same scene on every backend this machine has, compared pixel by pixel.
Result Backends() {
    Result r{"GFX-04", "P1"};
    std::vector<std::pair<std::string, Capture>> shots;
    for (const char* driver : {"direct3d12", "vulkan"}) {
        const std::string capture = TempPath(std::string("gfx04_") + driver + ".png");
        std::filesystem::remove(capture);
        SetEnv("COLDX_CAPTURE", capture.c_str());
        SetEnv("COLDX_HEADLESS", "1");
        SetEnv("SDL_GPU_DRIVER", driver);
        Probe game;
        game.Settings().width = 256;
        game.Settings().height = 128;
        Texture2D ramp, white;
        RenderTarget2D target;
        game.load = [&] {
            std::vector<Color> px(16 * 16);
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    px[y * 16 + x] = Color(static_cast<std::uint8_t>(x * 16), static_cast<std::uint8_t>(y * 16), 128, 255) * ((x + y) % 3 ? 1.0f : 0.5f);
            ramp = Texture2D::FromPixels(game.Graphics(), 16, 16, px.data());
            white = Solid(game.Graphics(), Color::White);
        };
        game.update = [](Probe& p, const GameTime&) {
            if (p.updates == 3) p.Exit();
        };
        game.draw = [&](Probe& p, SpriteBatch& b, const GameTime&) {
            if (target.IsEmpty()) target = MarkedTarget(p.Graphics(), b, white, 32, Color::Cyan, Color::Magenta);
            const BlendState blends[] = {BlendState::AlphaBlend, BlendState::Additive, BlendState::NonPremultiplied, BlendState::Opaque};
            for (int i = 0; i < 4; ++i) {
                b.Begin(SpriteSortMode::Deferred, blends[i], i % 2 ? SamplerState::LinearClamp : SamplerState::PointClamp);
                b.Draw(ramp, Rectangle(i * 40, 0, 40, 40), Color::White);
                b.Draw(ramp, Vector2(i * 40.0f + 20, 50), std::nullopt, Color::White * 0.7f, 0.5f, Vector2(8, 8), 2.0f,
                       SpriteEffects::FlipHorizontally, 0);
                b.End();
            }
            b.Begin();
            b.Draw(target, Vector2(180, 60), Color::White);
            b.End();
        };
        game.Run();
        Capture c;
        if (c.Load(capture)) shots.emplace_back(driver, std::move(c));
        std::filesystem::remove(capture);
    }
    SetEnv("COLDX_CAPTURE", nullptr);
    SetEnv("COLDX_HEADLESS", nullptr);
    SetEnv("SDL_GPU_DRIVER", nullptr);
    if (shots.size() < 2) {
        r.status = Status::NeedsHuman;
        r.notes = Format("Only %zu backend(s) ran here.", shots.size());
        return r;
    }
    int worst = 0, differing = 0;
    for (size_t i = 0; i < shots[0].second.pixels.size(); ++i) {
        const Color a = shots[0].second.pixels[i], b = shots[1].second.pixels[i];
        const int d = std::max({std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b), std::abs(a.a - b.a)});
        worst = std::max(worst, d);
        differing += d > 0 ? 1 : 0;
    }
    r.status = worst <= 1 ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("Direct3D 12 vs Vulkan on this GPU: %d of %zu pixels differ, by at most %d. ", differing,
                     shots[0].second.pixels.size(), worst) +
              "Intel and AMD GPUs, and Metal, need other machines.";
    return r;
}

// GFX-05: 64 render targets drawn once, switched 256 times a frame for a minute, through a
// resize and a fullscreen round trip. Each must keep its contents.
Result RenderTargetCache() {
    Result r{"GFX-05", "P0"};
    const std::string capture = TempPath("gfx05.png");
    std::filesystem::remove(capture);
    SetEnv("COLDX_CAPTURE", capture.c_str());
    constexpr int Count = 64;
    auto colorOf = [](int i) { return Color(static_cast<std::uint8_t>(40 + (i % 8) * 30), static_cast<std::uint8_t>(40 + (i / 8) * 30), 200); };
    Probe game;
    game.Settings().resizable = true;
    Texture2D white;
    std::vector<RenderTarget2D> targets;
    bool filled = false;
    double start = 0;
    game.load = [&] {
        white = Solid(game.Graphics(), Color::White);
        for (int i = 0; i < Count; ++i) targets.push_back(RenderTarget2D::Create(game.Graphics(), 128, 128));
        start = Now();
    };
    game.update = [&](Probe& p, const GameTime&) {
        const double t = Now() - start;
        if (p.updates == 1800) p.SetWindowSize(1000, 700);
        if (p.updates == 2400) p.SetFullscreen(true);
        if (p.updates == 2700) p.SetFullscreen(false);
        if (t > 60.0 && p.updates > 3000) p.Exit();
    };
    game.draw = [&](Probe& p, SpriteBatch& b, const GameTime&) {
        GraphicsDevice& g = p.Graphics();
        if (!filled) {
            for (int i = 0; i < Count; ++i) {
                g.SetRenderTarget(&targets[i]);
                g.Clear(colorOf(i));
                b.Begin();
                b.Draw(white, Rectangle(0, 0, 16, 16), Color::Black);
                b.End();
            }
            g.SetRenderTarget(nullptr);
            filled = true;
        }
        for (int k = 0; k < 4; ++k)
            for (int i = 0; i < Count; ++i) g.SetRenderTarget(&targets[i]);
        g.SetRenderTarget(nullptr);
        g.Clear(Color::Black);
        b.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        for (int i = 0; i < Count; ++i) b.Draw(targets[i], Rectangle((i % 8) * 70, (i / 8) * 70, 64, 64), Color::White);
        b.End();
    };
    const int rc = game.Run();
    SetEnv("COLDX_CAPTURE", nullptr);
    Capture c;
    int wrong = 0;
    if (c.Load(capture)) {
        for (int i = 0; i < Count; ++i) {
            const int x = (i % 8) * 70, y = (i / 8) * 70;
            if (!Near(c.At(x + 40, y + 40), colorOf(i), 0) || !Near(c.At(x + 2, y + 2), Color::Black, 0)) ++wrong;
        }
    } else {
        wrong = Count;
    }
    r.status = rc == 0 && wrong == 0 ? Status::Pass : Status::Fail;
    r.notes = Format("64 targets drawn once, switched 256 times a frame for %d updates, through a resize and a "
                     "fullscreen round trip: %d lost contents. Targets always keep their contents (XNA's "
                     "PreserveContents). There is no RenderTargetUsage to choose DiscardContents.",
                     game.updates, wrong);
    std::filesystem::remove(capture);
    return r;
}

// GFX-06: a soft-edged premultiplied sprite, scaled with linear filtering over two
// backgrounds. No pixel may be darker than both the sprite and the background.
Result BlendFringes() {
    Result r{"GFX-06", "P1"};
    const std::string capture = TempPath("gfx06.png");
    std::filesystem::remove(capture);
    SetEnv("COLDX_CAPTURE", capture.c_str());
    SetEnv("COLDX_HEADLESS", "1");
    const Color sprite(255, 200, 40), backgrounds[2] = {Color(128, 128, 128), Color(30, 90, 220)};
    Probe game;
    game.Settings().width = 400;
    game.Settings().height = 200;
    Texture2D premultiplied, straightBlack, white;
    game.load = [&] {
        std::vector<Color> pm(32 * 32), straight(32 * 32);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                const float d = std::sqrt((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f));
                const float a = std::clamp(14.0f - d, 0.0f, 1.0f);
                pm[y * 32 + x] = sprite * a;
                // Straight alpha with black in the transparent texels: the classic source of halos.
                straight[y * 32 + x] = a > 0 ? Color(sprite.r, sprite.g, sprite.b, static_cast<std::uint8_t>(a * 255)) : Color(0, 0, 0, 0);
            }
        premultiplied = Texture2D::FromPixels(game.Graphics(), 32, 32, pm.data());
        straightBlack = Texture2D::FromPixels(game.Graphics(), 32, 32, straight.data());
        white = Solid(game.Graphics(), Color::White);
    };
    game.update = [](Probe& p, const GameTime&) {
        if (p.updates == 3) p.Exit();
    };
    game.draw = [&](Probe&, SpriteBatch& b, const GameTime&) {
        b.Begin();
        b.Draw(white, Rectangle(0, 0, 200, 200), backgrounds[0]);
        b.Draw(white, Rectangle(200, 0, 200, 200), backgrounds[1]);
        b.Draw(premultiplied, Rectangle(20, 20, 128, 128), Color::White);
        b.Draw(premultiplied, Rectangle(220, 20, 128, 128), Color::White);
        b.End();
        b.Begin(SpriteSortMode::Deferred, BlendState::NonPremultiplied, SamplerState::LinearClamp);
        b.Draw(straightBlack, Rectangle(20, 150, 40, 40), Color::White);
        b.End();
    };
    game.Run();
    SetEnv("COLDX_CAPTURE", nullptr);
    SetEnv("COLDX_HEADLESS", nullptr);
    Capture c;
    int fringe = 0, straightFringe = 0;
    if (c.Load(capture)) {
        for (int half = 0; half < 2; ++half)
            for (int y = 20; y < 148; ++y)
                for (int x = 20 + half * 200; x < 148 + half * 200; ++x) {
                    const Color p = c.At(x, y), bg = backgrounds[half];
                    auto below = [](int v, int a, int b) { return v < std::min(a, b) - 1; };
                    if (below(p.r, bg.r, sprite.r) || below(p.g, bg.g, sprite.g) || below(p.b, bg.b, sprite.b)) ++fringe;
                }
        for (int y = 150; y < 190; ++y)
            for (int x = 20; x < 60; ++x) {
                const Color p = c.At(x, y);
                if (p.r < 128 - 2 && p.g < 128 - 2 && p.r < sprite.r) ++straightFringe;
            }
    }
    r.status = fringe == 0 ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("Premultiplied sprite scaled 4x with linear filtering over grey and blue: %d pixels darker than "
                     "both sprite and background (0 means no halo). For contrast, straight-alpha art with black "
                     "transparent texels under NonPremultiplied shows %d darkened edge pixels, the usual XNA artifact. "
                     "FromFile premultiplies PNGs, which avoids it. Blend equations are checked exactly by core.sprite. "
                     "Comparing against FNA screenshots needs FNA.",
                     fringe, straightFringe);
    std::filesystem::remove(capture);
    return r;
}

} // namespace

void RegisterGraphics() {
    AutoTests().push_back({"GFX-01", "P0", "Window resize", WindowResize});
    AutoTests().push_back({"GFX-02", "P0", "Fullscreen toggle", Fullscreen});
    AutoTests().push_back({"GFX-03", "P0", "High-DPI", HighDpi});
    AutoTests().push_back({"GFX-04", "P1", "Backends", Backends});
    AutoTests().push_back({"GFX-05", "P0", "Render target contents", RenderTargetCache});
    AutoTests().push_back({"GFX-06", "P1", "Premultiplied alpha and blending", BlendFringes});
    MissingFeatures().push_back({"GFX-09", "P2", Status::Missing,
                                 "Custom shaders when drawing (Effect) do not exist yet: spec milestone 9. Shaders compile "
                                 "(coldx_compile_shaders) but SpriteBatch only uses its built-in pair."});
}

} // namespace ft
