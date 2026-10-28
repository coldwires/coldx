#pragma once

#include <string>

#include <coldx/core/rectangle.h>

namespace coldx {

// Typed text, for name entry, chat and consoles: characters as the keyboard layout and any
// input method (IME) produce them, accents and other scripts included. Keyboard is still
// the way to read Backspace, Enter, arrows and shortcuts. They never appear here.
//
// Off until Start. While on, platforms with an on-screen keyboard or an IME may show it.
// Use from Initialize on.
class TextInput {
public:
    static void Start();
    static void Stop();
    static bool IsActive();

    // UTF-8 text typed since the previous Update. Empty when nothing was typed, or when
    // text input is off.
    static const std::string& GetText();

    // With an IME: the text being composed and not yet committed, UTF-8, to draw at the
    // caret, and where in it the IME's cursor is, in bytes. Empty without an IME.
    static const std::string& Composition();
    static int CompositionCursor();

    // Where the caret is, in back buffer pixels, so an IME can place its candidate list
    // beside it rather than over it.
    static void SetCaretArea(const Rectangle& area);
};

// The system clipboard's text, UTF-8.
class Clipboard {
public:
    static bool HasText();
    // Empty if there is none.
    static std::string GetText();
    static void SetText(const std::string& text);
};

} // namespace coldx
