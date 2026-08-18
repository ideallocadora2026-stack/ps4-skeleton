#include "audio.hpp"

#include <orbis/AudioOut.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#define DR_MP3_IMPLEMENTATION
#include "third_party/dr_mp3.h"

namespace gw
{
namespace
{
const int OUTPUT_RATE = 48000;
const int BUFFER_FRAMES = 768;
const int PAD_SPEAKER_PORT = 4;
const char* GENERAL_TRACKS[5] = {
    "/app0/assets/audio/music/general_1.mp3",
    "/app0/assets/audio/music/general_2.mp3",
    "/app0/assets/audio/music/general_3.mp3",
    "/app0/assets/audio/music/general_4.mp3",
    "/app0/assets/audio/music/general_5.mp3"
};
const char* BOSS_TRACK = "/app0/assets/audio/music/boss.mp3";
const char* SHOP_TRACK = "/app0/assets/audio/music/shop.mp3";
const char* EFFECT_FILES[static_cast<int>(SoundEffect::Count)] = {
    "/app0/assets/audio/sfx/coin_1.mp3",
    "/app0/assets/audio/sfx/coin_2.mp3",
    "/app0/assets/audio/sfx/grenade.mp3",
    "/app0/assets/audio/sfx/boss_destroyed.mp3",
    "/app0/assets/audio/sfx/player_damage.mp3",
    "/app0/assets/audio/sfx/heart.mp3"
};

float clampSample(float value)
{
    return std::max(-1.0f, std::min(1.0f, value));
}
}

struct AudioEngine::Impl
{
    struct Sample
    {
        std::vector<float> stereo;
        std::vector<float> mono;
        uint64_t frames;

        Sample() : frames(0) {}
    };

    struct Voice
    {
        int effect;
        uint64_t frame;
    };

    struct Request
    {
        int effect;
        int pad;
    };

    struct MusicStream
    {
        drmp3 decoder;
        bool open;
        unsigned channels;
        unsigned rate;
        std::vector<float> decoded;
        uint64_t decodedFrames;
        uint64_t decodedIndex;
        uint64_t sourceAccumulator;
        float currentLeft;
        float currentRight;
        bool hasCurrent;

        MusicStream()
            : open(false), channels(0), rate(0), decodedFrames(0), decodedIndex(0),
              sourceAccumulator(0), currentLeft(0.0f), currentRight(0.0f), hasCurrent(false)
        {
            std::memset(&decoder, 0, sizeof(decoder));
        }
    };

    Impl()
        : running(false), threadCreated(false), mutexReady(false), mainHandle(-1),
          requestedMode(static_cast<int>(MusicMode::General)), musicVolume(1.0f), soundVolume(1.0f),
          activeMode(MusicMode::Silent), generalTrack(0)
    {
        for (int i = 0; i < 4; ++i)
        {
            requestedUsers[i].store(-1);
            openedUsers[i] = -1;
            padHandles[i] = -1;
        }
    }

    ~Impl()
    {
        shutdown();
    }

    std::atomic<bool> running;
    bool threadCreated;
    bool mutexReady;
    OrbisPthread thread;
    OrbisPthreadMutex requestMutex;
    int mainHandle;
    std::atomic<int> requestedMode;
    std::atomic<float> musicVolume;
    std::atomic<float> soundVolume;
    std::atomic<int> requestedUsers[4];
    int openedUsers[4];
    int padHandles[4];
    std::vector<Request> requests;
    Sample samples[static_cast<int>(SoundEffect::Count)];
    std::vector<Voice> tvVoices;
    std::vector<Voice> padVoices[4];
    MusicMode activeMode;
    MusicStream generalStream;
    MusicStream bossStream;
    MusicStream shopStream;
    int generalTrack;

    bool initialize();
    void shutdown();
    bool loadSample(SoundEffect effect, const char* path);
    bool openMusic(MusicStream& stream, const char* path);
    void closeMusic(MusicStream& stream);
    bool readSourceFrame(MusicStream& stream, float& left, float& right);
    bool readOutputFrame(MusicStream& stream, float& left, float& right);
    bool restartActiveMusic();
    void updateMusicMode();
    void reconcileControllerPorts();
    void drainRequests();
    void mixVoiceList(std::vector<Voice>& voices, float* output, bool stereo, float volume);
    void mixMusic(float* stereoOutput);
    void renderBlock();
    void queue(SoundEffect effect, int pad);
    static void* threadEntry(void* value);
};

bool AudioEngine::Impl::loadSample(SoundEffect effect, const char* path)
{
    drmp3_config config;
    drmp3_uint64 sourceFrames = 0;
    float* decoded = drmp3_open_file_and_read_pcm_frames_f32(path, &config, &sourceFrames, nullptr);
    if (!decoded || sourceFrames == 0 || config.channels == 0 || config.sampleRate == 0)
    {
        if (decoded) drmp3_free(decoded, nullptr);
        std::printf("Audio: efeito nao carregado: %s\n", path);
        return false;
    }

    Sample& sample = samples[static_cast<int>(effect)];
    sample.frames = static_cast<uint64_t>(std::ceil(sourceFrames * (static_cast<double>(OUTPUT_RATE) / config.sampleRate)));
    sample.stereo.resize(static_cast<size_t>(sample.frames) * 2);
    sample.mono.resize(static_cast<size_t>(sample.frames));
    for (uint64_t frame = 0; frame < sample.frames; ++frame)
    {
        const double sourcePosition = frame * (static_cast<double>(config.sampleRate) / OUTPUT_RATE);
        const uint64_t first = std::min<uint64_t>(static_cast<uint64_t>(sourcePosition), sourceFrames - 1);
        const uint64_t second = std::min<uint64_t>(first + 1, sourceFrames - 1);
        const float fraction = static_cast<float>(sourcePosition - first);
        const float firstLeft = decoded[first * config.channels];
        const float firstRight = config.channels > 1 ? decoded[first * config.channels + 1] : firstLeft;
        const float secondLeft = decoded[second * config.channels];
        const float secondRight = config.channels > 1 ? decoded[second * config.channels + 1] : secondLeft;
        const float left = firstLeft + (secondLeft - firstLeft) * fraction;
        const float right = firstRight + (secondRight - firstRight) * fraction;
        sample.stereo[frame * 2] = left;
        sample.stereo[frame * 2 + 1] = right;
        sample.mono[frame] = (left + right) * 0.5f;
    }
    drmp3_free(decoded, nullptr);
    return true;
}

bool AudioEngine::Impl::openMusic(MusicStream& stream, const char* path)
{
    closeMusic(stream);
    if (!drmp3_init_file(&stream.decoder, path, nullptr))
    {
        std::printf("Audio: musica nao carregada: %s\n", path);
        return false;
    }
    stream.open = true;
    stream.channels = stream.decoder.channels;
    stream.rate = stream.decoder.sampleRate;
    stream.decoded.resize(4096 * std::max(1u, stream.channels));
    stream.decodedFrames = 0;
    stream.decodedIndex = 0;
    stream.sourceAccumulator = 0;
    stream.hasCurrent = false;
    return stream.channels > 0 && stream.rate > 0;
}

void AudioEngine::Impl::closeMusic(MusicStream& stream)
{
    if (stream.open) drmp3_uninit(&stream.decoder);
    stream.open = false;
    stream.decodedFrames = 0;
    stream.decodedIndex = 0;
    stream.sourceAccumulator = 0;
    stream.hasCurrent = false;
}

bool AudioEngine::Impl::readSourceFrame(MusicStream& stream, float& left, float& right)
{
    if (!stream.open) return false;
    if (stream.decodedIndex >= stream.decodedFrames)
    {
        stream.decodedFrames = drmp3_read_pcm_frames_f32(&stream.decoder, 4096, &stream.decoded[0]);
        stream.decodedIndex = 0;
        if (stream.decodedFrames == 0) return false;
    }
    const uint64_t index = stream.decodedIndex++ * stream.channels;
    left = stream.decoded[index];
    right = stream.channels > 1 ? stream.decoded[index + 1] : left;
    return true;
}

bool AudioEngine::Impl::readOutputFrame(MusicStream& stream, float& left, float& right)
{
    if (!stream.hasCurrent)
    {
        if (!readSourceFrame(stream, stream.currentLeft, stream.currentRight)) return false;
        stream.hasCurrent = true;
    }
    left = stream.currentLeft;
    right = stream.currentRight;
    stream.sourceAccumulator += stream.rate;
    while (stream.sourceAccumulator >= OUTPUT_RATE)
    {
        if (!readSourceFrame(stream, stream.currentLeft, stream.currentRight))
        {
            stream.hasCurrent = false;
            return false;
        }
        stream.sourceAccumulator -= OUTPUT_RATE;
    }
    return true;
}

bool AudioEngine::Impl::restartActiveMusic()
{
    if (activeMode == MusicMode::General)
    {
        generalTrack = (generalTrack + 1) % 5;
        return openMusic(generalStream, GENERAL_TRACKS[generalTrack]);
    }
    if (activeMode == MusicMode::Boss) return openMusic(bossStream, BOSS_TRACK);
    if (activeMode == MusicMode::Shop) return openMusic(shopStream, SHOP_TRACK);
    return false;
}

void AudioEngine::Impl::updateMusicMode()
{
    const MusicMode next = static_cast<MusicMode>(requestedMode.load());
    if (next == activeMode) return;
    activeMode = next;
    if (activeMode == MusicMode::General && !generalStream.open)
        openMusic(generalStream, GENERAL_TRACKS[generalTrack]);
    else if (activeMode == MusicMode::Boss)
        openMusic(bossStream, BOSS_TRACK);
    else if (activeMode == MusicMode::Shop)
        openMusic(shopStream, SHOP_TRACK);
}

void AudioEngine::Impl::reconcileControllerPorts()
{
    for (int pad = 0; pad < 4; ++pad)
    {
        const int desired = requestedUsers[pad].load();
        if (desired == openedUsers[pad]) continue;
        if (padHandles[pad] >= 0) sceAudioOutClose(padHandles[pad]);
        padHandles[pad] = -1;
        openedUsers[pad] = desired;
        padVoices[pad].clear();
        if (desired >= 0)
            padHandles[pad] = sceAudioOutOpen(desired, static_cast<OrbisAudioOutPort>(PAD_SPEAKER_PORT), 0,
                                              BUFFER_FRAMES, OUTPUT_RATE, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
    }
}

void AudioEngine::Impl::drainRequests()
{
    std::vector<Request> queued;
    if (mutexReady)
    {
        scePthreadMutexLock(&requestMutex);
        queued.swap(requests);
        scePthreadMutexUnlock(&requestMutex);
    }
    for (unsigned i = 0; i < queued.size(); ++i)
    {
        const Request& request = queued[i];
        Voice voice = {request.effect, 0};
        if (request.pad >= 0 && request.pad < 4 && padHandles[request.pad] >= 0)
            padVoices[request.pad].push_back(voice);
        else
            tvVoices.push_back(voice);
    }
}

void AudioEngine::Impl::mixVoiceList(std::vector<Voice>& voices, float* output, bool stereo, float volume)
{
    for (unsigned voiceIndex = 0; voiceIndex < voices.size();)
    {
        Voice& voice = voices[voiceIndex];
        if (voice.effect < 0 || voice.effect >= static_cast<int>(SoundEffect::Count))
        {
            voices.erase(voices.begin() + voiceIndex);
            continue;
        }
        const Sample& sample = samples[voice.effect];
        const uint64_t remaining = sample.frames > voice.frame ? sample.frames - voice.frame : 0;
        const uint64_t frames = std::min<uint64_t>(remaining, BUFFER_FRAMES);
        for (uint64_t frame = 0; frame < frames; ++frame)
        {
            if (stereo)
            {
                output[frame * 2] += sample.stereo[(voice.frame + frame) * 2] * volume;
                output[frame * 2 + 1] += sample.stereo[(voice.frame + frame) * 2 + 1] * volume;
            }
            else output[frame] += sample.mono[voice.frame + frame] * volume;
        }
        voice.frame += frames;
        if (voice.frame >= sample.frames) voices.erase(voices.begin() + voiceIndex);
        else ++voiceIndex;
    }
}

void AudioEngine::Impl::mixMusic(float* output)
{
    MusicStream* stream = nullptr;
    if (activeMode == MusicMode::General) stream = &generalStream;
    else if (activeMode == MusicMode::Boss) stream = &bossStream;
    else if (activeMode == MusicMode::Shop) stream = &shopStream;
    if (!stream) return;

    const float volume = musicVolume.load() * 0.72f;
    for (int frame = 0; frame < BUFFER_FRAMES; ++frame)
    {
        float left = 0.0f;
        float right = 0.0f;
        if (!readOutputFrame(*stream, left, right))
        {
            if (!restartActiveMusic()) return;
            if (activeMode == MusicMode::General) stream = &generalStream;
            else if (activeMode == MusicMode::Boss) stream = &bossStream;
            else stream = &shopStream;
            if (!readOutputFrame(*stream, left, right)) return;
        }
        output[frame * 2] += left * volume;
        output[frame * 2 + 1] += right * volume;
    }
}

void AudioEngine::Impl::renderBlock()
{
    reconcileControllerPorts();
    drainRequests();
    updateMusicMode();

    float mainMix[BUFFER_FRAMES * 2];
    float padMix[4][BUFFER_FRAMES];
    int16_t mainOutput[BUFFER_FRAMES * 2];
    int16_t padOutput[4][BUFFER_FRAMES];
    std::memset(mainMix, 0, sizeof(mainMix));
    std::memset(padMix, 0, sizeof(padMix));
    mixMusic(mainMix);
    const float effectsVolume = soundVolume.load() * 0.88f;
    mixVoiceList(tvVoices, mainMix, true, effectsVolume);
    for (int pad = 0; pad < 4; ++pad) mixVoiceList(padVoices[pad], padMix[pad], false, effectsVolume);

    for (int sample = 0; sample < BUFFER_FRAMES * 2; ++sample)
        mainOutput[sample] = static_cast<int16_t>(clampSample(mainMix[sample]) * 32767.0f);
    for (int pad = 0; pad < 4; ++pad)
        for (int sample = 0; sample < BUFFER_FRAMES; ++sample)
            padOutput[pad][sample] = static_cast<int16_t>(clampSample(padMix[pad][sample]) * 32767.0f);

    OrbisAudioOutOutputParam outputs[5];
    unsigned count = 0;
    outputs[count].handle = mainHandle;
    outputs[count++].pointer = mainOutput;
    for (int pad = 0; pad < 4; ++pad)
    {
        if (padHandles[pad] < 0) continue;
        outputs[count].handle = padHandles[pad];
        outputs[count++].pointer = padOutput[pad];
    }
    if (sceAudioOutOutputs(outputs, count) < 0) sceKernelUsleep(1000);
}

void* AudioEngine::Impl::threadEntry(void* value)
{
    Impl* audio = static_cast<Impl*>(value);
    while (audio->running.load()) audio->renderBlock();
    return nullptr;
}

bool AudioEngine::Impl::initialize()
{
    if (running.load()) return true;
    if (sceAudioOutInit() != 0)
    {
        std::printf("Audio: sceAudioOutInit falhou.\n");
        return false;
    }
    for (int effect = 0; effect < static_cast<int>(SoundEffect::Count); ++effect)
        loadSample(static_cast<SoundEffect>(effect), EFFECT_FILES[effect]);

    mainHandle = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                                 BUFFER_FRAMES, OUTPUT_RATE, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (mainHandle <= 0)
    {
        std::printf("Audio: porta principal indisponivel (%d).\n", mainHandle);
        mainHandle = -1;
        return false;
    }
    if (scePthreadMutexInit(&requestMutex, nullptr, "gw_audio_requests") != 0)
    {
        sceAudioOutClose(mainHandle);
        mainHandle = -1;
        return false;
    }
    mutexReady = true;
    running.store(true);
    if (scePthreadCreate(&thread, nullptr, &Impl::threadEntry, this, "gw_audio") != 0)
    {
        running.store(false);
        scePthreadMutexDestroy(&requestMutex);
        mutexReady = false;
        sceAudioOutClose(mainHandle);
        mainHandle = -1;
        return false;
    }
    threadCreated = true;
    return true;
}

void AudioEngine::Impl::shutdown()
{
    running.store(false);
    if (threadCreated)
    {
        scePthreadJoin(thread, nullptr);
        threadCreated = false;
    }
    for (int pad = 0; pad < 4; ++pad)
    {
        if (padHandles[pad] >= 0) sceAudioOutClose(padHandles[pad]);
        padHandles[pad] = -1;
        openedUsers[pad] = -1;
    }
    if (mainHandle >= 0) sceAudioOutClose(mainHandle);
    mainHandle = -1;
    closeMusic(generalStream);
    closeMusic(bossStream);
    closeMusic(shopStream);
    if (mutexReady)
    {
        scePthreadMutexDestroy(&requestMutex);
        mutexReady = false;
    }
}

void AudioEngine::Impl::queue(SoundEffect effect, int pad)
{
    if (!running.load() || !mutexReady) return;
    const int index = static_cast<int>(effect);
    if (index < 0 || index >= static_cast<int>(SoundEffect::Count) || samples[index].frames == 0) return;
    scePthreadMutexLock(&requestMutex);
    if (requests.size() < 32) requests.push_back({index, pad});
    scePthreadMutexUnlock(&requestMutex);
}

AudioEngine::AudioEngine() : impl_(new Impl()) {}
AudioEngine::~AudioEngine() { delete impl_; }
bool AudioEngine::initialize() { return impl_->initialize(); }
void AudioEngine::shutdown() { impl_->shutdown(); }
void AudioEngine::setMusicMode(MusicMode mode) { impl_->requestedMode.store(static_cast<int>(mode)); }
void AudioEngine::setVolumes(float music, float sound)
{
    impl_->musicVolume.store(std::max(0.0f, std::min(1.0f, music)));
    impl_->soundVolume.store(std::max(0.0f, std::min(1.0f, sound)));
}
void AudioEngine::setControllerUser(int pad, int userId)
{
    if (pad >= 0 && pad < 4) impl_->requestedUsers[pad].store(userId);
}
void AudioEngine::playTv(SoundEffect effect) { impl_->queue(effect, -1); }
void AudioEngine::playController(SoundEffect effect, int pad) { impl_->queue(effect, pad); }
}
