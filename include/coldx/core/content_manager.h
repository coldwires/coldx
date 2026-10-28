#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <coldx/core/rime.h>
#include <coldx/core/sprite_font.h>

namespace coldx {

class GraphicsDevice;

namespace detail {
class Package;

// A number per type, without RTTI: the address of a variable each T has its own copy of.
// Not const, so the linker never folds two of them into one.
template <class T>
const void* TypeKey() {
    static char key;
    return &key;
}
}

// When ContentManager reads loose files from RootDirectory.
enum class LooseFiles {
    // Debug builds always. Release builds only while no package is mounted, so a game
    // that ships a package is not changed by files dropped beside it.
    Auto,
    Always,
    Never,
};

// Loads content by name, caches it, and hands out copies. Built in: Texture2D (.png),
// SoundEffect (.wav, .ogg), Music (.ogg), Effect (.fx, compiled by coldxc's [effect]), and
// fonts through LoadFont. Games add their own
// types with RegisterDataLoader (packed and loose content) or RegisterLoader (loose files).
//
// Names are relative to RootDirectory and may leave out the extension:
// Load<Texture2D>("sprites/player") finds sprites/player.png. Mounted .rime packages are
// searched first, newest mount first, then loose files (see LooseFiles). The package
// "<RootDirectory>.rime" beside the executable, content.rime by default, is mounted the
// first time anything loads. Loaded types must be copyable. coldx's are handles, so a copy
// shares the loaded data.
class ContentManager {
public:
    explicit ContentManager(GraphicsDevice& graphics);
    ~ContentManager();
    ContentManager(const ContentManager&) = delete;
    ContentManager& operator=(const ContentManager&) = delete;

    // "content" by default. Relative to the executable's folder unless absolute.
    void SetRootDirectory(std::string directory);
    const std::string& RootDirectory() const { return root_; }

    // Adds a .rime package, searched before everything mounted earlier. Patches and DLC
    // mount after the base game. Relative to the executable's folder unless absolute.
    // False, logged, if it cannot be read.
    bool Mount(const std::string& path);
    void SetLooseFiles(LooseFiles mode) { loose_ = mode; }

    // The key encrypted packages were built with (coldxc --key-file). coldx_add_content's
    // KEY_FILE sets it for every ContentManager through SetDefaultKey, from a source file it
    // generates into the game, so most games never call these.
    void SetKey(const rime::Key& key) {
        key_ = key;
        hasKey_ = true;
    }
    static void SetDefaultKey(const rime::Key& key);

    // The cached T for name, loading it first if needed. Missing content logs what was
    // tried and returns an empty T, uncached, so a later Load tries again. Content that is
    // found but does not decode is cached as whatever its loader returned (an empty T).
    template <class T>
    T Load(const std::string& name) {
        const auto key = Key(detail::TypeKey<T>(), name);
        if (auto found = cache_.find(key); found != cache_.end()) return *static_cast<const T*>(found->second.get());
        auto loader = loaders_.find(detail::TypeKey<T>());
        if (loader == loaders_.end()) {
            NoLoader(name);
            return T{};
        }
        std::shared_ptr<void> loaded = Find(loader->second, name);
        if (!loaded) return T{};
        cache_[key] = loaded;
        return *static_cast<const T*>(loaded.get());
    }

    // A font baked at pixelHeight from name (.ttf or .otf). Each size and smoothing is cached
    // separately.
    SpriteFont LoadFont(const std::string& name, float pixelHeight, FontSmoothing smoothing = FontSmoothing::Smooth);

    // The bytes of name exactly as stored, extension included: for a game's own files
    // (levels, dialogue, settings) without writing a loader. Not cached. Empty, logged,
    // when missing.
    std::optional<std::vector<std::uint8_t>> ReadBytes(const std::string& name);

    // Teaches Load<T> a new type from bytes, which works for packed and loose content:
    // load receives the bytes of the first entry or file found with one of extensions (or
    // the name as given, if it has one) and the name it was found under. It should return
    // an empty T and log on failure. Replaces any loader for T.
    template <class T>
    void RegisterDataLoader(std::vector<std::string> extensions,
                            std::function<T(const std::vector<std::uint8_t>& bytes, const std::string& name)> load) {
        Loader loader;
        loader.extensions = std::move(extensions);
        loader.fromBytes = [load = std::move(load)](const std::vector<std::uint8_t>& bytes, const std::string& name) {
            return std::static_pointer_cast<void>(std::make_shared<T>(load(bytes, name)));
        };
        loaders_[detail::TypeKey<T>()] = std::move(loader);
    }

    // Teaches Load<T> a new type from a file path. Loose files only: packed content has no
    // path, so prefer RegisterDataLoader. Replaces any loader for T.
    template <class T>
    void RegisterLoader(std::vector<std::string> extensions, std::function<T(const std::string& path)> load) {
        Loader loader;
        loader.extensions = std::move(extensions);
        loader.fromPath = [load = std::move(load)](const std::string& path) {
            return std::static_pointer_cast<void>(std::make_shared<T>(load(path)));
        };
        loaders_[detail::TypeKey<T>()] = std::move(loader);
    }

    template <class T>
    bool IsLoaded(const std::string& name) const {
        return cache_.count(Key(detail::TypeKey<T>(), name)) != 0;
    }

    // Drops everything cached. Copies the game still holds stay valid. Packages stay mounted.
    void Unload();

private:
    struct Loader {
        std::vector<std::string> extensions;
        std::function<std::shared_ptr<void>(const std::vector<std::uint8_t>& bytes, const std::string& name)> fromBytes;
        std::function<std::shared_ptr<void>(const std::string& path)> fromPath;
    };

    std::shared_ptr<void> Find(const Loader& loader, const std::string& name);
    // The candidates for name: as given if it has an extension, then name plus each one.
    std::vector<std::string> Candidates(const std::string& name, const std::vector<std::string>& extensions) const;
    // Bytes of the first candidate in a package, or a loose file if allowed. found is the
    // name it was found under. False if none, with what was tried logged.
    bool FindBytes(const std::vector<std::string>& candidates, std::vector<std::uint8_t>& bytes, std::string& found);
    bool LooseAllowed();
    void MountDefault();
    std::string Key(const void* type, const std::string& name) const;
    void NoLoader(const std::string& name) const;

    GraphicsDevice& graphics_;
    std::string root_ = "content";
    LooseFiles loose_ = LooseFiles::Auto;
    bool triedDefault_ = false;
    rime::Key key_{};
    bool hasKey_ = false;
    std::vector<std::unique_ptr<detail::Package>> packages_;  // newest last
    std::unordered_map<const void*, Loader> loaders_;
    std::unordered_map<std::string, std::shared_ptr<void>> cache_;
};

} // namespace coldx
