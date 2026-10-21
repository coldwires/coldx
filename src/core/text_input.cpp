#include <coldx/core/text_input.h>

#include <algorithm>

#include <SDL3/SDL.h>

#include "input.h"

namespace coldx {

void TextInput::Start() {
    detail::Input& in = detail::Input::Get();
    if (!in.window) {
        SDL_Log("coldx.core: TextInput::Start before Run; call it from Initialize on");
        return;
    }
    if (!SDL_StartTextInput(in.window)) SDL_Log("coldx.core: could not start text input: %s", SDL_GetError());
    in.textActive = true;
}

void TextInput::Stop() {
    detail::Input& in = detail::Input::Get();
    if (in.window) SDL_StopTextInput(in.window);
    in.textActive = false;
    in.liveText.clear();
    in.composition.clear();
    in.compositionCursor = 0;
}

bool TextInput::IsActive() {
    return detail::Input::Get().textActive;
}

const std::string& TextInput::GetText() {
    return detail::Input::Get().currentText;
}

const std::string& TextInput::Composition() {
    return detail::Input::Get().composition;
}

int TextInput::CompositionCursor() {
    return detail::Input::Get().compositionCursor;
}

void TextInput::SetCaretArea(const Rectangle& area) {
    const detail::Input& in = detail::Input::Get();
    if (!in.window) return;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    in.BackBufferToWindow(area.x, area.y, x0, y0);
    in.BackBufferToWindow(area.Right(), area.Bottom(), x1, y1);
    const SDL_Rect rect{static_cast<int>(x0), static_cast<int>(y0), std::max(1, static_cast<int>(x1 - x0)),
                        std::max(1, static_cast<int>(y1 - y0))};
    SDL_SetTextInputArea(in.window, &rect, 0);
}

bool Clipboard::HasText() {
    return SDL_HasClipboardText();
}

std::string Clipboard::GetText() {
    char* text = SDL_GetClipboardText();
    std::string result = text ? text : "";
    SDL_free(text);
    return result;
}

void Clipboard::SetText(const std::string& text) {
    if (!SDL_SetClipboardText(text.c_str())) SDL_Log("coldx.core: could not set the clipboard: %s", SDL_GetError());
}

} // namespace coldx
