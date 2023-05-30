#pragma once

#include "system_audio_module.h"
#include <pulse/pulseaudio.h>
#include "rtc_base/platform_thread.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"

class SystemModule : public SystemModuleInterface {
    public: 
    SystemModule();
    ~SystemModule();

    // Initiates a pulse audio.
    int32_t InitPulseAudio();

    // Terminates a pulse audio.
    int32_t TerminatePulseAudio();

    // Returns the current sinc id for the current process.
    int32_t UpdateSinkId();

    // Locks pulse audio.
    void PaLock();
    // Unlocks pulse audio.
    void PaUnLock();

    // Redirect callback to `this.PaContextStateCallbackHandler`.
    static void PaContextStateCallback(pa_context* c, void* pThis);

    // Callback to keep track of the state of the pulse audio context.
    void PaContextStateCallbackHandler(pa_context* c);

    // Waiting for the pulse audio operation to completion.
    void WaitForOperationCompletion(pa_operation* paOperation) const;

    // Callback to keep track of the state of the pulse audio stream.
    void PaStreamStateCallbackHandler(pa_stream* p);

    // Initiates the audio stream from the provided `_recSinkId`.
    void InitRecording();

    // Redirect callback to `this.PaStreamStateCallbackHandler`.
    static void PaStreamStateCallback(pa_stream* p, void* pThis);

    // Redirect callback to `this.PaStreamReadCallbackHandler`.
    static void PaStreamReadCallback(pa_stream* a /*unused1*/,
                                    size_t b /*unused2*/,
                                    void* pThis);

    // Used to signal audio reading.
    void PaStreamReadCallbackHandler();

    // Passes an audio frame to an AudioSource.
    int32_t ProcessRecordedData(int8_t* bufferData,
                                uint32_t bufferSizeInSamples,
                                uint32_t recDelay);

    // Disables signals for audio recording.
    void DisableReadCallback();

    // Enaables signals for audio recording.
    void EnableReadCallback();

    // Function to capture audio in another thread.
    bool RecThreadProcess();


    // Interface
    // Initialization and terminate.
    bool Init();
    int32_t Terminate();
    rtc::scoped_refptr<AudioSource> CreateSource();
    void ResetSource();

    // Settings.
    void SetRecordingSource(int id);
    void SetSystemAudioLevel(float level);
    float GetSystemAudioLevel() const;
    int32_t StopRecording();
    int32_t StartRecording();
    int32_t RecordingChannels();
    int32_t ReadRecordedData(const void* bufferData, size_t bufferSize);

    // Enumerate system audio outputs.
    std::vector<AudioSourceInfo> EnumerateSystemSource() const;

    bool _initialized = false;
    bool _paStateChanged = false;
    rtc::PlatformThread _ptrThreadRec;
    rtc::Event _timeEventRec;
    rtc::Event _recStartEvent;
    bool quit_ = false;
    bool _startRec = false;
    bool _recording = false;
    bool _recIsInitialized = false;
    const void* _tempSampleData = nullptr;
    size_t _tempSampleDataSize = 0;
    bool _mute = false;
    bool _on_new = false;
    float audio_multiplier = 1.0;
    int _recProcessId = -1;
    int _recSinkId = -1;
    int sample_rate_hz_ = 48000;
    uint8_t _recChannels = 2;
    char _paServerVersion[32];
    int32_t _configuredLatencyRec = 0;
    uint32_t _recStreamFlags = 0;
    int8_t* _recBuffer = nullptr;
    size_t _recordBufferSize = 0;
    size_t _recordBufferUsed = 0;
    pa_buffer_attr _recBufferAttr;
    webrtc::Mutex mutex_;
    pa_threaded_mainloop* _paMainloop = nullptr;
    pa_mainloop_api* _paMainloopApi = nullptr;
    pa_context* _paContext = nullptr;
    pa_stream* _recStream = nullptr;
    std::vector<int16_t> release_capture_buffer = std::vector<int16_t>(480 * 8);
};