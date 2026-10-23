#pragma once

#include <coldx/core/rime.h>

#include <cstdint>
#include <string>
#include <vector>

// Watermarks: a 40-bit mark written into the textures and sounds a game ships: the
// project's own (OwnerMark), or a build's fingerprint when it has a build id. Every layer
// can be read back from the suspect file alone, with the project's secret.
//
// PNG, all invisible:
//   chunk        a private block, its name derived from the secret
//   rows         the PNG filter chosen for each row: 2 bits a row
//   transparent  the colour stored in fully transparent pixels; each such pixel names
//                which payload byte it holds and checks itself, so any crop reads
//   pixels       the lowest bit of a colour channel (rgb, 80 places in every 37x37
//                tile) or alpha 254/255 on opaque pixels (alpha, for palette art, 240
//                places: three copies a tile), shuffled by the secret. 37 is prime, so sprite
//                grids of 8, 16, 32 or 64 never line up with the tiles
// WAV (16-bit PCM for the sample layer):
//   chunk        a private block
//   samples      the lowest bit of non-silent samples, at 80 places in every 2048
//
// Two ways to read. Reading an unknown mark (ReadPng, ReadWav) needs two copies of every
// bit: a crop of about 75x75 pixels or more, or a few blocks of sound; the reader tries
// every alignment and accepts one only with two copies of every bit, 90% agreement and a
// 40-bit check, so it never names a build by chance. Checking for a known mark (MatchPng,
// MatchWav) needs far less: a single 24x24 sprite in alpha mode. The payload is the 40-bit
// mark and that check. The chunk
// and row layers go when a file is re-saved by an editor; the others survive a lossless
// re-save and cropping or trimming, not resizing or lossy re-encoding.
namespace coldx::content::detail {

enum class PixelMark {
    Rgb,
    Alpha,
};

// The secret positions and whitening come from: the project seed, and the package key if
// there is one, so a game with a key has marks that cannot be found from its project alone.
std::uint64_t WatermarkSecret(std::uint64_t seed, const rime::Key* key);
// The fingerprint marked when there is no build id: the project's own, so its assets can
// be recognised as its own.
std::uint64_t OwnerMark(std::uint64_t secret);

// A marked copy of png (always written as 8-bit RGBA). False with error set.
bool MarkPng(const std::vector<std::uint8_t>& png, std::uint64_t secret, std::uint64_t fingerprint, PixelMark mode,
             std::vector<std::uint8_t>& out, std::string& error);
// A marked copy of a WAV file. False with error set.
bool MarkWav(const std::vector<std::uint8_t>& wav, std::uint64_t secret, std::uint64_t fingerprint, std::vector<std::uint8_t>& out,
             std::string& error);

// What one layer says. found: a fingerprint whose check matched; agree of total
// observations agree with it.
struct Finding {
    std::string layer;
    bool found = false;
    std::uint64_t fingerprint = 0;
    std::size_t agree = 0;
    std::size_t total = 0;
};

std::vector<Finding> ReadPng(const std::vector<std::uint8_t>& png, std::uint64_t secret);
std::vector<Finding> ReadWav(const std::vector<std::uint8_t>& wav, std::uint64_t secret);

// Whether a file carries one particular fingerprint, from far fewer marks than reading an
// unknown one takes: each marked pixel or sample either agrees with the bit that
// fingerprint puts there or not, and a file that does not carry it agrees half the time.
// agree and total count distinct places (a place's copies vote first); chance is the
// probability of agreeing this well by luck, after every alignment, mode and candidate
// tried; matched means it is below one in a million.
struct Match {
    std::string layer;
    std::uint64_t fingerprint = 0;
    std::size_t agree = 0;
    std::size_t total = 0;
    double chance = 1.0;
    bool matched = false;
};
std::vector<Match> MatchPng(const std::vector<std::uint8_t>& png, std::uint64_t secret, const std::vector<std::uint64_t>& fingerprints);
std::vector<Match> MatchWav(const std::vector<std::uint8_t>& wav, std::uint64_t secret, const std::vector<std::uint64_t>& fingerprints);

} // namespace coldx::content::detail
