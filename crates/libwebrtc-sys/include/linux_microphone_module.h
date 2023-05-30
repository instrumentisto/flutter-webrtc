#pragma once

#include "custom_audio.h"
#include "microphone_module.h"
#include "modules/audio_device/linux/audio_device_pulse_linux.h"

typedef webrtc::adm_linux_pulse::PulseAudioSymbolTable WebRTCPulseSymbolTable;
WebRTCPulseSymbolTable* _GetPulseSymbolTable();

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
  void SourceEnded();
  int32_t StopRecording();
  int32_t StartRecording();
  int32_t RecordingChannels();

  // END MicrophoneModuleInterface
  pa_threaded_mainloop* _paMainloop = nullptr;
  uint16_t _paDeviceIndex = -1;

 private:
  void PaLock();
  void PaUnLock();
  int32_t LatencyUsecs(pa_stream* stream);
  static void PaSourceInfoCallback(pa_context* c,
                                   const pa_source_info* i,
                                   int eol,
                                   void* pThis);
  void PaSourceInfoCallbackHandler(const pa_source_info* i, int eol);

  bool RecThreadProcess();
  int32_t ReadRecordedData(const void* bufferData, size_t bufferSize);
  int32_t ProcessRecordedData(int8_t* bufferData,
                              uint32_t bufferSizeInSamples,
                              uint32_t recDelay);
  bool KeyPressed() const;
  int16_t RecordingDevices();

  void EnableReadCallback();
  void DisableReadCallback();
  static void PaStreamReadCallback(pa_stream* a /*unused1*/,
                                   size_t b /*unused2*/,
                                   void* pThis);
  void PaStreamReadCallbackHandler();
  static void PaContextStateCallback(pa_context* c, void* pThis);
  static void PaServerInfoCallback(pa_context* c,
                                   const pa_server_info* i,
                                   void* pThis);
  void PaContextStateCallbackHandler(pa_context* c);
  void PaServerInfoCallbackHandler(const pa_server_info* i);

  int32_t InitPulseAudio();
  int32_t TerminatePulseAudio();
  void WaitForOperationCompletion(pa_operation* paOperation) const;
  int32_t CheckPulseAudioVersion();
  int32_t InitSamplingFrequency();
  int32_t InitRecording();
  int32_t GetDefaultDeviceInfo(bool recDevice, char* name, uint16_t& index);
  static void PaStreamStateCallback(pa_stream* p, void* pThis);
  void PaStreamStateCallbackHandler(pa_stream* p);
  static void PaStreamOverflowCallback(pa_stream* unused, void* pThis);
  void PaStreamOverflowCallbackHandler();

  int32_t StereoRecordingIsAvailable(bool& available);
  int32_t SetStereoRecording(bool enable);
  int32_t StereoRecording(bool& enabled) const;

  rtc::scoped_refptr<MicrophoneSource> source = nullptr;
  bool _inputDeviceIsSpecified = false;
  int sample_rate_hz_ = 0;
  uint8_t _recChannels = 1;
  bool _initialized = false;
  bool _recording = false;
  bool _recIsInitialized = false;
  bool quit_ = false;
  bool _startRec = false;
  uint32_t _sndCardPlayDelay = 0;
  int _deviceIndex = -1;
  int16_t _numRecDevices = 0;
  char* _playDeviceName = nullptr;
  char* _recDeviceName = nullptr;
  char* _playDisplayDeviceName = nullptr;
  char* _recDisplayDeviceName = nullptr;
  int8_t* _recBuffer = nullptr;
  size_t _recordBufferSize = 0;
  size_t _recordBufferUsed = 0;
  const void* _tempSampleData = nullptr;
  size_t _tempSampleDataSize = 0;
  int32_t _configuredLatencyRec = 0;
  bool _paStateChanged = false;
  bool _ended = false;
  pa_mainloop_api* _paMainloopApi = nullptr;
  pa_context* _paContext = nullptr;
  pa_stream* _recStream = nullptr;
  uint32_t _recStreamFlags = 0;
  pa_stream* _playStream = nullptr;

  // Stores thread ID in constructor.
  // We can then use RTC_DCHECK_RUN_ON(&worker_thread_checker_) to ensure that
  // other methods are called from the same thread.
  // Currently only does RTC_DCHECK(thread_checker_.IsCurrent()).
  webrtc::SequenceChecker thread_checker_;

  rtc::Event _timeEventRec;
  webrtc::Mutex mutex_;
  int16_t _numPlayDevices;
  pa_buffer_attr _recBufferAttr;
  int _inputDeviceIndex;
  rtc::Event _recStartEvent;
  rtc::PlatformThread _ptrThreadRec;
  char _paServerVersion[32];
  webrtc::AudioMixerManagerLinuxPulse _mixerManager;

#if defined(WEBRTC_USE_X11)
  char _oldKeyState[32];
  Display* _XDisplay;
#endif
};
