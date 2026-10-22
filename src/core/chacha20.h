#pragma once

#include <cstddef>
#include <cstdint>

namespace coldx::detail {

// ChaCha20 as in RFC 8439: a 256-bit key, a 96-bit nonce and a 32-bit block counter.
// XORs the keystream into data in place, so the same call encrypts and decrypts. Used for
// package entries; a key and nonce pair must never be used for two different messages.
void ChaCha20(const std::uint8_t key[32], const std::uint8_t nonce[12], std::uint32_t counter, std::uint8_t* data,
              std::size_t size);

} // namespace coldx::detail
