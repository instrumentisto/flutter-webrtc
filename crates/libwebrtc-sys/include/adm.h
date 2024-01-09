/*
 * This file is modified version of the one from Desktop App Toolkit, a set of
 * libraries for developing nice desktop applications.
 * https://github.com/desktop-app/lib_webrtc/blob/openal/webrtc/details/webrtc_openal_adm.h
 *
 * Copyright (c) 2014-2023 The Desktop App Toolkit Authors.
 *
 * Desktop App Toolkit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * In addition, as a special exception, the copyright holders give permission
 * to link the code of portions of this program with the OpenSSL library.
 *
 * Full license: https://github.com/desktop-app/legal/blob/master/LICENSE
 */

#ifndef BRIDGE_ADM_H_
#define BRIDGE_ADM_H_

#define WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE 1

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>

#include <AL/al.h>
#include <AL/alc.h>

#include "libwebrtc-sys/include/local_audio_source.h"

#include "api/audio/audio_frame.h"
#include "api/audio/audio_mixer.h"
#include "api/media_stream_interface.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_factory.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/audio_device_impl.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_USE_X11)
#include <X11/Xlib.h>
#endif

class AudioDeviceRecorder {
public:
  struct Data;

  AudioDeviceRecorder(std::string deviceId, ALCdevice* device);

  rtc::scoped_refptr<bridge::LocalAudioSource> source;
  ALCdevice* device;
  std::string deviceId;
  std::unique_ptr<Data> data;
};

class OpenALAudioDeviceModule : public webrtc::AudioDeviceModuleImpl {
 public:
  OpenALAudioDeviceModule(AudioLayer audio_layer,
                   webrtc::TaskQueueFactory* task_queue_factory);
  ~OpenALAudioDeviceModule();

  static rtc::scoped_refptr<OpenALAudioDeviceModule> Create(
      AudioLayer audio_layer,
      webrtc::TaskQueueFactory* task_queue_factory);

  static rtc::scoped_refptr<OpenALAudioDeviceModule> CreateForTest(
      AudioLayer audio_layer,
      webrtc::TaskQueueFactory* task_queue_factory);

  // Main initialization and termination.
  int32_t Init() override;

  rtc::scoped_refptr<bridge::LocalAudioSource> CreateAudioSource(uint32_t device_index);

  // Playout control.
  int16_t PlayoutDevices() override;
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(WindowsDeviceType device) override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[webrtc::kAdmMaxDeviceNameSize],
                            char guid[webrtc::kAdmMaxGuidSize]) override;
  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;
  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t StereoPlayoutIsAvailable(bool* available) const override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool* enabled) const override;
  int32_t PlayoutDelay(uint16_t* delayMS) const override;

  int32_t SpeakerVolumeIsAvailable(bool* available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t* volume) const override;
  int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t* minVolume) const override;

  int32_t SpeakerMuteIsAvailable(bool* available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool* enabled) const override;
  int32_t RegisterAudioCallback(webrtc::AudioTransport* audioCallback) override;

  // Capture control.
  int16_t RecordingDevices() override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[webrtc::kAdmMaxDeviceNameSize],
                              char guid[webrtc::kAdmMaxGuidSize]) override;
	int32_t SetRecordingDevice(uint16_t index) override;
	int32_t SetRecordingDevice(WindowsDeviceType device) override;
  int32_t RecordingIsAvailable(bool* available) override;
  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;

  int32_t MicrophoneVolumeIsAvailable(bool* available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t* volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t* minVolume) const override;

  int32_t MicrophoneMuteIsAvailable(bool* available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool* enabled) const override;

  int32_t StereoRecordingIsAvailable(bool* available) const override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool* enabled) const override;

 private:
  struct Data;

  bool _initialized = false;
  std::unique_ptr<Data> _data;

  bool quit = false;

 private:
  int restartPlayout();
  void openPlayoutDevice();

  void startPlayingOnThread();
  void ensureThreadStarted();
  void closePlayoutDevice();
  bool validatePlayoutDeviceId();

  void clearProcessedBuffers();
  bool clearProcessedBuffer();

  void unqueueAllBuffers();

  bool processPlayout();

  // NB! closePlayoutDevice should be called after this, so that next time
  // we start playing, we set the thread local context and event callback.
  void stopPlayingOnThread();

  void processPlayoutQueued();

  void startCaptureOnThread();
  void stopCaptureOnThread();
	int restartRecording();
	void openRecordingDevice();
  void closeRecordingDevice();
  bool processRecordedPart(bool firstInCycle);
  std::chrono::milliseconds countExactQueuedMsForLatency(
      std::chrono::time_point<std::chrono::steady_clock> now,
      bool playing);
  std::chrono::milliseconds queryRecordingLatencyMs();
  void restartRecordingQueued();
  bool validateRecordingDeviceId();
  void processRecordingQueued();

  rtc::Thread* _thread = nullptr;

  std::recursive_mutex _recording_mutex;
  std::string _recordingDeviceId;
  bool _recordingInitialized = false;
  bool _microphoneInitialized = false;
  bool _recordingFailed = false;
  ALCdevice *_recordingDevice = nullptr;
  std::chrono::milliseconds _recordingLatency = std::chrono::milliseconds(0);

  std::recursive_mutex _playout_mutex;
  std::string _playoutDeviceId;
  bool _playoutInitialized = false;
  bool _speakerInitialized = false;
  bool _playoutFailed = false;
  ALCdevice* _playoutDevice = nullptr;
  std::chrono::milliseconds _playoutLatency = std::chrono::milliseconds(0);
  ALCcontext* _playoutContext = nullptr;
  int _playoutChannels = 2;
  bridge::LocalAudioSource* _source;
  std::unordered_map<std::string, AudioDeviceRecorder*> _recorders;
};

#endif // BRIDGE_ADM_H_
