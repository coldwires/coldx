#pragma once

#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <coldx/core/sprite_font.h>

namespace coldx {

class GraphicsDevice;

// Loads content by name, caches it, and hands out copies. Built in: Texture2D (.png),
// SoundEffect (.wav, .ogg), Music (.ogg), and fonts through LoadFont. Games add their own
// types with RegisterLoader.
//
// Names are relative to RootDirectory and may leave out the extension:
// Load<Texture2D>("sprites/player") finds content/sprites/player.png. Loaded types must be
// copyable; coldx's are handles, so a copy shares the loaded data.
//
// Loads loose files. Packed content (.rime) is not written yet.
class ContentManager {
public:
    explicit ContentManager(GraphicsDevice& graphics);
    ~ContentManager();
    ContentManager(const ContentManager&) = delete;
    ContentManager& operator=(const ContentManager&) = delete;

    // "content" by default. Relative to the executable's folder unless absolute.
    void SetRootDirectory(std::string directory);
    const std::string& RootDirectory() const { return root_; }

    // The cached T for name, loading it first if needed. A missing file logs what was
    // tried and returns an empty T, uncached, so a later Load tries again. A file that exists
    // but does not decode is cached as whatever its loader returned (an empty T).
    template <class T>
    T Load(const std::string& name) {
        const auto key = Key(typeid(T), name);
        if (auto found = cache_.find(key); found != cache_.end()) return *static_cast<const T*>(found->second.get());
        auto loader = loaders_.find(std::type_index(typeid(T)));
        if (loader == loaders_.end()) {
            NoLoader(typeid(T).name(), name);
            return T{};
        }
        std::shared_ptr<void> loaded = loader->second.load(name);
        if (!loaded) return T{};
        cache_[key] = loaded;
        return *static_cast<const T*>(loaded.get());
    }

    // A font baked at pixelHeight from name (.ttf or .otf). Each size is cached separately.
    SpriteFont LoadFont(const std::string& name, float pixelHeight);

    // Teaches Load<T> a new type. load receives the full path of the first file found with
    // one of extensions (or the name as given, if it has an extension) and returns the T; it
    // should return an empty T and log on failure. Replaces any loader for T.
    template <class T>
    void RegisterLoader(std::vector<std::string> extensions, std::function<T(const std::string& path)> load) {
        Register(typeid(T), std::move(extensions), [this, load = std::move(load)](const std::string& path) {
            return std::static_pointer_cast<void>(std::make_shared<T>(load(path)));
        });
    }

    bool IsLoaded(const std::type_info& type, const std::string& name) const;

    // Drops everything cached. Copies the game still holds stay valid.
    void Unload();

private:
    struct Loader {
        std::vector<std::string> extensions;
        std::function<std::shared_ptr<void>(const std::string& name)> load;
    };

    void Register(const std::type_info& type, std::vector<std::string> extensions,
                  std::function<std::shared_ptr<void>(const std::string& path)> fromPath);
    std::string Key(const std::type_info& type, const std::string& name) const;
    void NoLoader(const char* type, const std::string& name) const;

    GraphicsDevice& graphics_;
    std::string root_ = "content";
    std::unordered_map<std::type_index, Loader> loaders_;
    std::unordered_map<std::string, std::shared_ptr<void>> cache_;
};

} // namespace coldx
