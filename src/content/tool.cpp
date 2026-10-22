#include <coldx/content/build.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "project.h"
#include "protect_build.h"
#include "watermark.h"

namespace coldx::content {

namespace {

void Usage() {
    std::printf(
        "coldxc: builds a .rime package from a content project.\n"
        "\n"
        "  coldxc --project content/content.coldx --output build/content.rime [options]\n"
        "  coldxc --new-key FILE\n"
        "  coldxc --project content/content.coldx --identify FILE\n"
        "\n"
        "  --project      the content project file; names in the package are relative to its folder\n"
        "  --output       the package to write; left untouched when nothing changed\n"
        "  --cache        a folder for processed results, so unchanged sources are not processed again\n"
        "  --key-file     encrypt every entry with the key in FILE\n"
        "  --build-id     mark the package (and the game, with --game-source) with a fingerprint of ID\n"
        "  --game-source  write the C++ file that gives the game its key and fingerprint\n"
        "  --new-key      write a new random key to FILE\n"
        "  --identify     print the fingerprint in a game, package, PNG or WAV built from --project\n"
        "                 (give --key-file too if the package was encrypted)\n"
        "  --verbose      one line per source\n");
}

} // namespace

int RunTool(int argc, char** argv, std::vector<std::unique_ptr<Processor>> extra) {
    BuildOptions options;
    std::string newKey, identify;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (!std::strcmp(a, "--project") && hasValue) {
            options.project = argv[++i];
        } else if (!std::strcmp(a, "--output") && hasValue) {
            options.output = argv[++i];
        } else if (!std::strcmp(a, "--cache") && hasValue) {
            options.cacheDirectory = argv[++i];
        } else if (!std::strcmp(a, "--key-file") && hasValue) {
            options.keyFile = argv[++i];
        } else if (!std::strcmp(a, "--build-id") && hasValue) {
            options.buildId = argv[++i];
        } else if (!std::strcmp(a, "--game-source") && hasValue) {
            options.gameSource = argv[++i];
        } else if (!std::strcmp(a, "--new-key") && hasValue) {
            newKey = argv[++i];
        } else if (!std::strcmp(a, "--identify") && hasValue) {
            identify = argv[++i];
        } else if (!std::strcmp(a, "--verbose")) {
            options.verbose = true;
        } else if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) {
            Usage();
            return 0;
        } else {
            std::fprintf(stderr, "coldxc: unexpected argument \"%s\"\n\n", a);
            Usage();
            return 2;
        }
    }
    if (!newKey.empty()) {
        std::string error;
        if (!detail::WriteNewKey(newKey, error)) {
            std::fprintf(stderr, "coldxc: %s\n", error.c_str());
            return 1;
        }
        std::printf("coldxc: wrote a new key to %s\n", newKey.c_str());
        return 0;
    }
    if (!identify.empty()) {
        if (options.project.empty()) {
            Usage();
            return 2;
        }
        std::ifstream file(std::filesystem::path(std::u8string(options.project.begin(), options.project.end())), std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(file)), {});
        detail::Project project;
        std::vector<std::string> errors;
        if (text.empty() || !detail::ParseProject(text, project, errors)) {
            std::fprintf(stderr, "coldxc: cannot read the project %s\n", options.project.c_str());
            return 1;
        }
        const std::uint64_t seed = rime::HashName(0, project.seed);
        rime::Key key{};
        if (!options.keyFile.empty()) {
            std::string error;
            if (!detail::ReadKeyFile(options.keyFile, key, error)) {
                std::fprintf(stderr, "coldxc: %s\n", error.c_str());
                return 1;
            }
        }
        std::ifstream suspect(std::filesystem::path(std::u8string(identify.begin(), identify.end())), std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(suspect)), {});
        const bool png = bytes.size() > 8 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G';
        const bool wav = bytes.size() > 12 && std::string(bytes.begin(), bytes.begin() + 4) == "RIFF";
        if (png || wav) {
            const std::uint64_t secret = detail::WatermarkSecret(seed, options.keyFile.empty() ? nullptr : &key);
            bool any = false;
            for (const detail::Finding& f : png ? detail::ReadPng(bytes, secret) : detail::ReadWav(bytes, secret)) {
                if (f.found)
                    std::printf("fingerprint 0x%016llx in %s (%zu of %zu marks agree)\n", static_cast<unsigned long long>(f.fingerprint),
                                f.layer.c_str(), f.agree, f.total);
                else
                    std::printf("no mark in %s\n", f.layer.c_str());
                any = any || f.found;
            }
            return any ? 0 : 1;
        }
        return detail::Identify(identify, seed) ? 0 : 1;
    }
    if (options.project.empty() || options.output.empty()) {
        Usage();
        return 2;
    }

    std::vector<std::unique_ptr<Processor>> owned = BuiltInProcessors();
    for (std::unique_ptr<Processor>& p : extra) owned.push_back(std::move(p));
    std::vector<const Processor*> processors;
    for (const auto& p : owned) processors.push_back(p.get());

    const BuildResult r = Build(options, processors);
    for (const std::string& e : r.errors) std::fprintf(stderr, "coldxc: error: %s\n", e.c_str());
    if (!r.ok) {
        std::fprintf(stderr, "coldxc: %s not written\n", options.output.c_str());
        return 1;
    }
    std::printf("coldxc: %s: %zu entries from %zu sources (%zu processed, %zu cached)%s%s\n", options.output.c_str(), r.entries,
                r.sources, r.processed, r.cached, r.written ? "" : ", unchanged", options.keyFile.empty() ? "" : ", encrypted");
    if (!options.buildId.empty())
        std::printf("coldxc: build id \"%s\" is fingerprint 0x%016llx\n", options.buildId.c_str(),
                    static_cast<unsigned long long>(r.fingerprint));
    return 0;
}

} // namespace coldx::content
