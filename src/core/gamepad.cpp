#include <coldx/core/gamepad.h>

#include <cmath>

#include <SDL3/SDL.h>

#include "input.h"

namespace coldx {

namespace {

// XNA's values: 7849 / 32767 for sticks, 30 / 255 for triggers.
constexpr float StickDeadZone = 0.24f;
constexpr float TriggerDeadZone = 0.12f;

// Past the dead zone, rescaled so the output still runs smoothly from 0 to 1.
float Rescale(float magnitude, float deadZone) {
    if (magnitude <= deadZone) return 0.0f;
    const float v = (magnitude - deadZone) / (1.0f - deadZone);
    return v > 1.0f ? 1.0f : v;
}

float Axis(float v, float deadZone) {
    return std::copysign(Rescale(std::fabs(v), deadZone), v);
}

Vector2 Stick(float x, float y, GamePadDeadZone mode) {
    switch (mode) {
    case GamePadDeadZone::None:
        return {x, y};
    case GamePadDeadZone::IndependentAxes:
        return {Axis(x, StickDeadZone), Axis(y, StickDeadZone)};
    case GamePadDeadZone::Circular: {
        const float length = std::sqrt(x * x + y * y);
        if (length <= StickDeadZone) return {};
        const float scale = Rescale(length, StickDeadZone) / length;
        return {x * scale, y * scale};
    }
    }
    return {};
}

bool ValidPlayer(int player) {
    return player >= 0 && player < GamePad::MaxPlayers;
}

} // namespace

GamePadState GamePad::GetState(int player, GamePadDeadZone deadZone) {
    if (!ValidPlayer(player)) return {};
    const detail::GamePadRaw& raw = detail::Input::Get().currentPads[static_cast<size_t>(player)];
    GamePadState state;
    if (!raw.connected) return state;
    state.isConnected = true;
    state.buttons = raw.buttons;
    state.leftStick = Stick(raw.leftX, raw.leftY, deadZone);
    state.rightStick = Stick(raw.rightX, raw.rightY, deadZone);
    state.leftTrigger = Rescale(raw.leftTrigger, TriggerDeadZone);
    state.rightTrigger = Rescale(raw.rightTrigger, TriggerDeadZone);
    return state;
}

bool GamePad::WasPressed(int player, Buttons button) {
    if (!ValidPlayer(player)) return false;
    const detail::Input& in = detail::Input::Get();
    const auto bit = static_cast<std::uint32_t>(button);
    const auto p = static_cast<size_t>(player);
    return (in.padPressed[p] & bit) || ((in.currentPads[p].buttons & bit) && !(in.previousPads[p].buttons & bit));
}

bool GamePad::WasReleased(int player, Buttons button) {
    if (!ValidPlayer(player)) return false;
    const detail::Input& in = detail::Input::Get();
    const auto bit = static_cast<std::uint32_t>(button);
    const auto p = static_cast<size_t>(player);
    return (in.padReleased[p] & bit) || (!(in.currentPads[p].buttons & bit) && (in.previousPads[p].buttons & bit));
}

namespace {

GamePadType TypeOf(SDL_GamepadType type) {
    switch (type) {
    case SDL_GAMEPAD_TYPE_STANDARD: return GamePadType::Standard;
    case SDL_GAMEPAD_TYPE_XBOX360: return GamePadType::Xbox360;
    case SDL_GAMEPAD_TYPE_XBOXONE: return GamePadType::XboxOne;
    case SDL_GAMEPAD_TYPE_PS3: return GamePadType::PS3;
    case SDL_GAMEPAD_TYPE_PS4: return GamePadType::PS4;
    case SDL_GAMEPAD_TYPE_PS5: return GamePadType::PS5;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: return GamePadType::SwitchPro;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return GamePadType::JoyConLeft;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return GamePadType::JoyConRight;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return GamePadType::JoyConPair;
    case SDL_GAMEPAD_TYPE_GAMECUBE: return GamePadType::GameCube;
    case SDL_GAMEPAD_TYPE_STEAM: return GamePadType::Steam;
    default: return GamePadType::Unknown;
    }
}

SDL_Gamepad* Pad(int player) {
    return ValidPlayer(player) ? detail::Input::Get().pads[static_cast<size_t>(player)] : nullptr;
}

} // namespace

GamePadCapabilities GamePad::GetCapabilities(int player) {
    GamePadCapabilities c;
    SDL_Gamepad* pad = Pad(player);
    if (!pad) return c;
    c.isConnected = true;
    const char* name = SDL_GetGamepadName(pad);
    c.name = name ? name : "";
    c.type = TypeOf(SDL_GetGamepadType(pad));
    c.hasRumble = SDL_GetBooleanProperty(SDL_GetGamepadProperties(pad), SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
    c.vendorId = SDL_GetGamepadVendor(pad);
    c.productId = SDL_GetGamepadProduct(pad);
    return c;
}

ButtonLabel GamePad::FaceButtonLabel(int player, Buttons button) {
    SDL_Gamepad* pad = Pad(player);
    if (!pad) return ButtonLabel::Unknown;
    SDL_GamepadButton which = SDL_GAMEPAD_BUTTON_INVALID;
    switch (button) {
    case Buttons::A: which = SDL_GAMEPAD_BUTTON_SOUTH; break;
    case Buttons::B: which = SDL_GAMEPAD_BUTTON_EAST; break;
    case Buttons::X: which = SDL_GAMEPAD_BUTTON_WEST; break;
    case Buttons::Y: which = SDL_GAMEPAD_BUTTON_NORTH; break;
    default: return ButtonLabel::Unknown;
    }
    switch (SDL_GetGamepadButtonLabel(pad, which)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return ButtonLabel::A;
    case SDL_GAMEPAD_BUTTON_LABEL_B: return ButtonLabel::B;
    case SDL_GAMEPAD_BUTTON_LABEL_X: return ButtonLabel::X;
    case SDL_GAMEPAD_BUTTON_LABEL_Y: return ButtonLabel::Y;
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return ButtonLabel::Cross;
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return ButtonLabel::Circle;
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return ButtonLabel::Square;
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return ButtonLabel::Triangle;
    default: return ButtonLabel::Unknown;
    }
}

bool GamePad::SetVibration(int player, float lowFrequency, float highFrequency) {
    if (!ValidPlayer(player)) return false;
    auto clamp = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    detail::Input& in = detail::Input::Get();
    in.rumbleLow[static_cast<size_t>(player)] = clamp(lowFrequency);
    in.rumbleHigh[static_cast<size_t>(player)] = clamp(highFrequency);
    return in.SendRumble(player);
}

} // namespace coldx
