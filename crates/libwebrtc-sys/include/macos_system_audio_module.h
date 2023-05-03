#pragma once

#include "system_audio_module.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"

class SystemModule : public SystemModuleInterface {
    public: 
    SystemModule() {};
    ~SystemModule() {};

    // Initialization and terminate.
    bool Init() { return false; };
    int32_t Terminate() { return 0; };
    rtc::scoped_refptr<AudioSource> CreateSource() { return nullptr; };
    void ResetSource() { return; };

    // Settings.
    void SetRecordingSource(int id) { return; };
    void SetSystemAudioLevel(float level) { return; };
    float GetSystemAudioLevel() const { return 0; };
    int32_t StopRecording() { return 0; };
    int32_t StartRecording() { return 0; };
    int32_t RecordingChannels() { return 0; };

    // Enumerate system audio outputs.
    std::vector<AudioSourceInfo> EnumerateSystemSource() const { return std::vector<AudioSourceInfo>(); };
};