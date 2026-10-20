#include "input_script.h"

#include <cstdlib>
#include <sstream>

#include <SDL3/SDL.h>

#include "input.h"

namespace coldx::detail {

namespace {

bool MouseButtonByName(const std::string& name, MouseState& m, bool down) {
    if (name == "left") m.left = down;
    else if (name == "right") m.right = down;
    else if (name == "middle") m.middle = down;
    else if (name == "x1") m.x1 = down;
    else if (name == "x2") m.x2 = down;
    else return false;
    return true;
}

// Key names may contain spaces ("Left Shift"), so the name is everything after the verb.
std::string Rest(const std::vector<std::string>& words, size_t from) {
    std::string out;
    for (size_t i = from; i < words.size(); ++i) {
        if (!out.empty()) out += ' ';
        out += words[i];
    }
    return out;
}

// Checks a command's shape. Returns an error message, or empty if it is fine.
std::string Validate(const std::vector<std::string>& w) {
    if (w.empty()) return "nothing after the update number";
    if (w[0] == "exit") return w.size() == 1 ? "" : "exit takes nothing";
    if (w[0] == "key") {
        if (w.size() < 3 || (w[1] != "down" && w[1] != "up")) return "expected: key down|up <name>";
        if (SDL_GetScancodeFromName(Rest(w, 2).c_str()) == SDL_SCANCODE_UNKNOWN) return "unknown key \"" + Rest(w, 2) + "\"";
        return "";
    }
    if (w[0] == "mouse") {
        if (w.size() == 4 && w[1] == "move") return "";
        MouseState probe;
        if (w.size() == 3 && (w[1] == "down" || w[1] == "up") && MouseButtonByName(w[2], probe, true)) return "";
        return "expected: mouse move <x> <y>, or mouse down|up left|right|middle|x1|x2";
    }
    if (w[0] == "wheel") return (w.size() == 2 || w.size() == 3) ? "" : "expected: wheel <dy> [<dx>]";
    return "unknown command \"" + w[0] + "\"";
}

} // namespace

void InputScript::Load(const std::string& path) {
    commands_.clear();
    if (path.empty()) return;
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) {
        SDL_Log("coldx.core: COLDX_INPUT: could not read %s: %s", path.c_str(), SDL_GetError());
        return;
    }
    std::istringstream lines(std::string(static_cast<const char*>(data), size));
    SDL_free(data);

    std::string line;
    for (int number = 1; std::getline(lines, line); ++number) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream words(line);
        int update = 0;
        if (!(words >> update)) {
            if (line.find_first_not_of(" \t\r") != std::string::npos)
                SDL_Log("coldx.core: COLDX_INPUT line %d: expected an update number first", number);
            continue;
        }
        Command command{update, {}};
        for (std::string word; words >> word;) command.words.push_back(word);
        const std::string error = Validate(command.words);
        if (!error.empty()) {
            SDL_Log("coldx.core: COLDX_INPUT line %d: %s", number, error.c_str());
            continue;
        }
        commands_.push_back(std::move(command));
    }
    SDL_Log("coldx.core: COLDX_INPUT: %zu commands from %s", commands_.size(), path.c_str());
}

bool InputScript::Apply(int update, Input& input) const {
    bool exit = false;
    for (const Command& c : commands_) {
        if (c.update != update) continue;
        const std::vector<std::string>& w = c.words;
        if (w[0] == "exit") {
            exit = true;
        } else if (w[0] == "key") {
            const SDL_Scancode code = SDL_GetScancodeFromName(Rest(w, 2).c_str());
            input.liveKeys.SetKey(static_cast<Key>(code), w[1] == "down");
        } else if (w[0] == "mouse" && w[1] == "move") {
            input.liveMouse.x = std::atoi(w[2].c_str());
            input.liveMouse.y = std::atoi(w[3].c_str());
        } else if (w[0] == "mouse") {
            MouseButtonByName(w[2], input.liveMouse, w[1] == "down");
        } else if (w[0] == "wheel") {
            input.liveMouse.scrollWheel += static_cast<float>(std::atof(w[1].c_str()));
            if (w.size() == 3) input.liveMouse.horizontalScrollWheel += static_cast<float>(std::atof(w[2].c_str()));
        }
    }
    return exit;
}

} // namespace coldx::detail
