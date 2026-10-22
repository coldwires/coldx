#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Strings that do not appear in the executable as text:
//
//     Log("%s", COLDX_HIDDEN("update needed: server says version 7").c_str());
//
// The literal is scrambled at compile time with a key made from the file, line and a
// counter, and unscrambled each time the expression runs. Strings are how a reader of a
// binary finds and names its code; this takes that away for the ones that matter. It is
// not encryption: the key is beside the data, and the text is in memory once revealed.
// Use it for a few telling strings, not every one.
namespace coldx::protect::detail {

constexpr std::uint64_t Mix(std::uint64_t h) {
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ull;
    h ^= h >> 33;
    return h;
}

constexpr std::uint64_t Seed(const char* file, unsigned line, unsigned counter) {
    std::uint64_t h = 0xCBF29CE484222325ull;
    for (const char* c = file; *c; ++c) h = (h ^ static_cast<unsigned char>(*c)) * 0x100000001B3ull;
    return Mix(h ^ (std::uint64_t{line} << 32) ^ counter);
}

constexpr char KeyByte(std::uint64_t seed, std::size_t i) {
    return static_cast<char>(Mix(seed + (i + 1) * 0x9E3779B97F4A7C15ull) & 0xFF);
}

template <std::size_t N, std::uint64_t S>
class Hidden {
public:
    consteval explicit Hidden(const char (&text)[N]) {
        for (std::size_t i = 0; i < N; ++i) data_[i] = static_cast<char>(text[i] ^ KeyByte(S, i));
    }

    std::string Reveal() const {
        // Read through volatile, so the optimizer cannot undo the scrambling at compile
        // time and put the text back into the code.
        const volatile char* stored = data_.data();
        std::string out(N - 1, '\0');
        for (std::size_t i = 0; i + 1 < N; ++i) out[i] = static_cast<char>(stored[i] ^ KeyByte(S, i));
        return out;
    }

private:
    std::array<char, N> data_{};
};

} // namespace coldx::protect::detail

// A std::string with the text of the literal, which is stored scrambled.
#define COLDX_HIDDEN(text)                                                                                                    \
    ([]() {                                                                                                                   \
        static constexpr ::coldx::protect::detail::Hidden<sizeof(text),                                                       \
                                                          ::coldx::protect::detail::Seed(__FILE__, __LINE__, __COUNTER__)> \
            hidden(text);                                                                                                     \
        return hidden.Reveal();                                                                                               \
    }())
