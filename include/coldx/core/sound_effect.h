#pragma once

#include <memory>
#include <string>

namespace coldx {

class SoundEffectInstance;

namespace detail {
struct SoundData;
}

enum class SoundState {
    Stopped,
    Playing,
    Paused,
};

// A sound decoded into memory: short effects that can play many times at once. Load from
// WAV or OGG, or build from samples. Loading works any time, before Run included; playing
// needs Run started. Copies share the decoded samples.
class SoundEffect {
public:
    SoundEffect();
    ~SoundEffect();

    // A relative path is taken from the executable's folder. Empty on failure, with the
    // reason logged.
    static SoundEffect FromFile(const std::string& path);
    // Interleaved samples, -1 to 1. channels is 1 or 2.
    static SoundEffect FromSamples(const float* samples, int frames, int channels, int sampleRate);

    // Plays once and forgets it. volume 0 to 1; pitch -1 to 1 is an octave down to an
    // octave up; pan -1 (left) to 1 (right). False if Run has not started, audio is off, or
    // all voices are busy.
    bool Play(float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f) const;

    // An instance to control one playback: pause, loop, change volume while playing.
    SoundEffectInstance CreateInstance() const;

    float Duration() const;  // seconds
    int SampleRate() const;
    int Channels() const;
    bool IsEmpty() const;

    // Scales every sound effect; music has its own volume in MediaPlayer.
    static void SetMasterVolume(float volume);
    static float MasterVolume();

    // How many sounds can play at once, effects and instances together. Beyond it Play
    // returns false; the skips are counted and logged at most once a second.
    static constexpr int MaxVoices = 256;

private:
    friend class SoundEffectInstance;
    std::shared_ptr<const detail::SoundData> data_;
};

// One controllable playback of a SoundEffect. Move-only; destroying it stops it.
class SoundEffectInstance {
public:
    SoundEffectInstance();
    ~SoundEffectInstance();
    SoundEffectInstance(SoundEffectInstance&& other) noexcept;
    SoundEffectInstance& operator=(SoundEffectInstance&& other) noexcept;
    SoundEffectInstance(const SoundEffectInstance&) = delete;
    SoundEffectInstance& operator=(const SoundEffectInstance&) = delete;

    // Starts from the beginning when stopped, resumes when paused. False if it cannot play
    // (no sound, Run not started, audio off, all voices busy).
    bool Play();
    void Pause();
    void Resume();
    void Stop();
    SoundState State() const;

    // Take effect immediately, also while playing. Ranges as for SoundEffect::Play.
    void SetVolume(float volume);
    float Volume() const;
    void SetPitch(float pitch);
    float Pitch() const;
    void SetPan(float pan);
    float Pan() const;
    // Set before Play: a looping instance plays until stopped.
    void SetLooped(bool looped);
    bool IsLooped() const;

private:
    friend class SoundEffect;
    std::shared_ptr<const detail::SoundData> data_;
    int slot_ = -1;
    unsigned generation_ = 0;
    SoundState state_ = SoundState::Stopped;
    float volume_ = 1.0f;
    float pitch_ = 0.0f;
    float pan_ = 0.0f;
    bool looped_ = false;

    void Release();
};

} // namespace coldx
