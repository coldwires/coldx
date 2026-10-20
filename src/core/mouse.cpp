#include <coldx/core/mouse.h>

#include <SDL3/SDL.h>

#include "input.h"

namespace coldx {

MouseState Mouse::GetState() {
    return detail::Input::Get().currentMouse;
}

bool Mouse::WasPressed(MouseButton button) {
    const detail::Input& in = detail::Input::Get();
    return in.currentMouse.IsDown(button) && !in.previousMouse.IsDown(button);
}

bool Mouse::WasReleased(MouseButton button) {
    const detail::Input& in = detail::Input::Get();
    return !in.currentMouse.IsDown(button) && in.previousMouse.IsDown(button);
}

float Mouse::ScrollWheelDelta() {
    const detail::Input& in = detail::Input::Get();
    return in.currentMouse.scrollWheel - in.previousMouse.scrollWheel;
}

float Mouse::HorizontalScrollWheelDelta() {
    const detail::Input& in = detail::Input::Get();
    return in.currentMouse.horizontalScrollWheel - in.previousMouse.horizontalScrollWheel;
}

void Mouse::SetPosition(int x, int y) {
    detail::Input& in = detail::Input::Get();
    if (!in.window) return;
    const float density = in.PixelDensity();
    SDL_WarpMouseInWindow(in.window, x / density, y / density);
    // Warping also queues a motion event; setting it here as well means the next Update sees
    // the position even where the platform does not report warps.
    in.liveMouse.x = x;
    in.liveMouse.y = y;
}

void Mouse::SetCursorVisible(bool visible) {
    if (visible)
        SDL_ShowCursor();
    else
        SDL_HideCursor();
}

} // namespace coldx
