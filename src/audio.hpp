#pragma once

namespace gw
{
enum class MusicMode
{
    General = 0,
    Boss,
    Shop,
    Silent
};

enum class SoundEffect
{
    Coin1 = 0,
    Coin2,
    Grenade,
    BossDestroyed,
    PlayerDamage,
    Heart,
    Count
};

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    bool initialize();
    void shutdown();
    void setMusicMode(MusicMode mode);
    void setVolumes(float music, float sound);
    void setControllerUser(int pad, int userId);
    void playTv(SoundEffect effect);
    void playController(SoundEffect effect, int pad);

private:
    AudioEngine(const AudioEngine&);
    AudioEngine& operator=(const AudioEngine&);

    struct Impl;
    Impl* impl_;
};
}
