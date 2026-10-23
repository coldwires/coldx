#pragma once

#include <coldx/core/rime.h>

#include <cstdint>
#include <string>

// Package keys, build fingerprints and the source file coldxc generates into a game.
namespace coldx::content::detail {

// A key file holds 64 hex digits (whitespace and # comments ignored). False with error set.
bool ReadKeyFile(const std::string& path, rime::Key& key, std::string& error);
// Writes a new random key. Refuses to overwrite an existing file.
bool WriteNewKey(const std::string& path, std::string& error);

// A build id as given ("tester-07", "2026-10-21-beta") to the 40-bit fingerprint stored.
std::uint64_t FingerprintOf(const std::string& buildId);
// The fingerprint as written into a package header or the game: masked with the package
// seed, so the raw value is not the same in every game.
std::uint64_t MaskFingerprint(std::uint64_t seed, std::uint64_t fingerprint);
// The marker that precedes the fingerprint in the game: different for every seed, so there
// is no one byte pattern to search every coldx game for.
std::uint64_t FingerprintMarker(std::uint64_t seed);

// The C++ source compiled into the game: installs key (if any) as ContentManager's default
// key, scattered and masked rather than stored as one array, and holds the marked
// fingerprint (if any). Written only when its text changes. False with error set.
bool WriteGameSource(const std::string& path, std::uint64_t seed, const rime::Key* key, const std::uint64_t* fingerprint,
                     std::string& error);

// Looks for the fingerprint in a game executable or a package: prints what it finds.
// Returns false if there is none.
bool Identify(const std::string& file, std::uint64_t seed);

} // namespace coldx::content::detail
