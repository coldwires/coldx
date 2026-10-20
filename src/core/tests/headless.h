#pragma once

// Runs a drawing headless and hands back the captured frame, so a render test is a scene
// plus the pixels it should produce. Expected colors are computed with Over and Tint, the
// same arithmetic the GPU does, never typed in from a screenshot.

#include <coldx/coldx.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "check.h"

namespace coldx_test {

class Scene : public coldx::Game {
public:
    std::function<void(Scene&)> load;
    std::function<void(Scene&)> draw;
    std::vector<coldx::Texture2D> textures;
    std::vector<coldx::RenderTarget2D> targets;
    coldx::SpriteBatch batch{Graphics()};

    Scene(int width, int height) {
        Settings().width = width;
        Settings().height = height;
        Settings().console = coldx::ConsoleMode::Never;
    }

    // Adds a texture from pixels, rows top to bottom, and returns its index.
    std::size_t AddTexture(int width, int height, std::vector<coldx::Color> pixels) {
        textures.push_back(coldx::Texture2D::FromPixels(Graphics(), width, height, pixels.data()));
        CHECK(!textures.back().IsEmpty());
        return textures.size() - 1;
    }

protected:
    void LoadContent() override {
        if (load) load(*this);
    }
    void Draw(const coldx::GameTime&) override {
        if (draw) draw(*this);
    }
};

// One capture of the last frame, as RGBA bytes.
class Frame {
public:
    explicit Frame(SDL_Surface* surface) : surface_(surface) {}
    ~Frame() { SDL_DestroySurface(surface_); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    bool Valid() const { return surface_ != nullptr; }
    int Width() const { return surface_ ? surface_->w : 0; }
    int Height() const { return surface_ ? surface_->h : 0; }

    coldx::Color At(int x, int y) const {
        const auto* p = static_cast<const std::uint8_t*>(surface_->pixels) + y * surface_->pitch + x * 4;
        return {p[0], p[1], p[2], p[3]};
    }

private:
    SDL_Surface* surface_;
};

// Runs scene for two frames with nothing shown and returns its last frame. name keeps
// captures from different tests and backends apart in the temp folder.
inline Frame RunAndCapture(Scene& scene, const std::string& name) {
    const char* driver = SDL_getenv("SDL_GPU_DRIVER");
    const std::string file = "coldx_" + name + "_" + (driver ? driver : "default") + ".png";
    const std::string capture = (std::filesystem::temp_directory_path() / file).string();
    std::filesystem::remove(capture);

    SDL_Environment* env = SDL_GetEnvironment();
    SDL_SetEnvironmentVariable(env, "COLDX_HEADLESS", "1", true);
    SDL_SetEnvironmentVariable(env, "COLDX_FRAMES", "2", true);
    SDL_SetEnvironmentVariable(env, "COLDX_CAPTURE", capture.c_str(), true);

    CHECK(scene.Run() == 0);
    SDL_Surface* loaded = SDL_LoadPNG(capture.c_str());
    CHECK(loaded != nullptr);
    if (!loaded) return Frame(nullptr);
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    return Frame(rgba);
}

// Premultiplied src over dst: the AlphaBlend equation.
inline coldx::Color Over(coldx::Color src, coldx::Color dst) {
    const float a = src.a / 255.0f;
    auto channel = [a](std::uint8_t s, std::uint8_t d) {
        const float v = s + d * (1.0f - a) + 0.5f;
        return static_cast<std::uint8_t>(v > 255.0f ? 255.0f : v);
    };
    return {channel(src.r, dst.r), channel(src.g, dst.g), channel(src.b, dst.b), channel(src.a, dst.a)};
}

// Texel times tint, per channel: what the sprite shader outputs.
inline coldx::Color Tint(coldx::Color texel, coldx::Color tint) {
    auto channel = [](std::uint8_t t, std::uint8_t c) { return static_cast<std::uint8_t>(t * c / 255.0f + 0.5f); };
    return {channel(texel.r, tint.r), channel(texel.g, tint.g), channel(texel.b, tint.b), channel(texel.a, tint.a)};
}

// Opaque results must match exactly; anything blended may round one step either way.
inline void CheckPixel(const Frame& frame, int x, int y, coldx::Color expected, int tolerance = 0) {
    if (!frame.Valid()) return;
    const coldx::Color got = frame.At(x, y);
    const bool ok = std::abs(got.r - expected.r) <= tolerance && std::abs(got.g - expected.g) <= tolerance &&
                    std::abs(got.b - expected.b) <= tolerance && std::abs(got.a - expected.a) <= tolerance;
    if (!ok) {
        std::printf("pixel (%d, %d): got %d,%d,%d,%d expected %d,%d,%d,%d\n", x, y, got.r, got.g, got.b, got.a,
                    expected.r, expected.g, expected.b, expected.a);
        ++Failures();
    }
}

} // namespace coldx_test
