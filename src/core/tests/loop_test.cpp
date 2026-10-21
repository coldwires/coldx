// The fixed-timestep loop drawing between updates: off, every draw follows an update and
// Alpha is 0; on, frames are drawn between updates with Alpha rising from 0 toward 1, while
// updates keep their own rate. Headless, so nothing paces the drawing but the loop itself.

#include <coldx/coldx.h>

#include <vector>

#include <SDL3/SDL.h>

#include "check.h"

using namespace coldx;
using coldx_test::CheckResult;

namespace {

class LoopGame : public Game {
public:
    int updates = 0;
    int draws = 0;
    int drawsSinceUpdate = 0;
    int maxDrawsBetween = 0;
    float lastAlpha = -1.0f;
    bool alphaInRange = true;
    bool alphaRises = true;
    bool updateAlphaZero = true;
    Uint64 start = 0;
    double seconds = 0;

protected:
    void Initialize() override { start = SDL_GetTicksNS(); }

    void Update(const GameTime& time) override {
        ++updates;
        updateAlphaZero = updateAlphaZero && time.Alpha() == 0.0f;
        drawsSinceUpdate = 0;
        lastAlpha = -1.0f;
        seconds = (SDL_GetTicksNS() - start) / 1e9;
        if (seconds >= 0.5) Exit();
    }

    void Draw(const GameTime& time) override {
        ++draws;
        ++drawsSinceUpdate;
        if (drawsSinceUpdate > maxDrawsBetween) maxDrawsBetween = drawsSinceUpdate;
        alphaInRange = alphaInRange && time.Alpha() >= 0.0f && time.Alpha() < 1.0f;
        alphaRises = alphaRises && time.Alpha() >= lastAlpha;
        lastAlpha = time.Alpha();
    }
};

} // namespace

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);
    {   // Off: one draw per frame that updated, Alpha always 0.
        LoopGame game;
        game.Settings().console = ConsoleMode::Never;
        CHECK(game.Run() == 0);
        std::printf("off: %d updates, %d draws in %.2f s\n", game.updates, game.draws, game.seconds);
        CHECK(game.maxDrawsBetween <= 1);
        CHECK(game.lastAlpha == 0.0f || game.lastAlpha == -1.0f);
        CHECK(game.alphaInRange && game.updateAlphaZero);
    }
    {   // On: many draws between updates, Alpha climbing within each step; updates stay at 60.
        LoopGame game;
        game.Settings().console = ConsoleMode::Never;
        game.Settings().drawBetweenUpdates = true;
        CHECK(game.Run() == 0);
        std::printf("on: %d updates, %d draws in %.2f s, up to %d draws between updates\n", game.updates, game.draws,
                    game.seconds, game.maxDrawsBetween);
        CHECK(game.draws > game.updates * 2);
        CHECK(game.maxDrawsBetween > 2);
        CHECK(game.updates >= 28 && game.updates <= 33);  // 0.5 s at 60 per second
        CHECK(game.alphaInRange && game.alphaRises && game.updateAlphaZero);
    }
    return CheckResult();
}
