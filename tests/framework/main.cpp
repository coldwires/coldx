// coldx_framework_tests: runs the framework stress tests and writes the report.
//
//   coldx_framework_tests --auto              every automated test, then the report
//   coldx_framework_tests --auto --only ID    one test; FS-01 runs only this way, since
//                                             switching the display mode can upset the taskbar
//   coldx_framework_tests --auto --soak 240   include TIME-04 for that many minutes
//   coldx_framework_tests                     menu of the tests a person judges
//   --report PATH                             where the report goes (framework_test_report.md)
//
// Automated tests open real windows, go fullscreen briefly and move the mouse pointer.

#include "framework_test.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>

#include <SDL3/SDL.h>

using namespace coldx;

namespace ft {
extern int SoakMinutes;
}

namespace {

using namespace ft;

const char* const SpecIds[] = {"GFX-01", "GFX-02", "GFX-03", "GFX-04", "GFX-05", "GFX-06", "GFX-07", "GFX-08", "GFX-09",
                               "TIME-01", "TIME-02", "TIME-03", "TIME-04", "AUD-01", "AUD-02", "AUD-03", "AUD-04",
                               "AUD-05", "INP-01", "INP-02", "INP-03", "INP-04", "INP-05", "INP-06", "INP-07",
                               "PLAT-01", "PLAT-02", "PLAT-03", "PLAT-04"};

std::string PriorityOf(const std::string& id) {
    for (const AutoTest& t : AutoTests())
        if (t.id == id) return t.priority;
    for (const HumanTest& t : HumanTests())
        if (t.id == id) return t.priority;
    for (const Result& m : MissingFeatures())
        if (m.id == id) return m.priority;
    return "";
}

// The menu of human tests. Exits with the chosen index, or -1 to quit.
class Menu : public Game {
public:
    Menu(const std::map<std::string, Status>& verdicts) : verdicts_(verdicts) {
        Settings().title = "coldx framework tests";
        Settings().console = ConsoleMode::Never;
    }
    int choice = -1;

protected:
    void LoadContent() override { font_ = SpriteFont::FromFile(Graphics(), FontPath, 20.0f); }
    void Update(const GameTime&) override {
        const int n = static_cast<int>(HumanTests().size());
        if (Keyboard::WasPressed(Key::Down)) selected_ = (selected_ + 1) % n;
        if (Keyboard::WasPressed(Key::Up)) selected_ = (selected_ + n - 1) % n;
        if (Keyboard::WasPressed(Key::Enter)) {
            choice = selected_;
            Exit();
        }
        if (Keyboard::WasPressed(Key::Escape)) Exit();
    }
    void Draw(const GameTime&) override {
        Graphics().Clear(Color(18, 20, 28));
        std::string text = "Tests a person judges. Up/Down choose, Enter run, Escape finish and write the report.\n\n";
        for (size_t i = 0; i < HumanTests().size(); ++i) {
            const HumanTest& t = HumanTests()[i];
            auto v = verdicts_.find(t.id);
            text += std::string(static_cast<int>(i) == selected_ ? "> " : "  ") + t.id + "  " + t.title +
                    (v != verdicts_.end() ? std::string("   [") + StatusName(v->second) + "]" : "") + "\n";
        }
        batch_.Begin();
        batch_.DrawString(font_, text, Vector2(30, 30), Color::White);
        batch_.End();
    }

private:
    const std::map<std::string, Status>& verdicts_;
    SpriteBatch batch_{Graphics()};
    SpriteFont font_;
    int selected_ = 0;
};

void WriteReport(const std::string& path, const std::map<std::string, Result>& autos, const std::map<std::string, Status>& human) {
    std::string out = "# coldx framework stress test report\n\n";
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = display ? SDL_GetCurrentDisplayMode(display) : nullptr;
    out += Format("Platform %s, %d logical CPUs, %d MB RAM", SDL_GetPlatform(), SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
    if (mode) out += Format(", primary display %dx%d at %.0f Hz", mode->w, mode->h, mode->refresh_rate);
    out += ".\n\n| ID | Priority | Result | Notes / suspected cause |\n|----|----------|--------|-------------------------|\n";
    std::vector<std::string> fails;
    for (const char* id : SpecIds) {
        std::string status = "NOT RUN", notes;
        if (auto a = autos.find(id); a != autos.end()) {
            status = StatusName(a->second.status);
            notes = a->second.notes;
            if (a->second.status == Status::Fail) fails.push_back(std::string(id) + ": " + a->second.notes);
        }
        for (const Result& m : MissingFeatures())
            if (m.id == id) {
                status = StatusName(Status::Missing);
                notes = m.notes;
            }
        if (auto h = human.find(id); h != human.end()) {
            notes += std::string(notes.empty() ? "" : " ") + "Human check: " + StatusName(h->second) + ".";
            if (h->second == Status::Fail) fails.push_back(std::string(id) + ": failed the human check.");
            if (h->second == Status::Pass && status == StatusName(Status::NeedsHuman)) status = "PASS";
            if (h->second == Status::Fail) status = "FAIL";
        } else if (status == "NOT RUN") {
            for (const HumanTest& t : HumanTests())
                if (t.id == id) {
                    status = StatusName(Status::NeedsHuman);
                    for (const std::string& line : t.instructions) notes += line + " ";
                }
        }
        out += "| " + std::string(id) + " | " + PriorityOf(id) + " | " + status + " | " + notes + " |\n";
    }
    out += "\n## Missing features\n\n";
    for (const Result& m : MissingFeatures()) out += "- " + m.id + " (" + m.priority + "): " + m.notes + "\n";
    out += "\n## Failures\n\n";
    if (fails.empty()) out += "None.\n";
    for (const std::string& f : fails) out += "- " + f + "\n";
    std::ofstream(path, std::ios::binary) << out;
    std::printf("\nReport written to %s\n", path.c_str());
}

} // namespace

int main(int argc, char** argv) {
    bool automatic = false;
    std::string only, report = "framework_test_report.md";
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--auto")) automatic = true;
        else if (!std::strcmp(argv[i], "--only") && i + 1 < argc) only = argv[++i];
        else if (!std::strcmp(argv[i], "--soak") && i + 1 < argc) SoakMinutes = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--report") && i + 1 < argc) report = argv[++i];
    }

    RegisterGraphics();
    RegisterGraphicsMore();
    RegisterTiming();
    RegisterAudio();
    RegisterInput();
    RegisterPlatform();
    RegisterHuman();
    RegisterFeatures();

    std::map<std::string, Result> autos;
    std::map<std::string, Status> human;

    if (automatic) {
        for (const AutoTest& t : AutoTests()) {
            if (!only.empty() && t.id != only) continue;
            if (only.empty() && t.onlyWhenNamed) {
                std::printf("%s %s ... not run: changes the display mode; run it with --only %s\n", t.id.c_str(),
                            t.title.c_str(), t.id.c_str());
                continue;
            }
            std::printf("%s %s ... ", t.id.c_str(), t.title.c_str());
            std::fflush(stdout);
            const double start = Now();
            Result r = t.run();
            std::printf("%s (%.1f s)\n    %s\n", StatusName(r.status), Now() - start, r.notes.c_str());
            std::fflush(stdout);
            autos[t.id] = r;
        }
    } else {
        for (;;) {
            Menu menu(human);
            menu.Run();
            if (menu.choice < 0) break;
            const HumanTest& test = HumanTests()[static_cast<size_t>(menu.choice)];
            if (!test.setup) {
                // Nothing to show on this machine; record it as still to do.
                human[test.id] = Status::NeedsHuman;
                continue;
            }
            HumanScene scene(test);
            test.setup(scene);
            scene.Run();
            human[test.id] = scene.Verdict();
        }
    }
    WriteReport(report, autos, human);
    int failed = 0;
    for (auto& [id, r] : autos) failed += r.status == Status::Fail ? 1 : 0;
    return failed;
}
