#include <coldx/core/storage.h>

#include <SDL3/SDL.h>

#include "storage_names.h"

namespace coldx {

namespace {

std::string company;
std::string name;
bool named = false;

// Plain file names only: nothing that could reach outside the folder.
bool ValidName(std::string_view file) {
    if (file.empty() || file == "." || file == "..") return false;
    for (char c : file) {
        if (c == '/' || c == '\\' || c == ':') return false;
    }
    return true;
}

std::string PathOf(std::string_view file) {
    if (!ValidName(file)) {
        SDL_Log("coldx.core: Storage: \"%.*s\" is not a plain file name", static_cast<int>(file.size()), file.data());
        return {};
    }
    const std::string folder = Storage::Folder();
    return folder.empty() ? std::string() : folder + std::string(file);
}

} // namespace

namespace detail {

void SetStorageNames(const std::string& companyName, const std::string& gameName) {
    company = companyName;
    name = gameName;
    named = true;
}

} // namespace detail

std::string Storage::Folder() {
    if (!named) {
        SDL_Log("coldx.core: Storage used before Run started; the folder is not known yet");
        return {};
    }
    char* path = SDL_GetPrefPath(company.c_str(), name.c_str());
    if (!path) {
        SDL_Log("coldx.core: Storage: SDL_GetPrefPath failed: %s", SDL_GetError());
        return {};
    }
    std::string result = path;
    SDL_free(path);
    return result;
}

bool Storage::WriteFile(std::string_view file, std::string_view contents) {
    const std::string path = PathOf(file);
    if (path.empty()) return false;
    const std::string temporary = path + ".tmp";
    if (!SDL_SaveFile(temporary.c_str(), contents.data(), contents.size())) {
        SDL_Log("coldx.core: Storage: could not write %s: %s", temporary.c_str(), SDL_GetError());
        return false;
    }
    if (!SDL_RenamePath(temporary.c_str(), path.c_str())) {
        SDL_Log("coldx.core: Storage: could not replace %s: %s", path.c_str(), SDL_GetError());
        SDL_RemovePath(temporary.c_str());
        return false;
    }
    return true;
}

std::optional<std::string> Storage::ReadFile(std::string_view file) {
    const std::string path = PathOf(file);
    if (path.empty() || !Exists(file)) return std::nullopt;
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) {
        SDL_Log("coldx.core: Storage: could not read %s: %s", path.c_str(), SDL_GetError());
        return std::nullopt;
    }
    std::string contents(static_cast<const char*>(data), size);
    SDL_free(data);
    return contents;
}

bool Storage::Exists(std::string_view file) {
    const std::string path = PathOf(file);
    SDL_PathInfo info;
    return !path.empty() && SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

bool Storage::Remove(std::string_view file) {
    const std::string path = PathOf(file);
    return !path.empty() && Exists(file) && SDL_RemovePath(path.c_str());
}

} // namespace coldx
