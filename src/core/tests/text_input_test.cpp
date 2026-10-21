// Text input, the clipboard and key names. Typed text arrives as SDL text events, pushed
// here the way the platform would send them; the input script's text command takes the
// same path. Runs headless with a variable timestep: an event pushed in Update N is seen
// by Update N + 1.

#include <coldx/coldx.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include <SDL3/SDL.h>

#include "check.h"

using namespace coldx;
using coldx_test::CheckResult;

namespace {

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
        if (!step(++updates) || updates > 100) Exit();
    }
};

void PushText(const char* text) {
    SDL_Event e{};
    e.type = SDL_EVENT_TEXT_INPUT;
    e.text.text = text;  // string literals: alive until the event is read
    SDL_PushEvent(&e);
}

void PushComposition(const char* text, int cursor) {
    SDL_Event e{};
    e.type = SDL_EVENT_TEXT_EDITING;
    e.edit.text = text;
    e.edit.start = cursor;
    SDL_PushEvent(&e);
}

void Typing() {
    // UTF-8: e acute, then two CJK characters, then hiragana.
    static const char* accented = "h\xC3\xA9llo ";
    static const char* cjk = "\xE4\xB8\x96\xE7\x95\x8C";
    static const char* kana = "\xE3\x81\x8B\xE3\x81\xAA";
    Driver game;
    int phase = 0;
    game.step = [&](int u) {
        switch (u) {
        case 1:
            CHECK(!TextInput::IsActive());
            PushText("ignored");  // text input is off
            break;
        case 2:
            CHECK(TextInput::GetText().empty());
            TextInput::Start();
            CHECK(TextInput::IsActive());
            PushText(accented);
            PushText(cjk);
            break;
        case 3:
            CHECK(TextInput::GetText() == std::string(accented) + cjk);
            PushComposition(kana, 3);
            break;
        case 4:
            CHECK(TextInput::GetText().empty());  // only what arrived since the last Update
            CHECK(TextInput::Composition() == kana && TextInput::CompositionCursor() == 3);
            TextInput::SetCaretArea(Rectangle(10, 10, 2, 16));
            PushComposition("", 0);  // committed or cancelled
            break;
        case 5:
            CHECK(TextInput::Composition().empty());
            TextInput::Stop();
            CHECK(!TextInput::IsActive());
            PushText("ignored too");
            break;
        case 6:
            CHECK(TextInput::GetText().empty());
            phase = 1;
            return false;
        }
        return true;
    };
    CHECK(game.Run() == 0);
    CHECK(phase == 1);
    CHECK(!TextInput::IsActive());  // Run's end turns it off
}

void Script() {
    const std::string path = (std::filesystem::temp_directory_path() / "coldx_text_script.txt").string();
    {
        std::ofstream file(path);
        file << "2 text hello   world\n3 text bad\n";  // runs of spaces become one
    }
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_INPUT", path.c_str(), true);
    Driver game;
    game.initialize = [] { TextInput::Start(); };
    std::string at2, at3;
    game.step = [&](int u) {
        if (u == 2) {
            at2 = TextInput::GetText();
            TextInput::Stop();
        }
        if (u == 3) at3 = TextInput::GetText();
        return u < 3;
    };
    CHECK(game.Run() == 0);
    SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_INPUT");
    std::filesystem::remove(path);
    CHECK(at2 == "hello world");
    CHECK(at3.empty());  // typed while text input was off
}

void ClipboardAndKeys() {
    Driver game;
    game.step = [&](int) {
        // The real clipboard: put back what was there.
        const bool had = Clipboard::HasText();
        const std::string saved = Clipboard::GetText();
        Clipboard::SetText("coldx \xC3\xA9 clipboard");
        CHECK(Clipboard::HasText() && Clipboard::GetText() == "coldx \xC3\xA9 clipboard");
        Clipboard::SetText(had ? saved : "");

        // Layout-independent: whichever key types 'a' is named "A".
        const Key a = Keyboard::KeyForCharacter('a');
        CHECK(a != Key::Unknown);
        CHECK(Keyboard::KeyName(a) == "A");
        CHECK(Keyboard::KeyForCharacter('A') == a);
        CHECK(Keyboard::KeyName(Key::LeftShift) == "Left Shift");
        CHECK(Keyboard::KeyName(Key::F5) == "F5");
        CHECK(Keyboard::KeyName(Key::Space) == "Space");
        CHECK(Keyboard::KeyName(Key::Unknown).empty());
        std::printf("this layout: Key::Q is \"%s\", 'z' is on key %d\n", Keyboard::KeyName(Key::Q).c_str(),
                    static_cast<int>(Keyboard::KeyForCharacter('z')));
        return false;
    };
    CHECK(game.Run() == 0);
}

} // namespace

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);
    Typing();
    Script();
    ClipboardAndKeys();
    return CheckResult();
}
