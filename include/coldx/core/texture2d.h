#pragma once

#include <memory>
#include <string>

#include <coldx/core/color.h>
#include <coldx/core/rectangle.h>

namespace coldx {

class GraphicsDevice;

namespace detail {
struct TextureAccess;
}

// A GPU texture. Pixels are RGBA with premultiplied alpha. A handle: copies share one
// texture, which is freed when the last copy goes, as in XNA. An empty Texture2D
// (default-constructed, moved from, or a failed load) draws nothing.
//
// Create textures from Initialize or LoadContent on: the graphics device does not exist
// before Run starts. Release them before the Game is destroyed; members of the game class
// are, automatically.
class Texture2D {
public:
    Texture2D();
    ~Texture2D();
    Texture2D(const Texture2D&);
    Texture2D& operator=(const Texture2D&);
    Texture2D(Texture2D&&) noexcept;
    Texture2D& operator=(Texture2D&&) noexcept;

    // Loads a PNG and premultiplies its alpha. A relative path is taken from the folder the
    // executable is in, not the working directory. Empty on failure, with the reason logged.
    static Texture2D FromFile(GraphicsDevice& graphics, const std::string& path);

    // width * height pixels, rows top to bottom, already premultiplied.
    static Texture2D FromPixels(GraphicsDevice& graphics, int width, int height, const Color* pixels);

    // Replaces every pixel, for every copy. pixels must hold Width() * Height() entries,
    // premultiplied.
    void SetData(const Color* pixels);

    // Copies pixels back from the GPU: the whole texture, or a region of it, rows top to
    // bottom, premultiplied. pixels must hold the region's width * height entries. It waits
    // for the GPU, so it suits screenshots and collision masks, not every frame. Inside Draw
    // it sees what earlier frames drew, not this one. False for an empty texture or a region
    // not wholly inside it.
    bool GetData(Color* pixels) const;
    bool GetData(const Rectangle& region, Color* pixels) const;
    // Writes the texture to a PNG with straight alpha, which FromFile loads back as the same
    // image. False on failure, with the reason logged.
    bool SaveAsPng(const std::string& path) const;

    int Width() const;
    int Height() const;
    bool IsEmpty() const;
    explicit operator bool() const { return !IsEmpty(); }

private:
    friend struct detail::TextureAccess;
    friend class RenderTarget2D;
    struct Impl;
    // Null on failure, with the reason logged.
    static std::shared_ptr<Impl> Make(GraphicsDevice& graphics, int width, int height, bool renderTarget);
    std::shared_ptr<Impl> impl_;
};

} // namespace coldx
