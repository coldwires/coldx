#pragma once

#include <coldx/core/rime.h>

#include <cstdint>
#include <string>
#include <vector>

// Watermarks: a build's fingerprint written into the textures and sounds it ships, so a
// ripped file can be traced to the build it came from. Every layer can be read back from
// the suspect file alone, with the project's secret.
//
// PNG, all invisible:
//   chunk        a private block, its name derived from the secret
//   rows         the PNG filter chosen for each row: 2 bits a row
//   transparent  the colour stored in fully transparent pixels
//   pixels       the lowest bit of a colour channel (rgb) or alpha 254/255 on opaque
//                pixels (alpha, for palette art), at positions scattered by the secret
// WAV (16-bit PCM for the sample layer):
//   chunk        a private block
//   samples      the lowest bit of non-silent samples, scattered by the secret
//
// The first layers go when a file is re-saved by an editor; the pixel and sample layers
// survive a lossless re-save, not resizing or lossy re-encoding.
namespace coldx::content::detail {

enum class PixelMark {
    Rgb,
    Alpha,
};

// The secret positions and whitening come from: the project seed, and the package key if
// there is one, so a game with a key has marks that cannot be found from its project alone.
std::uint64_t WatermarkSecret(std::uint64_t seed, const rime::Key* key);

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

} // namespace coldx::content::detail
