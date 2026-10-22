#include <coldx/core/content_manager.h>
#include <coldx/core/music.h>
#include <coldx/core/sound_effect.h>
#include <coldx/core/texture2d.h>

#include <cstdio>

#include <SDL3/SDL.h>

#include "content_path.h"
#include "rime_package.h"

namespace coldx {

namespace {

bool IsFile(const std::string& path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

bool HasExtension(const std::string& name) {
    const size_t slash = name.find_last_of("/\\");
    const size_t dot = name.find_last_of('.');
    return dot != std::string::npos && (slash == std::string::npos || dot > slash);
}

bool ReadFile(const std::string& path, std::vector<std::uint8_t>& out) {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) {
        SDL_Log("coldx.core: could not read %s: %s", path.c_str(), SDL_GetError());
        return false;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    out.assign(bytes, bytes + size);
    SDL_free(data);
    return true;
}

} // namespace

ContentManager::ContentManager(GraphicsDevice& graphics) : graphics_(graphics) {
    RegisterDataLoader<Texture2D>({".png"}, [this](const std::vector<std::uint8_t>& bytes, const std::string& name) {
        return Texture2D::FromMemory(graphics_, bytes.data(), bytes.size(), name);
    });
    RegisterDataLoader<SoundEffect>({".wav", ".ogg"}, [](const std::vector<std::uint8_t>& bytes, const std::string& name) {
        return SoundEffect::FromMemory(bytes.data(), bytes.size(), name);
    });
    RegisterDataLoader<Music>({".ogg"}, [](const std::vector<std::uint8_t>& bytes, const std::string& name) {
        return Music::FromMemory(bytes, name);
    });
}

ContentManager::~ContentManager() = default;

void ContentManager::SetRootDirectory(std::string directory) {
    root_ = std::move(directory);
}

bool ContentManager::Mount(const std::string& path) {
    std::unique_ptr<detail::Package> package = detail::Package::Open(detail::ResolveContentPath(path));
    if (!package) return false;
    packages_.push_back(std::move(package));
    return true;
}

void ContentManager::MountDefault() {
    if (triedDefault_) return;
    triedDefault_ = true;
    const std::string path = detail::ResolveContentPath(root_ + ".rime");
    if (!IsFile(path)) return;
    // Mounted under everything the game mounted itself, so its patches still win.
    std::unique_ptr<detail::Package> package = detail::Package::Open(path);
    if (package) packages_.insert(packages_.begin(), std::move(package));
}

bool ContentManager::LooseAllowed() {
    switch (loose_) {
    case LooseFiles::Always: return true;
    case LooseFiles::Never: return false;
    case LooseFiles::Auto:
    default:
#ifdef NDEBUG
        return packages_.empty();
#else
        return true;
#endif
    }
}

std::vector<std::string> ContentManager::Candidates(const std::string& name, const std::vector<std::string>& extensions) const {
    std::vector<std::string> out;
    if (HasExtension(name)) out.push_back(name);
    for (const std::string& ext : extensions) out.push_back(name + ext);
    return out;
}

bool ContentManager::FindBytes(const std::vector<std::string>& candidates, std::vector<std::uint8_t>& bytes, std::string& found) {
    MountDefault();
    for (auto package = packages_.rbegin(); package != packages_.rend(); ++package) {
        for (const std::string& name : candidates) {
            if (const detail::Package::Entry* entry = (*package)->Find(name)) {
                found = name;
                return (*package)->Read(*entry, bytes);
            }
        }
    }
    std::string tried;
    for (const std::string& name : candidates) tried += (tried.empty() ? "" : ", ") + name;
    if (!packages_.empty()) tried += " in " + std::to_string(packages_.size()) + " package(s)";
    if (LooseAllowed()) {
        for (const std::string& name : candidates) {
            const std::string path = detail::ResolveContentPath(root_.empty() ? name : root_ + "/" + name);
            if (IsFile(path)) {
                found = path;
                return ReadFile(path, bytes);
            }
            tried += ", " + path;
        }
    } else {
        tried += " (loose files are off)";
    }
    SDL_Log("coldx.core: content not found; tried %s", tried.c_str());
    return false;
}

std::shared_ptr<void> ContentManager::Find(const Loader& loader, const std::string& name) {
    const std::vector<std::string> candidates = Candidates(name, loader.extensions);
    if (loader.fromBytes) {
        std::vector<std::uint8_t> bytes;
        std::string found;
        return FindBytes(candidates, bytes, found) ? loader.fromBytes(bytes, found) : nullptr;
    }
    // A path loader sees loose files only.
    MountDefault();
    std::string tried;
    if (LooseAllowed()) {
        for (const std::string& candidate : candidates) {
            const std::string path = detail::ResolveContentPath(root_.empty() ? candidate : root_ + "/" + candidate);
            if (IsFile(path)) return loader.fromPath(path);
            tried += (tried.empty() ? "" : ", ") + path;
        }
    }
    SDL_Log("coldx.core: content \"%s\" not found; tried %s (a path loader reads loose files only)", name.c_str(),
            tried.empty() ? "nothing, loose files are off" : tried.c_str());
    return nullptr;
}

std::optional<std::vector<std::uint8_t>> ContentManager::ReadBytes(const std::string& name) {
    std::vector<std::uint8_t> bytes;
    std::string found;
    if (!FindBytes({name}, bytes, found)) return std::nullopt;
    return bytes;
}

std::string ContentManager::Key(const std::type_info& type, const std::string& name) const {
    return std::string(type.name()) + '|' + name;
}

void ContentManager::NoLoader(const char* type, const std::string& name) const {
    SDL_Log("coldx.core: no content loader for %s (loading \"%s\"); register one with RegisterDataLoader", type,
            name.c_str());
}

bool ContentManager::IsLoaded(const std::type_info& type, const std::string& name) const {
    return cache_.count(Key(type, name)) != 0;
}

SpriteFont ContentManager::LoadFont(const std::string& name, float pixelHeight) {
    char size[32];
    std::snprintf(size, sizeof(size), "@%g", pixelHeight);
    const std::string key = Key(typeid(SpriteFont), name + size);
    if (auto found = cache_.find(key); found != cache_.end()) return *static_cast<const SpriteFont*>(found->second.get());
    std::vector<std::uint8_t> bytes;
    std::string found;
    if (!FindBytes(Candidates(name, {".ttf", ".otf"}), bytes, found)) return {};
    SpriteFont font = SpriteFont::FromMemory(graphics_, bytes.data(), bytes.size(), pixelHeight);
    if (font.IsEmpty()) return font;
    cache_[key] = std::make_shared<SpriteFont>(font);
    return font;
}

void ContentManager::Unload() {
    cache_.clear();
}

} // namespace coldx
