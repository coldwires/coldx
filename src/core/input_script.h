#pragma once

#include <string>
#include <vector>

namespace coldx::detail {

struct Input;

// Scripted input from the file named by COLDX_INPUT, for running a game with nobody at the
// keyboard. One command per line, applied just before the numbered Update (1 is the first):
//
//     # comment
//     10 key down Up          SDL key names: Up, Escape, A, Space, Left Shift, ...
//     11 key up Up
//     20 mouse move 100 200   back buffer pixels
//     21 mouse down left      left, right, middle, x1, x2
//     22 mouse up left
//     30 wheel 1              notches up; optional second number is horizontal
//     40 exit
class InputScript {
public:
    // Reads path; bad lines are logged with their number and skipped. An empty path is no
    // script.
    void Load(const std::string& path);
    bool Empty() const { return commands_.empty(); }

    // Applies the commands for this update to input's live state. True if one was "exit".
    bool Apply(int update, Input& input) const;

private:
    struct Command {
        int update;
        std::vector<std::string> words;
    };
    std::vector<Command> commands_;
};

} // namespace coldx::detail
