#include <coldx/core/log.h>

#include <cstdarg>

#include <SDL3/SDL.h>

namespace coldx {

void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, format, args);
    va_end(args);
}

} // namespace coldx
