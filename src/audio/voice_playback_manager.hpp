#pragma once
#include <defs/platform.hpp>
#include <defs/minimal_geode.hpp>

#include "stream.hpp"
#include <util/time.hpp>
#include <util/singleton.hpp>

/*
* VoicePlaybackManager is responsible for playing voices of multiple people
* at the same time efficiently and without memory leaks (?).
* Not thread safe.
*/
class GLOBED_DLL VoicePlaybackManager : public SingletonBase<VoicePlaybackManager> {
public:
#ifdef GLOBED_VOICE_SUPPORT
    Result<> playFrameStreamed(int playerId, const EncodedAudioFrame& frame);
#endif
    void playRawDataStreamed(int playerId, const float* pcm, size_t samples);
    void stopAllStreams();

    void prepareStream(int playerId);
    void removeStream(int playerId);
    bool isSpeaking(int playerId);
    void setVolume(int playerId, float volume);
    float getVolume(int playerId);
    void muteEveryone();
    void setVolumeAll(float volume);

    void updateEstimator(int playerId, float dt);
    void updateAllEstimators(float dt);

    float getLoudness(int playerId);
    util::time::time_point getLastPlaybackTime(int playerId);

    void forEachStream(std::function<void(int, AudioStream&)> func);

private:
#ifdef GLOBED_VOICE_SUPPORT
    std::unordered_map<int, std::unique_ptr<AudioStream>> streams;
#endif
};
