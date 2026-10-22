#include <coldx/content/processor.h>

#include <cstring>

#include <SDL3/SDL.h>

// The built-in processors check that a source is what its section says, so a broken file
// fails the build rather than the game, and store it unchanged (docs/DECISIONS/0012).
namespace coldx::content {

namespace {

bool StartsWith(const std::vector<std::uint8_t>& bytes, const char* magic, size_t at = 0) {
    const size_t n = std::strlen(magic);
    return bytes.size() >= at + n && std::memcmp(bytes.data() + at, magic, n) == 0;
}

class Stored : public Processor {
public:
    bool Process(const Source& source, const Settings&, std::vector<Output>& outputs, std::string& error) const override {
        if (!Check(source.bytes, error)) return false;
        outputs.push_back({source.name, Type(), source.bytes});
        return true;
    }

protected:
    virtual rime::AssetType Type() const = 0;
    virtual bool Check(const std::vector<std::uint8_t>& bytes, std::string& error) const = 0;
};

class TextureProcessor : public Stored {
public:
    std::string Name() const override { return "texture"; }

protected:
    rime::AssetType Type() const override { return rime::AssetType::Texture; }
    bool Check(const std::vector<std::uint8_t>& bytes, std::string& error) const override {
        if (!StartsWith(bytes, "\x89PNG\r\n\x1a\n")) {
            error = "not a PNG";
            return false;
        }
        // Decoded once here, so a damaged PNG fails the build.
        SDL_IOStream* io = SDL_IOFromConstMem(bytes.data(), bytes.size());
        SDL_Surface* image = io ? SDL_LoadPNG_IO(io, true) : nullptr;
        if (!image) {
            error = std::string("PNG does not decode: ") + SDL_GetError();
            return false;
        }
        const bool tooBig = image->w > 16384 || image->h > 16384;
        if (tooBig) error = "larger than 16384 pixels on a side, more than GPUs take";
        SDL_DestroySurface(image);
        return !tooBig;
    }
};

class SoundProcessor : public Stored {
public:
    std::string Name() const override { return "sound"; }

protected:
    rime::AssetType Type() const override { return rime::AssetType::Sound; }
    bool Check(const std::vector<std::uint8_t>& bytes, std::string& error) const override {
        if ((StartsWith(bytes, "RIFF") && StartsWith(bytes, "WAVE", 8)) || StartsWith(bytes, "OggS")) return true;
        error = "neither WAV nor OGG";
        return false;
    }
};

class MusicProcessor : public Stored {
public:
    std::string Name() const override { return "music"; }

protected:
    rime::AssetType Type() const override { return rime::AssetType::Music; }
    bool Check(const std::vector<std::uint8_t>& bytes, std::string& error) const override {
        if (StartsWith(bytes, "OggS")) return true;
        error = "not OGG (music streams OGG Vorbis only)";
        return false;
    }
};

class FontProcessor : public Stored {
public:
    std::string Name() const override { return "font"; }

protected:
    rime::AssetType Type() const override { return rime::AssetType::Font; }
    bool Check(const std::vector<std::uint8_t>& bytes, std::string& error) const override {
        static const std::uint8_t trueType[4] = {0, 1, 0, 0};  // has a zero byte, so not a C string
        if (bytes.size() >= 4 && std::memcmp(bytes.data(), trueType, 4) == 0) return true;
        if (StartsWith(bytes, "OTTO") || StartsWith(bytes, "true") || StartsWith(bytes, "ttcf")) return true;
        error = "not a TrueType or OpenType font";
        return false;
    }
};

class DataProcessor : public Stored {
public:
    std::string Name() const override { return "data"; }

protected:
    rime::AssetType Type() const override { return rime::AssetType::Data; }
    bool Check(const std::vector<std::uint8_t>&, std::string&) const override { return true; }
};

} // namespace

std::vector<std::unique_ptr<Processor>> BuiltInProcessors() {
    std::vector<std::unique_ptr<Processor>> list;
    list.push_back(std::make_unique<TextureProcessor>());
    list.push_back(std::make_unique<SoundProcessor>());
    list.push_back(std::make_unique<MusicProcessor>());
    list.push_back(std::make_unique<FontProcessor>());
    list.push_back(std::make_unique<DataProcessor>());
    return list;
}

} // namespace coldx::content
