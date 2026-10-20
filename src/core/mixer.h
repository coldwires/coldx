#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "audio_data.h"
#include "spsc_queue.h"

namespace coldx::detail {

constexpr int MixerVoices = 64;

enum class MixCommandType : std::uint8_t {
    Play,
    Stop,
    Pause,
    Resume,
    SetVolume,
    SetPitch,
    SetPan,
    SetMasterVolume,
    PlayMusic,
    StopMusic,
    PauseMusic,
    ResumeMusic,
    SetMusicVolume,
};

struct MixCommand {
    MixCommandType type = MixCommandType::Stop;
    int slot = 0;
    std::uint32_t generation = 0;
    const SoundData* sound = nullptr;  // kept alive by the game thread until the voice ends
    MusicStream* music = nullptr;      // owned by the mixer until it hands it back
    float volume = 1.0f;
    float pitch = 0.0f;  // octaves
    float pan = 0.0f;
    bool looped = false;
};

// Mixes up to MixerVoices sounds and one music stream into interleaved stereo float at a
// fixed output rate, with linear resampling, pitch, pan and volume.
//
// Two threads: the game thread calls Submit, Ended, MusicEnded and TakeRetiredMusic; the
// audio thread calls Mix. They meet only through lock-free queues and atomics, and the
// audio thread never allocates or frees.
class Mixer {
public:
    explicit Mixer(int outputRate) : rate_(outputRate) {}

    // Game thread. False when the queue is full (the command is dropped).
    bool Submit(const MixCommand& command) { return commands_.Push(command); }
    // Game thread. The generation of the last voice on this slot to stop or finish.
    std::uint32_t Ended(int slot) const { return ended_[static_cast<size_t>(slot)].load(std::memory_order_acquire); }
    // Game thread. The generation of the last music stream to stop or finish.
    std::uint32_t MusicEnded() const { return musicEnded_.load(std::memory_order_acquire); }
    // Game thread. A stream the mixer is done with, to destroy; null when there is none.
    MusicStream* TakeRetiredMusic();

    // Audio thread. Applies queued commands, then writes frames stereo frames.
    void Mix(float* out, int frames);

    int OutputRate() const { return rate_; }

private:
    struct Voice {
        const SoundData* sound = nullptr;
        std::uint32_t generation = 0;
        double position = 0.0;
        float volume = 1.0f;
        float pitch = 0.0f;
        float pan = 0.0f;
        bool looped = false;
        bool paused = false;
    };

    void Apply(const MixCommand& c);
    void End(Voice& v, int slot);
    void RetireMusic();
    void MixVoice(Voice& v, int slot, float* out, int frames);
    void MixMusic(float* out, int frames);

    const int rate_;
    SpscQueue<MixCommand, 1024> commands_;
    SpscQueue<MusicStream*, 16> retired_;
    std::array<Voice, MixerVoices> voices_{};
    std::array<std::atomic<std::uint32_t>, MixerVoices> ended_{};
    float master_ = 1.0f;
    MusicStream* music_ = nullptr;
    bool musicPaused_ = false;
    float musicVolume_ = 1.0f;
    std::uint32_t musicGeneration_ = 0;
    std::atomic<std::uint32_t> musicEnded_{0};
};

} // namespace coldx::detail
