#pragma once

#include <cstdint>
#include <vector>

// zlib streams, for writing and reading PNG image data (stb_image_write and stb_image).
namespace coldx::content::detail {

std::vector<std::uint8_t> Deflate(const std::vector<std::uint8_t>& data);
// False if data is not a valid zlib stream.
bool Inflate(const std::vector<std::uint8_t>& data, std::vector<std::uint8_t>& out);

} // namespace coldx::content::detail
