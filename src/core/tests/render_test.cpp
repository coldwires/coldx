// Draws a known scene headless, captures it, and compares pixels against values computed
// from the same inputs. CTest runs it once per GPU backend via SDL_GPU_DRIVER.

#include <coldx/coldx.h>

#include <filesystem>
#include <optional>
#include <string>

#include <SDL3/SDL.h>

#include "check.h"

using namespace coldx;

namespace {

// 2x2 texture: red, green / blue, half-transparent white (premultiplied).
const Color Texels[4] = {
    Color(255, 0, 0),
    Color(0, 255, 0),
    Color(0, 0, 255),
    Color(128, 128, 128, 128),
};

class Scene : public Game {
public:
    Texture2D texture;

    Scene() {
        Settings().width = 64;
        Settings().height = 32;
        Settings().console = ConsoleMode::Never;
    }

protected:
    void LoadContent() override {
        texture = Texture2D::FromPixels(Graphics(), 2, 2, Texels);
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color::Black);
        batch.Begin();
        batch.Draw(texture, Vector2(4, 4), Color::White);
        batch.Draw(texture, Vector2(10, 4), Color::Red);
        batch.Draw(texture, Vector2(16, 4), Color::White * 0.5f);
        batch.Draw(texture, Vector2(22, 4), Rectangle(1, 0, 1, 1), Color::White);
        batch.Draw(texture, Rectangle(28, 4, 2, 2), Color::White);
        batch.End();
    }

private:
    SpriteBatch batch{Graphics()};
};

// Premultiplied source over an opaque destination, the SpriteBatch blend.
Color Over(Color src, Color dst) {
    const float a = src.a / 255.0f;
    auto channel = [a](std::uint8_t s, std::uint8_t d) {
        return static_cast<std::uint8_t>(s + d * (1.0f - a) + 0.5f);
    };
    return {channel(src.r, dst.r), channel(src.g, dst.g), channel(src.b, dst.b), channel(src.a, dst.a)};
}

Color Tint(Color texel, Color tint) {
    auto channel = [](std::uint8_t t, std::uint8_t c) { return static_cast<std::uint8_t>(t * c / 255.0f + 0.5f); };
    return {channel(texel.r, tint.r), channel(texel.g, tint.g), channel(texel.b, tint.b), channel(texel.a, tint.a)};
}

Color PixelAt(SDL_Surface* s, int x, int y) {
    const auto* p = static_cast<const std::uint8_t*>(s->pixels) + y * s->pitch + x * 4;
    return {p[0], p[1], p[2], p[3]};
}

void CheckPixel(SDL_Surface* s, int x, int y, Color expected, int tolerance) {
    const Color got = PixelAt(s, x, y);
    CHECK_NEAR(got.r, expected.r, tolerance);
    CHECK_NEAR(got.g, expected.g, tolerance);
    CHECK_NEAR(got.b, expected.b, tolerance);
    CHECK_NEAR(got.a, expected.a, tolerance);
}

// Blended texels may round one step either way on the GPU; opaque ones must be exact.
void CheckSprite(SDL_Surface* s, int x, int y, Color tint) {
    for (int ty = 0; ty < 2; ++ty) {
        for (int tx = 0; tx < 2; ++tx) {
            const Color texel = Texels[ty * 2 + tx];
            const Color tinted = Tint(texel, tint);
            CheckPixel(s, x + tx, y + ty, Over(tinted, Color::Black), tinted.a == 255 ? 0 : 1);
        }
    }
}

} // namespace

int main() {
    const char* driver = SDL_getenv("SDL_GPU_DRIVER");
    const std::string name = std::string("coldx_render_test_") + (driver ? driver : "default") + ".png";
    const std::string capture = (std::filesystem::temp_directory_path() / name).string();
    std::filesystem::remove(capture);

    SDL_Environment* env = SDL_GetEnvironment();
    SDL_SetEnvironmentVariable(env, "COLDX_HEADLESS", "1", true);
    SDL_SetEnvironmentVariable(env, "COLDX_FRAMES", "2", true);
    SDL_SetEnvironmentVariable(env, "COLDX_CAPTURE", capture.c_str(), true);

    std::optional<Texture2D> survivor;
    {
        Scene scene;
        CHECK(scene.Run() == 0);
        CHECK(scene.texture.Width() == 2 && scene.texture.Height() == 2);

        SDL_Surface* loaded = SDL_LoadPNG(capture.c_str());
        CHECK(loaded != nullptr);
        if (loaded) {
            SDL_Surface* s = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(loaded);
            CHECK(s->w == 64 && s->h == 32);
            CheckSprite(s, 4, 4, Color::White);
            CheckSprite(s, 10, 4, Color::Red);
            CheckSprite(s, 16, 4, Color::White * 0.5f);
            CheckPixel(s, 22, 4, Texels[1], 0);
            CheckPixel(s, 23, 4, Color::Black, 0);
            CheckSprite(s, 28, 4, Color::White);
            for (auto [x, y] : {std::pair{0, 0}, {3, 3}, {6, 6}, {63, 31}, {30, 4}})
                CheckPixel(s, x, y, Color::Black, 0);
            SDL_DestroySurface(s);
        }

        // Outlives the game and its GPU device; destroying it afterwards must not crash.
        survivor = std::move(scene.texture);
    }
    survivor.reset();

    return coldx_test::CheckResult();
}
