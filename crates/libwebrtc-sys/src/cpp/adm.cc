/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>

#include <chrono>
#include <thread>
#include "adm.h"
#include "api/make_ref_counted.h"
#include "common_audio/wav_file.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"

// Main initializaton and termination
int32_t CustomAudioDeviceModule::Init() {
  if (webrtc::AudioDeviceModuleImpl::Init() != 0) {
    return -1;
  }
  return audio_recorder->Init();
};

int32_t CustomAudioDeviceModule::Terminate() {
  quit = true;
  return 0;
}

CustomAudioDeviceModule::~CustomAudioDeviceModule() {
  Terminate();
}

rtc::scoped_refptr<CustomAudioDeviceModule> CustomAudioDeviceModule::Create(
    AudioLayer audio_layer,
    webrtc::TaskQueueFactory* task_queue_factory) {
  return CustomAudioDeviceModule::CreateForTest(audio_layer,
                                                task_queue_factory);
}

int32_t CustomAudioDeviceModule::SetRecordingDevice(uint16_t index) {
  return audio_recorder->SetRecordingDevice(index);
}

int32_t CustomAudioDeviceModule::InitMicrophone() {
  return audio_recorder->InitMicrophone();
}

bool CustomAudioDeviceModule::MicrophoneIsInitialized() const {
  return audio_recorder->MicrophoneIsInitialized();
}

rtc::scoped_refptr<AudioSource> CustomAudioDeviceModule::CreateSystemSource() {
  auto system = system_recorder->CreateSource();
  return system;
}

std::vector<AudioSourceInfo> CustomAudioDeviceModule::EnumerateSystemSource()
    const {
  return system_recorder->EnumerateSystemSource();
}

void CustomAudioDeviceModule::SetRecordingSource(int id) {
  system_recorder->SetRecordingSource(id);
};

rtc::scoped_refptr<AudioSource>
CustomAudioDeviceModule::CreateMicrophoneSource() {
  auto microphone = audio_recorder->CreateSource();
  return microphone;
}

void CustomAudioDeviceModule::SetSystemAudioVolume(float level) {
  system_recorder->SetSystemAudioLevel(level);
}

float CustomAudioDeviceModule::GetSystemAudioVolume() const {
  return system_recorder->GetSystemAudioLevel();
}

void CustomAudioDeviceModule::AddSource(
    rtc::scoped_refptr<AudioSource> source) {
  {
    std::unique_lock<std::mutex> lock(source_mutex);
    sources.push_back(source);
    cv.notify_all();
  }
  mixer->AddSource(source.get());
}

void CustomAudioDeviceModule::RemoveSource(
    rtc::scoped_refptr<AudioSource> source) {
  {
    std::unique_lock<std::mutex> lock(source_mutex);
    for (int i = 0; i < sources.size(); ++i) {
      if (sources[i] == source) {
        sources.erase(sources.begin() + i);
        break;
      }
    }
  }
  mixer->RemoveSource(source.get());
}

// Microphone mute control
int32_t CustomAudioDeviceModule::MicrophoneVolumeIsAvailable(bool* available) {
  return audio_recorder->MicrophoneVolumeIsAvailable(available);
}
int32_t CustomAudioDeviceModule::SetMicrophoneVolume(uint32_t volume) {
  return audio_recorder->SetMicrophoneVolume(volume);
}
int32_t CustomAudioDeviceModule::MicrophoneVolume(uint32_t* volume) const {
  return audio_recorder->MicrophoneVolume(volume);
}
int32_t CustomAudioDeviceModule::MaxMicrophoneVolume(
    uint32_t* maxVolume) const {
  return audio_recorder->MaxMicrophoneVolume(maxVolume);
}
int32_t CustomAudioDeviceModule::MinMicrophoneVolume(
    uint32_t* minVolume) const {
  return audio_recorder->MinMicrophoneVolume(minVolume);
}
int32_t CustomAudioDeviceModule::MicrophoneMuteIsAvailable(bool* available) {
  return audio_recorder->MicrophoneMuteIsAvailable(available);
}
int32_t CustomAudioDeviceModule::SetMicrophoneMute(bool enable) {
  return audio_recorder->SetMicrophoneMute(enable);
}
int32_t CustomAudioDeviceModule::MicrophoneMute(bool* enabled) const {
  return audio_recorder->MicrophoneMute(enabled);
}

// Audio device module delegates StartRecording to `audio_recorder`.
int32_t CustomAudioDeviceModule::StartRecording() {
  return 0;
}

rtc::scoped_refptr<CustomAudioDeviceModule>
CustomAudioDeviceModule::CreateForTest(
    AudioLayer audio_layer,
    webrtc::TaskQueueFactory* task_queue_factory) {
  // The "AudioDeviceModule::kWindowsCoreAudio2" audio layer has its own
  // dedicated factory method which should be used instead.
  if (audio_layer == AudioDeviceModule::kWindowsCoreAudio2) {
    return nullptr;
  }

  // Create the generic reference counted (platform independent) implementation.
  auto audio_device = rtc::make_ref_counted<CustomAudioDeviceModule>(
      audio_layer, task_queue_factory);

  // Ensure that the current platform is supported.
  if (audio_device->CheckPlatform() == -1) {
    return nullptr;
  }

  // Create the platform-dependent implementation.
  if (audio_device->CreatePlatformSpecificObjects() == -1) {
    return nullptr;
  }

  // Ensure that the generic audio buffer can communicate with the platform
  // specific parts.
  if (audio_device->AttachAudioBuffer() == -1) {
    return nullptr;
  }

  audio_device->RecordProcess();
  return audio_device;
}

CustomAudioDeviceModule::CustomAudioDeviceModule(
    AudioLayer audio_layer,
    webrtc::TaskQueueFactory* task_queue_factory)
    : webrtc::AudioDeviceModuleImpl(audio_layer, task_queue_factory),
      audio_recorder(new MicrophoneModule()),
      system_recorder(new SystemModule()) {}

void CustomAudioDeviceModule::RecordProcess() {
  const auto attributes =
      rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kRealtime);
  ptrThreadRec = rtc::PlatformThread::SpawnJoinable(
      [this] {
        webrtc::AudioFrame frame;
        auto cb = GetAudioDeviceBuffer();
        while (!quit) {
          {
            std::unique_lock<std::mutex> lock(source_mutex);
            cv.wait(lock, [&]() { return sources.size() > 0; });
          }

          mixer->Mix(std::max(system_recorder->RecordingChannels(),
                              audio_recorder->RecordingChannels()),
                     &frame);
          cb->SetRecordingChannels(frame.num_channels());
          cb->SetRecordingSampleRate(frame.sample_rate_hz());
          cb->SetRecordedBuffer(frame.data(), frame.sample_rate_hz() / 100);
          cb->DeliverRecordedData();
        }
      },
      "audio_device_module_rec_thread", attributes);
}
