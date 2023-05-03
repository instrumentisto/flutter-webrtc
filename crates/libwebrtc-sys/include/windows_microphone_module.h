#pragma once

#include "custom_audio.h"
#include "microphone_module.h"
#include "modules/audio_device/win/audio_device_core_win.h"
#include "rtc_base\win\scoped_com_initializer.h"

class MicrophoneModule : public MicrophoneModuleInterface {
 public:
  MicrophoneModule();
  ~MicrophoneModule();

  // MicrophoneModuleInterface

  // Initialization and terminate.
  int32_t Init();
  int32_t Terminate();
  int32_t InitMicrophone();

  // Microphone control.
  int32_t MicrophoneMuteIsAvailable(bool* available);
  int32_t SetMicrophoneMute(bool enable);
  int32_t MicrophoneMute(bool* enabled) const;
  bool MicrophoneIsInitialized();
  int32_t MicrophoneVolumeIsAvailable(bool* available);
  int32_t SetMicrophoneVolume(uint32_t volume);
  int32_t MicrophoneVolume(uint32_t* volume) const;
  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const;
  int32_t MinMicrophoneVolume(uint32_t* minVolume) const;

  // Settings.
  int32_t SetRecordingDevice(uint16_t index);

  // Microphone source.
  rtc::scoped_refptr<AudioSource> CreateSource();
  void ResetSource();
  int32_t StopRecording();
  int32_t StartRecording();
  int32_t RecordingChannels();

  // END MicrophoneModuleInterface

 private:
  // Сopied from "audio_device_core_win.h"

  int32_t _EnumerateEndpointDevicesAll(EDataFlow dataFlow) const;
  void _TraceCOMError(HRESULT hr) const;
  int32_t InitMicrophoneLocked() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  int16_t RecordingDevicesLocked() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  int32_t _GetListDevice(EDataFlow dir, int index, IMMDevice** ppDevice);
  int32_t _RefreshDeviceList(EDataFlow dir);
  int16_t _DeviceListCount(EDataFlow dir);
  int32_t _GetDeviceName(IMMDevice* pDevice, LPWSTR pszBuffer, int bufferLen);
  int32_t _GetDefaultDeviceID(EDataFlow dir,
                              ERole role,
                              LPWSTR szBuffer,
                              int bufferLen);
  int16_t RecordingDevices();
  DWORD DoCaptureThreadPollDMO();
  DWORD InitCaptureThreadPriority();
  static DWORD WINAPI WSAPICaptureThreadPollDMO(LPVOID context);
  static DWORD WINAPI WSAPICaptureThread(LPVOID context);
  DWORD DoCaptureThread();
  void RevertCaptureThreadPriority();
  int32_t InitRecording() RTC_LOCKS_EXCLUDED(mutex_);
  int32_t InitRecordingDMO();
  int SetDMOProperties();
  int SetBoolProperty(IPropertyStore* ptrPS,
                      REFPROPERTYKEY key,
                      VARIANT_BOOL value);
  int32_t _GetDefaultDeviceIndex(EDataFlow dir, ERole role, int* index);
  int SetVtI4Property(IPropertyStore* ptrPS, REFPROPERTYKEY key, LONG value);
  int32_t _GetDeviceID(IMMDevice* pDevice, LPWSTR pszBuffer, int bufferLen);
  int32_t _GetDefaultDevice(EDataFlow dir, ERole role, IMMDevice** ppDevice);

  webrtc::ScopedCOMInitializer _comInit;
  webrtc::AudioDeviceBuffer* _ptrAudioBuffer;
  mutable webrtc::Mutex mutex_;
  mutable webrtc::Mutex volume_mutex_ RTC_ACQUIRED_AFTER(mutex_);

  IMMDeviceEnumerator* _ptrEnumerator;
  IMMDeviceCollection* _ptrRenderCollection;
  IMMDeviceCollection* _ptrCaptureCollection;
  IMMDevice* _ptrDeviceOut;
  IMMDevice* _ptrDeviceIn;

  PAvRevertMmThreadCharacteristics _PAvRevertMmThreadCharacteristics;
  PAvSetMmThreadCharacteristicsA _PAvSetMmThreadCharacteristicsA;
  PAvSetMmThreadPriority _PAvSetMmThreadPriority;
  HMODULE _avrtLibrary;
  bool _winSupportAvrt;

  IAudioClient* _ptrClientOut;
  IAudioClient* _ptrClientIn;
  IAudioRenderClient* _ptrRenderClient;
  IAudioCaptureClient* _ptrCaptureClient;
  IAudioEndpointVolume* _ptrCaptureVolume;
  ISimpleAudioVolume* _ptrRenderSimpleVolume;

  // DirectX Media Object (DMO) for the built-in AEC.
  rtc::scoped_refptr<IMediaObject> _dmo;
  rtc::scoped_refptr<IMediaBuffer> _mediaBuffer;
  bool _builtInAecEnabled;

  HANDLE _hRenderSamplesReadyEvent;
  HANDLE _hPlayThread;
  HANDLE _hRenderStartedEvent;
  HANDLE _hShutdownRenderEvent;

  HANDLE _hCaptureSamplesReadyEvent;
  HANDLE _hRecThread;
  HANDLE _hCaptureStartedEvent;
  HANDLE _hShutdownCaptureEvent;

  HANDLE _hMmTask;

  UINT _playAudioFrameSize;
  uint32_t _playSampleRate;
  uint32_t _devicePlaySampleRate;
  uint32_t _playBlockSize;
  uint32_t _devicePlayBlockSize;
  uint32_t _playChannels;
  uint32_t _sndCardPlayDelay;
  UINT64 _writtenSamples;
  UINT64 _readSamples;

  UINT _recAudioFrameSize;
  uint32_t _recSampleRate;
  uint32_t _recBlockSize;
  uint32_t _recChannels;

  uint16_t _recChannelsPrioList[3];
  uint16_t _playChannelsPrioList[2];

  LARGE_INTEGER _perfCounterFreq;
  double _perfCounterFactor;

  bool _initialized;
  bool _recording;
  bool _playing;
  bool _recIsInitialized;
  bool _playIsInitialized;
  bool _speakerIsInitialized;
  bool _microphoneIsInitialized;

  bool _usingInputDeviceIndex;
  bool _usingOutputDeviceIndex;
  webrtc::AudioDeviceModule::WindowsDeviceType _inputDevice;
  webrtc::AudioDeviceModule::WindowsDeviceType _outputDevice;
  uint16_t _inputDeviceIndex;
  uint16_t _outputDeviceIndex;

  // END Сopied from "audio_device_core_win.h"
};