// AUD-01 to AUD-05. Where the question is about the samples themselves (loop seams,
// clicks), the test drives the mixer directly and measures its output; the device cannot be
// listened to from here.

#include "framework_test.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

#include <SDL3/SDL.h>

#include "audio_data.h"
#include "mixer.h"

using namespace coldx;

namespace ft {

namespace {

constexpr int Rate = 48000;

SoundEffect Tone(float seconds, float hz) {
    std::vector<float> s(static_cast<size_t>(seconds * Rate));
    for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3f * std::sin(6.2831853f * hz * i / Rate);
    return SoundEffect::FromSamples(s.data(), static_cast<int>(s.size()), 1, Rate);
}

// AUD-01: 100, 300 and 500 sounds in one frame, every fifth frame, 20 s each.
Result ManySounds() {
    Result r{"AUD-01", "P0"};
    SetEnv("COLDX_HEADLESS", "1");
    Probe game;
    const SoundEffect blip = Tone(0.1f, 880);
    const int levels[] = {100, 300, 500};
    struct Stat {
        int played = 0, asked = 0;
        double worstMs = 0, totalMs = 0;
        int bursts = 0;
    } stats[3];
    double levelStart = 0;
    int level = 0;
    game.load = [&] { levelStart = Now(); };
    game.update = [&](Probe& p, const GameTime&) {
        if (Now() - levelStart > 20.0) {
            levelStart = Now();
            if (++level == 3) {
                p.Exit();
                return;
            }
        }
        if (p.updates % 5 != 0) return;
        Stat& s = stats[level];
        const double t0 = Now();
        for (int i = 0; i < levels[level]; ++i) s.played += blip.Play(0.05f, (i % 24) / 24.0f - 0.5f) ? 1 : 0;
        const double ms = (Now() - t0) * 1000;
        s.asked += levels[level];
        s.worstMs = std::max(s.worstMs, ms);
        s.totalMs += ms;
        ++s.bursts;
    };
    const int rc = game.Run();
    SetEnv("COLDX_HEADLESS", nullptr);
    std::string notes;
    double worst = 0;
    for (int i = 0; i < 3; ++i) {
        notes += Format("%d per burst: %d of %d played, burst took %.2f ms average, %.2f ms worst; ", levels[i], stats[i].played,
                        stats[i].asked, stats[i].totalMs / std::max(1, stats[i].bursts), stats[i].worstMs);
        worst = std::max(worst, stats[i].worstMs);
    }
    r.status = rc == 0 && worst < 4.0 ? Status::NeedsHuman : Status::Fail;
    r.notes = notes + Format("voice limit %d: extra sounds are skipped, and the skips are logged once a second. "
                             "Whether the mix ever drops out has to be heard.",
                             SoundEffect::MaxVoices);
    return r;
}

std::vector<unsigned char> ReadAll(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), {});
}

// AUD-03: the looped track through its seam, sample by sample against the decoded track,
// and how long switching tracks takes on the game thread.
Result StreamingMusic() {
    Result r{"AUD-03", "P1"};
    const std::string path = "content/loop.ogg";
    const std::vector<unsigned char> bytes = ReadAll(path);
    const auto data = detail::LoadMusic(bytes, path.c_str());
    if (!data) {
        r.status = Status::Fail;
        r.notes = "content/loop.ogg missing or unreadable beside the runner.";
        return r;
    }
    // The reference: the whole track read once through a stream of its own.
    std::vector<float> track;
    {
        auto reader = detail::MusicStream::Open(data);
        std::vector<float> chunk(1024 * static_cast<size_t>(data->channels));
        for (int n; (n = reader->Read(chunk.data(), 1024)) > 0;)
            track.insert(track.end(), chunk.begin(), chunk.begin() + n * data->channels);
    }
    // SoundEffect decoding of the same file must give the same number of frames.
    const auto decoded = detail::DecodeSound(bytes, path.c_str());
    const std::int64_t streamFrames = static_cast<std::int64_t>(track.size()) / data->channels;
    const std::int64_t effectFrames = decoded ? decoded->frames : -1;

    auto stream = detail::MusicStream::Open(data);
    detail::Mixer mixer(Rate);
    detail::MixCommand play;
    play.type = detail::MixCommandType::PlayMusic;
    play.music = stream.get();
    play.looped = true;
    play.generation = 1;
    mixer.Submit(play);

    const int channels = data->channels;
    const double step = static_cast<double>(data->sampleRate) / Rate;
    const auto trackFrames = streamFrames;
    const int outFrames = static_cast<int>(trackFrames / step) + Rate;  // past the seam by a second
    std::vector<float> out(static_cast<size_t>(outFrames) * 2);
    for (int done = 0; done < outFrames; done += 1024) mixer.Mix(out.data() + done * 2, std::min(1024, outFrames - done));

    // Expected: the track repeated, read by the same linear interpolation.
    auto sample = [&](std::int64_t frame, int c) { return track[static_cast<size_t>((frame % trackFrames) * channels + c)]; };
    double worst = 0;
    int worstAt = 0;
    for (int i = 0; i < outFrames; ++i) {
        const double pos = i * step;
        const auto k = static_cast<std::int64_t>(pos);
        const float f = static_cast<float>(pos - static_cast<double>(k));
        for (int c = 0; c < 2; ++c) {
            const int src = std::min(c, channels - 1);
            const float expected = sample(k, src) + (sample(k + 1, src) - sample(k, src)) * f;
            const double d = std::fabs(out[static_cast<size_t>(i) * 2 + c] - expected);
            if (d > worst) {
                worst = d;
                worstAt = i;
            }
        }
    }
    const int seamAt = static_cast<int>(trackFrames / step);

    // Switching: MediaPlayer::Play on the game thread, timed, twenty times.
    SetEnv("COLDX_HEADLESS", "1");
    Probe game;
    const Music music = Music::FromFile(path);
    double worstSwitch = 0;
    game.update = [&](Probe& p, const GameTime&) {
        if (p.updates % 6 == 0) {
            const double t0 = Now();
            MediaPlayer::Play(music);
            worstSwitch = std::max(worstSwitch, (Now() - t0) * 1000);
        }
        if (p.updates == 120) p.Exit();
    };
    game.Run();
    SetEnv("COLDX_HEADLESS", nullptr);

    const bool seamless = worst < 1e-4;
    const bool sameLength = effectFrames == streamFrames;
    r.status = seamless && sameLength && worstSwitch < 2.0 ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("Looped %.2f s track through its seam (output frame %d): largest difference from the track %.2g at "
                     "frame %d, %s. Switching tracks took %.2f ms at worst on the game thread. ",
                     data->duration, seamAt, worst, worstAt, seamless ? "no gap and no jump at the loop" : "THE LOOP IS NOT SEAMLESS",
                     worstSwitch) +
              (sameLength ? std::string()
                          : Format("Bug: the same file decoded as a SoundEffect has %lld frames, as a stream %lld. Likely "
                                   "cause: DecodeOgg (src/core/audio_codecs.cpp) passes a frame count where "
                                   "stb_vorbis_get_samples_float_interleaved takes a float count, and stops when one stereo "
                                   "frame no longer fits. ",
                                   static_cast<long long>(effectFrames), static_cast<long long>(streamFrames))) +
              "Crossfades are AUD-06. Whether the loop sounds seamless needs a person (an encoder can add a click to "
              "the file itself).";
    return r;
}

// AUD-04: a looping tone whose volume and pan change at every mixer block, the way a game
// would move them each frame. A click is a jump between neighbouring samples far beyond
// anything the tone itself produces.
Result LiveParameters() {
    Result r{"AUD-04", "P1"};
    detail::SoundData tone;
    tone.channels = 1;
    tone.sampleRate = Rate;
    tone.frames = Rate / 100;  // 100 Hz, exactly one cycle, loops cleanly
    for (int i = 0; i < tone.frames; ++i) tone.samples.push_back(0.5f * std::sin(6.2831853f * i / tone.frames));
    const double toneStep = 0.5 * 6.2831853 / tone.frames;  // largest step of the tone itself

    auto run = [&](bool vary, bool pitch) {
        detail::Mixer mixer(Rate);
        detail::MixCommand play;
        play.type = detail::MixCommandType::Play;
        play.slot = 0;
        play.generation = 1;
        play.sound = &tone;
        play.looped = true;
        mixer.Submit(play);
        std::vector<float> out(512 * 2);
        double worst = 0;
        float prevL = 0;
        for (int block = 0; block < 400; ++block) {
            if (vary) {
                const float t = block / 400.0f;
                detail::MixCommand c;
                c.slot = 0;
                c.generation = 1;
                c.type = detail::MixCommandType::SetVolume;
                c.volume = 0.5f + 0.5f * std::sin(t * 40);  // a fast sweep, as a game fading per frame
                mixer.Submit(c);
                c.type = detail::MixCommandType::SetPan;
                c.pan = std::sin(t * 30);
                mixer.Submit(c);
                if (pitch) {
                    c.type = detail::MixCommandType::SetPitch;
                    c.pitch = 0.5f * std::sin(t * 20);
                    mixer.Submit(c);
                }
            }
            mixer.Mix(out.data(), 512);
            for (int i = 0; i < 512; ++i) {
                const float l = out[static_cast<size_t>(i) * 2];
                if (block > 0 || i > 0) worst = std::max(worst, static_cast<double>(std::fabs(l - prevL)));
                prevL = l;
            }
        }
        return worst;
    };
    const double steady = run(false, false), varied = run(true, false), withPitch = run(true, true);
    const double ratio = varied / steady;
    r.status = ratio < 2.0 ? Status::NeedsHuman : Status::Fail;
    r.notes = Format("Largest step between samples: %.4f for the steady tone (theory %.4f), %.4f with volume and pan "
                     "changing every 512-frame block (%.1fx), %.4f with pitch changing too. ",
                     steady, toneStep, varied, ratio, withPitch) +
              (ratio < 2.0 ? "No jumps beyond the tone's own."
                           : "Gain changes land as a step at a block boundary instead of a ramp: an audible zipper or "
                             "click when volume or pan is moved continuously. Likely cause: Mixer::Apply sets volume "
                             "and pan at once (src/core/mixer.cpp); ramping them across the next block would fix it.") +
              " Ranges match XNA's (volume 0..1, pitch and pan -1..1, clamped).";
    return r;
}

// AUD-05: no audio device at all; the game must run, silently.
Result NoDevice() {
    Result r{"AUD-05", "P2"};
    SetEnv("COLDX_HEADLESS", "1");
    SetEnv("SDL_AUDIO_DRIVER", "no-such-driver");
    Probe game;
    bool played = true;
    const SoundEffect blip = Tone(0.1f, 440);
    game.update = [&](Probe& p, const GameTime&) {
        if (p.updates == 2) played = blip.Play();
        if (p.updates == 10) p.Exit();
    };
    const int rc = game.Run();
    SetEnv("SDL_AUDIO_DRIVER", nullptr);
    SetEnv("COLDX_HEADLESS", nullptr);
    r.status = rc == 0 && !played && game.updates == 10 ? Status::Pass : Status::Fail;
    r.notes = Format("With an audio driver that does not exist the game ran %d updates, exit code %d; Play returned %s. "
                     "coldx logs \"audio off\" and carries on.",
                     game.updates, rc, played ? "true" : "false");
    return r;
}

} // namespace

void RegisterAudio() {
    AutoTests().push_back({"AUD-01", "P0", "Many simultaneous sounds", ManySounds});
    AutoTests().push_back({"AUD-03", "P1", "Streaming music", StreamingMusic});
    AutoTests().push_back({"AUD-04", "P1", "Live parameter changes", LiveParameters});
    AutoTests().push_back({"AUD-05", "P2", "No audio device", NoDevice});
}

} // namespace ft
