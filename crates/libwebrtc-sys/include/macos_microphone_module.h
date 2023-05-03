#pragma once

#include "custom_audio.h"
#include "microphone_module.h"

class MicrophoneModule : public MicrophoneModuleInterface {
 public:
  MicrophoneModule() {}
  ~MicrophoneModule() {}

  // MicrophoneModuleInterface

  // Initialization and terminate.
  int32_t Init() {return 0;}
  int32_t Terminate() {return 0;}
  int32_t InitMicrophone() {return 0;}

  // Microphone control.
  int32_t MicrophoneMuteIsAvailable(bool* available) {return 0;}
  int32_t SetMicrophoneMute(bool enable) {return 0;}
  int32_t MicrophoneMute(bool* enabled) const {return 0;}
  bool MicrophoneIsInitialized() {return 0;}
  int32_t MicrophoneVolumeIsAvailable(bool* available) {return 0;}
  int32_t SetMicrophoneVolume(uint32_t volume) {return 0;}
  int32_t MicrophoneVolume(uint32_t* volume) const {return 0;}
  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const {return 0;}
  int32_t MinMicrophoneVolume(uint32_t* minVolume) const {return 0;}

  // Settings.
  int32_t SetRecordingDevice(uint16_t index) {return 0;}

  // Microphone source.
  rtc::scoped_refptr<AudioSource> CreateSource() {return nullptr;}
  void ResetSource() {}
  int32_t StopRecording() {return 0;}
  int32_t StartRecording() {return 0;}
  int32_t RecordingChannels() {return 0;}
};