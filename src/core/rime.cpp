#include <coldx/core/rime.h>

#include <algorithm>
#include <array>

#include <SDL3/SDL.h>

#include "rime_bytes.h"

namespace coldx::rime {

std::string NormalizeName(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c == '/' && (out.empty() || out.back() == '/')) continue;  // leading or doubled
        out += c;
    }
    while (out.size() >= 2 && out[0] == '.' && out[1] == '/') out.erase(0, 2);
    return out;
}

// FNV-1a over the seed and the name, then a finalizer, so names that differ by a letter
// land far apart and the hash says nothing about the name's length or prefix.
std::uint64_t HashName(std::uint64_t seed, std::string_view name) {
    std::uint64_t h = 0xCBF29CE484222325ull;
    auto mix = [&h](std::uint8_t b) {
        h ^= b;
        h *= 0x100000001B3ull;
    };
    for (int i = 0; i < 8; ++i) mix(static_cast<std::uint8_t>(seed >> (8 * i)));
    for (char c : name) mix(static_cast<std::uint8_t>(c));
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ull;
    h ^= h >> 33;
    return h;
}

std::uint32_t Crc32(const void* data, std::size_t size) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t crc = 0xFFFFFFFFu;
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

bool PackageWriter::Add(std::string_view name, AssetType type, std::vector<std::uint8_t> bytes) {
    std::string normal = NormalizeName(name);
    const std::uint64_t hash = HashName(seed_, normal);
    for (const Pending& p : entries_) {
        if (p.name == normal) {
            SDL_Log("coldx.core: rime: \"%s\" added twice", normal.c_str());
            return false;
        }
        if (p.hash == hash) {
            // A 64-bit collision; vanishingly rare, and a different seed fixes it.
            SDL_Log("coldx.core: rime: \"%s\" and \"%s\" hash alike; change the package seed", normal.c_str(),
                    p.name.c_str());
            return false;
        }
    }
    entries_.push_back({hash, std::move(normal), type, std::move(bytes)});
    return true;
}

std::vector<std::uint8_t> PackageWriter::Finish() const {
    std::vector<const Pending*> sorted;
    for (const Pending& p : entries_) sorted.push_back(&p);
    std::sort(sorted.begin(), sorted.end(), [](const Pending* a, const Pending* b) { return a->hash < b->hash; });

    const std::uint64_t tableOffset = HeaderSize;
    std::uint64_t dataOffset = detail::AlignUp(tableOffset + EntrySize * sorted.size());
    std::vector<std::uint8_t> out(static_cast<size_t>(dataOffset), 0);

    std::uint8_t* h = out.data();
    std::copy(std::begin(Magic), std::end(Magic), h);
    detail::Put16(h + 4, Version);
    detail::Put16(h + 6, static_cast<std::uint16_t>(HeaderSize));
    detail::Put32(h + 8, 0);
    detail::Put32(h + 12, static_cast<std::uint32_t>(sorted.size()));
    detail::Put64(h + 16, seed_);
    detail::Put64(h + 24, tableOffset);

    for (size_t i = 0; i < sorted.size(); ++i) {
        const Pending& p = *sorted[i];
        std::uint8_t* e = out.data() + tableOffset + EntrySize * i;
        detail::Put64(e, p.hash);
        e[8] = static_cast<std::uint8_t>(p.type);
        e[9] = 0;
        detail::Put32(e + 12, Crc32(p.bytes.data(), p.bytes.size()));
        detail::Put64(e + 16, dataOffset);
        detail::Put64(e + 24, p.bytes.size());
        detail::Put64(e + 32, p.bytes.size());
        const std::uint64_t end = detail::AlignUp(dataOffset + p.bytes.size());
        out.resize(static_cast<size_t>(end), 0);
        std::copy(p.bytes.begin(), p.bytes.end(), out.begin() + static_cast<std::ptrdiff_t>(dataOffset));
        dataOffset = end;
    }
    return out;
}

} // namespace coldx::rime
