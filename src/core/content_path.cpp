#include "content_path.h"

#include <SDL3/SDL.h>

namespace coldx::detail {

std::string ResolveContentPath(const std::string& path) {
    const bool absolute = !path.empty() && (path[0] == '/' || path[0] == '\\' || (path.size() > 1 && path[1] == ':'));
    if (absolute) return path;
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) + path : path;
}

} // namespace coldx::detail
