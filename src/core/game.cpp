#include <coldx/core/game.h>

#include <algorithm>

#include <SDL3/SDL.h>

#include "audio.h"
#include "debug_options.h"
#include "graphics_access.h"
#include "input.h"
#include "input_script.h"
#include "storage_names.h"

namespace coldx {

struct Game::Impl {
    GameSettings settings;
    std::unique_ptr<GraphicsDevice> graphics = detail::GraphicsAccess::Create();
    std::unique_ptr<GameComponentCollection> components = std::make_unique<GameComponentCollection>();
    SDL_Window* window = nullptr;
    bool exitRequested = false;
    bool sdlStarted = false;
};

Game::Game() : impl_(std::make_unique<Impl>()) {}

// The device outlives Run and goes here, after the derived game's members (its textures and
// batches) have been destroyed, so they release against a live device.
Game::~Game() {
    // Components may hold GPU resources: they go before the device.
    impl_->components.reset();
    impl_->graphics.reset();
    if (impl_->sdlStarted) SDL_Quit();
}

GameSettings& Game::Settings() {
    return impl_->settings;
}

GraphicsDevice& Game::Graphics() {
    return *impl_->graphics;
}

GameComponentCollection& Game::Components() {
    return *impl_->components;
}

void Game::Exit() {
    impl_->exitRequested = true;
}

static void ReportFatal(const char* what, bool showDialog) {
    SDL_Log("coldx.core: %s: %s", what, SDL_GetError());
    if (showDialog) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "coldx", what, nullptr);
}

static bool WantConsole(ConsoleMode mode, const detail::DebugOptions& options) {
    if (options.console) return *options.console;
    switch (mode) {
    case ConsoleMode::Always:
        return true;
    case ConsoleMode::Never:
        return false;
    case ConsoleMode::DebugBuilds:
    default:
#ifdef NDEBUG
        return false;
#else
        return true;
#endif
    }
}

int Game::Run() {
    Impl& s = *impl_;
    const GameSettings& cfg = s.settings;
    const detail::DebugOptions debug = detail::DebugOptions::FromEnvironment();
    s.exitRequested = false;

    if (WantConsole(cfg.console, debug)) detail::ShowConsole();
    if (!debug.logPath.empty()) detail::StartLogFile(debug.logPath);
    if (debug.headless) SDL_Log("coldx.core: headless, %dx%d", cfg.width, cfg.height);
    detail::SetStorageNames(cfg.company, cfg.name.empty() ? cfg.title : cfg.name);

    // A modal dialog would hang a headless run with nobody to dismiss it.
    const bool dialogs = !debug.headless;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        ReportFatal("Could not initialize SDL", dialogs);
        detail::StopLogFile();
        return 1;
    }
    s.sdlStarted = true;

    SDL_WindowFlags flags = 0;
    if (cfg.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (cfg.fullscreen && !debug.headless) flags |= SDL_WINDOW_FULLSCREEN;
    if (debug.headless) flags |= SDL_WINDOW_HIDDEN;
    s.window = SDL_CreateWindow(cfg.title.c_str(), cfg.width, cfg.height, flags);
    if (!s.window) {
        ReportFatal("Could not create the window", dialogs);
        detail::StopLogFile();
        return 1;
    }
    SDL_Window* presentTo = debug.headless ? nullptr : s.window;
    if (!detail::GraphicsAccess::Open(*s.graphics, presentTo, cfg.vsync, cfg.width, cfg.height)) {
        ReportFatal("Could not create the graphics device", dialogs);
        SDL_DestroyWindow(s.window);
        s.window = nullptr;
        detail::StopLogFile();
        return 1;
    }

    detail::Input& input = detail::Input::Get();
    input.Start(s.window);
    detail::Audio& audio = detail::Audio::Get();
    audio.Start(debug.headless);
    detail::InputScript script;
    script.Load(debug.inputPath);

    Initialize();
    s.components->InitializeAll();
    LoadContent();

    const double step = cfg.targetElapsedSeconds > 0.0 ? cfg.targetElapsedSeconds : 1.0 / 60.0;
    const int maxUpdates = std::max(1, cfg.maxUpdatesPerFrame);
    const double maxElapsed = step * maxUpdates;

    double total = 0.0;
    double accumulator = 0.0;
    long long updates = 0;
    long long frames = 0;
    long long slowFrames = 0;
    Uint64 previous = SDL_GetTicksNS();

    // Every Update goes through here: scripted input, then the per-Update input copies. A
    // scripted exit stops before the Update it is numbered for.
    auto runUpdate = [&](const GameTime& time) {
        if (script.Apply(static_cast<int>(updates + 1), input)) {
            s.exitRequested = true;
            return;
        }
        ++updates;
        input.BeginUpdate();
        Update(time);
        s.components->UpdateAll(time);
    };

    while (!s.exitRequested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input.HandleEvent(event);
            switch (event.type) {
            case SDL_EVENT_QUIT:
                s.exitRequested = true;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                OnResize(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                OnFocusChanged(true);
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                OnFocusChanged(false);
                break;
            default:
                break;
            }
        }
        if (s.exitRequested) break;

        const Uint64 now = SDL_GetTicksNS();
        const double elapsed = std::min(static_cast<double>(now - previous) / 1e9, maxElapsed);
        previous = now;

        GameTime drawTime;
        if (cfg.fixedTimeStep) {
            accumulator = std::min(accumulator + elapsed, maxElapsed);
            if (accumulator < step) {
                SDL_DelayPrecise(static_cast<Uint64>((step - accumulator) * 1e9));
                continue;
            }
            const bool slow = accumulator >= 2.0 * step;
            if (slow) ++slowFrames;
            while (accumulator >= step && !s.exitRequested) {
                accumulator -= step;
                total += step;
                runUpdate(GameTime(static_cast<float>(step), total, slow));
            }
            drawTime = GameTime(static_cast<float>(step), total, slow);
        } else {
            total += elapsed;
            drawTime = GameTime(static_cast<float>(elapsed), total, false);
            runUpdate(drawTime);
        }
        if (s.exitRequested) break;

        if (detail::GraphicsAccess::BeginFrame(*s.graphics)) {
            Draw(drawTime);
            s.components->DrawAll(drawTime);
            ++frames;
        } else if (SDL_GetWindowFlags(s.window) & SDL_WINDOW_MINIMIZED) {
            // Nothing is presented while minimized, so nothing else paces the loop.
            SDL_Delay(16);
        }
        detail::GraphicsAccess::EndFrame(*s.graphics);
        audio.Pump();

        if (debug.frames > 0 && frames >= debug.frames) s.exitRequested = true;
    }

    SDL_Log("coldx.core: exit after %lld frames, %lld updates, %lld slow frames, %.3fs game time",
            frames, updates, slowFrames, total);
    if (!debug.capturePath.empty()) {
        if (detail::GraphicsAccess::SaveBackBuffer(*s.graphics, debug.capturePath))
            SDL_Log("coldx.core: captured %dx%d to %s", s.graphics->BackBufferWidth(),
                    s.graphics->BackBufferHeight(), debug.capturePath.c_str());
    }

    OnExiting();
    s.components->UnloadAll();
    UnloadContent();
    audio.Stop();
    input.Stop();

    detail::GraphicsAccess::ReleaseWindow(*s.graphics);
    SDL_DestroyWindow(s.window);
    s.window = nullptr;
    detail::StopLogFile();
    return 0;
}

} // namespace coldx
