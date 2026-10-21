#include "framework_test.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// wingdi.h declares a function named Rectangle, which collides with coldx::Rectangle.
#define NOGDI
#include <windows.h>
#include <psapi.h>
#endif

using namespace coldx;

namespace ft {

const char* StatusName(Status status) {
    switch (status) {
    case Status::Pass: return "PASS";
    case Status::Fail: return "FAIL";
    case Status::NeedsHuman: return "NEEDS HUMAN CHECK";
    case Status::Missing: return "MISSING FEATURE";
    case Status::Skipped: return "SKIPPED";
    }
    return "?";
}

std::vector<AutoTest>& AutoTests() {
    static std::vector<AutoTest> tests;
    return tests;
}

std::vector<HumanTest>& HumanTests() {
    static std::vector<HumanTest> tests;
    return tests;
}

std::vector<Result>& MissingFeatures() {
    static std::vector<Result> missing;
    return missing;
}

std::string Format(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

SDL_Window* OnlyWindow() {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* w = count >= 1 ? windows[0] : nullptr;
    SDL_free(windows);
    return w;
}

bool Near(Color a, Color b, int tolerance) {
    return std::abs(a.r - b.r) <= tolerance && std::abs(a.g - b.g) <= tolerance && std::abs(a.b - b.b) <= tolerance;
}

Texture2D Solid(GraphicsDevice& graphics, Color color) {
    return Texture2D::FromPixels(graphics, 1, 1, &color);
}

double ResidentMegabytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return static_cast<double>(counters.WorkingSetSize) / (1024.0 * 1024.0);
#endif
    return -1.0;
}

double Now() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    return std::chrono::duration<double>(clock::now() - start).count();
}

void SetEnv(const char* name, const char* value) {
    SDL_Environment* env = SDL_GetEnvironment();
    if (value)
        SDL_SetEnvironmentVariable(env, name, value, true);
    else
        SDL_UnsetEnvironmentVariable(env, name);
}

std::string TempPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / ("coldx_ft_" + name)).string();
}

bool Capture::Load(const std::string& path) {
    SDL_Surface* loaded = SDL_LoadPNG(path.c_str());
    if (!loaded) return false;
    SDL_Surface* s = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!s) return false;
    width = s->w;
    height = s->h;
    pixels.resize(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        const auto* row = static_cast<const std::uint8_t*>(s->pixels) + y * s->pitch;
        for (int x = 0; x < width; ++x)
            pixels[static_cast<size_t>(y) * width + x] = Color(row[x * 4], row[x * 4 + 1], row[x * 4 + 2], row[x * 4 + 3]);
    }
    SDL_DestroySurface(s);
    return true;
}

HumanScene::HumanScene(const HumanTest& test) : test_(test) {
    Settings().title = "coldx framework test " + test.id;
    Settings().console = ConsoleMode::Never;
    Settings().resizable = true;
}

void HumanScene::LoadContent() {
    font_ = SpriteFont::FromFile(Graphics(), FontPath, 20.0f);
    const Color white = Color::White;
    white_ = Texture2D::FromPixels(Graphics(), 1, 1, &white);
    if (load) load();
}

void HumanScene::Update(const GameTime& time) {
    // A scene taking typed text needs P and F as letters: F1 and F2 judge it instead.
    const bool typing = TextInput::IsActive();
    if (Keyboard::WasPressed(typing ? Key::F1 : Key::P)) {
        verdict_ = Status::Pass;
        Exit();
    } else if (Keyboard::WasPressed(typing ? Key::F2 : Key::F)) {
        verdict_ = Status::Fail;
        Exit();
    } else if (Keyboard::WasPressed(Key::Escape)) {
        verdict_ = Status::Skipped;
        Exit();
    }
    if (update) update(time);
}

void HumanScene::Draw(const GameTime& time) {
    Graphics().Clear(Color(18, 20, 28));
    if (draw) draw(batch_, time);

    std::string text = test_.id + " (" + test_.priority + "): " + test_.title + "\n";
    for (const std::string& line : test_.instructions) text += line + "\n";
    text += TextInput::IsActive() ? "F1 = pass   F2 = fail   Escape = skip\n\n" : "P = pass   F = fail   Escape = skip\n\n";
    if (readout)
        for (const std::string& line : readout()) text += line + "\n";

    batch_.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
    const Vector2 size = font_.MeasureString(text);
    batch_.Draw(white_, Rectangle(10, 10, static_cast<int>(size.x) + 20, static_cast<int>(size.y) + 16), Color::Black * 0.75f);
    batch_.DrawString(font_, text, Vector2(20, 18), Color::White);
    batch_.End();
}

} // namespace ft
