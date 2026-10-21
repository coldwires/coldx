// TIME-01 to TIME-04.

#include "framework_test.h"

#include <algorithm>
#include <cmath>
#include <thread>

#include <SDL3/SDL.h>

using namespace coldx;

namespace ft {

int SoakMinutes = 0;  // set by the runner's --soak option

namespace {

// TIME-01: fixed 60 Hz with vsync on this display for a minute, counted against the wall
// clock. Draws are counted too.
Result FixedTimestep() {
    Result r{"TIME-01", "P0"};
    Probe game;
    std::vector<double> updateTimes, drawTimes;
    double start = 0;
    game.load = [&] { start = Now(); };
    game.update = [&](Probe& p, const GameTime&) {
        updateTimes.push_back(Now());
        if (Now() - start > 63.0) p.Exit();
    };
    game.draw = [&](Probe&, SpriteBatch&, const GameTime&) { drawTimes.push_back(Now()); };
    game.Run();

    // A 60-second window starting two seconds in, after startup settles.
    const double from = start + 2.0, to = from + 60.0;
    auto countIn = [&](const std::vector<double>& times) {
        return std::count_if(times.begin(), times.end(), [&](double t) { return t >= from && t < to; });
    };
    const long long updates = countIn(updateTimes), draws = countIn(drawTimes);
    int worstSecond = 60;
    for (int s = 0; s < 60; ++s) {
        const double a = from + s, b = a + 1;
        const int n = static_cast<int>(std::count_if(updateTimes.begin(), updateTimes.end(), [&](double t) { return t >= a && t < b; }));
        if (std::abs(n - 60) > std::abs(worstSecond - 60)) worstSecond = n;
    }
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = display ? SDL_GetCurrentDisplayMode(display) : nullptr;
    r.status = std::abs(updates - 3600) <= 1 ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("%lld updates and %lld draws in 60 s at %.0f Hz with vsync on (want 3600 +-1); the most off single "
                     "second had %d updates. Draws are not decoupled from updates: with a fixed timestep coldx draws "
                     "once per frame that ran updates, as XNA does, so a high-refresh display shows 60 fps, not "
                     "interpolated frames. Smoothness of motion, and 60 Hz or slow machines, need a person.",
                     updates, draws, mode ? mode->refresh_rate : 0.0f, worstSecond);
    return r;
}

// TIME-02: 200 ms stalls, once and then five in a row: the loop must catch up within its
// cap, flag the slow frames, and settle back to one update per frame.
Result Stalls() {
    Result r{"TIME-02", "P0"};
    SetEnv("COLDX_HEADLESS", "1");
    Probe game;
    int updatesThisFrame = 0, maxUpdatesInFrame = 0, slowUpdates = 0, framesToSettle = 0;
    bool afterStall = false;
    std::vector<int> settle;
    double start = 0, stallTotal = 0;
    game.load = [&] { start = Now(); };
    game.update = [&](Probe& p, const GameTime& time) {
        ++updatesThisFrame;
        if (time.IsRunningSlowly()) ++slowUpdates;
        const int u = p.updates;
        const bool stall = u == 120 || (u >= 240 && u <= 300 && u % 12 == 0);
        if (stall) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            stallTotal += 0.2;
            afterStall = true;
            framesToSettle = 0;
        }
        if (u == 420) p.Exit();
    };
    game.draw = [&](Probe&, SpriteBatch&, const GameTime&) {
        maxUpdatesInFrame = std::max(maxUpdatesInFrame, updatesThisFrame);
        if (afterStall) {
            ++framesToSettle;
            if (updatesThisFrame == 1 && framesToSettle > 1) {
                settle.push_back(framesToSettle);
                afterStall = false;
            }
        }
        updatesThisFrame = 0;
    };
    game.Run();
    SetEnv("COLDX_HEADLESS", nullptr);
    const double wall = Now() - start;
    const double gameTime = game.updates / 60.0;
    const int worstSettle = settle.empty() ? -1 : *std::max_element(settle.begin(), settle.end());
    const bool ok = maxUpdatesInFrame <= game.Settings().maxUpdatesPerFrame && slowUpdates > 0 && worstSettle > 0 && worstSettle <= 4;
    r.status = ok ? Status::Pass : Status::Fail;
    r.notes = Format("%.1f s of stalls: at most %d updates in one frame (cap %d), %d updates flagged IsRunningSlowly, back "
                     "to one update per frame within %d frames after every stall. Game time %.2f s vs %.2f s of wall "
                     "clock: the time beyond the cap is dropped on purpose, so the game slows instead of spiralling.",
                     stallTotal, maxUpdatesInFrame, game.Settings().maxUpdatesPerFrame, slowUpdates, worstSettle, gameTime, wall);
    return r;
}

// TIME-03: vsync on and off every 1.5 s; frame rate must follow, nothing may break.
Result VsyncToggle() {
    Result r{"TIME-03", "P1"};
    Probe game;
    game.Settings().fixedTimeStep = false;
    std::vector<std::pair<bool, double>> phases;
    double phaseStart = 0;
    int frames = 0;
    bool on = true;
    game.load = [&] { phaseStart = Now(); };
    game.update = [&](Probe& p, const GameTime&) {
        ++frames;
        if (Now() - phaseStart >= 1.5) {
            phases.push_back({on, frames / (Now() - phaseStart)});
            on = !on;
            p.SetVSync(on);
            frames = 0;
            phaseStart = Now();
            if (phases.size() == 8) p.Exit();
        }
    };
    const int rc = game.Run();
    // Phase 0 is warm-up; after it, on and off phases alternate, so each set is averaged by
    // its own count.
    double onFps = 0, offFps = 0;
    int onCount = 0, offCount = 0;
    for (size_t i = 1; i < phases.size(); ++i) {
        (phases[i].first ? onFps : offFps) += phases[i].second;
        ++(phases[i].first ? onCount : offCount);
    }
    if (onCount) onFps /= onCount;
    if (offCount) offFps /= offCount;
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = display ? SDL_GetCurrentDisplayMode(display) : nullptr;
    const float hz = mode ? mode->refresh_rate : 0.0f;
    const bool ok = rc == 0 && hz > 0 && std::abs(onFps - hz) < hz * 0.1 && offFps > hz * 1.5;
    r.status = ok ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("7 toggles: vsync on averaged %.0f fps (display %.0f Hz), off %.0f fps; no errors. Tearing with vsync "
                     "off, and 60 Hz displays, need a person.",
                     onFps, hz, offFps);
    return r;
}

// TIME-04: a typical scene for a long time; memory must not climb and the fixed step must
// still be exact at the end.
Result LongSession() {
    Result r{"TIME-04", "P0"};
    if (SoakMinutes <= 0) {
        r.status = Status::NeedsHuman;
        r.notes = "Not run: pass --soak <minutes> (the spec asks for 240).";
        return r;
    }
    SetEnv("COLDX_HEADLESS", "1");
    Probe game;
    Texture2D atlas;
    SoundEffect blip;
    std::vector<std::pair<double, double>> memory;
    std::vector<double> updateTimes;
    double start = 0;
    game.load = [&] {
        std::vector<Color> px(16 * 16, Color(200, 120, 40));
        atlas = Texture2D::FromPixels(game.Graphics(), 16, 16, px.data());
        std::vector<float> s(4800);
        for (size_t i = 0; i < s.size(); ++i) s[i] = 0.2f * std::sin(i * 0.1f);
        blip = SoundEffect::FromSamples(s.data(), static_cast<int>(s.size()), 1, 48000);
        start = Now();
    };
    game.update = [&](Probe& p, const GameTime&) {
        const double t = Now() - start;
        updateTimes.push_back(Now());
        if (p.updates % 15 == 0) blip.Play(0.2f, static_cast<float>(std::sin(t)) * 0.5f);
        (void)Keyboard::GetState();
        (void)Mouse::GetState();
        (void)GamePad::GetState(0);
        if (p.updates % 1800 == 0) memory.push_back({t, ResidentMegabytes()});
        if (t > SoakMinutes * 60.0) p.Exit();
    };
    game.draw = [&](Probe& p, SpriteBatch& b, const GameTime& time) {
        b.Begin();
        for (int i = 0; i < 5000; ++i)
            b.Draw(atlas, Vector2(std::fmod(i * 7.0f + static_cast<float>(time.Total()) * 30, 1280.0f), static_cast<float>(i % 720)),
                   Color::White);
        b.End();
        (void)p;
    };
    game.Run();
    SetEnv("COLDX_HEADLESS", nullptr);
    // Memory: after the first minute, which includes warm-up, to the end.
    double first = -1, last = -1;
    for (auto& [t, mb] : memory) {
        if (t >= 60 && first < 0) first = mb;
        last = mb;
    }
    const double end = updateTimes.empty() ? 0 : updateTimes.back();
    const long long lastMinute = std::count_if(updateTimes.begin(), updateTimes.end(), [&](double t) { return t >= end - 60.5 && t < end - 0.5; });
    const bool ok = first > 0 && last - first < 5.0 && std::abs(lastMinute - 3600) <= 1;
    r.status = ok ? Status::Pass : Status::Fail;
    r.notes = Format("%d minutes headless with 5000 sprites, a sound every quarter second, input polling: memory %.1f MB "
                     "after one minute, %.1f MB at the end (%+.1f); %lld updates in the final minute (want 3600 +-1).",
                     SoakMinutes, first, last, last - first, lastMinute);
    return r;
}

} // namespace

void RegisterTiming() {
    AutoTests().push_back({"TIME-01", "P0", "Fixed timestep accuracy", FixedTimestep});
    AutoTests().push_back({"TIME-02", "P0", "Slow frames", Stalls});
    AutoTests().push_back({"TIME-03", "P1", "Vsync on/off", VsyncToggle});
    AutoTests().push_back({"TIME-04", "P0", "Long session", LongSession});
}

} // namespace ft
