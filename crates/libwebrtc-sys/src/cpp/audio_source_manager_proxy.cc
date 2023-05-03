
#include "audio_source_manager_proxy.h"

  AudioSourceManagerProxy::AudioSourceManagerProxy(rtc::Thread* primary_thread,
                          rtc::scoped_refptr<CustomAudioDeviceModule> c)
      : adm(c), primary_thread_(primary_thread) {}

  std::unique_ptr<AudioSourceManager> AudioSourceManagerProxy::Create(
      rtc::Thread* primary_thread,
      rtc::scoped_refptr<CustomAudioDeviceModule> c) {
    return std::unique_ptr<AudioSourceManager>(
        new AudioSourceManagerProxy(primary_thread, std::move(c)));
  }

  rtc::scoped_refptr<AudioSource> AudioSourceManagerProxy::CreateMicrophoneSource() {
    TRACE_BOILERPLATE(CreateMicrophoneSource);
    webrtc::MethodCall<AudioSourceManager, rtc::scoped_refptr<AudioSource>>
        call(adm.get(), &AudioSourceManager::CreateMicrophoneSource);
    return call.Marshal(primary_thread_);
  };

  rtc::scoped_refptr<AudioSource> AudioSourceManagerProxy::CreateSystemSource() {
    TRACE_BOILERPLATE(CreateSystemSource);
    webrtc::MethodCall<AudioSourceManager, rtc::scoped_refptr<AudioSource>>
        call(adm.get(), &AudioSourceManager::CreateSystemSource);
    return call.Marshal(primary_thread_);
  }

  std::vector<AudioSourceInfo> AudioSourceManagerProxy::EnumerateSystemSource() const {
    TRACE_BOILERPLATE(EnumerateSystemSource);
    webrtc::ConstMethodCall<AudioSourceManager, std::vector<AudioSourceInfo>>
        call(adm.get(), &AudioSourceManager::EnumerateSystemSource);
    return call.Marshal(primary_thread_);
  }

  void AudioSourceManagerProxy::SetSystemAudioVolume(float level) {
    TRACE_BOILERPLATE(SetSystemAudioVolume);
    webrtc::MethodCall<AudioSourceManager, void, float>
        call(adm.get(), &AudioSourceManager::SetSystemAudioVolume, std::move(level));
    return call.Marshal(primary_thread_);
  }

  float AudioSourceManagerProxy::GetSystemAudioVolume() const {
    TRACE_BOILERPLATE(GetSystemAudioVolume);
    webrtc::ConstMethodCall<AudioSourceManager, float>
        call(adm.get(), &AudioSourceManager::GetSystemAudioVolume);
    return call.Marshal(primary_thread_);
  }

  void AudioSourceManagerProxy::SetRecordingSource(int id) {
    TRACE_BOILERPLATE(SetRecordingSource);
    webrtc::MethodCall<AudioSourceManager, void, int>
        call(adm.get(), &AudioSourceManager::SetRecordingSource, std::move(id));
    return call.Marshal(primary_thread_);
  }


  void AudioSourceManagerProxy::AddSource(rtc::scoped_refptr<AudioSource> source) {
    TRACE_BOILERPLATE(AddSource);
    webrtc::MethodCall<AudioSourceManager, void,
                       rtc::scoped_refptr<AudioSource>>
        call(adm.get(), &AudioSourceManager::AddSource, std::move(source));
    return call.Marshal(primary_thread_);
  }

  void AudioSourceManagerProxy::RemoveSource(rtc::scoped_refptr<AudioSource> source) {
    TRACE_BOILERPLATE(RemoveSource);
    webrtc::MethodCall<AudioSourceManager, void,
                       rtc::scoped_refptr<AudioSource>>
        call(adm.get(), &AudioSourceManager::RemoveSource, std::move(source));
    return call.Marshal(primary_thread_);
  }