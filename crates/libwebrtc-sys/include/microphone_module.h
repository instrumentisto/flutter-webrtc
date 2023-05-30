
#pragma once
#include "custom_audio.h"


class MicrophoneModuleInterface;

// Microphone audio source.
class MicrophoneSource : public AudioSource {
 public:
  MicrophoneSource(MicrophoneModuleInterface* module);
  ~MicrophoneSource();

 private:
  // Microphone capture module.
  MicrophoneModuleInterface* module;
  // Used to control the module.
  static int sources_num;
};

// Interface to provide microphone audio capture.
class MicrophoneModuleInterface {
public:

// Initialization and terminate.
virtual int32_t Init() = 0;
virtual int32_t Terminate() = 0;
virtual int32_t InitMicrophone() = 0;

// Microphone control.
virtual int32_t MicrophoneMuteIsAvailable(bool* available) = 0;
virtual int32_t SetMicrophoneMute(bool enable) = 0;
virtual int32_t MicrophoneMute(bool* enabled) const = 0;
virtual bool MicrophoneIsInitialized () = 0;
virtual int32_t MicrophoneVolumeIsAvailable(bool* available) = 0;
virtual int32_t SetMicrophoneVolume(uint32_t volume) = 0;
virtual int32_t MicrophoneVolume(uint32_t* volume) const = 0;
virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const = 0;
virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const = 0;

// Settings.
virtual int32_t SetRecordingDevice(uint16_t index) = 0;

// Microphone source.
virtual rtc::scoped_refptr<AudioSource> CreateSource() = 0;
virtual void ResetSource() = 0;
virtual int32_t StopRecording() = 0;
virtual int32_t StartRecording() = 0;
virtual int32_t RecordingChannels() = 0;
virtual void SourceEnded() = 0;
rtc::scoped_refptr<MicrophoneSource> source = nullptr;
};


