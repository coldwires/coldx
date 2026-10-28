// GameComponent and DrawableGameComponent: the order of every lifecycle call, and what
// happens when components are added, removed or reordered in the middle of a pass. Each
// call appends to one log. The test compares the log with the expected sequence.

#include <coldx/coldx.h>

#include <functional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "check.h"

using namespace coldx;
using coldx_test::CheckResult;

namespace {

std::vector<std::string> calls;

void Expect(const std::vector<std::string>& expected, const char* what) {
    if (calls != expected) {
        std::printf("%s:\n  got     ", what);
        for (const auto& c : calls) std::printf("%s ", c.c_str());
        std::printf("\n  expected ");
        for (const auto& c : expected) std::printf("%s ", c.c_str());
        std::printf("\n");
        ++coldx_test::Failures();
    }
    calls.clear();
}

class Logged : public DrawableGameComponent {
public:
    Logged(Game& game, std::string name, int updateOrder, int drawOrder) : DrawableGameComponent(game), name_(std::move(name)) {
        SetUpdateOrder(updateOrder);
        SetDrawOrder(drawOrder);
    }
    ~Logged() override { calls.push_back(name_ + ".destroy"); }
    void Initialize() override { calls.push_back(name_ + ".init"); }
    void LoadContent() override { calls.push_back(name_ + ".load"); }
    void UnloadContent() override { calls.push_back(name_ + ".unload"); }
    void Update(const GameTime&) override {
        calls.push_back(name_ + ".update");
        if (onUpdate) onUpdate();
    }
    void Draw(const GameTime&) override { calls.push_back(name_ + ".draw"); }
    std::function<void()> onUpdate;

private:
    std::string name_;
};

// Updates but never draws.
class Plain : public GameComponent {
public:
    Plain(Game& game, std::string name) : GameComponent(game), name_(std::move(name)) {}
    void Initialize() override { calls.push_back(name_ + ".init"); }
    void Update(const GameTime&) override { calls.push_back(name_ + ".update"); }

private:
    std::string name_;
};

class TestGame : public Game {
public:
    int frame = 0;
    std::function<void(int)> onUpdate;
    std::function<void(int)> check;

    TestGame() {
        Settings().fixedTimeStep = false;
        Settings().console = ConsoleMode::Never;
    }

protected:
    void Initialize() override { calls.push_back("game.init"); }
    void LoadContent() override { calls.push_back("game.load"); }
    void UnloadContent() override { calls.push_back("game.unload"); }
    void Update(const GameTime&) override {
        // The previous frame is complete here. Its update and draw passes have both run.
        if (check) check(frame);
        ++frame;
        calls.push_back("game.update");
        if (onUpdate) onUpdate(frame);
    }
    void Draw(const GameTime&) override { calls.push_back("game.draw"); }
};

} // namespace

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);

    {
        TestGame game;
        // Update order: c 0, b 1, a 2. Draw order: c 1, b 5 (a draws at 9 but is hidden).
        auto& a = game.Components().Emplace<Logged>(game, "a", 2, 9);
        auto& b = game.Components().Emplace<Logged>(game, "b", 1, 5);
        auto& c = game.Components().Emplace<Logged>(game, "c", 0, 1);
        auto& p = game.Components().Emplace<Plain>(game, "p");  // update order 0, after c: added later
        a.SetVisible(false);
        CHECK(game.Components().Count() == 4);
        Logged* d = nullptr;

        game.onUpdate = [&](int frame) {
            if (frame == 2) {
                // During a's update: add d (initialized at once, first updated next pass),
                // remove b (unloaded at once, destroyed after the pass), and move c to the end.
                a.onUpdate = [&] {
                    d = &game.Components().Emplace<Logged>(game, "d", 3, 0);
                    game.Components().Remove(b);
                    c.SetUpdateOrder(10);
                    a.onUpdate = nullptr;
                };
            }
            if (frame == 3) p.SetEnabled(false);
            if (frame == 4) game.Exit();
        };
        game.check = [&](int frame) {
            switch (frame) {
            case 0:
                Expect({"game.init", "a.init", "a.load", "b.init", "b.load", "c.init", "c.load", "p.init", "game.load"},
                       "initialization");
                break;
            case 1:
                Expect({"game.update", "c.update", "p.update", "b.update", "a.update", "game.draw", "c.draw", "b.draw"},
                       "first frame");
                break;
            case 2:
                Expect({"game.update", "c.update", "p.update", "b.update", "a.update", "d.init", "d.load", "b.unload",
                        "b.destroy", "game.draw", "d.draw", "c.draw"},
                       "frame with changes mid-pass");
                break;
            case 3:
                Expect({"game.update", "a.update", "d.update", "c.update", "game.draw", "d.draw", "c.draw"},
                       "frame after the changes (p disabled)");
                break;
            }
        };
        CHECK(game.Run() == 0);
        CHECK(game.Components().Count() == 4);  // a, c, p, d
        // At exit, drawable components unload before the game's own UnloadContent.
        Expect({"game.update", "a.update", "d.update", "c.update", "a.unload", "c.unload", "d.unload", "game.unload"},
               "exit");
    }
    // Destroying the game destroys its components.
    Expect({"a.destroy", "c.destroy", "d.destroy"}, "destruction");

    return CheckResult();
}
