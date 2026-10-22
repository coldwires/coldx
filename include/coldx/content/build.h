#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <coldx/content/processor.h>

namespace coldx::content {

struct BuildOptions {
    std::string project;  // the content.coldx file
    std::string output;   // the .rime to write
    // A folder for processed results, so an unchanged source is not processed again.
    // Empty: no cache.
    std::string cacheDirectory;
    bool verbose = false;
};

struct BuildResult {
    bool ok = false;
    std::size_t sources = 0;
    std::size_t entries = 0;
    std::size_t processed = 0;  // sources run through their processor this time
    std::size_t cached = 0;     // sources whose cached result was reused
    bool written = false;       // false when the package came out the same as the file there
    std::vector<std::string> errors;
};

// Reads the project, runs every matched source through its section's processor and
// writes the package. The file is only rewritten when its bytes change, so a build system
// sees no change when nothing did. processors are searched by Name(); later ones replace
// earlier ones of the same name.
BuildResult Build(const BuildOptions& options, const std::vector<const Processor*>& processors);

// The coldxc command line. extra processors are added to the built-in ones, so a game can
// build its own tool:
//
//     int main(int argc, char** argv) {
//         std::vector<std::unique_ptr<coldx::content::Processor>> mine;
//         mine.push_back(std::make_unique<MapProcessor>());
//         return coldx::content::RunTool(argc, argv, std::move(mine));
//     }
//
// Returns 0 on success, 1 on a failed build, 2 on bad arguments.
int RunTool(int argc, char** argv, std::vector<std::unique_ptr<Processor>> extra = {});

} // namespace coldx::content
