#pragma once

#include <cstdint>

// Little-endian integers in byte buffers, for the .rime header and entry table.
namespace coldx::detail {

inline void Put16(std::uint8_t* p, std::uint16_t v) {
    for (int i = 0; i < 2; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
inline void Put32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
inline void Put64(std::uint8_t* p, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
inline std::uint16_t Get16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
inline std::uint32_t Get32(const std::uint8_t* p) {
    std::uint32_t v = 0;
    for (int i = 3; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}
inline std::uint64_t Get64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}
// Entry data starts on 16-byte boundaries.
inline std::uint64_t AlignUp(std::uint64_t v) {
    return (v + 15) & ~std::uint64_t{15};
}

} // namespace coldx::detail
