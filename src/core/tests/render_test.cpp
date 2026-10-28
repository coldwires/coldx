// The basic sprite paths: position, tint, fade, source rectangle, destination rectangle.
// Also checks that a texture outliving its Game is destroyed without a crash.

#include <optional>

#include "headless.h"

using namespace coldx;
using namespace coldx_test;

namespace {

// 2x2: red, green / blue, half-transparent white (premultiplied).
const std::vector<Color> Texels = {
    Color(255, 0, 0),
    Color(0, 255, 0),
    Color(0, 0, 255),
    Color(128, 128, 128, 128),
};

void CheckSprite(const Frame& f, int x, int y, Color tint) {
    for (int ty = 0; ty < 2; ++ty) {
        for (int tx = 0; tx < 2; ++tx) {
            const Color tinted = Tint(Texels[ty * 2 + tx], tint);
            CheckPixel(f, x + tx, y + ty, Over(tinted, Color::Black), tinted.a == 255 ? 0 : 1);
        }
    }
}

} // namespace

int main() {
    std::optional<Texture2D> survivor;
    {
        Scene scene(64, 32);
        scene.load = [](Scene& s) { s.AddTexture(2, 2, Texels); };
        scene.draw = [](Scene& s) {
            const Texture2D& t = s.textures[0];
            s.Graphics().Clear(Color::Black);
            s.batch.Begin();
            s.batch.Draw(t, Vector2(4, 4), Color::White);
            s.batch.Draw(t, Vector2(10, 4), Color::Red);
            s.batch.Draw(t, Vector2(16, 4), Color::White * 0.5f);
            s.batch.Draw(t, Vector2(22, 4), Rectangle(1, 0, 1, 1), Color::White);
            s.batch.Draw(t, Rectangle(28, 4, 2, 2), Color::White);
            s.batch.End();
        };
        const Frame f = RunAndCapture(scene, "basic");
        CHECK(f.Width() == 64 && f.Height() == 32);
        CHECK(scene.textures[0].Width() == 2 && scene.textures[0].Height() == 2);
        CheckSprite(f, 4, 4, Color::White);
        CheckSprite(f, 10, 4, Color::Red);
        CheckSprite(f, 16, 4, Color::White * 0.5f);
        CheckPixel(f, 22, 4, Texels[1]);
        CheckPixel(f, 23, 4, Color::Black);
        CheckSprite(f, 28, 4, Color::White);
        for (auto [x, y] : {std::pair{0, 0}, {3, 3}, {6, 6}, {63, 31}, {30, 4}}) CheckPixel(f, x, y, Color::Black);

        // Outlives the game and its GPU device. Destroying it afterwards must not crash.
        survivor = std::move(scene.textures[0]);
    }
    survivor.reset();

    return CheckResult();
}
