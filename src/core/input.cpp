#include "input.h"

#include <SDL3/SDL.h>

namespace coldx::detail {

namespace {

struct ButtonMap {
    SDL_GamepadButton sdl;
    Buttons button;
};

constexpr ButtonMap PadButtons[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, Buttons::A},
    {SDL_GAMEPAD_BUTTON_EAST, Buttons::B},
    {SDL_GAMEPAD_BUTTON_WEST, Buttons::X},
    {SDL_GAMEPAD_BUTTON_NORTH, Buttons::Y},
    {SDL_GAMEPAD_BUTTON_BACK, Buttons::Back},
    {SDL_GAMEPAD_BUTTON_START, Buttons::Start},
    {SDL_GAMEPAD_BUTTON_GUIDE, Buttons::Guide},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Buttons::LeftShoulder},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Buttons::RightShoulder},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, Buttons::LeftStick},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, Buttons::RightStick},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, Buttons::DPadUp},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Buttons::DPadDown},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Buttons::DPadLeft},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Buttons::DPadRight},
};

float Axis(SDL_Gamepad* pad, SDL_GamepadAxis axis) {
    const float v = SDL_GetGamepadAxis(pad, axis) / 32767.0f;
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

bool MouseButtonFromSdl(Uint8 button, MouseButton& out) {
    switch (button) {
    case SDL_BUTTON_LEFT: out = MouseButton::Left; return true;
    case SDL_BUTTON_RIGHT: out = MouseButton::Right; return true;
    case SDL_BUTTON_MIDDLE: out = MouseButton::Middle; return true;
    case SDL_BUTTON_X1: out = MouseButton::X1; return true;
    case SDL_BUTTON_X2: out = MouseButton::X2; return true;
    default: return false;
    }
}

void SetDown(MouseState& m, MouseButton button, bool down) {
    switch (button) {
    case MouseButton::Left: m.left = down; break;
    case MouseButton::Right: m.right = down; break;
    case MouseButton::Middle: m.middle = down; break;
    case MouseButton::X1: m.x1 = down; break;
    case MouseButton::X2: m.x2 = down; break;
    }
}

} // namespace

Input& Input::Get() {
    static Input input;
    return input;
}

void Input::Start(SDL_Window* w) {
    Stop();
    *this = Input{};
    window = w;
}

void Input::Stop() {
    if (textActive && window) SDL_StopTextInput(window);
    textActive = false;
    for (SDL_Gamepad*& pad : pads) {
        if (pad) SDL_CloseGamepad(pad);
        pad = nullptr;
    }
}

float Input::PixelDensity() const {
    const float density = window ? SDL_GetWindowPixelDensity(window) : 1.0f;
    return density > 0.0f ? density : 1.0f;
}

// Window coordinates are scaled by the pixel density to window pixels, then, with a virtual
// resolution, through where the back buffer is shown.
void Input::WindowToBackBuffer(float x, float y, int& outX, int& outY) const {
    const float density = PixelDensity();
    if (!presentation.Active() || !window) {
        outX = static_cast<int>(x * density);
        outY = static_cast<int>(y * density);
        return;
    }
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    const Rectangle placed = FitRectangle(w, h, presentation.width, presentation.height, presentation.mode);
    OutputToImage(placed, presentation.width, presentation.height, x * density, y * density, outX, outY);
}

void Input::BackBufferToWindow(int x, int y, float& outX, float& outY) const {
    const float density = PixelDensity();
    if (!presentation.Active() || !window) {
        outX = x / density;
        outY = y / density;
        return;
    }
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    const Rectangle placed = FitRectangle(w, h, presentation.width, presentation.height, presentation.mode);
    ImageToOutput(placed, presentation.width, presentation.height, x, y, outX, outY);
    outX /= density;
    outY /= density;
}

void Input::SetKey(Key key, bool down) {
    if (down && liveKeys.IsKeyUp(key)) liveKeysPressed.SetKey(key, true);
    if (!down && liveKeys.IsKeyDown(key)) liveKeysReleased.SetKey(key, true);
    liveKeys.SetKey(key, down);
}

void Input::SetMouseButton(MouseButton button, bool down) {
    const auto bit = static_cast<std::uint8_t>(1u << static_cast<unsigned>(button));
    if (down && !liveMouse.IsDown(button)) liveMousePressed |= bit;
    if (!down && liveMouse.IsDown(button)) liveMouseReleased |= bit;
    SetDown(liveMouse, button, down);
}

void Input::HandleEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        SetKey(static_cast<Key>(event.key.scancode), event.type == SDL_EVENT_KEY_DOWN);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        WindowToBackBuffer(event.motion.x, event.motion.y, liveMouse.x, liveMouse.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        WindowToBackBuffer(event.button.x, event.button.y, liveMouse.x, liveMouse.y);
        if (MouseButton b{}; MouseButtonFromSdl(event.button.button, b)) SetMouseButton(b, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        // Pads are read whole at each Update. These only catch the presses in between.
        for (int player = 0; player < GamePad::MaxPlayers; ++player) {
            if (!pads[player] || SDL_GetGamepadID(pads[player]) != event.gbutton.which) continue;
            for (const ButtonMap& b : PadButtons) {
                if (b.sdl != event.gbutton.button) continue;
                const auto bit = static_cast<std::uint32_t>(b.button);
                if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) livePadPressed[static_cast<size_t>(player)] |= bit;
                else livePadReleased[static_cast<size_t>(player)] |= bit;
            }
        }
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (textActive && event.text.text) liveText += event.text.text;
        break;
    case SDL_EVENT_TEXT_EDITING:
        composition = event.edit.text ? event.edit.text : "";
        compositionCursor = event.edit.start;
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
        // "Natural" scrolling reports the opposite sign. Undo it so up is always up.
        const float flip = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
        liveMouse.scrollWheel += event.wheel.y * flip;
        liveMouse.horizontalScrollWheel += event.wheel.x * flip;
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        // Releases that happen while another window has focus never arrive here.
        liveKeys = KeyboardState{};
        composition.clear();
        compositionCursor = 0;
        liveMouse.left = liveMouse.right = liveMouse.middle = liveMouse.x1 = liveMouse.x2 = false;
        break;
    case SDL_EVENT_GAMEPAD_ADDED: {
        for (int player = 0; player < GamePad::MaxPlayers; ++player) {
            if (pads[player] && SDL_GetGamepadID(pads[player]) == event.gdevice.which) return;
        }
        for (int player = 0; player < GamePad::MaxPlayers; ++player) {
            if (pads[player]) continue;
            pads[player] = SDL_OpenGamepad(event.gdevice.which);
            if (pads[player]) {
                const char* name = SDL_GetGamepadName(pads[player]);
                SDL_Log("coldx.core: gamepad \"%s\" connected as player %d", name ? name : "?", player);
            } else {
                SDL_Log("coldx.core: could not open gamepad: %s", SDL_GetError());
            }
            return;
        }
        SDL_Log("coldx.core: gamepad ignored, all %d players are taken", GamePad::MaxPlayers);
        break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
        for (int player = 0; player < GamePad::MaxPlayers; ++player) {
            if (pads[player] && SDL_GetGamepadID(pads[player]) == event.gdevice.which) {
                SDL_CloseGamepad(pads[player]);
                pads[player] = nullptr;
                SDL_Log("coldx.core: gamepad for player %d disconnected", player);
            }
        }
        break;
    default:
        break;
    }
}

bool Input::SendRumble(int player) {
    SDL_Gamepad* pad = pads[static_cast<size_t>(player)];
    if (!pad) return false;
    auto strength = [](float v) { return static_cast<Uint16>(v * 65535.0f + 0.5f); };
    rumbleSentAt[static_cast<size_t>(player)] = SDL_GetTicks();
    return SDL_RumbleGamepad(pad, strength(rumbleLow[static_cast<size_t>(player)]),
                             strength(rumbleHigh[static_cast<size_t>(player)]), 0xFFFF);
}

void Input::BeginUpdate() {
    previousKeys = currentKeys;
    currentKeys = liveKeys;
    previousMouse = currentMouse;
    currentMouse = liveMouse;
    keysPressed = liveKeysPressed;
    keysReleased = liveKeysReleased;
    liveKeysPressed = liveKeysReleased = KeyboardState{};
    mousePressed = liveMousePressed;
    mouseReleased = liveMouseReleased;
    liveMousePressed = liveMouseReleased = 0;
    padPressed = livePadPressed;
    padReleased = livePadReleased;
    livePadPressed = livePadReleased = {};
    currentText.swap(liveText);
    liveText.clear();
    for (int player = 0; player < GamePad::MaxPlayers; ++player) {
        previousPads[player] = currentPads[player];
        GamePadRaw raw;
        SDL_Gamepad* pad = pads[player];
        if (pad && SDL_GamepadConnected(pad)) {
            raw.connected = true;
            for (const ButtonMap& b : PadButtons) {
                if (SDL_GetGamepadButton(pad, b.sdl)) raw.buttons |= static_cast<std::uint32_t>(b.button);
            }
            raw.leftX = Axis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            raw.leftY = -Axis(pad, SDL_GAMEPAD_AXIS_LEFTY);
            raw.rightX = Axis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
            raw.rightY = -Axis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
            raw.leftTrigger = Axis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
            raw.rightTrigger = Axis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        }
        currentPads[player] = raw;

        const auto p = static_cast<size_t>(player);
        const bool rumbling = rumbleLow[p] > 0.0f || rumbleHigh[p] > 0.0f;
        if (pad && rumbling && SDL_GetTicks() - rumbleSentAt[p] > 30000) SendRumble(player);
    }
}

} // namespace coldx::detail
