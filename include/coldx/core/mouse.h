#pragma once

#include <coldx/core/point.h>

namespace coldx {

enum class MouseButton {
    Left,
    Right,
    Middle,
    X1,
    X2,
};

struct MouseState {
    // Back buffer pixels, (0, 0) at the top left. Outside the window it keeps the last
    // position inside it.
    int x = 0;
    int y = 0;
    // Notches scrolled since Run started: up and right are positive. Fractional on
    // touchpads and smooth-scrolling wheels.
    float scrollWheel = 0.0f;
    float horizontalScrollWheel = 0.0f;
    bool left = false;
    bool right = false;
    bool middle = false;
    bool x1 = false;
    bool x2 = false;

    constexpr Point Position() const { return {x, y}; }

    constexpr bool IsDown(MouseButton button) const {
        switch (button) {
        case MouseButton::Left: return left;
        case MouseButton::Right: return right;
        case MouseButton::Middle: return middle;
        case MouseButton::X1: return x1;
        case MouseButton::X2: return x2;
        }
        return false;
    }
};

class Mouse {
public:
    // The state as of the start of the current Update.
    static MouseState GetState();

    // Down now, up at the start of the previous Update; and the reverse.
    static bool WasPressed(MouseButton button);
    static bool WasReleased(MouseButton button);

    // Notches scrolled since the previous Update.
    static float ScrollWheelDelta();
    static float HorizontalScrollWheelDelta();

    // Moves the pointer, in back buffer pixels. The next Update sees the new position.
    static void SetPosition(int x, int y);
    static void SetCursorVisible(bool visible);
};

} // namespace coldx
