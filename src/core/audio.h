#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "audio_data.h"
#include "mixer.h"

struct SDL_AudioStream;

namespace coldx::detail {

// The game-thread side of audio: the output device, which voice slots are in use, and
// the rule that nothing the audio thread may still be reading is freed. A slot's sound
// data is released only after the mixer reports that slot's voice ended.
class Audio {
public:
    static constexpr int OutputRate = 48000;

    static Audio& Get();

    // Run start and end. Start opens the default output. If that fails, audio is off for
    // the run and every play call returns false.
    void Start(bool headless);
    void Stop();
    bool Running() const { return stream_ != nullptr; }

    // Once per frame: frees slots whose voices have ended and music the mixer handed back.
    void Pump();

    // A free slot for data, or -1. The slot is held until Release.
    int Acquire(std::shared_ptr<const SoundData> data, std::uint32_t& generation);
    // Lets the slot go, stopping its voice. It is reused once the voice has ended. A slot
    // whose voice ended on its own is reused even if never released.
    void Release(int slot, std::uint32_t generation);
    // True once that generation's voice has ended, including after the slot is reused.
    bool Ended(int slot, std::uint32_t generation) const;
    // Sends a command. On failure (queue full, audio off) logs once and returns false.
    bool Submit(const MixCommand& command);

    bool PlayMusic(std::shared_ptr<const MusicData> data, bool looped, float fadeSeconds = 0.0f);
    bool MusicActive() const;
    void StopMusic(float fadeSeconds = 0.0f);

    float masterVolume = 1.0f;
    float musicVolume = 1.0f;
    std::uint32_t musicGeneration = 0;
    bool musicStarted = false;

private:
    struct Slot {
        std::shared_ptr<const SoundData> data;
        std::uint32_t generation = 0;
        bool inUse = false;
        bool released = false;
    };

    static void Feed(void* self, SDL_AudioStream* stream, int additional, int total);

    std::unique_ptr<Mixer> mixer_;
    SDL_AudioStream* stream_ = nullptr;
    std::array<Slot, MixerVoices> slots_{};
    std::vector<std::unique_ptr<MusicStream>> streams_;
    std::array<float, 2 * 1024> chunk_{};
    bool warnedFull_ = false;
};

} // namespace coldx::detail
