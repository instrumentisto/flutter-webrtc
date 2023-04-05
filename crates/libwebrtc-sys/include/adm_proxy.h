#pragma once

#include "modules/audio_device/include/audio_device.h"
#include "pc/proxy.h"
#include "linux_micro.h"
#include "adm.h"

namespace webrtc {

using AudioDeviceModuleInterface = AudioDeviceModule;

// Define proxy for `AudioDeviceModule`.
BEGIN_PRIMARY_PROXY_MAP(AudioDeviceModule)
PROXY_PRIMARY_THREAD_DESTRUCTOR()
PROXY_CONSTMETHOD1(int32_t, ActiveAudioLayer, AudioDeviceModule::AudioLayer*)
PROXY_METHOD1(int32_t, RegisterAudioCallback, AudioTransport*)
PROXY_METHOD0(int32_t, Init)
PROXY_METHOD0(int32_t, Terminate)
PROXY_CONSTMETHOD0(bool, Initialized)
PROXY_METHOD0(int16_t, PlayoutDevices)
PROXY_METHOD0(int16_t, RecordingDevices)
PROXY_METHOD3(int32_t, PlayoutDeviceName, uint16_t, char*, char*)
PROXY_METHOD3(int32_t, RecordingDeviceName, uint16_t, char*, char*)
PROXY_METHOD1(int32_t, SetPlayoutDevice, uint16_t)
PROXY_METHOD1(int32_t, SetPlayoutDevice, WindowsDeviceType)
PROXY_METHOD1(int32_t, SetRecordingDevice, uint16_t)
PROXY_METHOD1(int32_t, SetRecordingDevice, WindowsDeviceType)
PROXY_METHOD1(int32_t, PlayoutIsAvailable, bool*)
PROXY_METHOD0(int32_t, InitPlayout)
PROXY_CONSTMETHOD0(bool, PlayoutIsInitialized)
PROXY_METHOD1(int32_t, RecordingIsAvailable, bool*)
PROXY_METHOD0(int32_t, InitRecording)
PROXY_CONSTMETHOD0(bool, RecordingIsInitialized)
PROXY_METHOD0(int32_t, StartPlayout)
PROXY_METHOD0(int32_t, StopPlayout)
PROXY_CONSTMETHOD0(bool, Playing)
PROXY_METHOD0(int32_t, StartRecording)
PROXY_METHOD0(int32_t, StopRecording)
PROXY_CONSTMETHOD0(bool, Recording)
PROXY_METHOD0(int32_t, InitSpeaker)
PROXY_CONSTMETHOD0(bool, SpeakerIsInitialized)
PROXY_METHOD0(int32_t, InitMicrophone)
PROXY_CONSTMETHOD0(bool, MicrophoneIsInitialized)
PROXY_METHOD1(int32_t, SpeakerVolumeIsAvailable, bool*)
PROXY_METHOD1(int32_t, SetSpeakerVolume, uint32_t)
PROXY_CONSTMETHOD1(int32_t, SpeakerVolume, uint32_t*)
PROXY_CONSTMETHOD1(int32_t, MaxSpeakerVolume, uint32_t*)
PROXY_CONSTMETHOD1(int32_t, MinSpeakerVolume, uint32_t*)
PROXY_METHOD1(int32_t, MicrophoneVolumeIsAvailable, bool*)
PROXY_METHOD1(int32_t, SetMicrophoneVolume, uint32_t)
PROXY_CONSTMETHOD1(int32_t, MicrophoneVolume, uint32_t*)
PROXY_CONSTMETHOD1(int32_t, MaxMicrophoneVolume, uint32_t*)
PROXY_CONSTMETHOD1(int32_t, MinMicrophoneVolume, uint32_t*)
PROXY_METHOD1(int32_t, SpeakerMuteIsAvailable, bool*)
PROXY_METHOD1(int32_t, SetSpeakerMute, bool)
PROXY_CONSTMETHOD1(int32_t, SpeakerMute, bool*)
PROXY_METHOD1(int32_t, MicrophoneMuteIsAvailable, bool*)
PROXY_METHOD1(int32_t, SetMicrophoneMute, bool)
PROXY_CONSTMETHOD1(int32_t, MicrophoneMute, bool*)
PROXY_CONSTMETHOD1(int32_t, StereoPlayoutIsAvailable, bool*)
PROXY_METHOD1(int32_t, SetStereoPlayout, bool)
PROXY_CONSTMETHOD1(int32_t, StereoPlayout, bool*)
PROXY_CONSTMETHOD1(int32_t, StereoRecordingIsAvailable, bool*)
PROXY_METHOD1(int32_t, SetStereoRecording, bool)
PROXY_CONSTMETHOD1(int32_t, StereoRecording, bool*)
PROXY_CONSTMETHOD1(int32_t, PlayoutDelay, uint16_t*)
PROXY_CONSTMETHOD0(bool, BuiltInAECIsAvailable)
PROXY_CONSTMETHOD0(bool, BuiltInAGCIsAvailable)
PROXY_CONSTMETHOD0(bool, BuiltInNSIsAvailable)
PROXY_METHOD1(int32_t, EnableBuiltInAEC, bool)
PROXY_METHOD1(int32_t, EnableBuiltInAGC, bool)
PROXY_METHOD1(int32_t, EnableBuiltInNS, bool)
PROXY_CONSTMETHOD0(int32_t, GetPlayoutUnderrunCount)
#if defined(WEBRTC_IOS)
  PROXY_CONSTMETHOD1(int, GetPlayoutAudioParameters, AudioParameters*)
  PROXY_CONSTMETHOD1(int, GetRecordAudioParameters, AudioParameters*)
#endif  // WEBRTC_IOS
END_PROXY_MAP(AudioDeviceModule)
}  // namespace webrtc

class SourceManagerProxy : SourceManager {
  SourceManagerProxy(rtc::Thread* primary_thread, rtc::scoped_refptr<ADM> c) : adm(c), primary_thread_(primary_thread) {}
  public:
  static std::unique_ptr<SourceManager> Create( 
      rtc::Thread* primary_thread, 
      rtc::scoped_refptr<ADM> c) {
         return std::unique_ptr<SourceManager>(new SourceManagerProxy(primary_thread, std::move(c)));
      }

  webrtc::AudioMixer::Source* CreateMicroSource() override {
    TRACE_BOILERPLATE(CreateMicroSource);              
    webrtc::MethodCall<SourceManager,  webrtc::AudioMixer::Source*> call(adm.get(), &SourceManager::CreateMicroSource); 
    return call.Marshal(primary_thread_);
  };

  webrtc::AudioMixer::Source* CreateSystemSource() override {
    std::cout << "P ADDD" << std::endl;
    TRACE_BOILERPLATE(CreateSystemSource);      
    webrtc::MethodCall<SourceManager,  webrtc::AudioMixer::Source*> call(adm.get(), &SourceManager::CreateSystemSource); 
    return call.Marshal(primary_thread_);
  }

  void AddSource(webrtc::AudioMixer::Source* source) override {
    TRACE_BOILERPLATE(AddSource);
    webrtc::MethodCall<SourceManager, void, webrtc::AudioMixer::Source*> call(adm.get(), &SourceManager::AddSource, std::move(source));
    return call.Marshal(primary_thread_);  
  }

  void RemoveSource(webrtc::AudioMixer::Source* source) override {
    TRACE_BOILERPLATE(RemoveSource);
    webrtc::MethodCall<SourceManager, void, webrtc::AudioMixer::Source*> call(adm.get(), &SourceManager::RemoveSource, std::move(source));
    return call.Marshal(primary_thread_);  
  }

  private:              
  mutable rtc::scoped_refptr<ADM> adm;                                               
  mutable rtc::Thread* primary_thread_;
};
