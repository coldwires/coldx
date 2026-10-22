#include <coldx/content/build.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "project.h"
#include "protect_build.h"
#include "watermark.h"

namespace coldx::content {

namespace fs = std::filesystem;

namespace {

fs::path Utf8Path(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

bool ReadFile(const fs::path& path, std::vector<std::uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), {});
    return !file.bad();
}

// FNV-1a 64, fed piece by piece: the cache key of a source and how it is processed.
struct Hasher {
    std::uint64_t h = 0xCBF29CE484222325ull;
    void Add(const void* data, size_t size) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            h ^= p[i];
            h *= 0x100000001B3ull;
        }
    }
    void Add(const std::string& s) {
        Add(s.data(), s.size());
        Add("\0", 1);  // so "ab"+"c" and "a"+"bc" differ
    }
};

std::uint64_t CacheKey(const Processor& p, const detail::Section& section, const Source& source) {
    Hasher h;
    h.Add("coldxc cache 3");
    h.Add(p.Name());
    const int version = p.Version();
    h.Add(&version, sizeof(version));
    for (const auto& [key, value] : section.settings) h.Add(key + "=" + value);
    h.Add(source.name);
    h.Add(source.bytes.data(), source.bytes.size());
    // A watermarked output depends on the build's fingerprint and secret too.
    const std::uint64_t mark[2] = {source.hasFingerprint ? source.fingerprint : 0, source.watermarkSecret};
    h.Add(mark, sizeof(mark));
    return h.h;
}

// Cache files: "CXC2", then per output a u32 name length, the name, a u8 type, a u8
// compress flag, a u64 size and the bytes. Anything that does not parse is treated as no cache.
void PutU32(std::string& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out += static_cast<char>(v >> (8 * i));
}
void PutU64(std::string& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out += static_cast<char>(v >> (8 * i));
}

bool LoadCache(const fs::path& file, std::vector<Output>& outputs) {
    std::vector<std::uint8_t> b;
    if (!ReadFile(file, b) || b.size() < 4 || std::string(b.begin(), b.begin() + 4) != "CXC2") return false;
    size_t at = 4;
    auto take = [&](size_t n, std::uint64_t& v) {
        if (b.size() - at < n) return false;
        v = 0;
        for (size_t i = 0; i < n; ++i) v |= std::uint64_t{b[at + i]} << (8 * i);
        at += n;
        return true;
    };
    std::vector<Output> read;
    while (at < b.size()) {
        std::uint64_t nameLength = 0, type = 0, compress = 0, size = 0;
        if (!take(4, nameLength) || b.size() - at < nameLength) return false;
        Output o;
        o.name.assign(b.begin() + static_cast<std::ptrdiff_t>(at), b.begin() + static_cast<std::ptrdiff_t>(at + nameLength));
        at += static_cast<size_t>(nameLength);
        if (!take(1, type) || !take(1, compress) || !take(8, size) || b.size() - at < size) return false;
        o.type = static_cast<rime::AssetType>(type);
        o.compress = compress != 0;
        o.bytes.assign(b.begin() + static_cast<std::ptrdiff_t>(at), b.begin() + static_cast<std::ptrdiff_t>(at + size));
        at += static_cast<size_t>(size);
        read.push_back(std::move(o));
    }
    outputs = std::move(read);
    return true;
}

void SaveCache(const fs::path& file, const std::vector<Output>& outputs) {
    std::string out = "CXC2";
    for (const Output& o : outputs) {
        PutU32(out, static_cast<std::uint32_t>(o.name.size()));
        out += o.name;
        out += static_cast<char>(o.type);
        out += static_cast<char>(o.compress ? 1 : 0);
        PutU64(out, o.bytes.size());
        out.append(o.bytes.begin(), o.bytes.end());
    }
    const fs::path temp = file.string() + ".tmp";
    std::ofstream(temp, std::ios::binary) << out;
    std::error_code ec;
    fs::rename(temp, file, ec);
}

} // namespace

BuildResult Build(const BuildOptions& options, const std::vector<const Processor*>& processors) {
    BuildResult result;
    auto fail = [&](std::string message) {
        result.errors.push_back(std::move(message));
        return result;
    };
    const fs::path projectPath = Utf8Path(options.project);
    std::vector<std::uint8_t> text;
    if (!ReadFile(projectPath, text)) return fail("cannot read the project file " + options.project);

    detail::Project project;
    std::vector<std::string> parseErrors;
    if (!detail::ParseProject(std::string(text.begin(), text.end()), project, parseErrors)) {
        for (std::string& e : parseErrors) result.errors.push_back(options.project + ": " + e);
        return result;
    }

    std::map<std::string, const Processor*> byName;
    for (const Processor* p : processors) byName[p->Name()] = p;
    for (const detail::Section& section : project.sections) {
        const std::string where = options.project + ": line " + std::to_string(section.line) + ": ";
        auto found = byName.find(section.processor);
        if (found == byName.end()) {
            std::string known;
            for (const auto& [name, p] : byName) known += (known.empty() ? "" : ", ") + name;
            result.errors.push_back(where + "no processor \"" + section.processor + "\" (this tool has " + known + ")");
            continue;
        }
        const std::vector<std::string> keys = found->second->SettingKeys();
        for (const auto& [key, value] : section.settings)
            if (key != "from" && std::find(keys.begin(), keys.end(), key) == keys.end())
                result.errors.push_back(where + "[" + section.processor + "] has no setting \"" + key + "\"");
    }
    if (!result.errors.empty()) return result;

    // Which section takes each file. A section's patterns and names are relative to the
    // project folder, or to its "from" folder. The project file and the output never count.
    const fs::path directory = projectPath.parent_path().empty() ? fs::path(".") : projectPath.parent_path();
    std::error_code ec;
    const fs::path outputFull = fs::weakly_canonical(Utf8Path(options.output), ec);
    const fs::path projectFull = fs::weakly_canonical(projectPath, ec);
    struct Pick {
        std::string name;
        fs::path path;
        const detail::Section* section;
    };
    std::vector<Pick> sources;
    std::map<fs::path, const detail::Section*> taken;
    std::map<fs::path, std::vector<std::string>> listings;
    for (const detail::Section& section : project.sections) {
        const auto from = section.settings.find("from");
        const fs::path base = from == section.settings.end() ? directory : directory / Utf8Path(from->second);
        if (!fs::is_directory(base, ec)) {
            result.errors.push_back(options.project + ": line " + std::to_string(section.line) + ": no folder " + base.string());
            continue;
        }
        auto listed = listings.find(base);
        if (listed == listings.end()) listed = listings.emplace(base, detail::ListFiles(base.string())).first;
        bool any = false;
        for (const std::string& file : listed->second) {
            bool matches = false;
            for (const std::string& pattern : section.patterns) matches = matches || detail::GlobMatch(pattern, file);
            if (!matches) continue;
            const fs::path full = fs::weakly_canonical(base / Utf8Path(file), ec);
            if (full == projectFull || full == outputFull) continue;
            any = true;
            if (auto [it, fresh] = taken.emplace(full, &section); !fresh) {
                result.errors.push_back(options.project + ": " + file + " is matched by line " + std::to_string(it->second->line) +
                                        " and line " + std::to_string(section.line));
                continue;
            }
            sources.push_back({file, base / Utf8Path(file), &section});
        }
        if (!any)
            std::fprintf(stderr, "coldx.content: %s: line %d: [%s] matches no files\n", options.project.c_str(), section.line,
                         section.processor.c_str());
    }
    if (!result.errors.empty()) return result;
    if (!project.seedSet && options.verbose)
        std::printf("coldx.content: no seed set; set seed = <your game> so this package's name hashes are its own\n");

    if (!options.cacheDirectory.empty()) fs::create_directories(Utf8Path(options.cacheDirectory), ec);
    const std::uint64_t seed = rime::HashName(0, project.seed);
    rime::PackageWriter writer(seed);
    rime::Key key{};
    const bool encrypt = !options.keyFile.empty();
    if (encrypt) {
        std::string error;
        if (!detail::ReadKeyFile(options.keyFile, key, error)) return fail(error);
        writer.SetKey(key);
    }
    if (!options.buildId.empty()) {
        result.fingerprint = detail::FingerprintOf(options.buildId);
        writer.SetFingerprint(detail::MaskFingerprint(seed, result.fingerprint));
    }
    if (!options.gameSource.empty()) {
        std::string error;
        if (!detail::WriteGameSource(options.gameSource, seed, encrypt ? &key : nullptr,
                                     options.buildId.empty() ? nullptr : &result.fingerprint, error))
            return fail(error);
    }
    for (const Pick& pick : sources) {
        const std::string& file = pick.name;
        const detail::Section* section = pick.section;
        const Processor& processor = *byName.at(section->processor);
        Source source{file, pick.path.string(), {}};
        source.hasFingerprint = !options.buildId.empty();
        source.fingerprint = result.fingerprint;
        source.watermarkSecret = detail::WatermarkSecret(seed, encrypt ? &key : nullptr);
        if (!ReadFile(pick.path, source.bytes)) {
            result.errors.push_back(file + ": cannot read");
            continue;
        }
        ++result.sources;
        std::vector<Output> outputs;
        fs::path cacheFile;
        if (!options.cacheDirectory.empty()) {
            char name[17];
            std::snprintf(name, sizeof(name), "%016llx", static_cast<unsigned long long>(CacheKey(processor, *section, source)));
            cacheFile = Utf8Path(options.cacheDirectory) / name;
        }
        if (!cacheFile.empty() && LoadCache(cacheFile, outputs)) {
            ++result.cached;
            if (options.verbose) std::printf("  cached     %s\n", file.c_str());
        } else {
            std::string error;
            if (!processor.Process(source, section->settings, outputs, error)) {
                result.errors.push_back(file + ": [" + section->processor + "] " + (error.empty() ? "failed" : error));
                continue;
            }
            ++result.processed;
            if (!cacheFile.empty()) SaveCache(cacheFile, outputs);
            if (options.verbose) std::printf("  processed  %s\n", file.c_str());
        }
        for (Output& o : outputs) {
            if (!writer.Add(o.name, o.type, std::move(o.bytes), o.compress))
                result.errors.push_back(file + ": its output \"" + o.name + "\" is already in the package");
        }
    }
    if (!result.errors.empty()) return result;

    result.entries = writer.Count();
    const std::vector<std::uint8_t> package = writer.Finish();
    std::vector<std::uint8_t> existing;
    const fs::path output = Utf8Path(options.output);
    if (ReadFile(output, existing) && existing == package) {
        result.ok = true;
        return result;
    }
    if (output.has_parent_path()) fs::create_directories(output.parent_path(), ec);
    const fs::path temp = output.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary);
        out.write(reinterpret_cast<const char*>(package.data()), static_cast<std::streamsize>(package.size()));
        if (!out) return fail("cannot write " + temp.string());
    }
    fs::rename(temp, output, ec);
    if (ec) return fail("cannot replace " + options.output + ": " + ec.message());
    result.written = true;
    result.ok = true;
    return result;
}

} // namespace coldx::content
