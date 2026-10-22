#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The .rime package format, version 1. ContentManager::Mount reads packages; coldxc and
// the content module write them through PackageWriter. Games rarely need this header.
//
// Layout, all integers little-endian:
//
//   header, 48 bytes
//     0  magic      9F 9C B9 B6
//     4  u16        format version (1)
//     6  u16        header size (48)
//     8  u32        package flags (0; reserved for protection)
//     12 u32        entry count
//     16 u64        name hash seed
//     24 u64        offset of the entry table
//     32 u64        key check: 0, or the first 8 bytes of the key's ChaCha20 keystream
//                   for an all-0xFF nonce, so a wrong key is told apart from damage
//     40 u64        build fingerprint, masked with the seed; 0 for none
//   entry table: entry count entries of 40 bytes, sorted by hash
//     0  u64        name hash
//     8  u8         asset type
//     9  u8         entry flags: compressed (LZ4 block), encrypted (ChaCha20)
//     10 u16        reserved
//     12 u32        CRC-32 of the bytes after compression and before encryption
//     16 u64        offset of the stored bytes
//     24 u64        stored size
//     32 u64        original size: the stored size, or the size after decompressing
//   the stored bytes of each entry, each starting on a 16-byte boundary
//
// An encrypted entry is ChaCha20 (RFC 8439) with the package key, counter 0, and the
// nonce: the entry's name hash, then its CRC-32. The nonce changes with the content, so
// one key never encrypts two different contents under one nonce, and builds stay
// deterministic. Encryption with a key the game carries keeps out casual extraction, not
// a determined reader.
//
// No name, type name or extension is written: entries are known by the hash of their
// content path, seeded per package.
namespace coldx::rime {

inline constexpr std::uint8_t Magic[4] = {0x9F, 0x9C, 0xB9, 0xB6};
inline constexpr std::uint16_t Version = 1;
inline constexpr std::size_t HeaderSize = 48;
inline constexpr std::size_t EntrySize = 40;

// What an entry holds, as a number. 128 and up are for a game's own processors.
enum class AssetType : std::uint8_t {
    Data = 0,  // bytes as given
    Texture = 1,
    Sound = 2,
    Music = 3,
    Font = 4,
    Effect = 5,  // reserved
    FirstCustom = 128,
};

// Per-entry flags. A reader refuses an entry with a flag it does not know.
enum EntryFlags : std::uint8_t {
    Compressed = 1u << 0,  // the stored bytes are one LZ4 block
    Encrypted = 1u << 1,   // ChaCha20, after compression
};

using Key = std::array<std::uint8_t, 32>;
// The value stored in the header for key; never 0.
std::uint64_t KeyCheck(const Key& key);

// A texture stored decoded, for loading without a PNG decode: this tag (not text), then
// u32 width, u32 height, and width * height premultiplied RGBA pixels, rows top to bottom.
inline constexpr std::uint8_t RawTextureTag[4] = {0xC7, 0x8E, 0x31, 0xD4};
inline constexpr std::size_t RawTextureHeader = 12;

// The form names are hashed in: lower case, forward slashes, no leading "./" or "/".
// "Sprites\Player.PNG" is "sprites/player.png".
std::string NormalizeName(std::string_view name);
// The hash an entry is stored under. name should be normalized.
std::uint64_t HashName(std::uint64_t seed, std::string_view name);
// CRC-32 (the zlib and PNG one).
std::uint32_t Crc32(const void* data, std::size_t size);

// Builds a package in memory.
class PackageWriter {
public:
    explicit PackageWriter(std::uint64_t seed) : seed_(seed) {}

    // Stored in the header as given; coldxc masks it first.
    void SetFingerprint(std::uint64_t value) { fingerprint_ = value; }
    // Encrypts every entry with key.
    void SetKey(const Key& key) {
        key_ = key;
        encrypt_ = true;
    }

    // Adds name (normalized here). False, logged, if the name or its hash is already in.
    // With compress, Finish stores the entry compressed if that saves at least a twentieth:
    // already-compressed data (PNG, OGG) stays as it is.
    bool Add(std::string_view name, AssetType type, std::vector<std::uint8_t> bytes, bool compress = true);
    std::size_t Count() const { return entries_.size(); }

    // The whole package. The same entries give the same bytes, in any order of Add.
    std::vector<std::uint8_t> Finish() const;

private:
    struct Pending {
        std::uint64_t hash;
        std::string name;
        AssetType type;
        std::vector<std::uint8_t> bytes;
        bool compress;
    };
    std::uint64_t seed_;
    Key key_{};
    bool encrypt_ = false;
    std::uint64_t fingerprint_ = 0;
    std::vector<Pending> entries_;
};

} // namespace coldx::rime
