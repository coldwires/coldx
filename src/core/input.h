#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <coldx/core/gamepad.h>
#include <coldx/core/keyboard.h>
#include <coldx/core/mouse.h>

#include "scaling.h"

union SDL_Event;
struct SDL_Window;
struct SDL_Gamepad;

namespace coldx::detail {

// A gamepad as read, before any dead zone.
struct GamePadRaw {
    bool connected = false;
    std::uint32_t buttons = 0;
    float leftX = 0, leftY = 0, rightX = 0, rightY = 0;  // -1..1, up positive
    float leftTrigger = 0, rightTrigger = 0;             // 0..1
};

// Everything the public input classes read. "live" follows SDL events as they arrive;
// "current" and "previous" are copies taken at the start of each Update, so a game sees one
// consistent state per Update and can tell what changed since the last one.
struct Input {
    SDL_Window* window = nullptr;
    // The game's virtual resolution, if any: mouse positions are in its pixels.
    Presentation presentation;
    KeyboardState liveKeys, currentKeys, previousKeys;
    MouseState liveMouse, currentMouse, previousMouse;
    // Text input: typed text gathers in liveText and moves to currentText each Update.
    bool textActive = false;
    std::string liveText, currentText;
    std::string composition;
    int compositionCursor = 0;
    std::array<SDL_Gamepad*, GamePad::MaxPlayers> pads{};
    std::array<GamePadRaw, GamePad::MaxPlayers> currentPads{}, previousPads{};
    // Requested rumble per player, and when it was last sent. SDL ends a rumble after at
    // most about 65 seconds, so BeginUpdate sends it again while it is on.
    std::array<float, GamePad::MaxPlayers> rumbleLow{}, rumbleHigh{};
    std::array<std::uint64_t, GamePad::MaxPlayers> rumbleSentAt{};

    // Sends player's requested rumble now. False if there is no pad or it cannot rumble.
    bool SendRumble(int player);

    static Input& Get();

    // Run start and end. Start forgets all state; Stop closes the pads.
    void Start(SDL_Window* window);
    void Stop();

    void HandleEvent(const SDL_Event& event);
    // Polls the pads and takes the per-Update copies.
    void BeginUpdate();

    // Window coordinates to back buffer pixels, and back.
    float PixelDensity() const;
    void WindowToBackBuffer(float x, float y, int& outX, int& outY) const;
    void BackBufferToWindow(int x, int y, float& outX, float& outY) const;
};

} // namespace coldx::detail
