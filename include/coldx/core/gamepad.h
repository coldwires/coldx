#pragma once

#include <cstdint>
#include <string>

#include <coldx/core/vector2.h>

namespace coldx {

// Named for the Xbox layout. SDL maps other pads onto it by position: A is the bottom face
// button, whatever it is labelled.
enum class Buttons : std::uint32_t {
    A = 1u << 0,
    B = 1u << 1,
    X = 1u << 2,
    Y = 1u << 3,
    Back = 1u << 4,
    Start = 1u << 5,
    Guide = 1u << 6,
    LeftShoulder = 1u << 7,
    RightShoulder = 1u << 8,
    LeftStick = 1u << 9,
    RightStick = 1u << 10,
    DPadUp = 1u << 11,
    DPadDown = 1u << 12,
    DPadLeft = 1u << 13,
    DPadRight = 1u << 14,
};

// How stick input near the center is ignored. Worn sticks rarely rest at exactly zero.
enum class GamePadDeadZone {
    None,
    IndependentAxes,  // each axis on its own; good for menus and 4-way movement. The default.
    Circular,         // the stick's distance from center; good for free 2D aiming
};

// The kind of pad, for showing matching button prompts.
enum class GamePadType {
    Unknown,
    Standard,  // a pad SDL knows the layout of but not the family
    Xbox360,
    XboxOne,  // also Xbox Series
    PS3,
    PS4,
    PS5,
    SwitchPro,
    JoyConLeft,
    JoyConRight,
    JoyConPair,
    GameCube,
    Steam,
};

// What a face button is printed with on the pad in hand. Buttons::A is the bottom face
// button on every pad, which is "B" on a Nintendo pad and a cross on a PlayStation one.
enum class ButtonLabel {
    Unknown,
    A,
    B,
    X,
    Y,
    Cross,
    Circle,
    Square,
    Triangle,
};

struct GamePadCapabilities {
    bool isConnected = false;
    std::string name;  // as the pad reports it, e.g. "DualSense Wireless Controller"
    GamePadType type = GamePadType::Unknown;
    bool hasRumble = false;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
};

struct GamePadState {
    bool isConnected = false;
    // -1 to 1, up and right positive, after the dead zone.
    Vector2 leftStick;
    Vector2 rightStick;
    // 0 to 1, after a small dead zone.
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
    std::uint32_t buttons = 0;

    constexpr bool IsButtonDown(Buttons b) const { return (buttons & static_cast<std::uint32_t>(b)) != 0; }
    constexpr bool IsButtonUp(Buttons b) const { return !IsButtonDown(b); }
};

// While the game's window does not have focus, SDL ignores button presses, so a pad in the
// player's hand does not act on a game in the background.
class GamePad {
public:
    // Players 0 to 3. Pads take the lowest free player as they connect; a disconnect frees
    // its player for the next pad.
    static constexpr int MaxPlayers = 4;

    // The state as of the start of the current Update. Not connected for an unused player.
    static GamePadState GetState(int player, GamePadDeadZone deadZone = GamePadDeadZone::IndependentAxes);

    // Pressed (or released) at any moment since the previous Update, a quick tap included.
    static bool WasPressed(int player, Buttons button);
    static bool WasReleased(int player, Buttons button);

    // Motor strength 0 to 1, low and high frequency; holds until changed. False when the
    // player has no pad or the pad cannot rumble.
    static bool SetVibration(int player, float lowFrequency, float highFrequency);

    // The pad on player now, live rather than per Update. Not connected for an unused player.
    static GamePadCapabilities GetCapabilities(int player);
    // How the pad labels a face button (A, B, X, Y); Unknown for other buttons or no pad.
    static ButtonLabel FaceButtonLabel(int player, Buttons button);
};

} // namespace coldx
