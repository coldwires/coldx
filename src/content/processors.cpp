#include <coldx/content/processor.h>

#include <cstring>

#include <SDL3/SDL.h>

#include "effect_compiler.h"
#include "watermark.h"

// The built-in processors check that a source is what its section says, so a broken file
// fails the build rather than the game, and store it unchanged.
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
        outputs.push_back({source.name, Type(), source.bytes, Compressible()});
        return true;
    }

protected:
    // Formats that are compressed already never shrink. Not trying saves build time.
    virtual bool Compressible() const { return true; }
    virtual rime::AssetType Type() const = 0;
    virtual bool Check(const std::vector<std::uint8_t>& bytes, std::string& error) const = 0;
};

class TextureProcessor : public Stored {
public:
    std::string Name() const override { return "texture"; }
    std::vector<std::string> SettingKeys() const override { return {"format", "watermark"}; }

    // format = png (the default) stores the PNG. format = raw stores the decoded,
    // premultiplied pixels, compressed: larger, but no PNG decode when it loads.
    // watermark = rgb or alpha marks the PNG with the project's mark or the build's
    // fingerprint (watermark.h).
    bool Process(const Source& source, const Settings& settings, std::vector<Output>& outputs, std::string& error) const override {
        const auto format = settings.find("format");
        const std::string kind = format == settings.end() ? "png" : format->second;
        const auto watermark = settings.find("watermark");
        const std::string mark = watermark == settings.end() ? "off" : watermark->second;
        if (mark != "off" && mark != "rgb" && mark != "alpha") {
            error = "watermark is off, rgb or alpha, not \"" + mark + "\"";
            return false;
        }
        if (mark != "off" && kind != "png") {
            error = "watermarks need format = png";
            return false;
        }
        if (kind == "png" && mark != "off") {
            if (!Check(source.bytes, error)) return false;
            Output out{source.name, rime::AssetType::Texture, {}, false};
            const detail::PixelMark mode = mark == "alpha" ? detail::PixelMark::Alpha : detail::PixelMark::Rgb;
            if (!detail::MarkPng(source.bytes, source.watermarkSecret, source.fingerprint, mode, out.bytes, error)) return false;
            outputs.push_back(std::move(out));
            return true;
        }
        if (kind == "png") return Stored::Process(source, settings, outputs, error);
        if (kind != "raw") {
            error = "format is png or raw, not \"" + kind + "\"";
            return false;
        }
        if (!Check(source.bytes, error)) return false;
        SDL_IOStream* io = SDL_IOFromConstMem(source.bytes.data(), source.bytes.size());
        SDL_Surface* loaded = io ? SDL_LoadPNG_IO(io, true) : nullptr;
        SDL_Surface* rgba = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32) : nullptr;
        SDL_DestroySurface(loaded);
        if (!rgba || !SDL_PremultiplySurfaceAlpha(rgba, false)) {
            error = std::string("could not decode to RGBA: ") + SDL_GetError();
            SDL_DestroySurface(rgba);
            return false;
        }
        Output out{source.name, rime::AssetType::Texture, {}, true};
        out.bytes.assign(std::begin(rime::RawTextureTag), std::end(rime::RawTextureTag));
        for (std::uint32_t v : {static_cast<std::uint32_t>(rgba->w), static_cast<std::uint32_t>(rgba->h)})
            for (int i = 0; i < 4; ++i) out.bytes.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        for (int y = 0; y < rgba->h; ++y) {
            const auto* row = static_cast<const std::uint8_t*>(rgba->pixels) + static_cast<size_t>(y) * rgba->pitch;
            out.bytes.insert(out.bytes.end(), row, row + static_cast<size_t>(rgba->w) * 4);
        }
        SDL_DestroySurface(rgba);
        outputs.push_back(std::move(out));
        return true;
    }

protected:
    rime::AssetType Type() const override { return rime::AssetType::Texture; }
    bool Compressible() const override { return false; }
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
    std::vector<std::string> SettingKeys() const override { return {"watermark"}; }

    // watermark = on marks WAV files with the project's mark or the build's fingerprint. OGG cannot be marked
    // without re-encoding it, so it is an error rather than a silent skip.
    bool Process(const Source& source, const Settings& settings, std::vector<Output>& outputs, std::string& error) const override {
        const auto watermark = settings.find("watermark");
        const std::string mark = watermark == settings.end() ? "off" : watermark->second;
        if (mark != "off" && mark != "on") {
            error = "watermark is on or off, not \"" + mark + "\"";
            return false;
        }
        if (mark == "off") return Stored::Process(source, settings, outputs, error);
        if (!Check(source.bytes, error)) return false;
        if (!StartsWith(source.bytes, "RIFF")) {
            error = "watermark = on needs WAV. Put OGG sounds in a section without it";
            return false;
        }
        Output out{source.name, rime::AssetType::Sound, {}, true};
        if (!detail::MarkWav(source.bytes, source.watermarkSecret, source.fingerprint, out.bytes, error)) return false;
        outputs.push_back(std::move(out));
        return true;
    }

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
    bool Compressible() const override { return false; }
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

// Compiles HLSL into an Effect (effect_compiler.h). "fx/glow.hlsl" becomes "fx/glow.fx",
// which Content().Load<Effect>("fx/glow") finds.
class EffectProcessor : public Processor {
public:
    std::string Name() const override { return "effect"; }
    // A coldx.hlsli change changes every effect, without any source changing.
    int Version() const override { return static_cast<int>(detail::EffectIncludeVersion()); }

    bool Process(const Source& source, const Settings&, std::vector<Output>& outputs, std::string& error) const override {
        Output out{detail::EffectEntryName(source.name), rime::AssetType::Effect, {}, true};
        if (!detail::CompileEffect(source.path, out.bytes, error)) return false;
        outputs.push_back(std::move(out));
        return true;
    }
};

} // namespace

std::vector<std::unique_ptr<Processor>> BuiltInProcessors() {
    std::vector<std::unique_ptr<Processor>> list;
    list.push_back(std::make_unique<TextureProcessor>());
    list.push_back(std::make_unique<SoundProcessor>());
    list.push_back(std::make_unique<MusicProcessor>());
    list.push_back(std::make_unique<FontProcessor>());
    list.push_back(std::make_unique<DataProcessor>());
    list.push_back(std::make_unique<EffectProcessor>());
    return list;
}

} // namespace coldx::content
