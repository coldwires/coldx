// Keyboard, mouse and gamepad state, the pressed/released edges, scripted input, and
// Storage. Input arrives the way it does in a real game: SDL events for keys and the mouse,
// SDL virtual gamepads for pads. Runs headless with a variable timestep, so each frame is
// exactly one Update and an event pushed during Update N is seen by Update N + 1.

#include <coldx/coldx.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>

#include <SDL3/SDL.h>

#include "check.h"

using namespace coldx;
using coldx_test::CheckResult;

namespace {

// Runs step(update) once per Update until it returns false.
class Driver : public Game {
public:
    std::function<void()> initialize;
    std::function<bool(int)> step;
    int updates = 0;

    Driver() {
        Settings().width = 64;
        Settings().height = 64;
        Settings().fixedTimeStep = false;
        Settings().console = ConsoleMode::Never;
    }

protected:
    void Initialize() override {
        if (initialize) initialize();
    }
    void Update(const GameTime&) override {
        if (!step(++updates) || updates > 200) Exit();
    }
};

void Headless(const char* inputScript = nullptr) {
    SDL_Environment* env = SDL_GetEnvironment();
    SDL_SetEnvironmentVariable(env, "COLDX_HEADLESS", "1", true);
    SDL_UnsetEnvironmentVariable(env, "COLDX_FRAMES");
    SDL_UnsetEnvironmentVariable(env, "COLDX_CAPTURE");
    if (inputScript)
        SDL_SetEnvironmentVariable(env, "COLDX_INPUT", inputScript, true);
    else
        SDL_UnsetEnvironmentVariable(env, "COLDX_INPUT");
}

void PushKey(SDL_Scancode code, bool down) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.scancode = code;
    e.key.down = down;
    SDL_PushEvent(&e);
}

void PushEvent(SDL_EventType type) {
    SDL_Event e{};
    e.type = type;
    SDL_PushEvent(&e);
}

void PushMouseMove(float x, float y) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.x = x;
    e.motion.y = y;
    SDL_PushEvent(&e);
}

void PushMouseButton(Uint8 button, bool down, float x, float y) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button = button;
    e.button.down = down;
    e.button.x = x;
    e.button.y = y;
    SDL_PushEvent(&e);
}

void PushWheel(float y, float x, bool flipped) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.y = y;
    e.wheel.x = x;
    e.wheel.direction = flipped ? SDL_MOUSEWHEEL_FLIPPED : SDL_MOUSEWHEEL_NORMAL;
    SDL_PushEvent(&e);
}

bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-3f;
}

void KeyboardEdges() {
    Headless();
    Driver game;
    game.step = [](int n) {
        switch (n) {
        case 1: PushKey(SDL_SCANCODE_UP, true); break;
        case 2:
            CHECK(Keyboard::GetState().IsKeyDown(Key::Up));
            CHECK(Keyboard::WasPressed(Key::Up));
            CHECK(!Keyboard::WasReleased(Key::Up));
            break;
        case 3:
            CHECK(Keyboard::GetState().IsKeyDown(Key::Up));
            CHECK(!Keyboard::WasPressed(Key::Up));  // still held: no second press
            PushKey(SDL_SCANCODE_UP, false);
            break;
        case 4:
            CHECK(Keyboard::GetState().IsKeyUp(Key::Up));
            CHECK(Keyboard::WasReleased(Key::Up));
            PushKey(SDL_SCANCODE_A, true);
            PushKey(SDL_SCANCODE_LSHIFT, true);
            break;
        case 5:
            CHECK(Keyboard::GetState().IsKeyDown(Key::A) && Keyboard::GetState().IsKeyDown(Key::LeftShift));
            PushEvent(SDL_EVENT_WINDOW_FOCUS_LOST);
            break;
        case 6:
            // Losing focus lets go of everything: the key-up would go to another window.
            CHECK(Keyboard::GetState().IsKeyUp(Key::A) && Keyboard::GetState().IsKeyUp(Key::LeftShift));
            CHECK(Keyboard::WasReleased(Key::A));
            return false;
        }
        return true;
    };
    CHECK(game.Run() == 0);
    CHECK(game.updates == 6);
}

void MouseState_() {
    Headless();
    Driver game;
    float density = 1.0f;
    game.initialize = [&] {
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        CHECK(count == 1);
        if (count == 1) density = SDL_GetWindowPixelDensity(windows[0]);
        SDL_free(windows);
    };
    game.step = [&](int n) {
        switch (n) {
        case 1: PushMouseMove(10, 20); break;
        case 2:
            CHECK(Mouse::GetState().Position() == Point(static_cast<int>(10 * density), static_cast<int>(20 * density)));
            PushMouseButton(SDL_BUTTON_LEFT, true, 11, 21);
            PushMouseButton(SDL_BUTTON_RIGHT, true, 11, 21);
            break;
        case 3:
            CHECK(Mouse::GetState().left && Mouse::GetState().right && !Mouse::GetState().middle);
            CHECK(Mouse::WasPressed(MouseButton::Left) && Mouse::WasPressed(MouseButton::Right));
            CHECK(Mouse::GetState().x == static_cast<int>(11 * density));
            PushMouseButton(SDL_BUTTON_LEFT, false, 11, 21);
            PushMouseButton(SDL_BUTTON_X2, true, 11, 21);
            break;
        case 4:
            CHECK(Mouse::WasReleased(MouseButton::Left) && !Mouse::WasReleased(MouseButton::Right));
            CHECK(Mouse::WasPressed(MouseButton::X2) && Mouse::GetState().IsDown(MouseButton::X2));
            PushWheel(2.0f, -1.0f, false);
            PushWheel(0.5f, 0.0f, false);
            break;
        case 5:
            CHECK(Near(Mouse::ScrollWheelDelta(), 2.5f));
            CHECK(Near(Mouse::HorizontalScrollWheelDelta(), -1.0f));
            CHECK(Near(Mouse::GetState().scrollWheel, 2.5f));
            // A "natural scrolling" device reports the opposite sign; up must still be up.
            PushWheel(-1.0f, 0.0f, true);
            break;
        case 6:
            CHECK(Near(Mouse::ScrollWheelDelta(), 1.0f));
            break;
        case 7:
            CHECK(Near(Mouse::ScrollWheelDelta(), 0.0f));
            PushEvent(SDL_EVENT_WINDOW_FOCUS_LOST);
            break;
        case 8:
            CHECK(!Mouse::GetState().right && !Mouse::GetState().x2);
            return false;
        }
        return true;
    };
    CHECK(game.Run() == 0);
    CHECK(game.updates == 8);
}

// A virtual pad SDL treats as a standard gamepad: every button and axis, in SDL order.
struct VirtualPad {
    SDL_JoystickID id = 0;
    SDL_Joystick* joystick = nullptr;
    Uint16 rumbleLow = 0, rumbleHigh = 0;

    static bool SDLCALL OnRumble(void* user, Uint16 low, Uint16 high) {
        auto* pad = static_cast<VirtualPad*>(user);
        pad->rumbleLow = low;
        pad->rumbleHigh = high;
        return true;
    }

    void Attach(const char* name, Uint16 vendor = 0, Uint16 product = 0) {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.vendor_id = vendor;
        desc.product_id = product;
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
        desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
        desc.name = name;
        desc.userdata = this;
        desc.Rumble = OnRumble;
        id = SDL_AttachVirtualJoystick(&desc);
        CHECK(id != 0);
        joystick = SDL_OpenJoystick(id);
        CHECK(joystick != nullptr);
    }

    void Detach() {
        SDL_CloseJoystick(joystick);
        joystick = nullptr;
        CHECK(SDL_DetachVirtualJoystick(id));
    }

    void Axis(SDL_GamepadAxis axis, Sint16 value) { SDL_SetJoystickVirtualAxis(joystick, axis, value); }
    void Button(SDL_GamepadButton button, bool down) { SDL_SetJoystickVirtualButton(joystick, button, down); }
};

float ExpectAxis(float v) {  // IndependentAxes dead zone, as documented
    const float m = std::fabs(v);
    return m <= 0.24f ? 0.0f : std::copysign((m - 0.24f) / 0.76f, v);
}

void GamePads() {
    Headless();
    // SDL drops pad button presses while no window has focus, and a headless window never
    // has it. A real game keeps that default; the test needs the presses.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    Driver game;
    VirtualPad first, second, third;
    game.initialize = [&] {
        first.Attach("coldx test pad 1");
        second.Attach("coldx test pad 2");
    };
    game.step = [&](int n) {
        switch (n) {
        case 1:
            CHECK(GamePad::GetState(0).isConnected && GamePad::GetState(1).isConnected);
            CHECK(!GamePad::GetState(2).isConnected && !GamePad::GetState(-1).isConnected);
            first.Button(SDL_GAMEPAD_BUTTON_SOUTH, true);
            first.Button(SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
            first.Axis(SDL_GAMEPAD_AXIS_LEFTX, 16384);      // half right
            first.Axis(SDL_GAMEPAD_AXIS_LEFTY, 16384);      // half down
            first.Axis(SDL_GAMEPAD_AXIS_RIGHTX, 5000);      // inside the dead zone
            first.Axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
            break;
        case 2: {
            const GamePadState s = GamePad::GetState(0);
            CHECK(s.IsButtonDown(Buttons::A) && s.IsButtonDown(Buttons::DPadLeft) && s.IsButtonUp(Buttons::B));
            CHECK(GamePad::WasPressed(0, Buttons::A) && !GamePad::WasPressed(1, Buttons::A));
            CHECK(Near(s.leftStick.x, ExpectAxis(16384 / 32767.0f)));
            CHECK(Near(s.leftStick.y, ExpectAxis(-16384 / 32767.0f)));  // up is positive
            CHECK(s.rightStick.x == 0.0f);
            CHECK(Near(s.rightTrigger, 1.0f));
            const GamePadState raw = GamePad::GetState(0, GamePadDeadZone::None);
            CHECK(Near(raw.rightStick.x, 5000 / 32767.0f));
            // Circular: the vector keeps its direction and is rescaled by length.
            const GamePadState round = GamePad::GetState(0, GamePadDeadZone::Circular);
            const float len = std::sqrt(2.0f) * 16384 / 32767.0f;
            CHECK(Near(std::sqrt(round.leftStick.x * round.leftStick.x + round.leftStick.y * round.leftStick.y),
                       (len - 0.24f) / 0.76f));
            CHECK(Near(round.leftStick.x, -round.leftStick.y));

            CHECK(GamePad::SetVibration(0, 0.5f, 1.0f));
            CHECK(first.rumbleLow == 32768 && first.rumbleHigh == 65535);
            CHECK(!GamePad::SetVibration(2, 1.0f, 1.0f));
            first.Button(SDL_GAMEPAD_BUTTON_SOUTH, false);
            break;
        }
        case 3:
            CHECK(GamePad::WasReleased(0, Buttons::A) && !GamePad::WasPressed(0, Buttons::A));
            first.Detach();
            break;
        case 4:
            CHECK(!GamePad::GetState(0).isConnected && GamePad::GetState(1).isConnected);
            third.Attach("coldx test pad 3");
            break;
        case 5:
            // The freed player 0 goes to the next pad; player 1 keeps its pad.
            CHECK(GamePad::GetState(0).isConnected && GamePad::GetState(1).isConnected);
            third.Button(SDL_GAMEPAD_BUTTON_NORTH, true);
            break;
        case 6:
            CHECK(GamePad::GetState(0).IsButtonDown(Buttons::Y) && GamePad::GetState(1).IsButtonUp(Buttons::Y));
            second.Detach();
            third.Detach();
            return false;
        }
        return true;
    };
    CHECK(game.Run() == 0);
    CHECK(game.updates == 6);
}

// Name, type and face labels from the vendor and product IDs a pad reports.
void PadKinds() {
    Headless();
    Driver game;
    VirtualPad sony, microsoft, nintendo;
    game.initialize = [&] {
        sony.Attach("DualSense Wireless Controller", 0x054C, 0x0CE6);
        microsoft.Attach("Xbox Series X Controller", 0x045E, 0x0B12);
        nintendo.Attach("Pro Controller", 0x057E, 0x2009);
    };
    game.step = [&](int n) {
        if (n == 1) {
            const GamePadCapabilities ps = GamePad::GetCapabilities(0);
            const GamePadCapabilities xb = GamePad::GetCapabilities(1);
            const GamePadCapabilities sw = GamePad::GetCapabilities(2);
            std::printf("pads: \"%s\" type %d, \"%s\" type %d, \"%s\" type %d\n", ps.name.c_str(), static_cast<int>(ps.type),
                        xb.name.c_str(), static_cast<int>(xb.type), sw.name.c_str(), static_cast<int>(sw.type));
            CHECK(ps.isConnected && ps.type == GamePadType::PS5 && ps.vendorId == 0x054C && ps.productId == 0x0CE6);
            CHECK(xb.isConnected && xb.type == GamePadType::XboxOne);
            CHECK(sw.isConnected && sw.type == GamePadType::SwitchPro);
            CHECK(ps.hasRumble);  // the virtual pads have a rumble callback
            CHECK(!ps.name.empty());
            CHECK(!GamePad::GetCapabilities(3).isConnected && GamePad::GetCapabilities(3).name.empty());
            // The bottom face button, Buttons::A, as each pad prints it.
            CHECK(GamePad::FaceButtonLabel(0, Buttons::A) == ButtonLabel::Cross);
            CHECK(GamePad::FaceButtonLabel(0, Buttons::Y) == ButtonLabel::Triangle);
            CHECK(GamePad::FaceButtonLabel(1, Buttons::A) == ButtonLabel::A);
            CHECK(GamePad::FaceButtonLabel(2, Buttons::A) == ButtonLabel::B);
            CHECK(GamePad::FaceButtonLabel(2, Buttons::B) == ButtonLabel::A);
            CHECK(GamePad::FaceButtonLabel(1, Buttons::Start) == ButtonLabel::Unknown);
            CHECK(GamePad::FaceButtonLabel(3, Buttons::A) == ButtonLabel::Unknown);
            sony.Detach();
            microsoft.Detach();
            nintendo.Detach();
            return false;
        }
        return true;
    };
    CHECK(game.Run() == 0);
}

void Script() {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "coldx_input_test.txt";
    {
        std::ofstream file(path);
        file << "# a comment\n"
                "2 key down Left Shift\n"
                "2 mouse move 30 40\n"
                "3 mouse down middle\n"
                "3 wheel 1.5 -2\n"
                "4 key up Left Shift\n"
                "4 mouse up middle\n"
                "5 bogus line\n"
                "6 key down NotAKey\n"
                "7 exit\n";
    }
    Headless(path.string().c_str());
    Driver game;
    game.step = [](int n) {
        switch (n) {
        case 1: CHECK(Keyboard::GetState().IsKeyUp(Key::LeftShift)); break;
        case 2:
            CHECK(Keyboard::WasPressed(Key::LeftShift));
            CHECK(Mouse::GetState().Position() == Point(30, 40));
            break;
        case 3:
            CHECK(Mouse::WasPressed(MouseButton::Middle));
            CHECK(Near(Mouse::ScrollWheelDelta(), 1.5f) && Near(Mouse::HorizontalScrollWheelDelta(), -2.0f));
            break;
        case 4: CHECK(Keyboard::WasReleased(Key::LeftShift) && Mouse::WasReleased(MouseButton::Middle)); break;
        default: break;
        }
        return true;  // the script's exit ends the run
    };
    CHECK(game.Run() == 0);
    CHECK(game.updates == 6);  // exit applies before update 7 runs
    std::filesystem::remove(path);
    Headless();
}

void Storage_() {
    Headless();
    Driver game;
    game.Settings().company = "coldx-test";
    game.Settings().name = "storage";
    std::string folder;
    game.initialize = [&] {
        folder = Storage::Folder();
        CHECK(!folder.empty());
        CHECK(folder.find("coldx-test") != std::string::npos && folder.find("storage") != std::string::npos);
        CHECK(folder.back() == '/' || folder.back() == '\\');

        Storage::Remove("a.txt");
        CHECK(!Storage::Exists("a.txt"));
        CHECK(!Storage::ReadFile("a.txt").has_value());
        CHECK(Storage::WriteFile("a.txt", "first"));
        CHECK(Storage::ReadFile("a.txt") == std::optional<std::string>("first"));
        CHECK(Storage::WriteFile("a.txt", std::string("second\0bytes", 12)));
        CHECK(Storage::ReadFile("a.txt") == std::optional<std::string>(std::string("second\0bytes", 12)));
        CHECK(!Storage::Exists("a.txt.tmp"));  // the temporary was renamed away
        CHECK(Storage::Remove("a.txt") && !Storage::Exists("a.txt"));
        CHECK(!Storage::Remove("a.txt"));

        for (const char* bad : {"", ".", "..", "../escape.txt", "sub/file.txt", "sub\\file.txt", "C:file.txt"})
            CHECK(!Storage::WriteFile(bad, "x"));
    };
    game.step = [](int) { return false; };
    CHECK(game.Run() == 0);

    // Leave nothing behind in the user's app data.
    std::error_code ignored;
    const std::filesystem::path made(folder);
    std::filesystem::remove(made, ignored);
    std::filesystem::remove(made.parent_path().parent_path(), ignored);
    CHECK(!std::filesystem::exists(made.parent_path().parent_path()));
}

} // namespace

int main() {
    KeyboardEdges();
    MouseState_();
    GamePads();
    PadKinds();
    Script();
    Storage_();
    return CheckResult();
}
