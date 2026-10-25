#include <coldx/core/keyboard.h>

#include <SDL3/SDL.h>

#include "input.h"

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
    return detail::Input::Get().currentKeys;
}

// The edges since the previous Update, including any that went and came back between
// two (a tap); the state comparison as well, for changes with no event (focus lost).
bool Keyboard::WasPressed(Key key) {
    const detail::Input& in = detail::Input::Get();
    return in.keysPressed.IsKeyDown(key) || (in.currentKeys.IsKeyDown(key) && in.previousKeys.IsKeyUp(key));
}

bool Keyboard::WasReleased(Key key) {
    const detail::Input& in = detail::Input::Get();
    return in.keysReleased.IsKeyDown(key) || (in.currentKeys.IsKeyUp(key) && in.previousKeys.IsKeyDown(key));
}

std::string Keyboard::KeyName(Key key) {
    const SDL_Keycode code = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(key), SDL_KMOD_NONE, false);
    const char* name = code != SDLK_UNKNOWN ? SDL_GetKeyName(code) : "";
    return name ? name : "";
}

Key Keyboard::KeyForCharacter(char32_t character) {
    // Keycodes for characters are the lowercase character.
    if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
    SDL_Keymod mods = SDL_KMOD_NONE;
    const SDL_Scancode code = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(character), &mods);
    return mods == SDL_KMOD_NONE ? static_cast<Key>(code) : Key::Unknown;
}

} // namespace coldx
