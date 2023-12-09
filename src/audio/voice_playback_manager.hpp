#pragma once
#include <defs.hpp>

#if GLOBED_VOICE_SUPPORT

#include "stream.hpp"

/*
* VoicePlaybackManager is responsible for playing voices of multiple people
* at the same time efficiently and without memory leaks.
* Not thread safe.
*/
class VoicePlaybackManager : GLOBED_SINGLETON(VoicePlaybackManager) {
public:
    VoicePlaybackManager();

    void playFrameStreamed(int playerId, const EncodedAudioFrame& frame);
    void stopAllStreams();

    void prepareStream(int playerId);
    void removeStream(int playerId);
    bool isSpeaking(int playerId);

private:
    std::unordered_map<int, std::unique_ptr<AudioStream>> streams;
};

#endif // GLOBED_VOICE_SUPPORT
