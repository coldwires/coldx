#pragma once

// The framework stress tests: automated ones that measure and decide, human ones that show
// instructions and live readouts and record the tester's verdict, and entries for what
// coldx does not have. One runner, coldx_framework_tests, for all of them.

#include <coldx/coldx.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

struct SDL_Window;

namespace ft {

enum class Status {
    Pass,
    Fail,
    NeedsHuman,
    Missing,  // the test needs something coldx does not have
    Skipped,
};

const char* StatusName(Status status);

struct Result {
    Result() = default;
    // A constructor, not aggregate initialisation: GCC warns on every Result{id, priority}
    // that leaves the rest out.
    Result(std::string id_, std::string priority_, Status status_ = Status::Skipped, std::string notes_ = {})
        : id(std::move(id_)), priority(std::move(priority_)), status(status_), notes(std::move(notes_)) {}
    std::string id;
    std::string priority;
    Status status = Status::Skipped;
    std::string notes;
};

// A test run without a person: does its work in one or more Game runs, decides, explains.
struct AutoTest {
    std::string id;
    std::string priority;
    std::string title;
    std::function<Result()> run;
    // Changes the desktop (the display mode), which can leave Windows' taskbar laid out for
    // the wrong resolution: run only when asked for by --only.
    bool onlyWhenNamed = false;
};

// A test a person has to judge: instructions, and a scene with live readouts.
struct HumanTest {
    std::string id;
    std::string priority;
    std::string title;
    std::vector<std::string> instructions;
    // Builds the scene's game. The runner adds the instruction overlay and the P/F/S keys.
    std::function<void(class HumanScene&)> setup;
};

std::vector<AutoTest>& AutoTests();
std::vector<HumanTest>& HumanTests();
// Entries for features coldx lacks, so the report lists them.
std::vector<Result>& MissingFeatures();

// Register* are called once, from main, before anything runs.
void RegisterGraphics();
void RegisterGraphicsMore();
void RegisterTiming();
void RegisterAudio();
void RegisterInput();
void RegisterPlatform();
void RegisterHuman();
void RegisterFeatures();

// The scene a HumanTest runs in. Tests fill in the hooks; the runner draws the
// instructions and readout lines over whatever the scene draws, and ends the scene on
// P (pass), F (fail) or Escape (skip). Scenes must not use those keys.
class HumanScene : public coldx::Game {
public:
    HumanScene(const HumanTest& test);

    std::function<void()> load;
    std::function<void(const coldx::GameTime&)> update;
    std::function<void(coldx::SpriteBatch&, const coldx::GameTime&)> draw;
    // Lines shown under the instructions, recomputed every frame.
    std::function<std::vector<std::string>()> readout;

    coldx::SpriteBatch& Batch() { return batch_; }
    const coldx::SpriteFont& Font() const { return font_; }
    const coldx::Texture2D& White() const { return white_; }
    Status Verdict() const { return verdict_; }

protected:
    void LoadContent() override;
    void Update(const coldx::GameTime& time) override;
    void Draw(const coldx::GameTime& time) override;

private:
    const HumanTest& test_;
    coldx::SpriteBatch batch_{Graphics()};
    coldx::SpriteFont font_;
    coldx::Texture2D white_;
    Status verdict_ = Status::Skipped;
};

// Helpers the tests share.

// printf into a std::string.
std::string Format(const char* format, ...);
// The game's window during a Run, or null.
SDL_Window* OnlyWindow();
// Same RGB within tolerance; alpha ignored.
bool Near(coldx::Color a, coldx::Color b, int tolerance);
// A 1x1 texture of one color.
coldx::Texture2D Solid(coldx::GraphicsDevice& graphics, coldx::Color color);

// The process's resident memory in megabytes, or -1 where it cannot be read.
double ResidentMegabytes();
// Seconds since an arbitrary start, high resolution.
double Now();
// Sets or clears a COLDX_* / SDL environment variable for the next Game run.
void SetEnv(const char* name, const char* value);
// A path in the temp folder for this run's files.
std::string TempPath(const std::string& name);
// The font every scene uses, copied beside the runner by the build.
constexpr const char* FontPath = "content/LiberationSans-Regular.ttf";

// A plain game for automated tests: variable or fixed step as asked, a hook per Update,
// and the frame count. Exit from the hook.
class Probe : public coldx::Game {
public:
    std::function<void()> load;
    std::function<void(Probe&, const coldx::GameTime&)> update;
    std::function<void(Probe&, coldx::SpriteBatch&, const coldx::GameTime&)> draw;
    std::function<void(int, int)> resized;
    std::function<void(bool)> focused;
    int updates = 0;
    int draws = 0;

    Probe() { Settings().console = coldx::ConsoleMode::Never; }
    coldx::SpriteBatch& Batch() { return batch_; }

protected:
    void LoadContent() override {
        if (load) load();
    }
    void Update(const coldx::GameTime& time) override {
        ++updates;
        if (update) update(*this, time);
    }
    void Draw(const coldx::GameTime& time) override {
        ++draws;
        Graphics().Clear(coldx::Color::Black);
        if (draw) draw(*this, batch_, time);
    }
    void OnResize(int w, int h) override {
        if (resized) resized(w, h);
    }
    void OnFocusChanged(bool f) override {
        if (focused) focused(f);
    }

private:
    coldx::SpriteBatch batch_{Graphics()};
};

// A captured frame (COLDX_CAPTURE), loaded as RGBA. Load is false if it cannot be read.
struct Capture {
    int width = 0;
    int height = 0;
    std::vector<coldx::Color> pixels;
    bool Load(const std::string& path);
    coldx::Color At(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
};

} // namespace ft
