#include "debug_options.h"

#include <cstdio>
#include <cstdlib>

#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace coldx::detail {

static std::string Env(const char* name) {
    const char* value = SDL_getenv(name);
    return value ? value : "";
}

DebugOptions DebugOptions::FromEnvironment() {
    DebugOptions o;
    const std::string console = Env("COLDX_CONSOLE");
    if (console == "1") o.console = true;
    if (console == "0") o.console = false;
    o.logPath = Env("COLDX_LOG");
    o.headless = Env("COLDX_HEADLESS") == "1";
    o.frames = std::atoi(Env("COLDX_FRAMES").c_str());
    o.capturePath = Env("COLDX_CAPTURE");
    return o;
}

void ShowConsole() {
#ifdef _WIN32
    // A launcher that redirected output wants it where it pointed, not in a new window.
    const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err && err != INVALID_HANDLE_VALUE && GetFileType(err) != FILE_TYPE_UNKNOWN) return;

    if (!AllocConsole()) return;
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
#endif
}

namespace {

FILE* logFile = nullptr;
SDL_LogOutputFunction previousOutput = nullptr;
void* previousUserdata = nullptr;

void SDLCALL LogToFile(void* userdata, int category, SDL_LogPriority priority, const char* message) {
    (void)userdata;
    if (logFile) {
        std::fprintf(logFile, "[%10.3f] %s\n", SDL_GetTicks() / 1000.0, message);
        // Flushed per line so the file is complete up to a crash.
        std::fflush(logFile);
    }
    if (previousOutput) previousOutput(previousUserdata, category, priority, message);
}

} // namespace

bool StartLogFile(const std::string& path) {
    if (logFile) return true;
#ifdef _WIN32
    if (fopen_s(&logFile, path.c_str(), "w") != 0) logFile = nullptr;
#else
    logFile = std::fopen(path.c_str(), "w");
#endif
    if (!logFile) {
        SDL_Log("coldx.core: could not open log file %s", path.c_str());
        return false;
    }
    SDL_GetLogOutputFunction(&previousOutput, &previousUserdata);
    SDL_SetLogOutputFunction(LogToFile, nullptr);
    return true;
}

void StopLogFile() {
    if (!logFile) return;
    SDL_SetLogOutputFunction(previousOutput, previousUserdata);
    std::fclose(logFile);
    logFile = nullptr;
}

} // namespace coldx::detail
