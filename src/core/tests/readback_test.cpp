// Reading pixels back: Texture2D::GetData whole and by region, from a render target after it
// was drawn, GraphicsDevice::GetBackBufferData, and SaveAsPng loading back as the same image.

#include <coldx/coldx.h>

#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "check.h"

using namespace coldx;
using coldx_test::CheckResult;

namespace {

bool Same(const std::vector<Color>& a, const std::vector<Color>& b, int tolerance = 0) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::abs(a[i].r - b[i].r) > tolerance || std::abs(a[i].g - b[i].g) > tolerance ||
            std::abs(a[i].b - b[i].b) > tolerance || std::abs(a[i].a - b[i].a) > tolerance) {
            std::printf("pixel %zu: got %d,%d,%d,%d want %d,%d,%d,%d\n", i, a[i].r, a[i].g, a[i].b, a[i].a, b[i].r, b[i].g,
                        b[i].b, b[i].a);
            return false;
        }
    }
    return true;
}

class ReadbackGame : public Game {
public:
    int frame = 0;
    std::string png = (std::filesystem::temp_directory_path() / "coldx_readback_test.png").string();

protected:
    void LoadContent() override {
        // 4x3, every pixel different, some half transparent (premultiplied).
        for (int i = 0; i < 12; ++i)
            pixels_.push_back(Color(static_cast<std::uint8_t>(i * 20), static_cast<std::uint8_t>(255 - i * 20),
                                    static_cast<std::uint8_t>(i * 7), 255));
        pixels_[5] = Color(64, 0, 32, 128);
        pixels_[6] = Color(0, 0, 0, 0);
        texture_ = Texture2D::FromPixels(Graphics(), 4, 3, pixels_.data());

        std::vector<Color> back(12);
        CHECK(texture_.GetData(back.data()));
        CHECK(Same(back, pixels_));

        // A region: rows 1..2, columns 1..2.
        std::vector<Color> region(4);
        CHECK(texture_.GetData(Rectangle(1, 1, 2, 2), region.data()));
        CHECK(Same(region, {pixels_[5], pixels_[6], pixels_[9], pixels_[10]}));

        // Outside the texture, and empty textures, refuse.
        CHECK(!texture_.GetData(Rectangle(3, 2, 2, 1), region.data()));
        CHECK(!texture_.GetData(Rectangle(-1, 0, 1, 1), region.data()));
        CHECK(!Texture2D().GetData(region.data()));

        // SetData then GetData sees the new pixels.
        std::vector<Color> flipped(pixels_.rbegin(), pixels_.rend());
        texture_.SetData(flipped.data());
        CHECK(texture_.GetData(back.data()));
        CHECK(Same(back, flipped));
        texture_.SetData(pixels_.data());

        // SaveAsPng writes straight alpha. FromFile premultiplies again, giving the same image to
        // within rounding on the half-transparent pixel.
        std::filesystem::remove(png);
        CHECK(texture_.SaveAsPng(png));
        Texture2D loaded = Texture2D::FromFile(Graphics(), png);
        CHECK(loaded.Width() == 4 && loaded.Height() == 3);
        CHECK(loaded.GetData(back.data()));
        CHECK(Same(back, pixels_, 1));
        std::filesystem::remove(png);

        target_ = RenderTarget2D::Create(Graphics(), 8, 8);
        const std::vector<Color> blue(4, Color::Blue);
        square_ = Texture2D::FromPixels(Graphics(), 2, 2, blue.data());
    }

    void Update(const GameTime&) override {
        ++frame;
        if (frame == 2) {
            // Frame 1 drew into the target and cleared the back buffer. Both are visible now.
            std::vector<Color> got(64);
            CHECK(target_.GetData(got.data()));
            std::vector<Color> want(64, Color::Red);
            for (int y = 2; y < 4; ++y)
                for (int x = 2; x < 4; ++x) want[static_cast<size_t>(y) * 8 + x] = Color::Blue;
            CHECK(Same(got, want));

            const int w = Graphics().BackBufferWidth(), h = Graphics().BackBufferHeight();
            std::vector<Color> screen(static_cast<size_t>(w) * h);
            CHECK(Graphics().GetBackBufferData(screen.data()));
            CHECK(screen[0] == Color(0, 128, 0, 255) && screen.back() == Color(0, 128, 0, 255));
            CHECK(screen[static_cast<size_t>(5) * w + 5] == Color::Red);
            Exit();
        }
    }

    void Draw(const GameTime&) override {
        if (frame != 1) return;
        Graphics().SetRenderTarget(&target_);
        Graphics().Clear(Color::Red);
        batch_.Begin();
        batch_.Draw(square_, Vector2(2, 2), Color::White);
        batch_.End();
        Graphics().SetRenderTarget(nullptr);
        Graphics().Clear(Color(0, 128, 0, 255));
        batch_.Begin();
        batch_.Draw(target_, Vector2(4, 4), Color::White);
        batch_.End();
    }

private:
    std::vector<Color> pixels_;
    Texture2D texture_, square_;
    RenderTarget2D target_;
    SpriteBatch batch_{Graphics()};
};

} // namespace

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);
    ReadbackGame game;
    game.Settings().console = ConsoleMode::Never;
    game.Settings().width = 32;
    game.Settings().height = 16;
    CHECK(game.Run() == 0);
    CHECK(game.frame == 2);
    return CheckResult();
}
