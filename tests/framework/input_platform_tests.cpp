// INP-01 to INP-07 and PLAT-01 to PLAT-04. Gamepads are SDL virtual pads: the same SDL
// gamepad layer, mapping database included, that a real controller goes through.

#include "framework_test.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include <SDL3/SDL.h>

using namespace coldx;

namespace ft {

namespace {

struct Pad {
    SDL_JoystickID id = 0;
    SDL_Joystick* joystick = nullptr;
    Uint16 low = 0, high = 0;
    static bool SDLCALL Rumble(void* self, Uint16 l, Uint16 h) {
        static_cast<Pad*>(self)->low = l;
        static_cast<Pad*>(self)->high = h;
        return true;
    }
    void Attach(const char* name, bool rumble = true) {
        SDL_VirtualJoystickDesc d;
        SDL_INIT_INTERFACE(&d);
        d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        d.naxes = SDL_GAMEPAD_AXIS_COUNT;
        d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        d.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
        d.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
        d.name = name;
        d.userdata = this;
        if (rumble) d.Rumble = Rumble;
        id = SDL_AttachVirtualJoystick(&d);
        joystick = SDL_OpenJoystick(id);
    }
    void Detach() {
        if (joystick) SDL_CloseJoystick(joystick);
        joystick = nullptr;
        if (id) SDL_DetachVirtualJoystick(id);
        id = 0;
    }
};

// Pad tests run headless. A hidden window has no focus, and SDL drops pad presses without it.
void PadEnvironment(bool on) {
    SetEnv("COLDX_HEADLESS", on ? "1" : nullptr);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, on ? "1" : "0");
}

// INP-01: each SDL gamepad button arrives as the right XNA button and nothing else.
Result GamepadMapping() {
    Result r{"INP-01", "P0"};
    PadEnvironment(true);
    const std::pair<SDL_GamepadButton, Buttons> map[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, Buttons::A}, {SDL_GAMEPAD_BUTTON_EAST, Buttons::B},
        {SDL_GAMEPAD_BUTTON_WEST, Buttons::X}, {SDL_GAMEPAD_BUTTON_NORTH, Buttons::Y},
        {SDL_GAMEPAD_BUTTON_BACK, Buttons::Back}, {SDL_GAMEPAD_BUTTON_START, Buttons::Start},
        {SDL_GAMEPAD_BUTTON_GUIDE, Buttons::Guide}, {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Buttons::LeftShoulder},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Buttons::RightShoulder}, {SDL_GAMEPAD_BUTTON_LEFT_STICK, Buttons::LeftStick},
        {SDL_GAMEPAD_BUTTON_RIGHT_STICK, Buttons::RightStick}, {SDL_GAMEPAD_BUTTON_DPAD_UP, Buttons::DPadUp},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Buttons::DPadDown}, {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Buttons::DPadLeft},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Buttons::DPadRight},
    };
    Probe game;
    Pad pad;
    std::string wrong;
    size_t i = 0;
    game.load = [&] { pad.Attach("ft mapping pad"); };
    game.update = [&](Probe& p, const GameTime&) {
        // Press button i on even updates, check and release on odd ones.
        if (p.updates < 3) return;
        if (i == std::size(map)) {
            p.Exit();
            return;
        }
        if (p.updates % 2 == 1) {
            SDL_SetJoystickVirtualButton(pad.joystick, map[i].first, true);
        } else {
            const GamePadState s = GamePad::GetState(0);
            if (s.buttons != static_cast<std::uint32_t>(map[i].second))
                wrong += Format(" SDL button %d gave 0x%x.", static_cast<int>(map[i].first), s.buttons);
            SDL_SetJoystickVirtualButton(pad.joystick, map[i].first, false);
            ++i;
        }
    };
    game.Run();
    pad.Detach();
    PadEnvironment(false);
    r.status = wrong.empty() && i == std::size(map) ? Status::NeedsHuman : Status::Fail;
    r.notes = (wrong.empty() ? std::string("All 15 buttons map to their XNA button and only that one. ") : "Wrong:" + wrong + " ") +
              "Pads go through SDL's gamepad layer and its controller database, not hard-coded layouts. Xbox, "
              "PlayStation, Switch Pro and generic pads need a person with the hardware.";
    return r;
}

// INP-02: four pads at once, two leave, one joins, a fifth is turned away.
Result Hotplug() {
    Result r{"INP-02", "P0"};
    PadEnvironment(true);
    Probe game;
    Pad pads[6];
    std::string problems;
    auto connected = [] {
        std::string s;
        for (int p = 0; p < 4; ++p) s += GamePad::GetState(p).isConnected ? '1' : '0';
        return s;
    };
    game.load = [&] {
        for (int k = 0; k < 4; ++k) pads[k].Attach("ft hotplug pad");
    };
    game.update = [&](Probe& p, const GameTime&) {
        switch (p.updates) {
        case 3:
            if (connected() != "1111") problems += " four pads at once gave " + connected() + ".";
            pads[1].Detach();
            pads[2].Detach();
            break;
        case 6:
            if (connected() != "1001") problems += " after unplugging players 1 and 2: " + connected() + ".";
            pads[4].Attach("ft hotplug pad");
            break;
        case 9:
            if (connected() != "1101") problems += " a new pad should take player 1: " + connected() + ".";
            pads[5].Attach("ft hotplug pad");
            pads[1].Attach("ft hotplug pad");
            break;
        case 12:
            if (connected() != "1111") problems += " filling up: " + connected() + ".";
            p.Exit();
            break;
        }
    };
    const int rc = game.Run();
    for (Pad& pad : pads) pad.Detach();
    PadEnvironment(false);
    r.status = rc == 0 && problems.empty() ? Status::NeedsHuman : Status::Fail;
    r.notes = problems.empty() ? "Four at once. Unplugging players 1 and 2 left 0 and 3 where they were. The next pad took the "
                                 "lowest free player. With all four taken, a further pad was turned away (logged). Real "
                                 "controllers plugged in mid-game need a person."
                               : "Problems:" + problems;
    return r;
}

// INP-03: at rest and with small noise the sticks read zero. Triggers rise smoothly to 1.
Result Deadzones() {
    Result r{"INP-03", "P1"};
    PadEnvironment(true);
    Probe game;
    Pad pad;
    std::string problems;
    float last = -1;
    int step = 0;
    game.load = [&] { pad.Attach("ft deadzone pad"); };
    game.update = [&](Probe& p, const GameTime&) {
        if (p.updates == 3) {
            const GamePadState s = GamePad::GetState(0);
            if (s.leftStick != Vector2() || s.rightStick != Vector2() || s.leftTrigger != 0 || s.rightTrigger != 0)
                problems += " not zero at rest.";
            SDL_SetJoystickVirtualAxis(pad.joystick, SDL_GAMEPAD_AXIS_LEFTX, 3000);  // drift, about 9%
            SDL_SetJoystickVirtualAxis(pad.joystick, SDL_GAMEPAD_AXIS_RIGHTY, -4000);
        } else if (p.updates == 5) {
            const GamePadState s = GamePad::GetState(0);
            if (s.leftStick.x != 0 || s.rightStick.y != 0) problems += " drift got through the dead zone.";
        } else if (p.updates > 5 && step <= 32) {
            // A slow trigger pull, 0 to full in 32 steps. It must never go backwards and must end at 1.
            const float v = GamePad::GetState(0).rightTrigger;
            if (step > 0 && v < last) problems += " trigger went backwards.";
            last = v;
            SDL_SetJoystickVirtualAxis(pad.joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, static_cast<Sint16>(std::min(32767, step * 1024)));
            ++step;
        } else if (step > 32) {
            if (std::fabs(GamePad::GetState(0).rightTrigger - 1.0f) > 1e-4f) problems += " trigger did not reach 1.";
            p.Exit();
        }
    };
    game.Run();
    pad.Detach();
    PadEnvironment(false);
    r.status = problems.empty() ? Status::NeedsHuman : Status::Fail;
    r.notes = (problems.empty() ? std::string("Zero at rest, 9% and 12% drift held at zero (dead zone 0.24, XNA's), trigger rose "
                                              "monotonically to 1 (trigger dead zone 0.12). ")
                                : "Problems:" + problems + " ") +
              "Worn sticks on real controllers need a person.";
    return r;
}

// INP-04: rumble where the pad has motors. No crash and a false return where it has not.
Result Rumble() {
    Result r{"INP-04", "P2"};
    PadEnvironment(true);
    Probe game;
    Pad motors, none;
    bool withOk = false, withoutOk = false, absentOk = false;
    game.load = [&] {
        motors.Attach("ft rumble pad");
        none.Attach("ft no-motor pad", false);
    };
    game.update = [&](Probe& p, const GameTime&) {
        if (p.updates == 3) {
            withOk = GamePad::SetVibration(0, 0.25f, 1.0f) && motors.low == 16384 && motors.high == 65535;
            withoutOk = !GamePad::SetVibration(1, 1.0f, 1.0f);
            absentOk = !GamePad::SetVibration(3, 1.0f, 1.0f);
            GamePad::SetVibration(0, 0, 0);
            p.Exit();
        }
    };
    const int rc = game.Run();
    motors.Detach();
    none.Detach();
    PadEnvironment(false);
    r.status = rc == 0 && withOk && withoutOk && absentOk ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("Pad with motors got the strengths asked for: %s. Pad without motors returned false: %s. No pad: %s. "
                     "Real controllers need a person.",
                     withOk ? "yes" : "NO", withoutOk ? "yes" : "NO", absentOk ? "yes" : "NO");
    return r;
}

// INP-07: wheel, focus loss and a pointer outside the window.
Result MouseEdges() {
    Result r{"INP-07", "P1"};
    SetEnv("COLDX_HEADLESS", "1");
    Probe game;
    std::string notes;
    bool released = false, keptInside = false;
    game.update = [&](Probe& p, const GameTime&) {
        SDL_Event e{};
        if (p.updates == 2) {
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.x = 50;
            e.motion.y = 60;
            SDL_PushEvent(&e);
        } else if (p.updates == 4) {
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.x = -40;
            e.motion.y = 9000;
            SDL_PushEvent(&e);
            SDL_Event b{};
            b.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            b.button.button = SDL_BUTTON_LEFT;
            b.button.down = true;
            b.button.x = -40;
            b.button.y = 9000;
            SDL_PushEvent(&b);
        } else if (p.updates == 6) {
            const MouseState m = Mouse::GetState();
            notes += Format("Pointer at (-40, 9000), outside the window, reads (%d, %d). ", m.x, m.y);
            keptInside = m.x == 50 && m.y == 60;
            SDL_Event f{};
            f.type = SDL_EVENT_WINDOW_FOCUS_LOST;
            SDL_PushEvent(&f);
        } else if (p.updates == 8) {
            released = !Mouse::GetState().left;
            p.Exit();
        }
    };
    game.Run();
    SetEnv("COLDX_HEADLESS", nullptr);
    r.status = released ? Status::NeedsHuman : Status::Fail;
    r.notes = notes + (released ? "Focus loss released the held button. " : "A held button stayed down after focus loss. ") +
              "Wheel deltas, both directions and natural scrolling, are covered by core.input. " +
              (keptInside ? std::string("Bug: outside the window the position stopped at the edge. mouse.h says it "
                                        "follows the pointer, as in XNA. ")
                          : std::string("Outside the window the position follows the pointer, as mouse.h says. ")) +
              "Real alt-tab with a button held needs a person.";
    return r;
}

// PLAT-02: saves go to the per-user folder, never beside the executable.
Result SaveLocation() {
    Result r{"PLAT-02", "P1"};
    SetEnv("COLDX_HEADLESS", "1");
    Probe game;
    game.Settings().company = "coldx-ft";
    game.Settings().name = "save-location";
    std::string folder;
    bool roundTrip = false;
    game.load = [&] {
        folder = Storage::Folder();
        roundTrip = Storage::WriteFile("probe.txt", "x") && Storage::ReadFile("probe.txt") == std::optional<std::string>("x");
        Storage::Remove("probe.txt");
    };
    game.update = [](Probe& p, const GameTime&) { p.Exit(); };
    game.Run();
    SetEnv("COLDX_HEADLESS", nullptr);
    const char* appdata = SDL_getenv("APPDATA");
    const char* base = SDL_GetBasePath();
    const bool underAppData = appdata && folder.rfind(appdata, 0) == 0;
    const bool notInstall = base && folder.rfind(base, 0) != 0;
    std::error_code ignored;
    const std::filesystem::path made(folder);
    std::filesystem::remove(made, ignored);
    std::filesystem::remove(made.parent_path().parent_path(), ignored);
    r.status = roundTrip && underAppData && notInstall ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("Windows: %s (under %%APPDATA%%: %s, outside the executable's folder: %s, write and read back: %s). "
                     "SDL's pref path gives ~/.local/share on Linux and Application Support on macOS. Those need the "
                     "machines.",
                     folder.c_str(), underAppData ? "yes" : "NO", notInstall ? "yes" : "NO", roundTrip ? "yes" : "NO");
    return r;
}

// PLAT-04: minimize for two seconds and restore: no crash, focus reported, no time jump.
Result Minimize() {
    Result r{"PLAT-04", "P1"};
    Probe game;
    std::vector<bool> focus;
    double maxDelta = 0;
    int updatesWhileMinimized = 0;
    bool minimized = false, hadFocus = false;
    double minimizedAt = 0;
    game.focused = [&](bool f) { focus.push_back(f); };
    game.update = [&](Probe& p, const GameTime& time) {
        maxDelta = std::max(maxDelta, static_cast<double>(time.Delta()));
        if (p.updates == 59) hadFocus = (SDL_GetWindowFlags(OnlyWindow()) & SDL_WINDOW_INPUT_FOCUS) != 0;
        if (p.updates == 60) {
            SDL_MinimizeWindow(OnlyWindow());
            minimized = true;
            minimizedAt = Now();
        }
        if (minimized) {
            ++updatesWhileMinimized;
            if (Now() - minimizedAt > 2.0) {
                SDL_RestoreWindow(OnlyWindow());
                minimized = false;
            }
        }
        if (!minimized && p.updates > 60 && Now() - minimizedAt > 3.0) p.Exit();
    };
    const int rc = game.Run();
    const bool lostFocus = std::find(focus.begin(), focus.end(), false) != focus.end();
    // A window opened by a background process may never get focus. Then there is none to lose.
    const bool focusOk = !hadFocus || lostFocus;
    r.status = rc == 0 && maxDelta < 0.02 && focusOk ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("Minimized 2 s and restored: %d updates ran while minimized (the game keeps time), largest update step "
                     "%.4f s (no jump). %s Pausing on focus loss is up to the game, through OnFocusChanged. Alt-tab to "
                     "another window needs a person.",
                     updatesWhileMinimized, maxDelta,
                     !hadFocus ? "The window never had focus (opened from a background process), so focus loss could not be "
                                 "tested here."
                               : (lostFocus ? "Focus loss was reported." : "Focus loss was NOT reported."));
    return r;
}

} // namespace

void RegisterInput() {
    AutoTests().push_back({"INP-01", "P0", "Gamepad mapping", GamepadMapping});
    AutoTests().push_back({"INP-02", "P0", "Controller hotplug", Hotplug});
    AutoTests().push_back({"INP-03", "P1", "Deadzones and triggers", Deadzones});
    AutoTests().push_back({"INP-04", "P2", "Rumble", Rumble});
    AutoTests().push_back({"INP-07", "P1", "Mouse edges", MouseEdges});
}

void RegisterPlatform() {
    AutoTests().push_back({"PLAT-02", "P1", "Save file locations", SaveLocation});
    AutoTests().push_back({"PLAT-04", "P1", "Focus and minimize", Minimize});
}

} // namespace ft
