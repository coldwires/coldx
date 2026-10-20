#pragma once

#include <optional>
#include <string>

namespace coldx::detail {

// Read from COLDX_* environment variables when Run starts, so they work on any game
// without the game passing anything through. README.md lists them.
struct DebugOptions {
    std::optional<bool> console;
    std::string logPath;
    bool headless = false;
    int frames = 0;
    std::string capturePath;
    std::string inputPath;

    static DebugOptions FromEnvironment();
};

// Windows: opens a console window for stdout and stderr, unless they are already redirected
// to a file or pipe. Elsewhere does nothing, since output already goes to the launching terminal.
void ShowConsole();

// Copies every SDL_Log line to a file, as well as the normal output.
bool StartLogFile(const std::string& path);
void StopLogFile();

} // namespace coldx::detail
