#include <coldx/core/sound_effect.h>

#include <SDL3/SDL.h>

#include "audio.h"
#include "content_path.h"

namespace coldx {

static_assert(SoundEffect::MaxVoices == detail::MixerVoices);

namespace {

float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

std::vector<unsigned char> ReadWhole(const std::string& path, bool& ok) {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    ok = data != nullptr;
    if (!data) {
        SDL_Log("coldx.core: could not load %s: %s", path.c_str(), SDL_GetError());
        return {};
    }
    std::vector<unsigned char> bytes(static_cast<const unsigned char*>(data), static_cast<const unsigned char*>(data) + size);
    SDL_free(data);
    return bytes;
}

// Sounds skipped for lack of a voice. A burst can skip hundreds in a frame; one log line
// each would cost more than the sounds, so they are summed and logged once a second.
void NoteSkipped() {
    static int skipped = 0;
    static Uint64 lastLog = 0;
    ++skipped;
    const Uint64 now = SDL_GetTicks();
    if (lastLog != 0 && now - lastLog < 1000) return;
    SDL_Log("coldx.core: %d sound(s) skipped: all %d voices are playing", skipped, SoundEffect::MaxVoices);
    skipped = 0;
    lastLog = now;
}

// Starts sound on a fresh slot with the given settings; the slot is held by the caller.
int Start(const std::shared_ptr<const detail::SoundData>& sound, float volume, float pitch, float pan, bool looped,
          std::uint32_t& generation) {
    detail::Audio& audio = detail::Audio::Get();
    if (!sound || !audio.Running()) return -1;
    const int slot = audio.Acquire(sound, generation);
    if (slot < 0) {
        NoteSkipped();
        return -1;
    }
    detail::MixCommand play;
    play.type = detail::MixCommandType::Play;
    play.slot = slot;
    play.generation = generation;
    play.sound = sound.get();
    play.volume = Clamp(volume, 0.0f, 1.0f);
    play.pitch = Clamp(pitch, -1.0f, 1.0f);
    play.pan = Clamp(pan, -1.0f, 1.0f);
    play.looped = looped;
    if (!audio.Submit(play)) {
        audio.Release(slot, generation);
        return -1;
    }
    return slot;
}

} // namespace

SoundEffect::SoundEffect() = default;
SoundEffect::~SoundEffect() = default;

SoundEffect SoundEffect::FromMemory(const void* data, std::size_t size, const std::string& name) {
    SoundEffect effect;
    if (!data) return effect;
    const auto* bytes = static_cast<const unsigned char*>(data);
    effect.data_ = detail::DecodeSound(std::vector<unsigned char>(bytes, bytes + size), name.c_str());
    return effect;
}

SoundEffect SoundEffect::FromFile(const std::string& path) {
    const std::string full = detail::ResolveContentPath(path);
    bool ok = false;
    const std::vector<unsigned char> bytes = ReadWhole(full, ok);
    SoundEffect effect;
    if (ok) effect.data_ = detail::DecodeSound(bytes, full.c_str());
    return effect;
}

SoundEffect SoundEffect::FromSamples(const float* samples, int frames, int channels, int sampleRate) {
    SoundEffect effect;
    if (!samples || frames <= 0 || (channels != 1 && channels != 2) || sampleRate <= 0) {
        SDL_Log("coldx.core: SoundEffect::FromSamples: need samples, frames > 0, 1 or 2 channels, a rate");
        return effect;
    }
    auto data = std::make_shared<detail::SoundData>();
    data->channels = channels;
    data->sampleRate = sampleRate;
    data->frames = frames;
    data->samples.assign(samples, samples + static_cast<size_t>(frames) * channels);
    effect.data_ = std::move(data);
    return effect;
}

bool SoundEffect::Play(float volume, float pitch, float pan) const {
    std::uint32_t generation = 0;
    const int slot = Start(data_, volume, pitch, pan, false, generation);
    if (slot < 0) return false;
    // Fire and forget: the slot frees itself once the voice ends.
    detail::Audio::Get().Release(slot, generation);
    return true;
}

SoundEffectInstance SoundEffect::CreateInstance() const {
    SoundEffectInstance instance;
    instance.data_ = data_;
    return instance;
}

float SoundEffect::Duration() const {
    return data_ ? static_cast<float>(data_->frames) / data_->sampleRate : 0.0f;
}

int SoundEffect::SampleRate() const {
    return data_ ? data_->sampleRate : 0;
}

int SoundEffect::Channels() const {
    return data_ ? data_->channels : 0;
}

bool SoundEffect::IsEmpty() const {
    return !data_;
}

void SoundEffect::SetMasterVolume(float volume) {
    detail::Audio& audio = detail::Audio::Get();
    audio.masterVolume = Clamp(volume, 0.0f, 1.0f);
    detail::MixCommand c;
    c.type = detail::MixCommandType::SetMasterVolume;
    c.volume = audio.masterVolume;
    audio.Submit(c);  // before Run, Start sends it
}

float SoundEffect::MasterVolume() {
    return detail::Audio::Get().masterVolume;
}

SoundEffectInstance::SoundEffectInstance() = default;

SoundEffectInstance::~SoundEffectInstance() {
    Release();
}

SoundEffectInstance::SoundEffectInstance(SoundEffectInstance&& other) noexcept
    : data_(std::move(other.data_)), slot_(other.slot_), generation_(other.generation_), state_(other.state_),
      volume_(other.volume_), pitch_(other.pitch_), pan_(other.pan_), looped_(other.looped_) {
    other.slot_ = -1;
    other.state_ = SoundState::Stopped;
}

SoundEffectInstance& SoundEffectInstance::operator=(SoundEffectInstance&& other) noexcept {
    if (this != &other) {
        Release();
        data_ = std::move(other.data_);
        slot_ = other.slot_;
        generation_ = other.generation_;
        state_ = other.state_;
        volume_ = other.volume_;
        pitch_ = other.pitch_;
        pan_ = other.pan_;
        looped_ = other.looped_;
        other.slot_ = -1;
        other.state_ = SoundState::Stopped;
    }
    return *this;
}

void SoundEffectInstance::Release() {
    if (slot_ >= 0) detail::Audio::Get().Release(slot_, generation_);
    slot_ = -1;
    state_ = SoundState::Stopped;
}

SoundState SoundEffectInstance::State() const {
    if (state_ != SoundState::Stopped && detail::Audio::Get().Ended(slot_, generation_)) return SoundState::Stopped;
    return state_;
}

bool SoundEffectInstance::Play() {
    const SoundState now = State();
    if (now == SoundState::Playing) return true;
    if (now == SoundState::Paused) {
        Resume();
        return true;
    }
    Release();
    std::uint32_t generation = 0;
    const int slot = Start(data_, volume_, pitch_, pan_, looped_, generation);
    if (slot < 0) return false;
    slot_ = slot;
    generation_ = generation;
    state_ = SoundState::Playing;
    return true;
}

static void Send(int slot, unsigned generation, detail::MixCommandType type, float value = 0.0f) {
    if (slot < 0) return;
    detail::MixCommand c;
    c.type = type;
    c.slot = slot;
    c.generation = generation;
    c.volume = c.pitch = c.pan = value;
    detail::Audio::Get().Submit(c);
}

void SoundEffectInstance::Pause() {
    if (State() != SoundState::Playing) return;
    Send(slot_, generation_, detail::MixCommandType::Pause);
    state_ = SoundState::Paused;
}

void SoundEffectInstance::Resume() {
    if (State() != SoundState::Paused) return;
    Send(slot_, generation_, detail::MixCommandType::Resume);
    state_ = SoundState::Playing;
}

void SoundEffectInstance::Stop() {
    Release();
}

void SoundEffectInstance::SetVolume(float volume) {
    volume_ = Clamp(volume, 0.0f, 1.0f);
    Send(slot_, generation_, detail::MixCommandType::SetVolume, volume_);
}

float SoundEffectInstance::Volume() const {
    return volume_;
}

void SoundEffectInstance::SetPitch(float pitch) {
    pitch_ = Clamp(pitch, -1.0f, 1.0f);
    Send(slot_, generation_, detail::MixCommandType::SetPitch, pitch_);
}

float SoundEffectInstance::Pitch() const {
    return pitch_;
}

void SoundEffectInstance::SetPan(float pan) {
    pan_ = Clamp(pan, -1.0f, 1.0f);
    Send(slot_, generation_, detail::MixCommandType::SetPan, pan_);
}

float SoundEffectInstance::Pan() const {
    return pan_;
}

void SoundEffectInstance::SetLooped(bool looped) {
    looped_ = looped;
}

bool SoundEffectInstance::IsLooped() const {
    return looped_;
}

} // namespace coldx
