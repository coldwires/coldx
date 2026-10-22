// stb's zlib compressor and decompressor, and nothing else of stb_image or
// stb_image_write: coldxc decodes images with SDL and writes PNGs itself (watermark.cpp).

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <cstdlib>

#include "zlib_codec.h"

namespace coldx::content::detail {

std::vector<std::uint8_t> Deflate(const std::vector<std::uint8_t>& data) {
    int size = 0;
    unsigned char* packed = stbi_zlib_compress(const_cast<unsigned char*>(data.data()), static_cast<int>(data.size()), &size, 9);
    std::vector<std::uint8_t> out(packed, packed + size);
    STBIW_FREE(packed);
    return out;
}

bool Inflate(const std::vector<std::uint8_t>& data, std::vector<std::uint8_t>& out) {
    int size = 0;
    char* raw = stbi_zlib_decode_malloc(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), &size);
    if (!raw) return false;
    out.assign(reinterpret_cast<std::uint8_t*>(raw), reinterpret_cast<std::uint8_t*>(raw) + size);
    STBI_FREE(raw);
    return true;
}

} // namespace coldx::content::detail
