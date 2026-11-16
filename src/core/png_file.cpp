#include "png_file.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

namespace coldx::detail {

bool WritePng(const char* path, int width, int height, const Color* pixels, bool straightAlpha) {
    std::vector<Color> copy;
    if (straightAlpha) {
        copy.assign(pixels, pixels + static_cast<size_t>(width) * height);
        for (Color& c : copy) {
            if (c.a == 0 || c.a == 255) continue;
            auto undo = [&](std::uint8_t v) { return static_cast<std::uint8_t>(std::min(255, (v * 255 + c.a / 2) / c.a)); };
            c = Color(undo(c.r), undo(c.g), undo(c.b), c.a);
        }
        pixels = copy.data();
    }
    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, const_cast<Color*>(pixels), width * 4);
    const bool saved = surface && SDL_SavePNG(surface, path);
    if (!saved) SDL_Log("coldx.core: could not write %s: %s", path, SDL_GetError());
    SDL_DestroySurface(surface);
    return saved;
}

} // namespace coldx::detail
