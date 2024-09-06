#pragma once
#include <defs/platform.hpp>

#include <asp/thread/Thread.hpp>
#include <asp/sync/Atomic.hpp>

#include <util/singleton.hpp>

class GLOBED_DLL VoiceRecordingManager : public SingletonBase<VoiceRecordingManager> {
protected:
    VoiceRecordingManager();
    friend class SingletonBase;

public:
#ifdef GLOBED_VOICE_SUPPORT
    asp::Thread<VoiceRecordingManager*> thread;
    asp::AtomicBool queuedStop = false, queuedStart = false, recording = false;

    void threadFunc(decltype(thread)::StopToken&);
#endif // GLOBED_VOICE_SUPPORT

    void startRecording();
    void stopRecording();
    bool isRecording();

private:
    void resetBools(bool recording);
};
