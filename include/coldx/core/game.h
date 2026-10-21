#pragma once

#include <memory>
#include <string>

#include <coldx/core/content_manager.h>
#include <coldx/core/game_component.h>
#include <coldx/core/game_time.h>
#include <coldx/core/graphics_device.h>

namespace coldx {

enum class ConsoleMode {
    DebugBuilds,
    Always,
    Never,
};

// Read once, when Run starts. Set them in the game's constructor.
struct GameSettings {
    std::string title = "coldx";
    // Name the Storage folder: <per-user app data>/<company>/<name>. name defaults to the
    // title. Pick them once; changing them later moves the player's saves out of reach.
    std::string company;
    std::string name;
    int width = 1280;
    int height = 720;
    bool resizable = false;
    bool fullscreen = false;
    bool vsync = true;

    // A fixed size to draw at, scaled into the window: the back buffer is this size, and
    // mouse positions are in its pixels. 0 x 0: the back buffer is the window's pixel size.
    int virtualWidth = 0;
    int virtualHeight = 0;
    ScaleMode scaleMode = ScaleMode::Fit;
    // Linear filtering when scaling. Off keeps pixel art sharp.
    bool smoothScaling = false;
    // The bars beside a scaled image.
    Color letterboxColor = Color::Black;

    bool fixedTimeStep = true;
    double targetElapsedSeconds = 1.0 / 60.0;
    // Most updates run in one frame. Time beyond that is dropped, so a stall (a breakpoint,
    // a slow load) slows the game down instead of making it run hundreds of updates to catch
    // up. Variable timestep clamps Delta to the same total.
    int maxUpdatesPerFrame = 5;

    // Windows only: whether to open a console window for log output. Other platforms log to
    // the terminal that launched the game. Skipped when output is already redirected.
    ConsoleMode console = ConsoleMode::DebugBuilds;
};

class Game {
public:
    Game();
    virtual ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Opens the window and runs the loop until Exit is called or the window is closed.
    // Returns 0 on a normal exit and 1 if the window or graphics device could not be created.
    int Run();

    // Ends the loop after the current update or draw returns.
    void Exit();

    GameSettings& Settings();

    // The window while the game runs. Before Run they set Settings(); during Run they
    // change the window at once. Headless, there is no window to change: a size change
    // resizes the back buffer and calls OnResize, and fullscreen is only remembered.
    // Fullscreen is borderless at the desktop's resolution.
    void SetFullscreen(bool fullscreen);
    void ToggleFullscreen();
    bool IsFullscreen() const;
    // In window coordinates; the back buffer follows in pixels, which differ on high-DPI
    // displays.
    void SetWindowSize(int width, int height);
    void SetWindowTitle(const std::string& title);
    void SetVSync(bool vsync);
    // A virtual resolution while running, or 0 x 0 to follow the window again. Calls
    // OnResize when the back buffer's size changes. Takes effect at the next frame.
    void SetVirtualResolution(int width, int height);
    void SetScaleMode(ScaleMode mode, bool smooth = false);
    // Exists from construction, so members can hold a reference to it. It is usable from
    // Initialize on.
    GraphicsDevice& Graphics();

    // Components update after this game's Update and draw after its Draw. They initialize
    // after its Initialize and before its LoadContent.
    GameComponentCollection& Components();

    // Content loaded by name from RootDirectory ("content" beside the executable).
    ContentManager& Content();

protected:
    virtual void Initialize() {}
    virtual void LoadContent() {}
    virtual void UnloadContent() {}
    virtual void Update(const GameTime&) {}
    virtual void Draw(const GameTime&) {}

    // The back buffer's new size in pixels. Only on a real change, not at startup. With a
    // virtual resolution the back buffer keeps its size, so resizing the window does not
    // call it.
    virtual void OnResize(int /*width*/, int /*height*/) {}
    virtual void OnFocusChanged(bool /*focused*/) {}
    // Called once when the loop ends, before UnloadContent.
    virtual void OnExiting() {}

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace coldx
