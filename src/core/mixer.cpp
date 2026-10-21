#include "mixer.h"

#include <cmath>
#include <cstring>

namespace coldx::detail {

namespace {

// Balance: centre is full volume on both sides; panning fades the far side only.
void PanGains(float pan, float& left, float& right) {
    left = pan > 0.0f ? 1.0f - pan : 1.0f;
    right = pan < 0.0f ? 1.0f + pan : 1.0f;
}

float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

MusicStream* Mixer::TakeRetiredMusic() {
    MusicStream* stream = nullptr;
    return retired_.Pop(stream) ? stream : nullptr;
}

void Mixer::End(Voice& v, int slot) {
    v.sound = nullptr;
    ended_[static_cast<size_t>(slot)].store(v.generation, std::memory_order_release);
}

void Mixer::RetireMusic() {
    // If the return queue is somehow full, the stream leaks rather than being freed here:
    // the audio thread must not free.
    if (music_) {
        retired_.Push(music_);
        musicEnded_.store(musicGeneration_, std::memory_order_release);
    }
    music_ = nullptr;
    musicPaused_ = false;
}

void Mixer::Apply(const MixCommand& c) {
    Voice* v = c.slot >= 0 && c.slot < MixerVoices ? &voices_[static_cast<size_t>(c.slot)] : nullptr;
    // Commands for a voice that has since been replaced are stale; drop them.
    const bool current = v && v->sound && v->generation == c.generation;
    switch (c.type) {
    case MixCommandType::Play:
        if (!v) break;
        if (v->sound) End(*v, c.slot);
        *v = Voice{c.sound, c.generation, 0.0, c.volume, c.pitch, c.pan, 0.0f, 0.0f, c.looped, false};
        // A new sound starts at its gain; only changes after that ramp.
        PanGains(c.pan, v->gainLeft, v->gainRight);
        v->gainLeft *= c.volume * master_;
        v->gainRight *= c.volume * master_;
        break;
    case MixCommandType::Stop:
        if (current) End(*v, c.slot);
        break;
    case MixCommandType::Pause:
        if (current) v->paused = true;
        break;
    case MixCommandType::Resume:
        if (current) v->paused = false;
        break;
    case MixCommandType::SetVolume:
        if (current) v->volume = c.volume;
        break;
    case MixCommandType::SetPitch:
        if (current) v->pitch = c.pitch;
        break;
    case MixCommandType::SetPan:
        if (current) v->pan = c.pan;
        break;
    case MixCommandType::SetMasterVolume:
        master_ = c.volume;
        break;
    case MixCommandType::PlayMusic:
        RetireMusic();
        music_ = c.music;
        musicGeneration_ = c.generation;
        musicGain_ = musicVolume_;
        if (music_) {
            music_->looped = c.looped;
            music_->bufferCount = 0;
            music_->position = 0.0;
        }
        break;
    case MixCommandType::StopMusic:
        RetireMusic();
        break;
    case MixCommandType::PauseMusic:
        musicPaused_ = true;
        break;
    case MixCommandType::ResumeMusic:
        musicPaused_ = false;
        break;
    case MixCommandType::SetMusicVolume:
        musicVolume_ = c.volume;
        break;
    }
}

// Linear interpolation between neighbouring frames, stepping by the source/output rate
// ratio times the pitch. A looping voice interpolates across the loop point. The gains
// move linearly from the last block's to this one's.
void Mixer::MixVoice(Voice& v, int slot, float* out, int frames) {
    const SoundData& s = *v.sound;
    const double step = static_cast<double>(s.sampleRate) / rate_ * std::exp2(static_cast<double>(v.pitch));
    float left = 0.0f, right = 0.0f;
    PanGains(v.pan, left, right);
    left *= v.volume * master_;
    right *= v.volume * master_;
    const float startLeft = v.gainLeft, startRight = v.gainRight;
    const float stepLeft = (left - startLeft) / static_cast<float>(frames);
    const float stepRight = (right - startRight) / static_cast<float>(frames);
    v.gainLeft = left;
    v.gainRight = right;
    const float* data = s.samples.data();
    const int channels = s.channels;

    for (int i = 0; i < frames; ++i) {
        if (v.position >= static_cast<double>(s.frames)) {
            if (!v.looped || s.frames == 0) {
                End(v, slot);
                return;
            }
            v.position = std::fmod(v.position, static_cast<double>(s.frames));
        }
        const auto index = static_cast<std::int64_t>(v.position);
        const float frac = static_cast<float>(v.position - static_cast<double>(index));
        std::int64_t next = index + 1;
        if (next >= s.frames) next = v.looped ? 0 : index;
        const float* a = data + index * channels;
        const float* b = data + next * channels;
        float l = a[0] + (b[0] - a[0]) * frac;
        float r = l;
        if (channels == 2) r = a[1] + (b[1] - a[1]) * frac;
        const float k = static_cast<float>(i + 1);
        out[i * 2] += l * (startLeft + stepLeft * k);
        out[i * 2 + 1] += r * (startRight + stepRight * k);
        v.position += step;
    }
}

// Music decodes into its buffer as it goes. The last frame of each chunk is carried to the
// front of the next, so interpolation never has a gap at a chunk boundary.
void Mixer::MixMusic(float* out, int frames) {
    MusicStream& m = *music_;
    const int channels = m.Channels();
    const double step = static_cast<double>(m.SampleRate()) / rate_;
    const float startGain = musicGain_, gainStep = (musicVolume_ - musicGain_) / static_cast<float>(frames);
    musicGain_ = musicVolume_;

    auto refill = [&]() -> bool {
        int carried = 0;
        if (m.bufferCount > 0) {
            std::memcpy(m.buffer.data(), m.buffer.data() + (m.bufferCount - 1) * channels, sizeof(float) * channels);
            m.position -= m.bufferCount - 1;
            carried = 1;
        }
        int read = m.Read(m.buffer.data() + carried * channels, MusicStream::BufferFrames);
        if (read == 0 && m.looped) {
            m.Rewind();
            read = m.Read(m.buffer.data() + carried * channels, MusicStream::BufferFrames);
        }
        if (read == 0) return false;
        m.bufferCount = carried + read;
        return true;
    };

    for (int i = 0; i < frames; ++i) {
        while (static_cast<int>(m.position) + 1 >= m.bufferCount) {
            if (!refill()) {
                RetireMusic();
                return;
            }
        }
        const auto index = static_cast<int>(m.position);
        const float frac = static_cast<float>(m.position - index);
        const float* a = m.buffer.data() + index * channels;
        const float* b = a + channels;
        const float l = a[0] + (b[0] - a[0]) * frac;
        const float r = channels == 2 ? a[1] + (b[1] - a[1]) * frac : l;
        const float gain = startGain + gainStep * static_cast<float>(i + 1);
        out[i * 2] += l * gain;
        out[i * 2 + 1] += r * gain;
        m.position += step;
    }
}

void Mixer::Mix(float* out, int frames) {
    MixCommand command;
    while (commands_.Pop(command)) Apply(command);

    std::memset(out, 0, sizeof(float) * 2 * static_cast<size_t>(frames));
    for (int slot = 0; slot < MixerVoices; ++slot) {
        Voice& v = voices_[static_cast<size_t>(slot)];
        if (v.sound && !v.paused) MixVoice(v, slot, out, frames);
    }
    if (music_ && !musicPaused_) MixMusic(out, frames);
    for (int i = 0; i < frames * 2; ++i) out[i] = Clamp(out[i], -1.0f, 1.0f);
}

} // namespace coldx::detail
