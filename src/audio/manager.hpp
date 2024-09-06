#pragma once

#include <defs/platform.hpp>
#include <util/singleton.hpp>

#ifdef GLOBED_VOICE_SUPPORT

#include <fmod.hpp>

#include <asp/sync.hpp>
#include <asp/thread.hpp>

#include "frame.hpp"
#include "sample_queue.hpp"

struct AudioRecordingDevice {
    int id = -1;
    std::string name;
    FMOD_GUID guid;
    int sampleRate;
    FMOD_SPEAKERMODE speakerMode;
    int speakerModeChannels;
    FMOD_DRIVER_STATE driverState;
};

struct AudioPlaybackDevice {
    int id = -1;
    std::string name;
    FMOD_GUID guid;
    int sampleRate;
    FMOD_SPEAKERMODE speakerMode;
    int speakerModeChannels;
};

constexpr size_t VOICE_TARGET_SAMPLERATE = 24000;
constexpr float VOICE_CHUNK_RECORD_TIME = 0.06f; // the audio buffer that is recorded at once (60ms)
constexpr size_t VOICE_TARGET_FRAMESIZE = VOICE_TARGET_SAMPLERATE * VOICE_CHUNK_RECORD_TIME; // opus framesize
constexpr size_t VOICE_CHANNELS = 1;
constexpr int MAX_AUDIO_CHANNELS = 512;

// This class might thread safe ?
class GLOBED_DLL GlobedAudioManager : public SingletonBase<GlobedAudioManager> {
protected:
    friend class SingletonBase;
    GlobedAudioManager();
    ~GlobedAudioManager();

public:
    // preinitialization, for more info open the implementation
    void preInitialize();

    std::vector<AudioRecordingDevice> getRecordingDevices();
    std::vector<AudioPlaybackDevice> getPlaybackDevices();

    // get the record device by device ID
    std::optional<AudioRecordingDevice> getRecordingDevice(int deviceId);
    // get the playback device by device ID
    std::optional<AudioPlaybackDevice> getPlaybackDevice(int deviceId);

    // get the current active record device
    AudioRecordingDevice getRecordingDevice();
    // get the current active playback device
    AudioPlaybackDevice getPlaybackDevice();

    // get if the recording device is set
    bool isRecordingDeviceSet();

    // if the current selected recording/playback is invalid (i.e. disconnected),
    // it will be reset. if no device is selected or a valid device is selected, nothing happens.
    void validateDevices();

    /* Recording API */

    // set the amount of record frames in a buffer (used by the lowerAudioLatency setting)
    void setRecordBufferCapacity(size_t frames);

    // start recording the voice and call the callback once a full frame is ready.
    // if `stopRecording()` is called at any point, the callback will be called with the remaining data.
    // in that case it may have less than the full 10 frames.
    // WARNING: the callback is called from the audio thread, not the GD/cocos thread.
    Result<> startRecording(std::function<void(const EncodedAudioFrame&)> callback);
    // start recording the voice and call the callback whenever new data is ready.
    // same rules apply as with `startRecording`, except the callback includes raw PCM samples,
    // and is called much more often.
    Result<> startRecordingRaw(std::function<void(const float*, size_t)> callback);
    // tell the audio thread to stop recording
    void stopRecording();
    // tell the audio thread to stop recording, don't call the callback with leftover data
    void haltRecording();
    bool isRecording();

    /* Background recording API */

    // start recording, similar to `startRecording` but the callback is not automatically called,
    // unless `resumePassiveRecording` has been called.
    Result<> startPassiveRecording(std::function<void(const EncodedAudioFrame&)> callback);

    void resumePassiveRecording();
    void pausePassiveRecording();

    /* Misc */

    // play a sound and return the channel associated with it
    [[nodiscard]] FMOD::Channel* playSound(FMOD::Sound* sound);

    // create a sound from raw PCM data
    [[nodiscard]] FMOD::Sound* createSound(const float* pcm, size_t samples, int sampleRate = VOICE_TARGET_SAMPLERATE);

    void setActiveRecordingDevice(int deviceId);
    void setActiveRecordingDevice(const AudioRecordingDevice& device);
    void setActivePlaybackDevice(int deviceId);
    void setActivePlaybackDevice(const AudioPlaybackDevice& device);

    void toggleLoopbacksAllowed(bool allowed);

    // get the cached system
    FMOD::System* getSystem();

    static const char* fmodErrorString(FMOD_RESULT result);
    static std::string formatFmodError(FMOD_RESULT result, const char* whatFailed);
    static const char* getOpusVersion();

private:
    /* devices */
    AudioRecordingDevice recordDevice;
    AudioPlaybackDevice playbackDevice; // unused

    asp::AtomicBool loopbacksAllowed = false;

    /* recording */
    asp::AtomicBool recordActive = false;
    asp::AtomicBool recordQueuedStop = false;
    asp::AtomicBool recordQueuedHalt = false;
    asp::AtomicBool recordingRaw = false;
    asp::AtomicBool recordingPassive = false;
    asp::AtomicBool recordingPassiveActive = false;
    FMOD::Sound* recordSound = nullptr;
    size_t recordChunkSize = 0;
    std::function<void(const EncodedAudioFrame&)> recordCallback;
    std::function<void(const float*, size_t)> recordRawCallback;
    AudioSampleQueue recordQueue;
    unsigned int recordLastPosition = 0;
    EncodedAudioFrame recordFrame;

    Result<> startRecordingInternal(bool passive = false);
    void recordContinueStream();
    void recordInvokeCallback();
    void recordInvokeRawCallback(float* pcm, size_t samples);
    void internalStopRecording();

    AudioEncoder encoder;

    /* misc */
    FMOD::System* cachedSystem = nullptr;

    asp::AtomicBool audioThreadSleeping = true;
    asp::Thread<GlobedAudioManager*> audioThreadHandle;

    void audioThreadFunc(decltype(audioThreadHandle)::StopToken&);
    Result<> audioThreadWork();
};

#else

class GlobedAudioManager : public SingletonBase<GlobedAudioManager> {
protected:
    friend class SingletonBase;
    GlobedAudioManager();
    ~GlobedAudioManager();

public:
    bool isRecording();
};

#endif // GLOBED_VOICE_SUPPORT
