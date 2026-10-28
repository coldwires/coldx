#include <cstring>

#include <SDL3/SDL.h>

#include "audio_data.h"

#define STB_VORBIS_HEADER_ONLY
#include <stb/stb_vorbis.c>
#include <dr_libs/dr_wav.h>

namespace coldx::detail {

namespace {

bool StartsWith(const std::vector<unsigned char>& file, const char* magic, size_t at = 0) {
    const size_t n = std::strlen(magic);
    return file.size() >= at + n && std::memcmp(file.data() + at, magic, n) == 0;
}

std::shared_ptr<SoundData> DecodeWav(const std::vector<unsigned char>& file, const char* name) {
    drwav wav;
    if (!drwav_init_memory(&wav, file.data(), file.size(), nullptr)) {
        SDL_Log("coldx.core: %s: not a WAV file dr_wav can read", name);
        return nullptr;
    }
    auto sound = std::make_shared<SoundData>();
    sound->channels = static_cast<int>(wav.channels);
    sound->sampleRate = static_cast<int>(wav.sampleRate);
    sound->frames = static_cast<std::int64_t>(wav.totalPCMFrameCount);
    sound->samples.resize(static_cast<size_t>(sound->frames) * wav.channels);
    const drwav_uint64 read = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, sound->samples.data());
    drwav_uninit(&wav);
    if (read != wav.totalPCMFrameCount) {
        SDL_Log("coldx.core: %s: WAV ended after %llu of %llu frames", name, static_cast<unsigned long long>(read),
                static_cast<unsigned long long>(wav.totalPCMFrameCount));
        sound->frames = static_cast<std::int64_t>(read);
        sound->samples.resize(static_cast<size_t>(read) * wav.channels);
    }
    return sound;
}

std::shared_ptr<SoundData> DecodeOgg(const std::vector<unsigned char>& file, const char* name) {
    int error = 0;
    stb_vorbis* v = stb_vorbis_open_memory(file.data(), static_cast<int>(file.size()), &error, nullptr);
    if (!v) {
        SDL_Log("coldx.core: %s: not an OGG Vorbis file stb_vorbis can read (error %d)", name, error);
        return nullptr;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(v);
    auto sound = std::make_shared<SoundData>();
    sound->channels = info.channels;
    sound->sampleRate = static_cast<int>(info.sample_rate);
    sound->samples.resize(static_cast<size_t>(stb_vorbis_stream_length_in_samples(v)) * info.channels);
    int total = 0;
    for (;;) {
        const int room = static_cast<int>(sound->samples.size() / info.channels) - total;
        if (room <= 0) sound->samples.resize(sound->samples.size() + 4096 * static_cast<size_t>(info.channels));
        const int got = stb_vorbis_get_samples_float_interleaved(
            v, info.channels, sound->samples.data() + static_cast<size_t>(total) * info.channels,
            (static_cast<int>(sound->samples.size() / info.channels) - total) * info.channels);
        if (got == 0) break;
        total += got;
    }
    stb_vorbis_close(v);
    sound->frames = total;
    sound->samples.resize(static_cast<size_t>(total) * info.channels);
    return sound;
}

} // namespace

std::shared_ptr<SoundData> DecodeSound(const std::vector<unsigned char>& file, const char* name) {
    std::shared_ptr<SoundData> sound;
    if (StartsWith(file, "RIFF") && StartsWith(file, "WAVE", 8))
        sound = DecodeWav(file, name);
    else if (StartsWith(file, "OggS"))
        sound = DecodeOgg(file, name);
    else
        SDL_Log("coldx.core: %s: not WAV or OGG", name);
    if (sound && sound->channels != 1 && sound->channels != 2) {
        SDL_Log("coldx.core: %s: %d channels. Only mono and stereo are supported", name, sound->channels);
        return nullptr;
    }
    return sound;
}

std::shared_ptr<MusicData> LoadMusic(std::vector<unsigned char> file, const char* name) {
    if (!StartsWith(file, "OggS")) {
        SDL_Log("coldx.core: %s: music must be OGG Vorbis", name);
        return nullptr;
    }
    int error = 0;
    stb_vorbis* v = stb_vorbis_open_memory(file.data(), static_cast<int>(file.size()), &error, nullptr);
    if (!v) {
        SDL_Log("coldx.core: %s: not an OGG Vorbis file stb_vorbis can read (error %d)", name, error);
        return nullptr;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(v);
    auto music = std::make_shared<MusicData>();
    music->channels = info.channels;
    music->sampleRate = static_cast<int>(info.sample_rate);
    music->duration = stb_vorbis_stream_length_in_seconds(v);
    stb_vorbis_close(v);
    if (music->channels != 1 && music->channels != 2) {
        SDL_Log("coldx.core: %s: %d channels. Only mono and stereo are supported", name, music->channels);
        return nullptr;
    }
    music->bytes = std::move(file);
    return music;
}

std::unique_ptr<MusicStream> MusicStream::Open(std::shared_ptr<const MusicData> data) {
    int error = 0;
    stb_vorbis* v = stb_vorbis_open_memory(data->bytes.data(), static_cast<int>(data->bytes.size()), &error, nullptr);
    if (!v) return nullptr;
    std::unique_ptr<MusicStream> stream(new MusicStream());
    stream->data_ = std::move(data);
    stream->decoder_ = v;
    stream->buffer.resize(static_cast<size_t>(BufferFrames + 1) * stream->data_->channels);
    return stream;
}

MusicStream::~MusicStream() {
    if (decoder_) stb_vorbis_close(decoder_);
}

int MusicStream::Read(float* out, int frames) {
    return stb_vorbis_get_samples_float_interleaved(decoder_, data_->channels, out, frames * data_->channels);
}

void MusicStream::Rewind() {
    stb_vorbis_seek_start(decoder_);
}

} // namespace coldx::detail
