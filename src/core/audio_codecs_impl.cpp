// The one translation unit that compiles dr_wav's and stb_vorbis's implementations.
// CMakeLists.txt turns warnings off for this file only: they are third-party code, and
// MSVC reports some of them during code generation, where a #pragma cannot reach.

#define DR_WAV_IMPLEMENTATION
#include <dr_libs/dr_wav.h>

#include <stb/stb_vorbis.c>
