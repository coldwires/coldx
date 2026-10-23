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
    std::unique_ptr<ContentManager> content = std::make_unique<ContentManager>(*graphics);
    SDL_Window* window = nullptr;
    bool headless = false;
    bool exitRequested = false;
    // The size OnResize last reported, so it fires on changes only: SDL also sends a size
    // event when the window is created.
    int reportedWidth = 0;
    int reportedHeight = 0;

    void ReportSize(Game& game, int width, int height);
    // Hands the virtual resolution settings to the device and to input.
    void ApplyPresentation();
    // Tells SDL which mode fullscreen uses, from the settings.
    void ApplyFullscreenMode();
    bool sdlStarted = false;
};

Game::Game() : impl_(std::make_unique<Impl>()) {}

void Game::Impl::ReportSize(Game& game, int width, int height) {
    if (width == reportedWidth && height == reportedHeight) return;
    reportedWidth = width;
    reportedHeight = height;
    game.OnResize(width, height);
}

void Game::Impl::ApplyFullscreenMode() {
    if (!window || headless) return;
    if (settings.fullscreenMode == FullscreenMode::Borderless) {
        SDL_SetWindowFullscreenMode(window, nullptr);
        return;
    }
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    SDL_DisplayMode mode{};
    const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display);
    const int w = settings.fullscreenWidth > 0 ? settings.fullscreenWidth : (desktop ? desktop->w : 0);
    const int h = settings.fullscreenHeight > 0 ? settings.fullscreenHeight : (desktop ? desktop->h : 0);
    if (!SDL_GetClosestFullscreenDisplayMode(display, w, h, settings.fullscreenRefreshRate, false, &mode)) {
        SDL_Log("coldx.core: no fullscreen mode near %dx%d (%s); using borderless", w, h, SDL_GetError());
        SDL_SetWindowFullscreenMode(window, nullptr);
        return;
    }
    if (!SDL_SetWindowFullscreenMode(window, &mode))
        SDL_Log("coldx.core: could not set fullscreen mode %dx%d: %s", mode.w, mode.h, SDL_GetError());
    else
        SDL_Log("coldx.core: exclusive fullscreen %dx%d at %.0f Hz", mode.w, mode.h, mode.refresh_rate);
}

void Game::Impl::ApplyPresentation() {
    detail::Presentation p;
    if (settings.virtualWidth > 0 && settings.virtualHeight > 0) {
        p.width = settings.virtualWidth;
        p.height = settings.virtualHeight;
    }
    p.mode = settings.pixelArt ? ScaleMode::Integer : settings.scaleMode;
    p.smooth = settings.pixelArt ? false : settings.smoothScaling;
    detail::GraphicsAccess::SetPixelArt(*graphics, settings.pixelArt);
    p.letterbox = settings.letterboxColor;
    detail::GraphicsAccess::SetPresentation(*graphics, p);
    detail::Input::Get().presentation = p;
}

// The device outlives Run and goes here, after the derived game's members (its textures and
// batches) have been destroyed, so they release against a live device.
Game::~Game() {
    // Components may hold GPU resources: they go before the device.
    impl_->components.reset();
    impl_->content.reset();
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

ContentManager& Game::Content() {
    return *impl_->content;
}

void Game::SetFullscreen(bool fullscreen) {
    impl_->settings.fullscreen = fullscreen;
    if (impl_->window && !impl_->headless && !SDL_SetWindowFullscreen(impl_->window, fullscreen))
        SDL_Log("coldx.core: could not change fullscreen: %s", SDL_GetError());
}

void Game::ToggleFullscreen() {
    SetFullscreen(!IsFullscreen());
}

void Game::SetFullscreenMode(FullscreenMode mode, int width, int height, float refreshRate) {
    GameSettings& c = impl_->settings;
    c.fullscreenMode = mode;
    c.fullscreenWidth = std::max(0, width);
    c.fullscreenHeight = std::max(0, height);
    c.fullscreenRefreshRate = std::max(0.0f, refreshRate);
    impl_->ApplyFullscreenMode();
}

std::vector<DisplayMode> Game::DisplayModes() const {
    std::vector<DisplayMode> result;
    if (!impl_->window || impl_->headless) return result;
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetDisplayForWindow(impl_->window), &count);
    for (int i = 0; modes && i < count; ++i) result.push_back({modes[i]->w, modes[i]->h, modes[i]->refresh_rate});
    SDL_free(modes);
    return result;
}

DisplayMode Game::DesktopDisplayMode() const {
    if (!impl_->window || impl_->headless) return {};
    const SDL_DisplayMode* m = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(impl_->window));
    return m ? DisplayMode{m->w, m->h, m->refresh_rate} : DisplayMode{};
}

bool Game::IsFullscreen() const {
    if (impl_->window && !impl_->headless) return (SDL_GetWindowFlags(impl_->window) & SDL_WINDOW_FULLSCREEN) != 0;
    return impl_->settings.fullscreen;
}

void Game::SetWindowSize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    impl_->settings.width = width;
    impl_->settings.height = height;
    if (!impl_->window) return;
    if (impl_->headless) {
        detail::GraphicsAccess::ResizeHeadless(*impl_->graphics, width, height);
        if (impl_->settings.virtualWidth <= 0) impl_->ReportSize(*this, width, height);
    } else if (!SDL_SetWindowSize(impl_->window, width, height)) {
        SDL_Log("coldx.core: could not resize the window: %s", SDL_GetError());
    }
}

void Game::SetWindowTitle(const std::string& title) {
    impl_->settings.title = title;
    if (impl_->window) SDL_SetWindowTitle(impl_->window, title.c_str());
}

void Game::SetVSync(bool vsync) {
    impl_->settings.vsync = vsync;
    detail::GraphicsAccess::SetVSync(*impl_->graphics, vsync);
}

void Game::SetVirtualResolution(int width, int height) {
    Impl& s = *impl_;
    if (width <= 0 || height <= 0) width = height = 0;
    s.settings.virtualWidth = width;
    s.settings.virtualHeight = height;
    if (!s.window) return;
    s.ApplyPresentation();
    if (width > 0) {
        s.ReportSize(*this, width, height);
    } else if (s.headless) {
        s.ReportSize(*this, s.settings.width, s.settings.height);
        detail::GraphicsAccess::ResizeHeadless(*s.graphics, s.settings.width, s.settings.height);
    } else {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(s.window, &w, &h);
        s.ReportSize(*this, w, h);
    }
}

void Game::SetScaleMode(ScaleMode mode, bool smooth) {
    impl_->settings.scaleMode = mode;
    impl_->settings.smoothScaling = smooth;
    if (impl_->window) impl_->ApplyPresentation();
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
    // Fullscreen is entered once the window exists, after its mode is chosen.
    const bool startFullscreen = cfg.fullscreen && !debug.headless;
    // Hidden until its fullscreen state is set, so a fullscreen start never shows a window.
    flags |= SDL_WINDOW_HIDDEN;
    s.window = SDL_CreateWindow(cfg.title.c_str(), cfg.width, cfg.height, flags);
    if (!s.window) {
        ReportFatal("Could not create the window", dialogs);
        detail::StopLogFile();
        return 1;
    }
    s.headless = debug.headless;
    s.ApplyFullscreenMode();
    if (startFullscreen && !SDL_SetWindowFullscreen(s.window, true))
        SDL_Log("coldx.core: could not start fullscreen: %s", SDL_GetError());
    if (!debug.headless) {
        SDL_ShowWindow(s.window);
        SDL_SyncWindow(s.window);
    }
    SDL_Window* presentTo = debug.headless ? nullptr : s.window;
    s.ApplyPresentation();
    if (!detail::GraphicsAccess::Open(*s.graphics, presentTo, cfg.vsync, cfg.width, cfg.height)) {
        ReportFatal("Could not create the graphics device", dialogs);
        SDL_DestroyWindow(s.window);
        s.window = nullptr;
        detail::StopLogFile();
        return 1;
    }

    detail::Input& input = detail::Input::Get();
    input.Start(s.window);
    s.ApplyPresentation();
    detail::Audio& audio = detail::Audio::Get();
    audio.Start(debug.headless);
    detail::InputScript script;
    script.Load(debug.inputPath);

    s.reportedWidth = s.graphics->BackBufferWidth();
    s.reportedHeight = s.graphics->BackBufferHeight();

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
                // Headless, the back buffer does not follow the hidden window; with a virtual
                // resolution it does not follow the window at all.
                if (!s.headless && s.settings.virtualWidth <= 0) s.ReportSize(*this, event.window.data1, event.window.data2);
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
            if (accumulator < step && !cfg.drawBetweenUpdates) {
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
            drawTime = GameTime(static_cast<float>(step), total, slow,
                                cfg.drawBetweenUpdates ? static_cast<float>(std::min(accumulator / step, 0.999)) : 0.0f);
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
