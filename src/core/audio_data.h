#pragma once

#include <cstdint>
#include <memory>
#include <vector>

struct stb_vorbis;

namespace coldx::detail {

// A fully decoded sound: interleaved float samples at the file's own rate. The mixer
// converts rate and channel count as it plays.
struct SoundData {
    std::vector<float> samples;
    int channels = 0;
    int sampleRate = 0;
    std::int64_t frames = 0;
};

// A compressed OGG Vorbis file, kept whole in memory.
struct MusicData {
    std::vector<unsigned char> bytes;
    int channels = 0;
    int sampleRate = 0;
    float duration = 0.0f;
};

// One playback of a MusicData: a decoder plus the buffer the mixer resamples from. Made
// and destroyed on the game thread. Between Play and its return to the game thread, only
// the audio thread touches it.
class MusicStream {
public:
    static constexpr int BufferFrames = 1024;

    // Null if the data does not decode.
    static std::unique_ptr<MusicStream> Open(std::shared_ptr<const MusicData> data);
    ~MusicStream();

    int Channels() const { return data_->channels; }
    int SampleRate() const { return data_->sampleRate; }
    // Up to frames interleaved frames into out. 0 at the end of the track.
    int Read(float* out, int frames);
    void Rewind();

    // Mixer state, kept here so the mixer allocates nothing on the audio thread.
    std::vector<float> buffer;  // (BufferFrames + 1) frames: one carried over for interpolation
    int bufferCount = 0;
    double position = 0.0;
    bool looped = false;

private:
    MusicStream() = default;
    std::shared_ptr<const MusicData> data_;
    stb_vorbis* decoder_ = nullptr;
};

// Decoding, with reasons logged on failure. Null on failure.
std::shared_ptr<SoundData> DecodeSound(const std::vector<unsigned char>& file, const char* name);
std::shared_ptr<MusicData> LoadMusic(std::vector<unsigned char> file, const char* name);

} // namespace coldx::detail
