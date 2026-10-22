#include <coldx/content/build.h>

#include <cstdio>
#include <cstring>

namespace coldx::content {

namespace {

void Usage() {
    std::printf(
        "coldxc: builds a .rime package from a content project.\n"
        "\n"
        "  coldxc --project content/content.coldx --output build/content.rime [--cache DIR] [--verbose]\n"
        "\n"
        "  --project   the content project file; names in the package are relative to its folder\n"
        "  --output    the package to write; left untouched when nothing changed\n"
        "  --cache     a folder for processed results, so unchanged sources are not processed again\n"
        "  --verbose   one line per source\n");
}

} // namespace

int RunTool(int argc, char** argv, std::vector<std::unique_ptr<Processor>> extra) {
    BuildOptions options;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (!std::strcmp(a, "--project") && hasValue) {
            options.project = argv[++i];
        } else if (!std::strcmp(a, "--output") && hasValue) {
            options.output = argv[++i];
        } else if (!std::strcmp(a, "--cache") && hasValue) {
            options.cacheDirectory = argv[++i];
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
    std::printf("coldxc: %s: %zu entries from %zu sources (%zu processed, %zu cached)%s\n", options.output.c_str(), r.entries,
                r.sources, r.processed, r.cached, r.written ? "" : ", unchanged");
    return 0;
}

} // namespace coldx::content
