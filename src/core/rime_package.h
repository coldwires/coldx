#pragma once

#include <coldx/core/rime.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct SDL_IOStream;

namespace coldx::detail {

// A mounted .rime: the entry table in memory, the file kept open, entries read on demand.
// Everything read from the file is checked before it is trusted: magic, version, sizes,
// that every entry lies inside the file, and each entry's CRC when it is read.
class Package {
public:
    struct Entry {
        std::uint64_t hash = 0;
        rime::AssetType type = rime::AssetType::Data;
        std::uint8_t flags = 0;
        std::uint32_t crc = 0;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        std::uint64_t originalSize = 0;
    };

    // Null, with the reason logged, if path is not a package this version can read.
    static std::unique_ptr<Package> Open(const std::string& path);
    // A package already in memory, for tests and embedded content.
    static std::unique_ptr<Package> FromMemory(std::vector<std::uint8_t> bytes, const std::string& label);
    ~Package();

    // The entry for a content name, or null. The name is normalized here.
    const Entry* Find(const std::string& name) const;
    // The entry's bytes, decrypted with key (null: none) and decompressed. False, logged, on
    // a read error, a missing or wrong key, a CRC mismatch or data that does not
    // decompress to its recorded size.
    bool Read(const Entry& entry, std::vector<std::uint8_t>& out, const rime::Key* key = nullptr) const;
    // 0 for a package with no encrypted entries.
    std::uint64_t KeyCheck() const { return keyCheck_; }

    const std::string& Label() const { return label_; }
    std::size_t Count() const { return entries_.size(); }

private:
    Package() = default;
    bool Parse(std::uint64_t fileSize);
    bool ReadAt(std::uint64_t offset, void* out, std::size_t size) const;

    std::string label_;
    SDL_IOStream* file_ = nullptr;
    std::vector<std::uint8_t> memory_;  // used instead of file_ when non-empty
    std::uint64_t seed_ = 0;
    std::uint64_t keyCheck_ = 0;
    std::vector<Entry> entries_;  // sorted by hash
};

} // namespace coldx::detail
