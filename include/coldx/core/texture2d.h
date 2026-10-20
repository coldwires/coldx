#pragma once

#include <memory>
#include <string>

#include <coldx/core/color.h>

namespace coldx {

class GraphicsDevice;

namespace detail {
struct TextureAccess;
}

// A GPU texture. Pixels are RGBA with premultiplied alpha. Move-only; an empty Texture2D
// (default-constructed, moved from, or a failed load) draws nothing.
//
// Create textures from Initialize or LoadContent on: the graphics device does not exist
// before Run starts. Release them before the Game is destroyed; members of the game class
// are, automatically.
class Texture2D {
public:
    Texture2D();
    ~Texture2D();
    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;
    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    // Loads a PNG and premultiplies its alpha. A relative path is taken from the folder the
    // executable is in, not the working directory. Empty on failure, with the reason logged.
    static Texture2D FromFile(GraphicsDevice& graphics, const std::string& path);

    // width * height pixels, rows top to bottom, already premultiplied.
    static Texture2D FromPixels(GraphicsDevice& graphics, int width, int height, const Color* pixels);

    // Replaces every pixel. pixels must hold Width() * Height() entries, premultiplied.
    void SetData(const Color* pixels);

    int Width() const;
    int Height() const;
    bool IsEmpty() const;
    explicit operator bool() const { return !IsEmpty(); }

private:
    friend struct detail::TextureAccess;
    friend class RenderTarget2D;
    struct Impl;
    // Null on failure, with the reason logged.
    static std::unique_ptr<Impl> Make(GraphicsDevice& graphics, int width, int height, bool renderTarget);
    std::unique_ptr<Impl> impl_;
};

} // namespace coldx
