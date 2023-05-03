#pragma once


#define WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE 1
#include <iostream>
#include "api/task_queue/task_queue_factory.h"
#include "modules/audio_device/audio_device_impl.h"

#include <memory>

#include "api/media_stream_interface.h"
#include "api/sequence_checker.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/audio_device_defines.h"

#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

#include "modules/audio_mixer/audio_mixer_impl.h"

#include "api/audio/audio_mixer.h"

#include "api/audio/audio_frame.h"

#include "custom_audio.h"

#if defined(WEBRTC_USE_X11)
#include <X11/Xlib.h>
#endif

#include "microphone_module.h"
#include "system_audio_module.h"

#if defined(WEBRTC_LINUX)
#include "modules/audio_device/linux/audio_mixer_manager_pulse_linux.h"
#include "modules/audio_device/linux/pulseaudiosymboltable_linux.h"
#include "linux_system_audio_module.h"
#include "linux_microphone_module.h"
#endif

#if defined(WEBRTC_WIN)
#include "windows_microphone_module.h"
#include "windows_system_audio_module.h"
#endif

#if defined(WEBRTC_MAC)
#include "macos_microphone_module.h"
#include "macos_system_audio_module.h"
#endif



class AudioSourceManager {
  public:
  // Creates a `AudioSource` from a microphone.
  virtual rtc::scoped_refptr<AudioSource> CreateMicrophoneSource() = 0;
  // Creates a `AudioSource` from a system audio.
  virtual rtc::scoped_refptr<AudioSource> CreateSystemSource() = 0;
  // Enumerates possible system audio sources.
  virtual std::vector<AudioSourceInfo> EnumerateSystemSource() const = 0;
  // Sets the system audio source.
  virtual void SetRecordingSource(int id) = 0;
  // Sets the volume of the system audio capture.
  virtual void SetSystemAudioVolume(float volume) = 0;
  // Returns the current volume of the system audio capture.
  virtual float GetSystemAudioVolume() const = 0;
  // Adds `AudioSource` to `AudioSourceManager`.
  virtual void AddSource(rtc::scoped_refptr<AudioSource> source) = 0;
  // Removes `AudioSource` to `AudioSourceManager`.
  virtual void RemoveSource(rtc::scoped_refptr<AudioSource> source) = 0;
};


class CustomAudioDeviceModule : public webrtc::AudioDeviceModuleImpl, public AudioSourceManager {
 public:
  CustomAudioDeviceModule(AudioLayer audio_layer, webrtc::TaskQueueFactory* task_queue_factory);
  ~CustomAudioDeviceModule();


  static rtc::scoped_refptr<CustomAudioDeviceModule> Create(
      AudioLayer audio_layer,
      webrtc::TaskQueueFactory* task_queue_factory);

  static rtc::scoped_refptr<CustomAudioDeviceModule> CreateForTest(
      AudioLayer audio_layer,
      webrtc::TaskQueueFactory* task_queue_factory);

  // Mixes source and sends on.
  void RecordProcess();

  // Main initializaton and termination.
  int32_t Init() override;
  int32_t Terminate();
  int32_t StartRecording() override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;

  // Microphone volume controls.
  int32_t MicrophoneVolumeIsAvailable(bool* available)  override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t* volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t* minVolume) const override;

  // AudioSourceManager interface.
  rtc::scoped_refptr<AudioSource> CreateMicrophoneSource() override;
  rtc::scoped_refptr<AudioSource>  CreateSystemSource() override;
  std::vector<AudioSourceInfo> EnumerateSystemSource() const override;
  void SetRecordingSource(int id) override;
  void SetSystemAudioVolume(float level) override;
  float GetSystemAudioVolume() const override;
  void AddSource(rtc::scoped_refptr<AudioSource>  source) override;
  void RemoveSource(rtc::scoped_refptr<AudioSource>  source) override;

  // Microphone mute control.
  int32_t MicrophoneMuteIsAvailable(bool* available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool* enabled) const override;

  private:
  // Mixes `AudioSource` to send.
  rtc::scoped_refptr<webrtc::AudioMixerImpl> mixer = webrtc::AudioMixerImpl::Create();

  // `AudioSource` for mixing.
  std::vector<rtc::scoped_refptr<AudioSource>> sources;
  std::mutex source_mutex;
  
  // Audio capture module.
  std::unique_ptr<MicrophoneModuleInterface> audio_recorder;
  std::unique_ptr<SystemModuleInterface> system_recorder;
  
  // Thread for processing audio frames.
  rtc::PlatformThread ptrThreadRec;

  // Used to wait for audio sources.
  std::condition_variable cv;
  bool quit = false;
};