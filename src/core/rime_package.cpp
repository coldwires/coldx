#include "rime_package.h"

#include <algorithm>
#include <cstring>

#include <SDL3/SDL.h>

#include "rime_bytes.h"

namespace coldx::detail {

std::unique_ptr<Package> Package::Open(const std::string& path) {
    SDL_IOStream* file = SDL_IOFromFile(path.c_str(), "rb");
    if (!file) {
        SDL_Log("coldx.core: could not open %s: %s", path.c_str(), SDL_GetError());
        return nullptr;
    }
    std::unique_ptr<Package> package(new Package());
    package->label_ = path;
    package->file_ = file;
    const Sint64 size = SDL_GetIOSize(file);
    if (size < 0 || !package->Parse(static_cast<std::uint64_t>(size))) return nullptr;
    return package;
}

std::unique_ptr<Package> Package::FromMemory(std::vector<std::uint8_t> bytes, const std::string& label) {
    std::unique_ptr<Package> package(new Package());
    package->label_ = label;
    package->memory_ = std::move(bytes);
    if (package->memory_.empty() || !package->Parse(package->memory_.size())) {
        if (package->memory_.empty()) SDL_Log("coldx.core: %s: empty, not a package", label.c_str());
        return nullptr;
    }
    return package;
}

Package::~Package() {
    if (file_) SDL_CloseIO(file_);
}

bool Package::ReadAt(std::uint64_t offset, void* out, std::size_t size) const {
    if (!memory_.empty()) {
        if (offset > memory_.size() || size > memory_.size() - offset) return false;
        std::memcpy(out, memory_.data() + offset, size);
        return true;
    }
    if (SDL_SeekIO(file_, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) < 0) return false;
    return size == 0 || SDL_ReadIO(file_, out, size) == size;
}

bool Package::Parse(std::uint64_t fileSize) {
    auto fail = [this](const char* why) {
        SDL_Log("coldx.core: %s: not a package coldx can read: %s", label_.c_str(), why);
        return false;
    };
    std::uint8_t h[rime::HeaderSize];
    if (fileSize < rime::HeaderSize || !ReadAt(0, h, sizeof(h))) return fail("too short");
    if (!std::equal(std::begin(rime::Magic), std::end(rime::Magic), h)) return fail("wrong magic");
    const std::uint16_t version = Get16(h + 4);
    if (version != rime::Version) {
        SDL_Log("coldx.core: %s: package version %u; this coldx reads version %u", label_.c_str(), version, rime::Version);
        return false;
    }
    if (Get16(h + 6) != rime::HeaderSize) return fail("unexpected header size");
    if (Get32(h + 8) != 0) return fail("uses package features this version does not have");
    const std::uint32_t count = Get32(h + 12);
    seed_ = Get64(h + 16);
    const std::uint64_t table = Get64(h + 24);
    if (table > fileSize || count > (fileSize - table) / rime::EntrySize) return fail("entry table outside the file");

    std::vector<std::uint8_t> raw(static_cast<size_t>(count) * rime::EntrySize);
    if (!raw.empty() && !ReadAt(table, raw.data(), raw.size())) return fail("could not read the entry table");
    entries_.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* e = raw.data() + static_cast<size_t>(i) * rime::EntrySize;
        Entry& entry = entries_[i];
        entry.hash = Get64(e);
        entry.type = static_cast<rime::AssetType>(e[8]);
        entry.flags = e[9];
        entry.crc = Get32(e + 12);
        entry.offset = Get64(e + 16);
        entry.size = Get64(e + 24);
        if (entry.offset > fileSize || entry.size > fileSize - entry.offset) return fail("an entry lies outside the file");
        if (Get64(e + 32) != entry.size) return fail("an entry's sizes disagree");
        if (i > 0 && entries_[i - 1].hash >= entry.hash) return fail("entry table not sorted");
    }
    SDL_Log("coldx.core: mounted %s, %u entries", label_.c_str(), count);
    return true;
}

const Package::Entry* Package::Find(const std::string& name) const {
    const std::uint64_t hash = rime::HashName(seed_, rime::NormalizeName(name));
    auto it = std::lower_bound(entries_.begin(), entries_.end(), hash, [](const Entry& e, std::uint64_t h) { return e.hash < h; });
    return it != entries_.end() && it->hash == hash ? &*it : nullptr;
}

bool Package::Read(const Entry& entry, std::vector<std::uint8_t>& out) const {
    if (entry.flags != 0) {
        SDL_Log("coldx.core: %s: an entry is compressed or encrypted, which this version cannot read", label_.c_str());
        return false;
    }
    out.resize(static_cast<size_t>(entry.size));
    if (!ReadAt(entry.offset, out.data(), out.size())) {
        SDL_Log("coldx.core: %s: read failed", label_.c_str());
        return false;
    }
    if (rime::Crc32(out.data(), out.size()) != entry.crc) {
        SDL_Log("coldx.core: %s: an entry is damaged (CRC mismatch)", label_.c_str());
        return false;
    }
    return true;
}

} // namespace coldx::detail
