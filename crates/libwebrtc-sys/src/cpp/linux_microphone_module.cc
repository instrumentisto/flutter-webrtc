#if WEBRTC_LINUX

#include "linux_microphone_module.h"
#include <iostream>
#include "api/make_ref_counted.h"
#include "rtc_base/logging.h"

WebRTCPulseSymbolTable* _GetPulseSymbolTable() {
  static WebRTCPulseSymbolTable* pulse_symbol_table =
      new WebRTCPulseSymbolTable();
  return pulse_symbol_table;
}

// Accesses Pulse functions through our late-binding symbol table instead of
// directly. This way we don't have to link to libpulse, which means our binary
// will work on systems that don't have it.
#define LATE(sym)                                             \
  LATESYM_GET(webrtc::adm_linux_pulse::PulseAudioSymbolTable, \
              _GetPulseSymbolTable(), sym)

void MicrophoneModule::PaServerInfoCallback(pa_context* c,
                                            const pa_server_info* i,
                                            void* pThis) {
  static_cast<MicrophoneModule*>(pThis)->PaServerInfoCallbackHandler(i);
}

static void subscribe_result(pa_context *c, int success, void *userdata) {
  auto thisP = (MicrophoneModule*) userdata;
  pa_threaded_mainloop_signal(thisP->_paMainloop, 0);
}

static void subscribe_sink_event(pa_context* c,
                          pa_subscription_event_type_t t,
                          uint32_t idx,
                          void* userdata) {
  auto thisP = (MicrophoneModule*)userdata;
  if (idx == thisP->_paDeviceIndex && t & pa_subscription_event_type_t::PA_SUBSCRIPTION_EVENT_REMOVE) {
    thisP->SourceEnded();
  }
}


int MicrophoneSource::sources_num = 0;
MicrophoneSource::MicrophoneSource(MicrophoneModuleInterface* module) {
  this->module = module;
  if (sources_num == 0) {
    module->StartRecording();
  }
  ++sources_num;
}

MicrophoneSource::~MicrophoneSource() {
  --sources_num;
  if (sources_num == 0) {
    module->StopRecording();
    module->ResetSource();
  }
}

int32_t MicrophoneModule::RecordingChannels() {
  return _recChannels;
}

void MicrophoneModule::ResetSource() {
  webrtc::MutexLock lock(&mutex_);
  source = nullptr;
}


rtc::scoped_refptr<AudioSource> MicrophoneModule::CreateSource() {
  if (!_recording) {
    InitRecording();
  }

  if (!source) {
    source = rtc::scoped_refptr<MicrophoneSource>(new MicrophoneSource(this));
  }
  auto result = source;
  source->Release();
  return result;
}

void MicrophoneModule::SourceEnded() {
 _ended = true;
}

int32_t MicrophoneModule::SetRecordingDevice(uint16_t index) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  auto start = _recIsInitialized;
  StopRecording();
  if (_recIsInitialized) {
    return -1;
  }

  const uint16_t nDevices(RecordingDevices());

  RTC_LOG(LS_VERBOSE) << "number of availiable input devices is " << nDevices;

  if (index > (nDevices - 1)) {
    RTC_LOG(LS_ERROR) << "device index is out of range [0," << (nDevices - 1)
                      << "]";
    return -1;
  }

  _inputDeviceIndex = index;
  _inputDeviceIsSpecified = true;

  if (start) {
    InitRecording();
    StartRecording();
  }
  return 0;
}


int32_t MicrophoneModule::ProcessRecordedData(int8_t* bufferData,
                                              uint32_t bufferSizeInSamples,
                                              uint32_t recDelay)
    RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
  // TODO(andrew): this is a temporary hack, to avoid non-causal far- and
  // near-end signals at the AEC for PulseAudio. I think the system delay is
  // being correctly calculated here, but for legacy reasons we add +10 ms
  // to the value in the AEC. The real fix will be part of a larger
  // investigation into managing system delay in the AEC.
  if (recDelay > 10)
    recDelay -= 10;
  else
    recDelay = 0;

  if (source != nullptr) {
    source->UpdateFrame((const int16_t*)bufferData, bufferSizeInSamples,
                        sample_rate_hz_, _recChannels);
  }

  // We have been unlocked - check the flag again.
  if (!_recording) {
    return -1;
  }

  return 0;
}



// -------------------------------------------------------------------------------
//  Everything below has been copied unchanged from audio_device_pulse_linux.cc
// -------------------------------------------------------------------------------



void MicrophoneModule::PaServerInfoCallbackHandler(const pa_server_info* i) {
  // Use PA native sampling rate
  sample_rate_hz_ = i->sample_spec.rate;

  // Copy the PA server version
  strncpy(_paServerVersion, i->server_version, 31);
  _paServerVersion[31] = '\0';

  if (_recDisplayDeviceName) {
    // Copy the source name
    strncpy(_recDisplayDeviceName, i->default_source_name,
            webrtc::kAdmMaxDeviceNameSize);
    _recDisplayDeviceName[webrtc::kAdmMaxDeviceNameSize - 1] = '\0';
  }

  if (_playDisplayDeviceName) {
    // Copy the sink name
    strncpy(_playDisplayDeviceName, i->default_sink_name,
            webrtc::kAdmMaxDeviceNameSize);
    _playDisplayDeviceName[webrtc::kAdmMaxDeviceNameSize - 1] = '\0';
  }

  LATE(pa_threaded_mainloop_signal)(_paMainloop, 0);
}

void MicrophoneModule::WaitForOperationCompletion(
    pa_operation* paOperation) const {
  if (!paOperation) {
    return;
  }

  while (LATE(pa_operation_get_state)(paOperation) == PA_OPERATION_RUNNING) {
    LATE(pa_threaded_mainloop_wait)(_paMainloop);
  }

  LATE(pa_operation_unref)(paOperation);
}

int32_t MicrophoneModule::CheckPulseAudioVersion() {
  PaLock();

  pa_operation* paOperation = NULL;

  // get the server info and update deviceName
  paOperation =
      LATE(pa_context_get_server_info)(_paContext, PaServerInfoCallback, this);

  WaitForOperationCompletion(paOperation);

  PaUnLock();

  // RTC_LOG(LS_VERBOSE) << "checking PulseAudio version: " << _paServerVersion;

  return 0;
}

int32_t MicrophoneModule::InitPulseAudio() {
  int retVal = 0;

  // Load libpulse
  if (!_GetPulseSymbolTable()->Load()) {
    // Most likely the Pulse library and sound server are not installed on
    // this system
    return -1;
  }

  // Create a mainloop API and connection to the default server
  // the mainloop is the internal asynchronous API event loop
  if (_paMainloop) {
    return -1;
  }
  _paMainloop = LATE(pa_threaded_mainloop_new)();
  if (!_paMainloop) {
    return -1;
  }

  // Start the threaded main loop
  retVal = LATE(pa_threaded_mainloop_start)(_paMainloop);
  if (retVal != PA_OK) {
    return -1;
  }

  PaLock();

  _paMainloopApi = LATE(pa_threaded_mainloop_get_api)(_paMainloop);
  if (!_paMainloopApi) {
    PaUnLock();
    return -1;
  }

  // Create a new PulseAudio context
  if (_paContext) {
    PaUnLock();
    return -1;
  }
  _paContext = LATE(pa_context_new)(_paMainloopApi, "WEBRTC VoiceEngine");

  if (!_paContext) {
    PaUnLock();
    return -1;
  }

  // Set state callback function
  LATE(pa_context_set_state_callback)(_paContext, PaContextStateCallback, this);

  // Connect the context to a server (default)
  _paStateChanged = false;
  retVal =
      LATE(pa_context_connect)(_paContext, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);

  if (retVal != PA_OK) {
    PaUnLock();
    return -1;
  }

  // Wait for state change
  while (!_paStateChanged) {
    LATE(pa_threaded_mainloop_wait)(_paMainloop);
  }

  // Now check to see what final state we reached.
  pa_context_state_t state = LATE(pa_context_get_state)(_paContext);

  if (state != PA_CONTEXT_READY) {
    if (state == PA_CONTEXT_FAILED) {
    } else if (state == PA_CONTEXT_TERMINATED) {
    } else {
      // Shouldn't happen, because we only signal on one of those three
      // states
    }
    PaUnLock();
    return -1;
  }

  PaUnLock();

  // Give the objects to the mixer manager
  _mixerManager.SetPulseAudioObjects(_paMainloop, _paContext);

  // Check the version
  if (CheckPulseAudioVersion() < 0) {
    return -1;
  }

  // Initialize sampling frequency
  if (InitSamplingFrequency() < 0 || sample_rate_hz_ == 0) {
    return -1;
  }

  return 0;
}

int32_t MicrophoneModule::InitSamplingFrequency() {
  PaLock();

  pa_operation* paOperation = NULL;

  // Get the server info and update sample_rate_hz_
  paOperation =
      LATE(pa_context_get_server_info)(_paContext, PaServerInfoCallback, this);

  WaitForOperationCompletion(paOperation);

  PaUnLock();

  return 0;
}

void MicrophoneModule::PaContextStateCallback(pa_context* c, void* pThis) {
  static_cast<MicrophoneModule*>(pThis)->PaContextStateCallbackHandler(c);
}

void MicrophoneModule::PaContextStateCallbackHandler(pa_context* c) {
  pa_context_state_t state = LATE(pa_context_get_state)(c);
  switch (state) {
    case PA_CONTEXT_UNCONNECTED:
      break;
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
      _paStateChanged = true;
      LATE(pa_threaded_mainloop_signal)(_paMainloop, 0);
      break;
    case PA_CONTEXT_READY:
      _paStateChanged = true;
      LATE(pa_threaded_mainloop_signal)(_paMainloop, 0);
      break;
  }
}

int32_t MicrophoneModule::TerminatePulseAudio() {
  // Do nothing if the instance doesn't exist
  // likely GetPulseSymbolTable.Load() fails
  if (!_paMainloop) {
    return 0;
  }

  PaLock();

  // Disconnect the context
  if (_paContext) {
    LATE(pa_context_disconnect)(_paContext);
  }

  // Unreference the context
  if (_paContext) {
    LATE(pa_context_unref)(_paContext);
  }

  PaUnLock();
  _paContext = NULL;

  // Stop the threaded main loop
  if (_paMainloop) {
    LATE(pa_threaded_mainloop_stop)(_paMainloop);
  }

  // Free the mainloop
  if (_paMainloop) {
    LATE(pa_threaded_mainloop_free)(_paMainloop);
  }

  _paMainloop = NULL;

  return 0;
}

int32_t MicrophoneModule::Init() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_initialized) {
    return 0;
  }

  // Initialize PulseAudio
  if (InitPulseAudio() < 0) {
    RTC_LOG(LS_ERROR) << "failed to initialize PulseAudio";
    if (TerminatePulseAudio() < 0) {
      RTC_LOG(LS_ERROR) << "failed to terminate PulseAudio";
    }
    return -1;
  }

#if defined(WEBRTC_USE_X11)
  // Get X display handle for typing detection
  _XDisplay = XOpenDisplay(NULL);
  if (!_XDisplay) {
    // RTC_LOG(LS_WARNING)
    // << "failed to open X display, typing detection will not work";
  }
#endif

  // RECORDING
  const auto attributes =
      rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kRealtime);
  _ptrThreadRec = rtc::PlatformThread::SpawnJoinable(
      [this] {
        while (RecThreadProcess()) {
        }
      },
      "webrtc_audio_module_rec_thread", attributes);

  _initialized = true;

  return 0;
}

int32_t MicrophoneModule::Terminate() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (!_initialized) {
    return 0;
  }

  {
    webrtc::MutexLock lock(&mutex_);
    quit_ = true;
  }
  _mixerManager.Close();

  // RECORDING
  _timeEventRec.Set();
  _ptrThreadRec.Finalize();

  // Terminate PulseAudio
  if (TerminatePulseAudio() < 0) {
    // RTC_LOG(LS_ERROR) << "failed to terminate PulseAudio";
    return -1;
  }

#if defined(WEBRTC_USE_X11)
  if (_XDisplay) {
    XCloseDisplay(_XDisplay);
    _XDisplay = NULL;
  }
#endif

  _initialized = false;
  _inputDeviceIsSpecified = false;

  return 0;
}

MicrophoneModule::MicrophoneModule() {
#if defined(WEBRTC_USE_X11)
  memset(_oldKeyState, 0, sizeof(_oldKeyState));
#endif
}

MicrophoneModule::~MicrophoneModule() {
  Terminate();
  if (_recBuffer) {
    delete[] _recBuffer;
    _recBuffer = NULL;
  }
  if (_playDeviceName) {
    delete[] _playDeviceName;
    _playDeviceName = NULL;
  }
  if (_recDeviceName) {
    delete[] _recDeviceName;
    _recDeviceName = NULL;
  }
}

void MicrophoneModule::PaSourceInfoCallback(pa_context* c,
                                            const pa_source_info* i,
                                            int eol,
                                            void* pThis) {
  static_cast<MicrophoneModule*>(pThis)->PaSourceInfoCallbackHandler(i, eol);
}

void MicrophoneModule::PaSourceInfoCallbackHandler(const pa_source_info* i,
                                                   int eol) {
  if (eol) {
    // Signal that we are done
    LATE(pa_threaded_mainloop_signal)(_paMainloop, 0);
    return;
  }

  // We don't want to list output devices
  if (i->monitor_of_sink == PA_INVALID_INDEX) {
    if (_numRecDevices == _deviceIndex) {
      // Convert the device index to the one of the source
      _paDeviceIndex = i->index;

      if (_recDeviceName) {
        // copy the source name
        strncpy(_recDeviceName, i->name, webrtc::kAdmMaxDeviceNameSize);
        _recDeviceName[webrtc::kAdmMaxDeviceNameSize - 1] = '\0';
      }
      if (_recDisplayDeviceName) {
        // Copy the source display name
        strncpy(_recDisplayDeviceName, i->description,
                webrtc::kAdmMaxDeviceNameSize);
        _recDisplayDeviceName[webrtc::kAdmMaxDeviceNameSize - 1] = '\0';
      }
    }

    _numRecDevices++;
  }
}

void MicrophoneModule::PaLock() {
  LATE(pa_threaded_mainloop_lock)(_paMainloop);
}

int16_t MicrophoneModule::RecordingDevices() {
  PaLock();

  pa_operation* paOperation = NULL;
  _numRecDevices = 1;  // Init to 1 to account for "default"

  // Get the whole list of devices and update _numRecDevices
  paOperation = LATE(pa_context_get_source_info_list)(
      _paContext, PaSourceInfoCallback, this);

  WaitForOperationCompletion(paOperation);

  PaUnLock();

  return _numRecDevices;
}

void MicrophoneModule::PaUnLock() {
  LATE(pa_threaded_mainloop_unlock)(_paMainloop);
}


int32_t MicrophoneModule::StopRecording() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  webrtc::MutexLock lock(&mutex_);

  if (!_recIsInitialized) {
    return 0;
  }

  if (_recStream == NULL) {
    return -1;
  }

  _recIsInitialized = false;
  _recording = false;

  RTC_LOG(LS_VERBOSE) << "stopping recording";

  // Stop Recording
  PaLock();

  DisableReadCallback();
  LATE(pa_stream_set_overflow_callback)(_recStream, NULL, NULL);

  // Unset this here so that we don't get a TERMINATED callback
  LATE(pa_stream_set_state_callback)(_recStream, NULL, NULL);

  if (LATE(pa_stream_get_state)(_recStream) != PA_STREAM_UNCONNECTED) {
    // Disconnect the stream
    if (LATE(pa_stream_disconnect)(_recStream) != PA_OK) {
      RTC_LOG(LS_ERROR) << "failed to disconnect rec stream, err="
                        << LATE(pa_context_errno)(_paContext);
      PaUnLock();
      return -1;
    }

    RTC_LOG(LS_VERBOSE) << "disconnected recording";
  }

  LATE(pa_stream_unref)(_recStream);
  _recStream = NULL;

  PaUnLock();

  // Provide the recStream to the mixer
  _mixerManager.SetRecStream(_recStream);

  if (_recBuffer) {
    delete[] _recBuffer;
    _recBuffer = NULL;
  }

  return 0;
}

int32_t MicrophoneModule::StartRecording() {
  RTC_DCHECK(thread_checker_.IsCurrent());

  if (!_recIsInitialized) {
    return -1;
  }

  if (_recording) {
    return 0;
  }

  // Set state to ensure that the recording starts from the audio thread.
  _startRec = true;

  // The audio thread will signal when recording has started.
  _timeEventRec.Set();
  if (!_recStartEvent.Wait(10000)) {
    {
      webrtc::MutexLock lock(&mutex_);
      _startRec = false;
    }
    StopRecording();
    RTC_LOG(LS_ERROR) << "failed to activate recording";
    return -1;
  }

  {
    webrtc::MutexLock lock(&mutex_);
    if (_recording) {
      // The recording state is set by the audio thread after recording
      // has started.
    } else {
      RTC_LOG(LS_ERROR) << "failed to activate recording";
      return -1;
    }
  }

  return 0;
}

int32_t MicrophoneModule::GetDefaultDeviceInfo(bool recDevice,
                                               char* name,
                                               uint16_t& index) {
  char tmpName[webrtc::kAdmMaxDeviceNameSize] = {0};
  // subtract length of "default: "
  uint16_t nameLen = webrtc::kAdmMaxDeviceNameSize - 9;
  char* pName = NULL;

  if (name) {
    // Add "default: "
    strcpy(name, "default: ");
    pName = &name[9];
  }

  // Tell the callback that we want
  // the name for this device
  if (recDevice) {
    _recDisplayDeviceName = tmpName;
  } else {
    _playDisplayDeviceName = tmpName;
  }

  // Set members
  _paDeviceIndex = -1;
  _deviceIndex = 0;
  _numPlayDevices = 0;
  _numRecDevices = 0;

  PaLock();

  pa_operation* paOperation = NULL;

  // Get the server info and update deviceName
  paOperation =
      LATE(pa_context_get_server_info)(_paContext, PaServerInfoCallback, this);

  WaitForOperationCompletion(paOperation);

  // Get the device index
  if (recDevice) {
    paOperation = LATE(pa_context_get_source_info_by_name)(
        _paContext, (char*)tmpName, PaSourceInfoCallback, this);
  }

  WaitForOperationCompletion(paOperation);

  PaUnLock();

  // Set the index
  index = _paDeviceIndex;

  if (name) {
    // Copy to name string
    strncpy(pName, tmpName, nameLen);
  }

  // Clear members
  _playDisplayDeviceName = NULL;
  _recDisplayDeviceName = NULL;
  _paDeviceIndex = -1;
  _deviceIndex = -1;
  _numPlayDevices = 0;
  _numRecDevices = 0;

  return 0;
}

int32_t MicrophoneModule::InitMicrophone() {
  if (_recording) {
    return -1;
  }

  if (!_inputDeviceIsSpecified) {
    return -1;
  }

  pa_context_set_subscribe_callback(_paContext, subscribe_sink_event, this);
  pa_operation* o = pa_context_subscribe(
      _paContext, pa_subscription_mask_t::PA_SUBSCRIPTION_MASK_SOURCE,
      subscribe_result, this);
  WaitForOperationCompletion(o);

  // Check if default device
  if (_inputDeviceIndex == 0) {
    uint16_t deviceIndex = 0;
    GetDefaultDeviceInfo(true, NULL, deviceIndex);
    _paDeviceIndex = deviceIndex;
  } else {
    // Get the PA device index from
    // the callback
    _deviceIndex = _inputDeviceIndex;

    // get recording devices
    RecordingDevices();
  }

  // The callback has now set the _paDeviceIndex to
  // the PulseAudio index of the device
  if (_mixerManager.OpenMicrophone(_paDeviceIndex) == -1) {
    return -1;
  }

  // Clear _deviceIndex
  _deviceIndex = -1;

  return 0;
}

int32_t MicrophoneModule::InitRecording() {
  RTC_DCHECK(thread_checker_.IsCurrent());

  if (_recording) {
    return -1;
  }

  if (!_inputDeviceIsSpecified) {
    return -1;
  }

  if (_recIsInitialized) {
    return 0;
  }

  // Initialize the microphone (devices might have been added or removed)
  if (InitMicrophone() == -1) {
  }

  // Set the rec sample specification
  pa_sample_spec recSampleSpec;
  recSampleSpec.channels = _recChannels;
  recSampleSpec.format = PA_SAMPLE_S16LE;
  recSampleSpec.rate = sample_rate_hz_;

  // Create a new rec stream
  _recStream =
      LATE(pa_stream_new)(_paContext, "recStream", &recSampleSpec, NULL);
  if (!_recStream) {
    return -1;
  }

  // Provide the recStream to the mixer
  _mixerManager.SetRecStream(_recStream);

  if (_configuredLatencyRec != WEBRTC_PA_NO_LATENCY_REQUIREMENTS) {
    _recStreamFlags = (pa_stream_flags_t)(PA_STREAM_AUTO_TIMING_UPDATE |
                                          PA_STREAM_INTERPOLATE_TIMING);

    // If configuring a specific latency then we want to specify
    // PA_STREAM_ADJUST_LATENCY to make the server adjust parameters
    // automatically to reach that target latency. However, that flag
    // doesn't exist in Ubuntu 8.04 and many people still use that,
    //  so we have to check the protocol version of libpulse.
    if (LATE(pa_context_get_protocol_version)(_paContext) >=
        WEBRTC_PA_ADJUST_LATENCY_PROTOCOL_VERSION) {
      _recStreamFlags |= PA_STREAM_ADJUST_LATENCY;
    }

    const pa_sample_spec* spec = LATE(pa_stream_get_sample_spec)(_recStream);
    if (!spec) {
      RTC_LOG(LS_ERROR) << "pa_stream_get_sample_spec(rec)";
      return -1;
    }

    size_t bytesPerSec = LATE(pa_bytes_per_second)(spec);
    uint32_t latency = bytesPerSec * WEBRTC_PA_LOW_CAPTURE_LATENCY_MSECS /
                       WEBRTC_PA_MSECS_PER_SEC;

    // Set the rec buffer attributes
    // Note: fragsize specifies a maximum transfer size, not a minimum, so
    // it is not possible to force a high latency setting, only a low one.
    _recBufferAttr.fragsize = latency;  // size of fragment
    _recBufferAttr.maxlength =
        latency + bytesPerSec * WEBRTC_PA_CAPTURE_BUFFER_EXTRA_MSECS /
                      WEBRTC_PA_MSECS_PER_SEC;

    _configuredLatencyRec = latency;
  }

  _recordBufferSize = sample_rate_hz_ / 100 * 2 * _recChannels;
  _recordBufferUsed = 0;
  _recBuffer = new int8_t[_recordBufferSize];

  // Enable overflow callback
  LATE(pa_stream_set_overflow_callback)
  (_recStream, PaStreamOverflowCallback, this);

  // Set the state callback function for the stream
  LATE(pa_stream_set_state_callback)(_recStream, PaStreamStateCallback, this);

  // Mark recording side as initialized
  _recIsInitialized = true;

  return 0;
}

void MicrophoneModule::PaStreamOverflowCallback(pa_stream* /*unused*/,
                                                void* pThis) {
  static_cast<MicrophoneModule*>(pThis)->PaStreamOverflowCallbackHandler();
}

void MicrophoneModule::PaStreamOverflowCallbackHandler() {
  RTC_LOG(LS_WARNING) << "Recording overflow";
}

void MicrophoneModule::PaStreamStateCallbackHandler(pa_stream* p) {
  RTC_LOG(LS_VERBOSE) << "stream state cb";

  pa_stream_state_t state = LATE(pa_stream_get_state)(p);
  switch (state) {
    case PA_STREAM_UNCONNECTED:
      RTC_LOG(LS_VERBOSE) << "unconnected";
      break;
    case PA_STREAM_CREATING:
      RTC_LOG(LS_VERBOSE) << "creating";
      break;
    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
      RTC_LOG(LS_VERBOSE) << "failed";
      break;
    case PA_STREAM_READY:
      RTC_LOG(LS_VERBOSE) << "ready";
      break;
  }

  LATE(pa_threaded_mainloop_signal)(_paMainloop, 0);
}

void MicrophoneModule::PaStreamStateCallback(pa_stream* p, void* pThis) {
  static_cast<MicrophoneModule*>(pThis)->PaStreamStateCallbackHandler(p);
}

bool MicrophoneModule::RecThreadProcess() {

  if (_ended) {
    _ended = false;

    StopRecording();
    if (source != nullptr) {
      source->ended();
    }
  }

  if (!_timeEventRec.Wait(1000)) {
    return true;
  }

  webrtc::MutexLock lock(&mutex_);
  if (quit_) {
    return false;
  }
  if (_startRec) {
    RTC_LOG(LS_VERBOSE) << "_startRec true, performing initial actions";

    _recDeviceName = NULL;

    // Set if not default device
    if (_inputDeviceIndex > 0) {
      // Get the recording device name
      _recDeviceName = new char[webrtc::kAdmMaxDeviceNameSize];
      _deviceIndex = _inputDeviceIndex;
      RecordingDevices();
    }

    PaLock();

    RTC_LOG(LS_VERBOSE) << "connecting stream";

    // Connect the stream to a source
    if (LATE(pa_stream_connect_record)(
            _recStream, _recDeviceName, &_recBufferAttr,
            (pa_stream_flags_t)_recStreamFlags) != PA_OK) {
      RTC_LOG(LS_ERROR) << "failed to connect rec stream, err="
                        << LATE(pa_context_errno)(_paContext);
    }

    RTC_LOG(LS_VERBOSE) << "connected";

    // Wait for state change
    while (LATE(pa_stream_get_state)(_recStream) != PA_STREAM_READY) {
      LATE(pa_threaded_mainloop_wait)(_paMainloop);
    }

    RTC_LOG(LS_VERBOSE) << "done";

    // We can now handle read callbacks
    EnableReadCallback();

    PaUnLock();

    // Clear device name
    if (_recDeviceName) {
      delete[] _recDeviceName;
      _recDeviceName = NULL;
    }

    _startRec = false;
    _recording = true;
    _recStartEvent.Set();

    return true;
  }

  if (_recording) {
    // Read data and provide it to VoiceEngine
    if (ReadRecordedData(_tempSampleData, _tempSampleDataSize) == -1) {
      return true;
    }

    _tempSampleData = NULL;
    _tempSampleDataSize = 0;

    PaLock();
    while (true) {
      // Ack the last thing we read
      if (LATE(pa_stream_drop)(_recStream) != 0) {
        RTC_LOG(LS_WARNING)
            << "failed to drop, err=" << LATE(pa_context_errno)(_paContext);
      }

      if (LATE(pa_stream_readable_size)(_recStream) <= 0) {
        // Then that was all the data
        break;
      }

      // Else more data.
      const void* sampleData;
      size_t sampleDataSize;

      if (LATE(pa_stream_peek)(_recStream, &sampleData, &sampleDataSize) != 0) {
        RTC_LOG(LS_ERROR) << "RECORD_ERROR, error = "
                          << LATE(pa_context_errno)(_paContext);
        break;
      }

      // Drop lock for sigslot dispatch, which could take a while.
      PaUnLock();
      // Read data and provide it to VoiceEngine
      if (ReadRecordedData(sampleData, sampleDataSize) == -1) {
        return true;
      }
      PaLock();

      // Return to top of loop for the ack and the check for more data.
    }

    EnableReadCallback();
    PaUnLock();

  }  // _recording

  return true;
}

int32_t MicrophoneModule::LatencyUsecs(pa_stream* stream) {
  if (!WEBRTC_PA_REPORT_LATENCY) {
    return 0;
  }

  if (!stream) {
    return 0;
  }

  pa_usec_t latency;
  int negative;
  if (LATE(pa_stream_get_latency)(stream, &latency, &negative) != 0) {
    RTC_LOG(LS_ERROR) << "Can't query latency";
    // We'd rather continue playout/capture with an incorrect delay than
    // stop it altogether, so return a valid value.
    return 0;
  }

  if (negative) {
    RTC_LOG(LS_VERBOSE)
        << "warning: pa_stream_get_latency reported negative delay";

    // The delay can be negative for monitoring streams if the captured
    // samples haven't been played yet. In such a case, "latency"
    // contains the magnitude, so we must negate it to get the real value.
    int32_t tmpLatency = (int32_t)-latency;
    if (tmpLatency < 0) {
      // Make sure that we don't use a negative delay.
      tmpLatency = 0;
    }

    return tmpLatency;
  } else {
    return (int32_t)latency;
  }
}

int32_t MicrophoneModule::ReadRecordedData(const void* bufferData,
                                           size_t bufferSize)
    RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
  size_t size = bufferSize;
  uint32_t numRecSamples = _recordBufferSize / (2 * _recChannels);

  // Account for the peeked data and the used data.
  uint32_t recDelay =
      (uint32_t)((LatencyUsecs(_recStream) / 1000) +
                 10 * ((size + _recordBufferUsed) / _recordBufferSize));

  if (_playStream) {
    // Get the playout delay.
    _sndCardPlayDelay = (uint32_t)(LatencyUsecs(_playStream) / 1000);
  }

  if (_recordBufferUsed > 0) {
    // Have to copy to the buffer until it is full.
    size_t copy = _recordBufferSize - _recordBufferUsed;
    if (size < copy) {
      copy = size;
    }

    memcpy(&_recBuffer[_recordBufferUsed], bufferData, copy);
    _recordBufferUsed += copy;
    bufferData = static_cast<const char*>(bufferData) + copy;
    size -= copy;

    if (_recordBufferUsed != _recordBufferSize) {
      // Not enough data yet to pass to VoE.
      return 0;
    }

    // nado=_recBuffer;

    // Provide data to VoiceEngine.
    if (ProcessRecordedData(_recBuffer, numRecSamples, recDelay) == -1) {
      // We have stopped recording.
      return -1;
    }

    _recordBufferUsed = 0;
  }

  // Now process full 10ms sample sets directly from the input.
  while (size >= _recordBufferSize) {
    // nado=(int8_t*)bufferData;

    // Provide data to VoiceEngine.
    if (ProcessRecordedData(static_cast<int8_t*>(const_cast<void*>(bufferData)),
                            numRecSamples, recDelay) == -1) {
      // We have stopped recording.
      return -1;
    }

    bufferData = static_cast<const char*>(bufferData) + _recordBufferSize;
    size -= _recordBufferSize;

    // We have consumed 10ms of data.
    recDelay -= 10;
  }

  // Now save any leftovers for later.
  if (size > 0) {
    memcpy(_recBuffer, bufferData, size);
    _recordBufferUsed = size;
  }

  return 0;
}

int32_t MicrophoneModule::StereoRecordingIsAvailable(bool& available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_recChannels == 2 && _recording) {
    available = true;
    return 0;
  }

  available = false;
  bool wasInitialized = _mixerManager.MicrophoneIsInitialized();
  int error = 0;

  if (!wasInitialized && InitMicrophone() == -1) {
    // Cannot open the specified device
    available = false;
    return 0;
  }

  // Check if the selected microphone can record stereo.
  bool isAvailable(false);

  error = _mixerManager.StereoRecordingIsAvailable(isAvailable);

  if (!error)
    available = isAvailable;

  // Close the initialized input mixer
  if (!wasInitialized) {
    _mixerManager.CloseMicrophone();
  }

  return error;
}

int32_t MicrophoneModule::SetStereoRecording(bool enable) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (enable)
    _recChannels = 2;
  else
    _recChannels = 1;

  return 0;
}

int32_t MicrophoneModule::StereoRecording(bool& enabled) const {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_recChannels == 2)
    enabled = true;
  else
    enabled = false;

  return 0;
}


bool MicrophoneModule::KeyPressed() const {
#if defined(WEBRTC_USE_X11)
  char szKey[32];
  unsigned int i = 0;
  char state = 0;

  if (!_XDisplay)
    return false;

  // Check key map status
  XQueryKeymap(_XDisplay, szKey);

  // A bit change in keymap means a key is pressed
  for (i = 0; i < sizeof(szKey); i++)
    state |= (szKey[i] ^ _oldKeyState[i]) & szKey[i];

  // Save old state
  memcpy((char*)_oldKeyState, (char*)szKey, sizeof(_oldKeyState));
  return (state != 0);
#else
  return false;
#endif
}

void MicrophoneModule::EnableReadCallback() {
  LATE(pa_stream_set_read_callback)(_recStream, &PaStreamReadCallback, this);
}

void MicrophoneModule::DisableReadCallback() {
  LATE(pa_stream_set_read_callback)(_recStream, NULL, NULL);
}

void MicrophoneModule::PaStreamReadCallback(pa_stream* a /*unused1*/,
                                            size_t b /*unused2*/,
                                            void* pThis) {
  static_cast<MicrophoneModule*>(pThis)->PaStreamReadCallbackHandler();
}

void MicrophoneModule::PaStreamReadCallbackHandler() {
  // We get the data pointer and size now in order to save one Lock/Unlock
  // in the worker thread.
  if (LATE(pa_stream_peek)(_recStream, &_tempSampleData,
                           &_tempSampleDataSize) != 0) {
    RTC_LOG(LS_ERROR) << "Can't read data!";
    return;
  }

  // Since we consume the data asynchronously on a different thread, we have
  // to temporarily disable the read callback or else Pulse will call it
  // continuously until we consume the data. We re-enable it below.
  DisableReadCallback();
  _timeEventRec.Set();
}

bool MicrophoneModule::MicrophoneIsInitialized() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  return (_mixerManager.MicrophoneIsInitialized());
};

// Microphone volume controls
int32_t MicrophoneModule::MicrophoneVolumeIsAvailable(bool* available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

  // Make an attempt to open up the
  // input mixer corresponding to the currently selected output device.
  if (!wasInitialized && InitMicrophone() == -1) {
    // If we end up here it means that the selected microphone has no
    // volume control.
    *available = false;
    return 0;
  }

  // Given that InitMicrophone was successful, we know that a volume control
  // exists.
  *available = true;

  // Close the initialized input mixer
  if (!wasInitialized) {
    _mixerManager.CloseMicrophone();
  }

  return 0;
}

int32_t MicrophoneModule::SetMicrophoneVolume(uint32_t volume) {
  return (_mixerManager.SetMicrophoneVolume(volume));
}

int32_t MicrophoneModule::MicrophoneVolume(uint32_t* volume) const {
  uint32_t level(0);

  if (_mixerManager.MicrophoneVolume(level) == -1) {
    RTC_LOG(LS_WARNING) << "failed to retrieve current microphone level";
    return -1;
  }

  *volume = level;

  return 0;
}

int32_t MicrophoneModule::MaxMicrophoneVolume(uint32_t* maxVolume) const {
  uint32_t maxVol(0);

  if (_mixerManager.MaxMicrophoneVolume(maxVol) == -1) {
    return -1;
  }

  *maxVolume = maxVol;

  return 0;
}

int32_t MicrophoneModule::MinMicrophoneVolume(uint32_t* minVolume) const {
  uint32_t minVol(0);

  if (_mixerManager.MinMicrophoneVolume(minVol) == -1) {
    return -1;
  }

  *minVolume = minVol;

  return 0;
}

// Microphone mute control
int32_t MicrophoneModule::MicrophoneMuteIsAvailable(bool* available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  bool isAvailable(false);
  bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

  // Make an attempt to open up the
  // input mixer corresponding to the currently selected input device.
  //
  if (!wasInitialized && InitMicrophone() == -1) {
    // If we end up here it means that the selected microphone has no
    // volume control, hence it is safe to state that there is no
    // boost control already at this stage.
    *available = false;
    return 0;
  }

  // Check if the selected microphone has a mute control
  //
  _mixerManager.MicrophoneMuteIsAvailable(isAvailable);
  *available = isAvailable;

  // Close the initialized input mixer
  //
  if (!wasInitialized) {
    _mixerManager.CloseMicrophone();
  }

  return 0;
}

int32_t MicrophoneModule::SetMicrophoneMute(bool enable) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  return (_mixerManager.SetMicrophoneMute(enable));
}

int32_t MicrophoneModule::MicrophoneMute(bool* enabled) const {
  RTC_DCHECK(thread_checker_.IsCurrent());
  bool muted(0);
  if (_mixerManager.MicrophoneMute(muted) == -1) {
    return -1;
  }

  *enabled = muted;
  return 0;
}

#endif