#include <coldx/core/keyboard.h>

#include <SDL3/SDL.h>

namespace coldx {

// Key values are cast straight to SDL scancodes. These pin the two numbering schemes together.
static_assert(static_cast<int>(Key::A) == SDL_SCANCODE_A);
static_assert(static_cast<int>(Key::Z) == SDL_SCANCODE_Z);
static_assert(static_cast<int>(Key::D1) == SDL_SCANCODE_1);
static_assert(static_cast<int>(Key::D0) == SDL_SCANCODE_0);
static_assert(static_cast<int>(Key::Escape) == SDL_SCANCODE_ESCAPE);
static_assert(static_cast<int>(Key::Semicolon) == SDL_SCANCODE_SEMICOLON);
static_assert(static_cast<int>(Key::CapsLock) == SDL_SCANCODE_CAPSLOCK);
static_assert(static_cast<int>(Key::F12) == SDL_SCANCODE_F12);
static_assert(static_cast<int>(Key::PrintScreen) == SDL_SCANCODE_PRINTSCREEN);
static_assert(static_cast<int>(Key::Up) == SDL_SCANCODE_UP);
static_assert(static_cast<int>(Key::LeftControl) == SDL_SCANCODE_LCTRL);
static_assert(static_cast<int>(Key::RightSuper) == SDL_SCANCODE_RGUI);
static_assert(KeyboardState::KeyCount >= SDL_SCANCODE_COUNT);

KeyboardState Keyboard::GetState() {
    KeyboardState state;
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    for (int i = 0; i < count; ++i) {
        if (keys[i]) state.SetKey(static_cast<Key>(i), true);
    }
    return state;
}

} // namespace coldx
