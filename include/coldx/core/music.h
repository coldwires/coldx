#pragma once

#include <memory>
#include <string>

namespace coldx {

namespace detail {
struct MusicData;
}

// Background music: an OGG Vorbis file kept compressed in memory and decoded as it plays,
// so a long track costs its file size, not minutes of samples. Play it with MediaPlayer.
// Loading works any time; copies share the data.
class Music {
public:
    Music();
    ~Music();

    // A relative path is taken from the executable's folder. Empty on failure, with the
    // reason logged.
    static Music FromFile(const std::string& path);

    float Duration() const;  // seconds
    bool IsEmpty() const;

private:
    friend class MediaPlayer;
    std::shared_ptr<const detail::MusicData> data_;
};

// Plays one Music at a time, alongside any sound effects.
class MediaPlayer {
public:
    // Replaces whatever is playing. False if Run has not started, audio is off, or the music
    // is empty.
    static bool Play(const Music& music, bool looped = true);
    static void Stop();
    static void Pause();
    static void Resume();
    // True from Play until Stop, or until an unlooped track ends. Still true while paused.
    static bool IsPlaying();

    // 0 to 1. Separate from SoundEffect::MasterVolume.
    static void SetVolume(float volume);
    static float Volume();
};

} // namespace coldx
