#pragma once

namespace coldx {

#if defined(__GNUC__) || defined(__clang__)
#define COLDX_PRINTF_FORMAT(f, a) __attribute__((format(printf, f, a)))
#else
#define COLDX_PRINTF_FORMAT(f, a)
#endif

// printf-style line to the same log as coldx's own messages: the console, the debugger
// output, and the COLDX_LOG file if one is set.
void Log(const char* format, ...) COLDX_PRINTF_FORMAT(1, 2);

} // namespace coldx
