#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
    // An OGG file already in memory; the Music keeps the bytes. name is for log lines.
    static Music FromMemory(std::vector<std::uint8_t> ogg, const std::string& name = "memory");

    float Duration() const;  // seconds
    bool IsEmpty() const;

private:
    friend class MediaPlayer;
    std::shared_ptr<const detail::MusicData> data_;
};

// Plays one Music at a time, alongside any sound effects; during a crossfade the old track
// fades out under the new one.
class MediaPlayer {
public:
    // Replaces whatever is playing. False if Run has not started, audio is off, or the music
    // is empty.
    static bool Play(const Music& music, bool looped = true);
    // The same, crossfading: the playing track fades out over fadeSeconds while this one
    // fades in. With nothing playing, it just starts.
    static bool CrossFade(const Music& music, float fadeSeconds, bool looped = true);
    static void Stop();
    // Fades the track out over fadeSeconds, then stops it. IsPlaying is false at once.
    static void FadeOut(float fadeSeconds);
    static void Pause();
    static void Resume();
    // True from Play until Stop, or until an unlooped track ends. Still true while paused.
    static bool IsPlaying();

    // 0 to 1. Separate from SoundEffect::MasterVolume.
    static void SetVolume(float volume);
    static float Volume();
};

} // namespace coldx
