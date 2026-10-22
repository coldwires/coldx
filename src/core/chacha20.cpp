#include "chacha20.h"

namespace coldx::detail {

namespace {

std::uint32_t Rotl(std::uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

std::uint32_t Load32(const std::uint8_t* p) {
    return std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8) | (std::uint32_t{p[2]} << 16) | (std::uint32_t{p[3]} << 24);
}

void QuarterRound(std::uint32_t* s, int a, int b, int c, int d) {
    s[a] += s[b];
    s[d] = Rotl(s[d] ^ s[a], 16);
    s[c] += s[d];
    s[b] = Rotl(s[b] ^ s[c], 12);
    s[a] += s[b];
    s[d] = Rotl(s[d] ^ s[a], 8);
    s[c] += s[d];
    s[b] = Rotl(s[b] ^ s[c], 7);
}

// One 64-byte keystream block (RFC 8439 section 2.3).
void Block(const std::uint32_t input[16], std::uint8_t out[64]) {
    std::uint32_t s[16];
    for (int i = 0; i < 16; ++i) s[i] = input[i];
    for (int round = 0; round < 10; ++round) {
        QuarterRound(s, 0, 4, 8, 12);
        QuarterRound(s, 1, 5, 9, 13);
        QuarterRound(s, 2, 6, 10, 14);
        QuarterRound(s, 3, 7, 11, 15);
        QuarterRound(s, 0, 5, 10, 15);
        QuarterRound(s, 1, 6, 11, 12);
        QuarterRound(s, 2, 7, 8, 13);
        QuarterRound(s, 3, 4, 9, 14);
    }
    for (int i = 0; i < 16; ++i) {
        const std::uint32_t v = s[i] + input[i];
        for (int k = 0; k < 4; ++k) out[i * 4 + k] = static_cast<std::uint8_t>(v >> (8 * k));
    }
}

} // namespace

void ChaCha20(const std::uint8_t key[32], const std::uint8_t nonce[12], std::uint32_t counter, std::uint8_t* data,
              std::size_t size) {
    std::uint32_t state[16] = {0x61707865, 0x3320646E, 0x79622D32, 0x6B206574};  // "expand 32-byte k"
    for (int i = 0; i < 8; ++i) state[4 + i] = Load32(key + 4 * i);
    state[12] = counter;
    for (int i = 0; i < 3; ++i) state[13 + i] = Load32(nonce + 4 * i);
    std::uint8_t stream[64];
    for (std::size_t done = 0; done < size; done += 64) {
        Block(state, stream);
        ++state[12];
        const std::size_t n = size - done < 64 ? size - done : 64;
        for (std::size_t i = 0; i < n; ++i) data[done + i] ^= stream[i];
    }
}

} // namespace coldx::detail
