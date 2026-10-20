#include <coldx/core/content_manager.h>
#include <coldx/core/music.h>
#include <coldx/core/sound_effect.h>
#include <coldx/core/texture2d.h>

#include <cstdio>

#include <SDL3/SDL.h>

#include "content_path.h"

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

// The first existing file for name: as given if it has an extension, else name plus each
// extension in turn. Empty if none exists, with what was tried logged.
std::string FindFile(const std::string& root, const std::string& name, const std::vector<std::string>& extensions) {
    const std::string base = detail::ResolveContentPath(root.empty() ? name : root + "/" + name);
    std::vector<std::string> candidates;
    if (HasExtension(name)) candidates.push_back(base);
    for (const std::string& ext : extensions) candidates.push_back(base + ext);
    for (const std::string& path : candidates)
        if (IsFile(path)) return path;
    std::string tried;
    for (const std::string& path : candidates) tried += (tried.empty() ? "" : ", ") + path;
    SDL_Log("coldx.core: content \"%s\" not found; tried %s", name.c_str(), tried.c_str());
    return {};
}

} // namespace

ContentManager::ContentManager(GraphicsDevice& graphics) : graphics_(graphics) {
    RegisterLoader<Texture2D>({".png"}, [this](const std::string& path) { return Texture2D::FromFile(graphics_, path); });
    RegisterLoader<SoundEffect>({".wav", ".ogg"}, [](const std::string& path) { return SoundEffect::FromFile(path); });
    RegisterLoader<Music>({".ogg"}, [](const std::string& path) { return Music::FromFile(path); });
}

ContentManager::~ContentManager() = default;

void ContentManager::SetRootDirectory(std::string directory) {
    root_ = std::move(directory);
}

void ContentManager::Register(const std::type_info& type, std::vector<std::string> extensions,
                              std::function<std::shared_ptr<void>(const std::string& path)> fromPath) {
    Loader loader;
    loader.extensions = extensions;
    loader.load = [this, extensions, fromPath = std::move(fromPath)](const std::string& name) -> std::shared_ptr<void> {
        const std::string path = FindFile(root_, name, extensions);
        return path.empty() ? nullptr : fromPath(path);
    };
    loaders_[std::type_index(type)] = std::move(loader);
}

std::string ContentManager::Key(const std::type_info& type, const std::string& name) const {
    return std::string(type.name()) + '|' + name;
}

void ContentManager::NoLoader(const char* type, const std::string& name) const {
    SDL_Log("coldx.core: no content loader for %s (loading \"%s\"); register one with RegisterLoader", type,
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
    const std::string path = FindFile(root_, name, {".ttf", ".otf"});
    if (path.empty()) return {};
    SpriteFont font = SpriteFont::FromFile(graphics_, path, pixelHeight);
    if (font.IsEmpty()) return font;
    cache_[key] = std::make_shared<SpriteFont>(font);
    return font;
}

void ContentManager::Unload() {
    cache_.clear();
}

} // namespace coldx
