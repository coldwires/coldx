// Sound effects, an instance you control, and streaming music.
//
// 1-5 play notes: one generated sound, pitched up and down. Space starts or stops a
// looping hum; P pauses it; Left and Right pan it. M starts or stops the music; Up and
// Down change its volume.

#include <coldx/coldx.h>

#include <cmath>
#include <cstdio>
#include <vector>

using namespace coldx;

namespace {

// A plucked note: a sine at 440 Hz that dies away over half a second.
SoundEffect MakeNote() {
    const int rate = 48000;
    std::vector<float> samples(rate / 2);
    for (size_t i = 0; i < samples.size(); ++i) {
        const float t = static_cast<float>(i) / rate;
        samples[i] = 0.4f * std::sin(MathHelper::TwoPi * 440.0f * t) * std::exp(-6.0f * t);
    }
    return SoundEffect::FromSamples(samples.data(), static_cast<int>(samples.size()), 1, rate);
}

// One second of a low hum whose length is a whole number of cycles, so it loops cleanly.
SoundEffect MakeHum() {
    const int rate = 48000;
    std::vector<float> samples(rate);
    for (size_t i = 0; i < samples.size(); ++i) {
        const float t = static_cast<float>(i) / rate;
        samples[i] = 0.15f * std::sin(MathHelper::TwoPi * 110.0f * t) + 0.05f * std::sin(MathHelper::TwoPi * 220.0f * t);
    }
    return SoundEffect::FromSamples(samples.data(), static_cast<int>(samples.size()), 1, rate);
}

const char* Name(SoundState state) {
    switch (state) {
    case SoundState::Playing: return "playing";
    case SoundState::Paused: return "paused";
    case SoundState::Stopped: return "stopped";
    }
    return "?";
}

} // namespace

class HelloAudio : public Game {
public:
    HelloAudio() {
        Settings().title = "hello_audio";
    }

protected:
    void LoadContent() override {
        font = SpriteFont::FromFile(Graphics(), "content/LiberationSans-Regular.ttf", 20.0f);
        note = MakeNote();
        hum = MakeHum().CreateInstance();
        hum.SetLooped(true);
        music = Music::FromFile("content/loop.ogg");
    }

    void Update(const GameTime&) override {
        if (Keyboard::WasPressed(Key::Escape)) Exit();

        // Semitones from A: A, C, D, E, G. pitch is in octaves.
        const Key keys[] = {Key::D1, Key::D2, Key::D3, Key::D4, Key::D5};
        const int semitones[] = {0, 3, 5, 7, 10};
        for (int i = 0; i < 5; ++i) {
            if (Keyboard::WasPressed(keys[i])) {
                note.Play(0.8f, semitones[i] / 12.0f);
                Log("hello_audio: note %d", i + 1);
            }
        }

        if (Keyboard::WasPressed(Key::Space)) {
            if (hum.State() == SoundState::Stopped)
                hum.Play();
            else
                hum.Stop();
        }
        if (Keyboard::WasPressed(Key::P)) {
            if (hum.State() == SoundState::Playing)
                hum.Pause();
            else
                hum.Resume();
        }
        if (Keyboard::WasPressed(Key::Left)) hum.SetPan(hum.Pan() - 0.25f);
        if (Keyboard::WasPressed(Key::Right)) hum.SetPan(hum.Pan() + 0.25f);

        if (Keyboard::WasPressed(Key::M)) {
            if (MediaPlayer::IsPlaying())
                MediaPlayer::Stop();
            else
                MediaPlayer::Play(music);
        }
        if (Keyboard::WasPressed(Key::Up)) MediaPlayer::SetVolume(MediaPlayer::Volume() + 0.1f);
        if (Keyboard::WasPressed(Key::Down)) MediaPlayer::SetVolume(MediaPlayer::Volume() - 0.1f);
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color(20, 24, 30));
        char status[256];
        std::snprintf(status, sizeof(status),
                      "1-5  play a note\n"
                      "Space  hum on/off   P  pause   Left/Right  pan\n"
                      "M  music on/off   Up/Down  music volume\n\n"
                      "hum: %s, pan %.2f\n"
                      "music: %s, volume %.1f",
                      Name(hum.State()), hum.Pan(), MediaPlayer::IsPlaying() ? "playing" : "stopped",
                      MediaPlayer::Volume());
        batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        batch.DrawString(font, status, Vector2(40, 40), Color::White);
        batch.End();
    }

private:
    SpriteBatch batch{Graphics()};
    SpriteFont font;
    SoundEffect note;
    SoundEffectInstance hum;
    Music music;
};

int main() {
    HelloAudio game;
    return game.Run();
}
