#pragma once

#include <coldx/content/processor.h>

#include <string>
#include <string_view>
#include <vector>

namespace coldx::content::detail {

// A content project file:
//
//     # comments run to the end of the line
//     seed = my game              # seeds the package's name hashes; optional
//     [texture] sprites/*.png ui/**/*.png
//     [font] fonts/*.ttf
//     [maps] levels/*.tmx         # a game's own processor
//     scale = 2                   # a setting for the section above
//
// A section names a processor and the files it takes, by patterns relative to the
// project file: * matches within a folder name, ? one character, ** any folders.
struct Section {
    std::string processor;
    std::vector<std::string> patterns;
    Settings settings;
    int line = 0;
};

struct Project {
    std::string seed = "coldx";
    bool seedSet = false;
    std::vector<Section> sections;
};

// False with errors (each naming its line) if the text is not a valid project.
bool ParseProject(std::string_view text, Project& project, std::vector<std::string>& errors);

// Whether path (forward slashes) matches pattern, ignoring case.
bool GlobMatch(std::string_view pattern, std::string_view path);

// Every file under directory, as relative forward-slash paths, sorted.
std::vector<std::string> ListFiles(const std::string& directory);

} // namespace coldx::content::detail
