#pragma once

#include <string>

namespace coldx::detail {

// Called by Game::Run with GameSettings::company and the game's name, which pick the
// Storage folder.
void SetStorageNames(const std::string& company, const std::string& name);

} // namespace coldx::detail
