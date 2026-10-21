// Audio: the mixer sample by sample, decoding, and the public classes end to end.
//
// The mixer is tested directly, with no device: every expected sample is computed here from
// the input and the documented rules (linear interpolation, balance pan, clamp). The end-to-end
// part runs a headless game, which uses SDL's dummy audio driver: silent, but it pulls audio
// in real time, so voices and music finish as they would aloud.

#include <coldx/coldx.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <vector>

#include <SDL3/SDL.h>

#include "audio_data.h"
#include "check.h"
#include "mixer.h"

using namespace coldx;
using namespace coldx::detail;
using coldx_test::CheckResult;

namespace {

constexpr int Rate = 48000;

bool Near(float a, float b, float tolerance = 1e-5f) {
    return std::fabs(a - b) <= tolerance;
}

SoundData Ramp(int frames, int channels, int sampleRate) {
    SoundData s;
    s.channels = channels;
    s.sampleRate = sampleRate;
    s.frames = frames;
    for (int i = 0; i < frames; ++i)
        for (int c = 0; c < channels; ++c) s.samples.push_back((c == 0 ? 0.01f : -0.01f) * static_cast<float>(i + 1));
    return s;
}

MixCommand Play(int slot, std::uint32_t generation, const SoundData& sound, float volume = 1.0f, float pitch = 0.0f,
                float pan = 0.0f, bool looped = false) {
    MixCommand c;
    c.type = MixCommandType::Play;
    c.slot = slot;
    c.generation = generation;
    c.sound = &sound;
    c.volume = volume;
    c.pitch = pitch;
    c.pan = pan;
    c.looped = looped;
    return c;
}

MixCommand Command(MixCommandType type, int slot = 0, std::uint32_t generation = 0, float value = 0.0f) {
    MixCommand c;
    c.type = type;
    c.slot = slot;
    c.generation = generation;
    c.volume = c.pitch = c.pan = value;
    return c;
}

std::vector<float> Mix(Mixer& mixer, int frames) {
    std::vector<float> out(static_cast<size_t>(frames) * 2);
    mixer.Mix(out.data(), frames);
    return out;
}

void Basics() {
    const SoundData mono = Ramp(10, 1, Rate);
    const SoundData stereo = Ramp(10, 2, Rate);

    {   // At the output rate, a mono sound is copied to both sides unchanged.
        Mixer m(Rate);
        m.Submit(Play(0, 1, mono));
        const auto out = Mix(m, 10);
        for (int i = 0; i < 10; ++i) CHECK(Near(out[i * 2], mono.samples[i]) && Near(out[i * 2 + 1], mono.samples[i]));
    }
    {   // Volume scales; pan fades only the far side.
        Mixer m(Rate);
        m.Submit(Play(0, 1, mono, 0.5f, 0.0f, 1.0f));
        m.Submit(Play(1, 1, stereo, 1.0f, 0.0f, -0.5f));
        const auto out = Mix(m, 10);
        for (int i = 0; i < 10; ++i) {
            CHECK(Near(out[i * 2], 0.0f + stereo.samples[i * 2]));
            CHECK(Near(out[i * 2 + 1], 0.5f * mono.samples[i] + 0.5f * stereo.samples[i * 2 + 1]));
        }
    }
    {   // Half the rate: every other output frame falls halfway between two source frames.
        const SoundData slow = Ramp(10, 1, Rate / 2);
        Mixer m(Rate);
        m.Submit(Play(0, 1, slow));
        const auto out = Mix(m, 18);
        for (int k = 0; k < 9; ++k) {
            CHECK(Near(out[(2 * k) * 2], slow.samples[k]));
            CHECK(Near(out[(2 * k + 1) * 2], (slow.samples[k] + slow.samples[k + 1]) / 2));
        }
    }
    {   // Pitch +1 is an octave up: twice the speed, every other frame.
        Mixer m(Rate);
        m.Submit(Play(0, 1, mono, 1.0f, 1.0f));
        const auto out = Mix(m, 5);
        for (int i = 0; i < 5; ++i) CHECK(Near(out[i * 2], mono.samples[i * 2]));
        CHECK(m.Ended(0) == 0);
        Mix(m, 1);
        CHECK(m.Ended(0) == 1);
    }
}

void Lifetimes() {
    const SoundData mono = Ramp(10, 1, Rate);
    {   // A sound ends: silence after it, and its slot reports the generation that ended.
        Mixer m(Rate);
        m.Submit(Play(3, 7, mono));
        const auto out = Mix(m, 20);
        CHECK(Near(out[9 * 2], mono.samples[9]));
        for (int i = 10; i < 20; ++i) CHECK(out[i * 2] == 0.0f && out[i * 2 + 1] == 0.0f);
        CHECK(m.Ended(3) == 7);
    }
    {   // Looping wraps without a gap.
        Mixer m(Rate);
        m.Submit(Play(0, 1, mono, 1.0f, 0.0f, 0.0f, true));
        const auto out = Mix(m, 35);
        for (int i = 0; i < 35; ++i) CHECK(Near(out[i * 2], mono.samples[i % 10]));
        CHECK(m.Ended(0) == 0);
    }
    {   // Between the last frame and the first, a loop interpolates across the seam: at half
        // rate, position 9.5 is halfway from the last sample back to the first.
        const SoundData slow = Ramp(10, 1, Rate / 2);
        Mixer m(Rate);
        m.Submit(Play(0, 1, slow, 1.0f, 0.0f, 0.0f, true));
        const auto out = Mix(m, 22);
        CHECK(Near(out[19 * 2], (slow.samples[9] + slow.samples[0]) / 2));
        CHECK(Near(out[20 * 2], slow.samples[0]));
    }
    {   // Pause holds the position; resume carries on from it. Stop ends it.
        Mixer m(Rate);
        m.Submit(Play(0, 1, mono));
        Mix(m, 3);
        m.Submit(Command(MixCommandType::Pause, 0, 1));
        auto out = Mix(m, 4);
        for (int i = 0; i < 4; ++i) CHECK(out[i * 2] == 0.0f);
        m.Submit(Command(MixCommandType::Resume, 0, 1));
        out = Mix(m, 2);
        CHECK(Near(out[0], mono.samples[3]) && Near(out[2], mono.samples[4]));
        m.Submit(Command(MixCommandType::Stop, 0, 1));
        out = Mix(m, 2);
        CHECK(out[0] == 0.0f && m.Ended(0) == 1);
    }
    {   // Commands for an older generation of a slot are ignored.
        Mixer m(Rate);
        m.Submit(Play(0, 1, mono));
        m.Submit(Play(0, 2, mono));
        m.Submit(Command(MixCommandType::Stop, 0, 1));
        m.Submit(Command(MixCommandType::SetVolume, 0, 1, 0.0f));
        const auto out = Mix(m, 2);
        CHECK(Near(out[0], mono.samples[0]));
        CHECK(m.Ended(0) == 1);  // generation 1 ended when 2 replaced it
    }
    {   // Master volume, and the clamp when voices add past full scale.
        SoundData loud;
        loud.channels = 1;
        loud.sampleRate = Rate;
        loud.frames = 4;
        loud.samples = {0.8f, 0.8f, -0.8f, 0.1f};
        Mixer m(Rate);
        m.Submit(Play(0, 1, loud));
        m.Submit(Play(1, 1, loud));
        auto out = Mix(m, 4);
        CHECK(out[0] == 1.0f && out[4] == -1.0f && Near(out[6], 0.2f));
        m.Submit(Command(MixCommandType::SetMasterVolume, 0, 0, 0.25f));
        m.Submit(Play(0, 2, loud));
        out = Mix(m, 1);
        CHECK(Near(out[0], 0.2f));
    }
}

// Volume and pan changes ramp across the next block instead of jumping; a new sound starts
// at its own gain.
void Ramping() {
    SoundData flat;
    flat.channels = 1;
    flat.sampleRate = Rate;
    flat.frames = 4;
    flat.samples = {0.5f, 0.5f, 0.5f, 0.5f};
    {   // Volume 1 to 0 over a 100-frame block: down by 1/100 each frame, silent after.
        Mixer m(Rate);
        m.Submit(Play(0, 1, flat, 1.0f, 0.0f, 0.0f, true));
        auto out = Mix(m, 10);
        CHECK(Near(out[0], 0.5f) && Near(out[18], 0.5f));
        m.Submit(Command(MixCommandType::SetVolume, 0, 1, 0.0f));
        out = Mix(m, 100);
        bool ramped = true;
        for (int i = 0; i < 100; ++i) ramped = ramped && Near(out[i * 2], 0.5f * (1.0f - (i + 1) / 100.0f));
        CHECK(ramped);
        out = Mix(m, 10);
        CHECK(out[0] == 0.0f && out[18] == 0.0f);
    }
    {   // Pan to the right: the left side fades over the block, the right stays.
        Mixer m(Rate);
        m.Submit(Play(0, 1, flat, 1.0f, 0.0f, 0.0f, true));
        Mix(m, 4);
        m.Submit(Command(MixCommandType::SetPan, 0, 1, 1.0f));
        const auto out = Mix(m, 4);
        CHECK(Near(out[0], 0.375f) && Near(out[2], 0.25f) && Near(out[6], 0.0f));
        CHECK(Near(out[1], 0.5f) && Near(out[7], 0.5f));
    }
    {   // Master volume ramps too.
        Mixer m(Rate);
        m.Submit(Play(0, 1, flat, 1.0f, 0.0f, 0.0f, true));
        Mix(m, 4);
        m.Submit(Command(MixCommandType::SetMasterVolume, 0, 0, 0.0f));
        const auto out = Mix(m, 2);
        CHECK(Near(out[0], 0.25f) && Near(out[2], 0.0f));
    }
}

std::vector<unsigned char> ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), {});
}

void Decoding() {
    {   // A 16-bit WAV written here: dr_wav reads s16 as value / 32768.
        const std::int16_t pcm[6] = {0, 16384, -16384, 32767, -32768, 100};
        std::vector<unsigned char> wav;
        auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) wav.push_back(static_cast<unsigned char>(v >> (8 * i))); };
        auto u16 = [&](std::uint16_t v) { wav.push_back(static_cast<unsigned char>(v)); wav.push_back(static_cast<unsigned char>(v >> 8)); };
        auto tag = [&](const char* t) { wav.insert(wav.end(), t, t + 4); };
        tag("RIFF"); u32(36 + sizeof(pcm)); tag("WAVE");
        tag("fmt "); u32(16); u16(1); u16(2); u32(22050); u32(22050 * 4); u16(4); u16(16);
        tag("data"); u32(sizeof(pcm));
        const auto* bytes = reinterpret_cast<const unsigned char*>(pcm);
        wav.insert(wav.end(), bytes, bytes + sizeof(pcm));
        const auto sound = DecodeSound(wav, "test.wav");
        CHECK(sound != nullptr);
        if (sound) {
            CHECK(sound->channels == 2 && sound->sampleRate == 22050 && sound->frames == 3);
            for (int i = 0; i < 6; ++i) CHECK(Near(sound->samples[i], pcm[i] / 32768.0f));
        }
        CHECK(DecodeSound(std::vector<unsigned char>{'n', 'o', 'p', 'e'}, "nope") == nullptr);
    }
    {   // The 441 Hz tone, 0.5 s at 44.1 kHz: lossy, so check length and pitch, not samples.
        const auto sound = DecodeSound(ReadFile(COLDX_TEST_DATA "/tone.ogg"), "tone.ogg");
        CHECK(sound != nullptr);
        if (sound) {
            CHECK(sound->channels == 1 && sound->sampleRate == 44100);
            CHECK(std::abs(sound->frames - 22050) < 100);
            int crossings = 0;
            for (std::int64_t i = 1; i < sound->frames; ++i)
                if ((sound->samples[i - 1] < 0.0f) != (sound->samples[i] < 0.0f)) ++crossings;
            CHECK(std::abs(crossings - 441) <= 4);  // two crossings per cycle, 220.5 cycles
        }
    }
    {   // Stereo, 441 Hz left and 882 Hz right: decoded whole, it has every frame a stream
        // read gives, and the channels stay apart.
        const std::vector<unsigned char> bytes = ReadFile(COLDX_TEST_DATA "/stereo.ogg");
        const auto sound = DecodeSound(bytes, "stereo.ogg");
        const auto data = LoadMusic(bytes, "stereo.ogg");
        CHECK(sound && data);
        if (sound && data) {
            CHECK(sound->channels == 2 && sound->sampleRate == 44100);
            auto stream = MusicStream::Open(data);
            std::vector<float> chunk(2 * 1000);
            std::int64_t streamed = 0;
            for (int n; (n = stream->Read(chunk.data(), 1000)) > 0;) streamed += n;
            CHECK(sound->frames == streamed);
            int left = 0, right = 0;
            for (std::int64_t i = 1; i < sound->frames; ++i) {
                left += (sound->samples[(i - 1) * 2] < 0.0f) != (sound->samples[i * 2] < 0.0f) ? 1 : 0;
                right += (sound->samples[(i - 1) * 2 + 1] < 0.0f) != (sound->samples[i * 2 + 1] < 0.0f) ? 1 : 0;
            }
            CHECK(std::abs(left - 441) <= 4 && std::abs(right - 882) <= 8);
        }
    }
}

// Music through the mixer must equal the fully decoded track, resampled by the same rule,
// across several of the stream's 1024-frame chunks.
void MusicMixing() {
    const std::vector<unsigned char> bytes = ReadFile(COLDX_TEST_DATA "/tone.ogg");
    const auto decoded = DecodeSound(bytes, "tone.ogg");
    const auto data = LoadMusic(bytes, "tone.ogg");
    CHECK(decoded && data);
    if (!decoded || !data) return;
    CHECK(Near(data->duration, 0.5f, 0.01f));

    auto stream = MusicStream::Open(data);
    Mixer m(Rate);
    MixCommand play;
    play.type = MixCommandType::PlayMusic;
    play.music = stream.get();
    play.generation = 1;
    m.Submit(play);
    const int frames = 5000;
    const auto out = Mix(m, frames);
    const double step = 44100.0 / Rate;
    bool all = true;
    for (int i = 0; i < frames && all; ++i) {
        const double pos = i * step;
        const auto k = static_cast<size_t>(pos);
        const float f = static_cast<float>(pos - k);
        const float expected = decoded->samples[k] + (decoded->samples[k + 1] - decoded->samples[k]) * f;
        all = Near(out[i * 2], expected, 1e-4f) && Near(out[i * 2 + 1], expected, 1e-4f);
        if (!all) std::printf("music frame %d: got %f expected %f\n", i, out[i * 2], expected);
    }
    CHECK(all);
    CHECK(m.MusicEnded() == 0);

    // Unlooped, it ends and is handed back; the game thread frees it.
    Mix(m, Rate);
    CHECK(m.MusicEnded() == 1);
    CHECK(m.TakeRetiredMusic() == stream.get());
}

// Crossfades and fade-outs: the old track fades under the new one and is handed back when
// its fade ends; after the fade the output is the new track alone, sample for sample.
void MusicFades() {
    const std::vector<unsigned char> toneBytes = ReadFile(COLDX_TEST_DATA "/tone.ogg");
    const std::vector<unsigned char> stereoBytes = ReadFile(COLDX_TEST_DATA "/stereo.ogg");
    const auto tone = LoadMusic(toneBytes, "tone.ogg");
    const auto stereo = LoadMusic(stereoBytes, "stereo.ogg");
    CHECK(tone && stereo);
    if (!tone || !stereo) return;
    auto play = [](MusicStream* stream, std::uint32_t generation, float fade) {
        MixCommand c;
        c.type = MixCommandType::PlayMusic;
        c.music = stream;
        c.generation = generation;
        c.looped = true;
        c.fade = fade;
        return c;
    };
    auto mixBlocks = [](Mixer& m, int frames) {  // in 480-frame blocks, as a device would pull
        std::vector<float> out;
        for (int done = 0; done < frames; done += 480) {
            const auto block = Mix(m, 480);
            out.insert(out.end(), block.begin(), block.end());
        }
        return out;
    };
    {   // Crossfade over 0.1 s (4800 frames).
        auto a = MusicStream::Open(tone), b = MusicStream::Open(stereo), reference = MusicStream::Open(stereo);
        Mixer m(Rate), alone(Rate);
        m.Submit(play(a.get(), 1, 0.0f));
        mixBlocks(m, 4800);
        m.Submit(play(b.get(), 2, 0.1f));
        alone.Submit(play(reference.get(), 1, 0.0f));
        mixBlocks(m, 4800);
        CHECK(m.MusicEnded() == 1);  // the old track counts as ended at once
        const auto after = mixBlocks(m, 4800);
        const auto expected = mixBlocks(alone, 9600);
        bool same = true;
        for (size_t i = 0; i < after.size() && same; ++i) same = Near(after[i], expected[4800 * 2 + i], 1e-5f);
        CHECK(same);
        CHECK(m.TakeRetiredMusic() == a.get());
        CHECK(m.TakeRetiredMusic() == nullptr);
    }
    {   // The curve itself: a fade-out over two 480-frame blocks is cos(pi/4) = 0.707 after the
        // first and 0 after the second, linear within each block.
        auto a = MusicStream::Open(stereo), reference = MusicStream::Open(stereo);
        Mixer m(Rate), alone(Rate);
        m.Submit(play(a.get(), 1, 0.0f));
        alone.Submit(play(reference.get(), 1, 0.0f));
        Mix(m, 480);
        MixCommand stop;
        stop.type = MixCommandType::StopMusic;
        stop.fade = 960.0f / Rate;
        m.Submit(stop);
        const auto faded = mixBlocks(m, 960);
        const auto full = mixBlocks(alone, 1440);
        bool curve = true;
        for (int i = 0; i < 960 && curve; ++i) {
            const float k = static_cast<float>(i % 480 + 1) / 480.0f;
            const float gain = i < 480 ? 1.0f + (0.70710678f - 1.0f) * k : 0.70710678f * (1.0f - k);
            curve = Near(faded[static_cast<size_t>(i) * 2], full[static_cast<size_t>(480 + i) * 2] * gain, 1e-5f);
            if (!curve)
                std::printf("fade frame %d: got %f want %f\n", i, faded[static_cast<size_t>(i) * 2],
                            full[static_cast<size_t>(480 + i) * 2] * gain);
        }
        CHECK(curve);
    }
    {   // Fade out over 0.05 s: then silence, the stream handed back, ended at once.
        auto a = MusicStream::Open(stereo);
        Mixer m(Rate);
        m.Submit(play(a.get(), 1, 0.0f));
        mixBlocks(m, 960);
        MixCommand stop;
        stop.type = MixCommandType::StopMusic;
        stop.fade = 0.05f;
        m.Submit(stop);
        const auto fading = mixBlocks(m, 2400);
        CHECK(m.MusicEnded() == 1);
        float loud = 0.0f;
        for (float v : fading) loud = std::max(loud, std::fabs(v));
        CHECK(loud > 0.05f);  // still audible while fading
        CHECK(Near(fading[fading.size() - 2], 0.0f, 1e-6f));  // gain reaches 0 at the end
        const auto after = mixBlocks(m, 960);
        bool silent = true;
        for (float v : after) silent = silent && v == 0.0f;
        CHECK(silent);
        CHECK(m.TakeRetiredMusic() == a.get());
    }
    {   // A plain Play during a crossfade cuts the fading track at once.
        auto a = MusicStream::Open(tone), b = MusicStream::Open(stereo), c = MusicStream::Open(tone);
        Mixer m(Rate);
        m.Submit(play(a.get(), 1, 0.0f));
        Mix(m, 480);
        m.Submit(play(b.get(), 2, 1.0f));
        Mix(m, 480);
        m.Submit(play(c.get(), 3, 0.0f));
        Mix(m, 480);
        CHECK(m.TakeRetiredMusic() == a.get());
        CHECK(m.TakeRetiredMusic() == b.get());
        CHECK(m.MusicEnded() == 2);
    }
}

class Driver : public Game {
public:
    std::function<void()> load;
    std::function<bool(double)> step;
    Driver() {
        Settings().fixedTimeStep = false;
        Settings().console = ConsoleMode::Never;
    }

protected:
    void LoadContent() override {
        if (load) load();
    }
    void Update(const GameTime& time) override {
        if (!step(time.Total()) || time.Total() > 10.0) Exit();
    }
};

void EndToEnd() {
    SDL_Environment* env = SDL_GetEnvironment();
    SDL_SetEnvironmentVariable(env, "COLDX_HEADLESS", "1", true);

    std::vector<float> beep(Rate / 10);  // 0.1 s
    for (size_t i = 0; i < beep.size(); ++i) beep[i] = 0.25f * std::sin(static_cast<float>(i) * 0.05f);
    const SoundEffect effect = SoundEffect::FromSamples(beep.data(), static_cast<int>(beep.size()), 1, Rate);
    CHECK(!effect.IsEmpty() && Near(effect.Duration(), 0.1f));
    CHECK(!effect.Play());  // before Run: no device

    std::vector<float> long_(Rate * 3);
    const SoundEffect longEffect = SoundEffect::FromSamples(long_.data(), static_cast<int>(long_.size()), 1, Rate);
    const SoundEffect fromFile = SoundEffect::FromFile(COLDX_TEST_DATA "/tone.ogg");
    CHECK(!fromFile.IsEmpty() && fromFile.SampleRate() == 44100);
    const Music music = Music::FromFile(COLDX_TEST_DATA "/tone.ogg");
    CHECK(!music.IsEmpty() && Near(music.Duration(), 0.5f, 0.01f));
    CHECK(Music::FromFile(COLDX_TEST_DATA "/missing.ogg").IsEmpty());

    SoundEffectInstance once, looping;
    int phase = 0;
    double mark = 0.0;
    Driver game;
    game.load = [&] {
        once = effect.CreateInstance();
        looping = effect.CreateInstance();
        looping.SetLooped(true);
        CHECK(once.State() == SoundState::Stopped);
        CHECK(once.Play() && once.State() == SoundState::Playing);
        CHECK(looping.Play());
        CHECK(MediaPlayer::Play(music, false) && MediaPlayer::IsPlaying());
    };
    game.step = [&](double t) {
        if (phase == 0 && t > 0.6) {
            CHECK(once.State() == SoundState::Stopped);        // 0.1 s sound, 0.6 s later
            CHECK(looping.State() == SoundState::Playing);     // still going
            CHECK(!MediaPlayer::IsPlaying());                  // 0.5 s track, unlooped
            looping.Pause();
            CHECK(looping.State() == SoundState::Paused);
            CHECK(MediaPlayer::Play(music, true));
            phase = 1;
            mark = t;
        } else if (phase == 1 && t > mark + 0.8) {
            CHECK(looping.State() == SoundState::Paused);
            CHECK(MediaPlayer::IsPlaying());  // looped past its 0.5 s
            MediaPlayer::Stop();
            CHECK(!MediaPlayer::IsPlaying());
            looping.Stop();
            CHECK(looping.State() == SoundState::Stopped);
            phase = 2;
            mark = t;
        } else if (phase == 2 && t > mark + 0.1) {
            // The stop has reached the audio thread, and the finished `once` instance holds
            // nothing: every voice is free. MaxVoices three-second sounds play; one more finds none.
            CHECK(once.State() == SoundState::Stopped);
            int played = 0;
            for (int i = 0; i < SoundEffect::MaxVoices + 1; ++i) played += longEffect.Play() ? 1 : 0;
            CHECK(played == SoundEffect::MaxVoices);
            phase = 3;
            return false;
        }
        return true;
    };
    CHECK(game.Run() == 0);
    CHECK(phase == 3);
    // After Run, nothing plays and instances report stopped.
    CHECK(!effect.Play() && !once.Play());
}

} // namespace

int main() {
    Basics();
    Lifetimes();
    Ramping();
    Decoding();
    MusicMixing();
    MusicFades();
    EndToEnd();
    return CheckResult();
}
