#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <coldx/core/rime.h>

namespace coldx::content {

// One source file a content project matched.
struct Source {
    std::string name;  // relative to the project file, forward slashes: "sprites/player.png"
    std::string path;  // where it is on disk
    std::vector<std::uint8_t> bytes;
    // What a processor that watermarks its output writes: this build's fingerprint
    // (BuildOptions::buildId), or without one the project's own mark; and the secret that
    // places the marks.
    std::uint64_t fingerprint = 0;
    std::uint64_t watermarkSecret = 0;
};

// One package entry a processor made. name is what the game loads it by, extension
// included; most processors keep the source's name.
struct Output {
    std::string name;
    rime::AssetType type = rime::AssetType::Data;
    std::vector<std::uint8_t> bytes;
    // Compress if that saves space. Turn off for data that never shrinks, to save build time.
    bool compress = true;
};

// The "key = value" lines under a section of the project file.
using Settings = std::map<std::string, std::string>;

// Turns a source into package entries. The built-in ones are texture, sound, music, font
// and data. A game adds its own by subclassing and passing it to RunTool or Build; its
// section in the project file is [Name()].
class Processor {
public:
    virtual ~Processor() = default;

    // The project file's section word: lower case letters, digits and underscores.
    virtual std::string Name() const = 0;
    // Bump when the output for the same input changes, so cached results are rebuilt.
    virtual int Version() const { return 1; }
    // The setting keys this processor reads; any other key in its section is an error,
    // so a typo does not go unnoticed.
    virtual std::vector<std::string> SettingKeys() const { return {}; }

    // Appends what source becomes to outputs (usually one entry, possibly none or
    // several). On failure returns false with error set; the build fails, naming the file.
    virtual bool Process(const Source& source, const Settings& settings, std::vector<Output>& outputs,
                         std::string& error) const = 0;
};

// texture (PNG), sound (WAV, OGG), music (OGG), font (TTF, OTF) and data (anything). Each
// checks its sources and stores them as they are, compressed when that helps.
// texture takes "format = raw" to store decoded pixels instead, which
// load without a PNG decode. texture takes "watermark = rgb" or "alpha", and sound
// "watermark = on" (WAV), to mark each file with the project's mark or the build's
// fingerprint.
std::vector<std::unique_ptr<Processor>> BuiltInProcessors();

} // namespace coldx::content
