#include "audio.h"

#include <algorithm>

#include <SDL3/SDL.h>

namespace coldx::detail {

Audio& Audio::Get() {
    static Audio audio;
    return audio;
}

// SDL's audio thread asks for more output. Mix it in chunks.
void Audio::Feed(void* self, SDL_AudioStream* stream, int additional, int /*total*/) {
    Audio& a = *static_cast<Audio*>(self);
    constexpr int ChunkFrames = static_cast<int>(std::tuple_size_v<decltype(chunk_)> / 2);
    int frames = additional / static_cast<int>(sizeof(float) * 2);
    while (frames > 0) {
        const int n = frames < ChunkFrames ? frames : ChunkFrames;
        a.mixer_->Mix(a.chunk_.data(), n);
        SDL_PutAudioStreamData(stream, a.chunk_.data(), n * static_cast<int>(sizeof(float) * 2));
        frames -= n;
    }
}

void Audio::Start(bool headless) {
    Stop();
    // Headless runs are tests and scripts. Mix in real time and play nothing aloud.
    if (headless && !SDL_GetHint(SDL_HINT_AUDIO_DRIVER)) SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("coldx.core: audio off: %s", SDL_GetError());
        return;
    }
    mixer_ = std::make_unique<Mixer>(OutputRate);
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, OutputRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, this);
    if (!stream_) {
        SDL_Log("coldx.core: audio off: could not open an output device: %s", SDL_GetError());
        mixer_.reset();
        return;
    }
    MixCommand master;
    master.type = MixCommandType::SetMasterVolume;
    master.volume = masterVolume;
    mixer_->Submit(master);
    MixCommand music;
    music.type = MixCommandType::SetMusicVolume;
    music.volume = musicVolume;
    mixer_->Submit(music);
    SDL_ResumeAudioStreamDevice(stream_);
    SDL_Log("coldx.core: audio driver %s, %d Hz", SDL_GetCurrentAudioDriver(), OutputRate);
}

void Audio::Stop() {
    // Destroying the stream waits out any running callback, so after this nothing else
    // reads the slots or streams.
    if (stream_) SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    mixer_.reset();
    for (Slot& s : slots_) s = Slot{};
    streams_.clear();
    musicStarted = false;
}

void Audio::Pump() {
    if (!mixer_) return;
    // A slot is free once its voice has ended, whether or not its holder let go. An
    // instance that finished keeps nothing busy. Ended() tells such a holder its voice is
    // gone even after the slot has been reused, because generations only go up.
    for (int i = 0; i < MixerVoices; ++i) {
        Slot& s = slots_[static_cast<size_t>(i)];
        if (s.inUse && mixer_->Ended(i) >= s.generation) {
            s.inUse = false;
            s.released = false;
            s.data.reset();
        }
    }
    while (MusicStream* done = mixer_->TakeRetiredMusic()) {
        std::erase_if(streams_, [done](const std::unique_ptr<MusicStream>& p) { return p.get() == done; });
    }
}

int Audio::Acquire(std::shared_ptr<const SoundData> data, std::uint32_t& generation) {
    if (!mixer_) return -1;
    Pump();
    for (int i = 0; i < MixerVoices; ++i) {
        Slot& s = slots_[static_cast<size_t>(i)];
        if (s.inUse) continue;
        s.inUse = true;
        s.released = false;
        s.data = std::move(data);
        generation = ++s.generation;
        return i;
    }
    return -1;
}

void Audio::Release(int slot, std::uint32_t generation) {
    if (!mixer_ || slot < 0 || slot >= MixerVoices) return;
    Slot& s = slots_[static_cast<size_t>(slot)];
    if (!s.inUse || s.generation != generation) return;
    MixCommand stop;
    stop.type = MixCommandType::Stop;
    stop.slot = slot;
    stop.generation = generation;
    if (mixer_->Ended(slot) >= generation) {
        s.inUse = false;
        s.data.reset();
    } else if (mixer_->Submit(stop)) {
        s.released = true;
    } else {
        // The mixer cannot be told to stop. Keep the data until the voice ends on its own.
        s.released = true;
    }
}

bool Audio::Ended(int slot, std::uint32_t generation) const {
    return !mixer_ || slot < 0 || mixer_->Ended(slot) >= generation;
}

bool Audio::Submit(const MixCommand& command) {
    if (!mixer_) return false;
    if (mixer_->Submit(command)) return true;
    if (!warnedFull_) SDL_Log("coldx.core: audio command queue full, commands dropped");
    warnedFull_ = true;
    return false;
}

bool Audio::PlayMusic(std::shared_ptr<const MusicData> data, bool looped, float fadeSeconds) {
    if (!mixer_ || !data) return false;
    Pump();
    std::unique_ptr<MusicStream> stream = MusicStream::Open(std::move(data));
    if (!stream) return false;
    MixCommand play;
    play.type = MixCommandType::PlayMusic;
    play.music = stream.get();
    play.looped = looped;
    play.generation = ++musicGeneration;
    play.fade = fadeSeconds > 0.0f ? fadeSeconds : 0.0f;
    if (!Submit(play)) return false;
    streams_.push_back(std::move(stream));
    musicStarted = true;
    return true;
}

bool Audio::MusicActive() const {
    return mixer_ && musicStarted && mixer_->MusicEnded() != musicGeneration;
}

void Audio::StopMusic(float fadeSeconds) {
    MixCommand stop;
    stop.type = MixCommandType::StopMusic;
    stop.fade = fadeSeconds > 0.0f ? fadeSeconds : 0.0f;
    Submit(stop);
    musicStarted = false;
}

} // namespace coldx::detail
