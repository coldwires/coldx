#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace coldx {

// Values are USB HID keyboard usage IDs. They name a physical key position, so Key::W is
// the same key on QWERTY and AZERTY layouts.
enum class Key : std::uint16_t {
    Unknown = 0,

    A = 4, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    D1 = 30, D2, D3, D4, D5, D6, D7, D8, D9, D0,

    Enter = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Space = 44,
    Minus = 45,
    Equals = 46,
    LeftBracket = 47,
    RightBracket = 48,
    Backslash = 49,
    Semicolon = 51,
    Apostrophe = 52,
    Grave = 53,
    Comma = 54,
    Period = 55,
    Slash = 56,
    CapsLock = 57,

    F1 = 58, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    PrintScreen = 70,
    ScrollLock = 71,
    Pause = 72,
    Insert = 73,
    Home = 74,
    PageUp = 75,
    Delete = 76,
    End = 77,
    PageDown = 78,
    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,

    LeftControl = 224,
    LeftShift = 225,
    LeftAlt = 226,
    LeftSuper = 227,
    RightControl = 228,
    RightShift = 229,
    RightAlt = 230,
    RightSuper = 231,
};

class KeyboardState {
public:
    static constexpr int KeyCount = 512;

    bool IsKeyDown(Key key) const {
        const auto i = static_cast<unsigned>(key);
        return i < KeyCount && (bits_[i / 64] >> (i % 64)) & 1u;
    }
    bool IsKeyUp(Key key) const { return !IsKeyDown(key); }

    void SetKey(Key key, bool down) {
        const auto i = static_cast<unsigned>(key);
        if (i >= KeyCount) return;
        const std::uint64_t mask = std::uint64_t{1} << (i % 64);
        bits_[i / 64] = down ? (bits_[i / 64] | mask) : (bits_[i / 64] & ~mask);
    }

private:
    std::array<std::uint64_t, KeyCount / 64> bits_{};
};

class Keyboard {
public:
    // The state as of the start of the current Update. Keys are released when the window
    // loses focus.
    static KeyboardState GetState();

    // Down now, up at the start of the previous Update; and the reverse. With a fixed
    // timestep running several updates in one frame, only the first of them sees the press.
    static bool WasPressed(Key key);
    static bool WasReleased(Key key);

    // What the key is called on the current layout, for showing bindings: Key::Q is "A"
    // on AZERTY, "Q" on QWERTY; non-character keys have their usual names ("Left Shift",
    // "F5"). Empty for a key with no name. From Initialize on.
    static std::string KeyName(Key key);
    // The key that types character on the current layout, unshifted, or Key::Unknown. 'a'
    // and 'A' give the same key. For default bindings by letter: KeyForCharacter('w').
    static Key KeyForCharacter(char32_t character);
};

} // namespace coldx
