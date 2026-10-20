#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace coldx {

// Files the game keeps for the player: settings, saves. They live in a per-user folder the
// game can always write to (on Windows, %APPDATA%\<company>\<name>), named by
// GameSettings::company and GameSettings::name. Usable from Initialize on.
//
// Names are plain file names, no folders: "settings.txt", "save1.dat".
class Storage {
public:
    // The folder, ending in a separator, created if needed. Empty on failure.
    static std::string Folder();

    // Replaces the whole file. Written to a temporary file and renamed over the old one,
    // so a crash mid-write leaves the previous contents, never half of the new.
    static bool WriteFile(std::string_view name, std::string_view contents);

    // The whole file, or nullopt if it does not exist or cannot be read.
    static std::optional<std::string> ReadFile(std::string_view name);

    static bool Exists(std::string_view name);
    // Not called DeleteFile: that is a macro once <windows.h> is included.
    static bool Remove(std::string_view name);
};

} // namespace coldx
