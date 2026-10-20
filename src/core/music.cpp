#include <coldx/core/music.h>

#include <SDL3/SDL.h>

#include "audio.h"
#include "content_path.h"

namespace coldx {

Music::Music() = default;
Music::~Music() = default;

Music Music::FromFile(const std::string& path) {
    const std::string full = detail::ResolveContentPath(path);
    size_t size = 0;
    void* data = SDL_LoadFile(full.c_str(), &size);
    Music music;
    if (!data) {
        SDL_Log("coldx.core: could not load %s: %s", full.c_str(), SDL_GetError());
        return music;
    }
    std::vector<unsigned char> bytes(static_cast<const unsigned char*>(data), static_cast<const unsigned char*>(data) + size);
    SDL_free(data);
    music.data_ = detail::LoadMusic(std::move(bytes), full.c_str());
    return music;
}

float Music::Duration() const {
    return data_ ? data_->duration : 0.0f;
}

bool Music::IsEmpty() const {
    return !data_;
}

bool MediaPlayer::Play(const Music& music, bool looped) {
    return detail::Audio::Get().PlayMusic(music.data_, looped);
}

void MediaPlayer::Stop() {
    detail::Audio::Get().StopMusic();
}

static void SendMusic(detail::MixCommandType type, float volume = 0.0f) {
    detail::MixCommand c;
    c.type = type;
    c.volume = volume;
    detail::Audio::Get().Submit(c);
}

void MediaPlayer::Pause() {
    SendMusic(detail::MixCommandType::PauseMusic);
}

void MediaPlayer::Resume() {
    SendMusic(detail::MixCommandType::ResumeMusic);
}

bool MediaPlayer::IsPlaying() {
    return detail::Audio::Get().MusicActive();
}

void MediaPlayer::SetVolume(float volume) {
    detail::Audio& audio = detail::Audio::Get();
    audio.musicVolume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    SendMusic(detail::MixCommandType::SetMusicVolume, audio.musicVolume);  // before Run, Start sends it
}

float MediaPlayer::Volume() {
    return detail::Audio::Get().musicVolume;
}

} // namespace coldx
