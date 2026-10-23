#pragma once

#include <memory>
#include <string>
#include <vector>

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

enum class FullscreenMode {
    // A window covering the display at the desktop's resolution. Alt-tab is instant.
    Borderless,
    // The display switches to a resolution of its own (GameSettings::fullscreenWidth and
    // fullscreenHeight, or the desktop's). Alt-tab may flicker while the display changes.
    Exclusive,
};

// A resolution and refresh rate a display can switch to.
struct DisplayMode {
    int width = 0;
    int height = 0;
    float refreshRate = 0.0f;  // Hz; 0 where the display does not say
    friend bool operator==(const DisplayMode&, const DisplayMode&) = default;
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
    FullscreenMode fullscreenMode = FullscreenMode::Borderless;
    // Exclusive only: the resolution to switch to, or 0 for the desktop's. The closest the
    // display has is used. fullscreenRefreshRate 0 takes the highest at that size.
    int fullscreenWidth = 0;
    int fullscreenHeight = 0;
    float fullscreenRefreshRate = 0.0f;
    bool vsync = true;

    // One switch for a pixel-art game: integer scaling and sharp scaling for the virtual
    // resolution (scaleMode and smoothScaling are then ignored), and every SpriteBatch
    // defaulting to SamplerState::PointClamp and pixel snapping. A batch can still choose
    // otherwise per Begin, or with SetPixelSnap. Set a virtual resolution too.
    bool pixelArt = false;

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
    // Fixed timestep only. Off (XNA's way): a frame is drawn only after an update, so a
    // 144 Hz display shows 60 frames a second. On: every display frame is drawn, updates
    // still run at their own rate, and GameTime::Alpha says where between two updates the
    // frame falls, for drawing interpolated positions. With vsync off it draws as fast as
    // it can. Unlike the other settings, a change while running takes effect at once.
    bool drawBetweenUpdates = false;

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
    // Fullscreen is borderless at the desktop's resolution unless the mode says otherwise.
    void SetFullscreen(bool fullscreen);
    void ToggleFullscreen();
    bool IsFullscreen() const;
    // How fullscreen looks from now on; applied at once if the game is fullscreen. Width
    // and height 0 mean the desktop's resolution.
    void SetFullscreenMode(FullscreenMode mode, int width = 0, int height = 0, float refreshRate = 0.0f);
    // The modes the window's display offers, largest first, and its desktop mode. Empty
    // before Run and headless.
    std::vector<DisplayMode> DisplayModes() const;
    DisplayMode DesktopDisplayMode() const;
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
