#pragma once

#include <string>

namespace coldx::detail {

// A relative path is taken from the folder the executable is in, so content beside the
// game is found whatever the working directory is. Absolute paths pass through.
std::string ResolveContentPath(const std::string& path);

} // namespace coldx::detail
