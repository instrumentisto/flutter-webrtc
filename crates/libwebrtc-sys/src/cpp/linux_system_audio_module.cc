#if WEBRTC_LINUX

#include "linux_system_audio_module.h"
#include <iostream>
const int32_t WEBRTC_PA_NO_LATENCY_REQUIREMENTS = -1;
const uint32_t WEBRTC_PA_ADJUST_LATENCY_PROTOCOL_VERSION = 13;
const uint32_t WEBRTC_PA_LOW_CAPTURE_LATENCY_MSECS = 10;
const uint32_t WEBRTC_PA_MSECS_PER_SEC = 1000;
const uint32_t WEBRTC_PA_CAPTURE_BUFFER_EXTRA_MSECS = 750;


int SystemSource::sources_num = 0;
SystemSource::SystemSource(SystemModuleInterface* module) {
  this->module = module;
  if (sources_num == 0) {
    module->StartRecording();
  }
  ++sources_num;
}

SystemSource::~SystemSource() {
  --sources_num;
  if (sources_num == 0) {
    module->StopRecording();
    module->ResetSource();
  }
}


static void subscribe_result(pa_context *c, int success, void *userdata) {
  auto thisP = (SystemModule*) userdata;
  pa_threaded_mainloop_signal(thisP->_paMainloop, 0);
}

static void subscribe_sink_event(pa_context* c,
                          pa_subscription_event_type_t t,
                          uint32_t idx,
                          void* userdata) {
  auto thisP = (SystemModule*)userdata;
  if (idx == thisP->_recSinkId) {
    if (t & pa_subscription_event_type_t::PA_SUBSCRIPTION_EVENT_REMOVE) {
      thisP->_mute = true;
    }
  } else if (t ==
             pa_subscription_event_type_t::PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
    thisP->_on_new = true;
  }
}

struct SinkData {
  std::vector<AudioSourceInfo>* vec;
  pa_threaded_mainloop* pa_mainloop;
};

static void subscribe_sink_cb(pa_context* paContext,
                              pa_sink_input_info* i,
                              int eol,
                              void* userdata) {
  auto data = (SinkData*)userdata;
  if (eol > 0) {
    pa_threaded_mainloop_signal(data->pa_mainloop, 0);
    return;
  }

  if (i != nullptr) {
    AudioSourceInfo info(
        i->index,
        std::string(pa_proplist_gets(i->proplist, "application.name")) + ": " +
            std::string(i->name),
        atoi(pa_proplist_gets(i->proplist, "application.process.id")),
        i->sample_spec.rate, i->sample_spec.channels);
    data->vec->push_back(info);
  }
}

// Initialization and terminate.
bool SystemModule::Init() {
  if (_initialized) {
    return 0;
  }

  // Initialize PulseAudio
  if (InitPulseAudio() < 0) {
    TerminatePulseAudio();
    return -1;
  }

  PaLock();

  pa_operation* paOperation = NULL;

  pa_context_set_subscribe_callback(_paContext, subscribe_sink_event, this);
  pa_subscription_mask_t mask;
  pa_operation* o = pa_context_subscribe(
      _paContext, pa_subscription_mask_t::PA_SUBSCRIPTION_MASK_SINK_INPUT,
      subscribe_result, this);

  WaitForOperationCompletion(o);

  PaUnLock();

  const auto attributes =
      rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kRealtime);
  _ptrThreadRec = rtc::PlatformThread::SpawnJoinable(
      [this] {
        while (RecThreadProcess()) {
        }
      },
      "webrtc_system_audio_module_rec_thread", attributes);

  _initialized = true;

  return 0;
}

SystemModule::SystemModule() {
  Init();
}
SystemModule::~SystemModule() {
  Terminate();
}

// Unlocks pulse audio.
void SystemModule::PaUnLock() {
  pa_threaded_mainloop_unlock(_paMainloop);
}

// Locks pulse audio.
void SystemModule::PaLock() {
  pa_threaded_mainloop_lock(_paMainloop);
}

// Redirect callback to `this.PaContextStateCallbackHandler`.
void SystemModule::PaContextStateCallback(pa_context* c, void* pThis) {
  static_cast<SystemModule*>(pThis)->PaContextStateCallbackHandler(c);
}

// Callback to keep track of the state of the pulse audio context.
void SystemModule::PaContextStateCallbackHandler(pa_context* c) {
  pa_context_state_t state = pa_context_get_state(c);
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
      pa_threaded_mainloop_signal(_paMainloop, 0);
      break;
    case PA_CONTEXT_READY:
      _paStateChanged = true;
      pa_threaded_mainloop_signal(_paMainloop, 0);
      break;
  }
}

// Terminates a pulse audio.
int32_t SystemModule::TerminatePulseAudio() {
  // Do nothing if the instance doesn't exist
  // likely GetPulseSymbolTable.Load() fails
  if (!_paMainloop) {
    return 0;
  }

  PaLock();

  // Disconnect the context
  if (_paContext) {
    pa_context_disconnect(_paContext);
  }

  // Unreference the context
  if (_paContext) {
    pa_context_unref(_paContext);
  }

  PaUnLock();
  _paContext = NULL;

  // Stop the threaded main loop
  if (_paMainloop) {
    pa_threaded_mainloop_stop(_paMainloop);
  }

  // Free the mainloop
  if (_paMainloop) {
    pa_threaded_mainloop_free(_paMainloop);
  }

  _paMainloop = NULL;

  return 0;
}

// Initiates a pulse audio.
int32_t SystemModule::InitPulseAudio() {
  int retVal = 0;

  // Create a mainloop API and connection to the default server
  // the mainloop is the internal asynchronous API event loop
  if (_paMainloop) {
    return -1;
  }
  _paMainloop = pa_threaded_mainloop_new();
  if (!_paMainloop) {
    return -1;
  }

  // Start the threaded main loop
  retVal = pa_threaded_mainloop_start(_paMainloop);
  if (retVal != PA_OK) {
    return -1;
  }

  PaLock();

  _paMainloopApi = pa_threaded_mainloop_get_api(_paMainloop);
  if (!_paMainloopApi) {
    PaUnLock();
    return -1;
  }

  // Create a new PulseAudio context
  if (_paContext) {
    PaUnLock();
    return -1;
  }
  _paContext = pa_context_new(_paMainloopApi, "WEBRTC SystemAudio");

  if (!_paContext) {
    PaUnLock();
    return -1;
  }

  // Set state callback function
  pa_context_set_state_callback(_paContext, PaContextStateCallback, this);

  // Connect the context to a server (default)
  _paStateChanged = false;
  retVal = pa_context_connect(_paContext, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);

  if (retVal != PA_OK) {
    PaUnLock();
    return -1;
  }

  // Wait for state change
  while (!_paStateChanged) {
    pa_threaded_mainloop_wait(_paMainloop);
  }

  // Now check to see what final state we reached.
  pa_context_state_t state = pa_context_get_state(_paContext);

  if (state != PA_CONTEXT_READY) {
    PaUnLock();
    return -1;
  }

  PaUnLock();

  return 0;
}

// Returns the current sinc id for the current process.
int32_t SystemModule::UpdateSinkId() {
  auto audio_sources = EnumerateSystemSource();

  auto sinkId = -1;
  for (int i = 0; i < audio_sources.size(); ++i) {
    if (audio_sources[i].GetProcessId() == _recProcessId) {
      sinkId = audio_sources[i].GetId();
    }
  }

  return sinkId;
}

// Function to capture audio in another thread.
bool SystemModule::RecThreadProcess() {
  if (_mute) {
    _mute = false;

    StopRecording();
    if (source != nullptr) {
      source->Mute();
    }
    _recSinkId = -1;
    return true;
  }

  if (_on_new) {
    _on_new = false;

    if (_recSinkId == -1) {

      auto recSinkId = UpdateSinkId();
      if (_recSinkId != recSinkId && recSinkId > 0) {
        _recSinkId = recSinkId;
        StopRecording();
        InitRecording();
        _startRec = true;
        _timeEventRec.Set();
        return true;
      }
    }
  }

  if (!_timeEventRec.Wait(1000)) {
    return true;
  }

  webrtc::MutexLock lock(&mutex_);
  if (quit_) {
    return false;
  }

  if (_startRec && _recProcessId != -1) {
    // get sink id
    _recSinkId = UpdateSinkId();
    if (_recSinkId < 0) {
      return true;
    }

    PaLock();

    pa_stream_set_monitor_stream(_recStream, _recSinkId);

    // Connect the stream to a source
    pa_stream_connect_record(_recStream, nullptr, nullptr, (pa_stream_flags_t)_recStreamFlags);

    // Wait for state change
    while (pa_stream_get_state(_recStream) != PA_STREAM_READY) {
      pa_threaded_mainloop_wait(_paMainloop);
    }

    // We can now handle read callbacks
    EnableReadCallback();

    PaUnLock();

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
      pa_stream_drop(_recStream);

      if (pa_stream_readable_size(_recStream) <= 0) {
        // Then that was all the data
        break;
      }

      // Else more data.
      const void* sampleData;
      size_t sampleDataSize;

      if (pa_stream_peek(_recStream, &sampleData, &sampleDataSize) != 0) {
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

// Disables signals for audio recording.
void SystemModule::DisableReadCallback() {
  pa_stream_set_read_callback(_recStream, NULL, NULL);
}

// Redirect callback to `this.PaStreamReadCallbackHandler`.
void SystemModule::PaStreamReadCallback(pa_stream* a /*unused1*/,
                                        size_t b /*unused2*/,
                                        void* pThis) {
  static_cast<SystemModule*>(pThis)->PaStreamReadCallbackHandler();
}

// Used to signal audio reading.
void SystemModule::PaStreamReadCallbackHandler() {
  // We get the data pointer and size now in order to save one Lock/Unlock
  // in the worker thread.
  if (pa_stream_peek(_recStream, &_tempSampleData, &_tempSampleDataSize) != 0) {
    return;
  }

  // Since we consume the data asynchronously on a different thread, we have
  // to temporarily disable the read callback or else Pulse will call it
  // continuously until we consume the data. We re-enable it below.
  DisableReadCallback();
  _timeEventRec.Set();
}

// Sorts and multiplie audio bytes.
void ReorderingBuffer(std::vector<int16_t>& output,
                      int16_t* data,
                      int sample_rare,
                      int channels,
                      float audio_multiplier) {
  int samples = sample_rare / 100;
  for (int i = 0; i < channels; ++i) {
    int k = i;
    for (int j = samples * i; k < samples * channels; ++j, k += channels) {
      double inSample = data[k] * audio_multiplier;
      if (inSample > INT16_MAX)
        inSample = INT16_MAX;
      if (inSample < INT16_MIN)
        inSample = INT16_MIN;

      output[j] = (int16_t)inSample;
    }
  }
}

// Passes an audio frame to an AudioSource.
int32_t SystemModule::ProcessRecordedData(int8_t* bufferData,
                                          uint32_t bufferSizeInSamples,
                                          uint32_t recDelay)
    RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
  if (source != nullptr) {
    ReorderingBuffer(release_capture_buffer, (int16_t*)bufferData, sample_rate_hz_, _recChannels,
                     audio_multiplier);
    source->UpdateFrame(release_capture_buffer.data(), bufferSizeInSamples,
                        sample_rate_hz_, _recChannels);
  }

  // We have been unlocked - check the flag again.
  if (!_recording) {
    return -1;
  }

  return 0;
}

// Enaables signals for audio recording.
void SystemModule::EnableReadCallback() {
  pa_stream_set_read_callback(_recStream, &PaStreamReadCallback, this);
}

int32_t SystemModule::ReadRecordedData(const void* bufferData,
                                       size_t bufferSize)
    RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
  size_t size = bufferSize;
  uint32_t numRecSamples = _recordBufferSize / (2 * (int)1);
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

    // Provide data to VoiceEngine.
    if (ProcessRecordedData(_recBuffer, numRecSamples, 0) == -1) {
      // We have stopped recording.
      return -1;
    }

    _recordBufferUsed = 0;
  }

  // Now process full 10ms sample sets directly from the input.
  while (size >= _recordBufferSize) {
    // Provide data to VoiceEngine.
    if (ProcessRecordedData(static_cast<int8_t*>(const_cast<void*>(bufferData)),
                            numRecSamples, 0) == -1) {
      // We have stopped recording.
      return -1;
    }

    bufferData = static_cast<const char*>(bufferData) + _recordBufferSize;
    size -= _recordBufferSize;
  }

  // Now save any leftovers for later.
  if (size > 0) {
    memcpy(_recBuffer, bufferData, size);
    _recordBufferUsed = size;
  }

  return 0;
}

// Callback to keep track of the state of the pulse audio stream.
void SystemModule::PaStreamStateCallbackHandler(pa_stream* p) {
  pa_stream_state_t state = pa_stream_get_state(p);
  switch (state) {
    case PA_STREAM_UNCONNECTED:
      break;
    case PA_STREAM_CREATING:
      break;
    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
      break;
    case PA_STREAM_READY:
      break;
  }

  pa_threaded_mainloop_signal(_paMainloop, 0);
}

// Redirect callback to `this.PaStreamStateCallbackHandler`.
void SystemModule::PaStreamStateCallback(pa_stream* p, void* pThis) {
  static_cast<SystemModule*>(pThis)->PaStreamStateCallbackHandler(p);
}

// Stops `RecThreadProcess`.
int32_t SystemModule::Terminate() {
  quit_ = true;
  return 0;
}

// Creates the new `AudioSource`.
rtc::scoped_refptr<AudioSource> SystemModule::CreateSource() {
  if (!_recording) {
    InitRecording();
  }

  if (!source) {
    source = rtc::scoped_refptr<SystemSource>(new SystemSource(this));
  }
  auto result = source;
  source->Release();
  source->Mute();
  return result;
}

// Resets the audio source.
void SystemModule::ResetSource() {
  source = nullptr;
}

// Changes the captured source.
void SystemModule::SetRecordingSource(int id) {
  auto start = _recIsInitialized;
  StopRecording();
  if (_recIsInitialized) {
    return;
  }

  _recProcessId = id;

  if (start) {
    InitRecording();
    StartRecording();
  }
}

// Waiting for the pulse audio operation to completion.
void SystemModule::WaitForOperationCompletion(pa_operation* paOperation) const {
  if (!paOperation) {
    return;
  }

  while (pa_operation_get_state(paOperation) == PA_OPERATION_RUNNING) {
    pa_threaded_mainloop_wait(_paMainloop);
  }

  pa_operation_unref(paOperation);
}

// Initiates the audio stream from the provided `_recSinkId`.
void SystemModule::InitRecording() {
  if (_recording) {
    return;
  }

  if (_recIsInitialized) {
    return;
  }

  if (_recProcessId < 0) {
    return;
  }

  auto audio_sources = EnumerateSystemSource();
  _recSinkId = -1;
  for (int i = 0; i < audio_sources.size(); ++i) {
    if (audio_sources[i].GetProcessId() == _recProcessId) {
      _recSinkId = audio_sources[i].GetId();
      _recChannels = audio_sources[i].GetChannels();
      sample_rate_hz_ = audio_sources[i].GetSampleRate();
    }
  }

  if (_recSinkId < 0) {
    return;
  }

  // Set the rec sample specification
  pa_sample_spec recSampleSpec;
  recSampleSpec.channels = (int)_recChannels;
  recSampleSpec.format = PA_SAMPLE_S16LE;
  recSampleSpec.rate = sample_rate_hz_;

  // Create a new rec stream
  _recStream = pa_stream_new(_paContext, "System stream", &recSampleSpec, NULL);

  if (!_recStream) {
    return;
  }

  if (_configuredLatencyRec != WEBRTC_PA_NO_LATENCY_REQUIREMENTS) {
    _recStreamFlags = (pa_stream_flags_t)(PA_STREAM_AUTO_TIMING_UPDATE |
                                          PA_STREAM_INTERPOLATE_TIMING);

    // If configuring a specific latency then we want to specify
    // PA_STREAM_ADJUST_LATENCY to make the server adjust parameters
    // automatically to reach that target latency. However, that flag
    // doesn't exist in Ubuntu 8.04 and many people still use that,
    //  so we have to check the protocol version of libpulse.
    if (pa_context_get_protocol_version(_paContext) >=
        WEBRTC_PA_ADJUST_LATENCY_PROTOCOL_VERSION) {
      _recStreamFlags |= PA_STREAM_ADJUST_LATENCY;
    }

    const pa_sample_spec* spec = pa_stream_get_sample_spec(_recStream);
    if (!spec) {
      return;
    }

    size_t bytesPerSec = pa_bytes_per_second(spec);
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

  pa_stream_set_state_callback(_recStream, PaStreamStateCallback, this);

  // Mark recording side as initialized
  _recIsInitialized = true;
}

// Sets the volume multiplier.
void SystemModule::SetSystemAudioLevel(float level) {
  audio_multiplier = level;
}

// Returns the volume multiplier.
float SystemModule::GetSystemAudioLevel() const {
  return audio_multiplier;
}

// Stops recording.
int32_t SystemModule::StopRecording() {
  webrtc::MutexLock lock(&mutex_);

  if (!_recIsInitialized) {
    return 0;
  }

  if (_recStream == NULL) {
    return -1;
  }

  _recIsInitialized = false;
  _recording = false;

  // Stop Recording
  PaLock();

  DisableReadCallback();
  pa_stream_set_overflow_callback(_recStream, NULL, NULL);

  // Unset this here so that we don't get a TERM    // RTC_LOG(LS_ERROR) << "Can't read data!";INATED callback
  pa_stream_set_state_callback(_recStream, NULL, NULL);

  auto state = pa_stream_get_state(_recStream);
  if (state != PA_STREAM_UNCONNECTED) {
    // Disconnect the stream
    if (pa_stream_disconnect(_recStream) != PA_OK) {
      PaUnLock();
      return -1;
    }
  }

  pa_stream_unref(_recStream);
  _recStream = NULL;

  PaUnLock();

  if (_recBuffer) {
    delete[] _recBuffer;
    _recBuffer = NULL;
  }

  return 0;
}

// Start recording.
int32_t SystemModule::StartRecording() {
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
    return -1;
  }

  {
    webrtc::MutexLock lock(&mutex_);
    if (!_recording) {
      return -1;
    }
  }
  return 0;
}

// Returns recording channels.
int32_t SystemModule::RecordingChannels() {
  return _recChannels;
}

// Enumerate system audio outputs.
std::vector<AudioSourceInfo> SystemModule::EnumerateSystemSource() const {
  auto result = std::vector<AudioSourceInfo>();

  SinkData data;
  data.vec = &result;
  data.pa_mainloop = _paMainloop;

  pa_threaded_mainloop_lock(_paMainloop);
  pa_operation* op = pa_context_get_sink_input_info_list(
      _paContext, reinterpret_cast<pa_sink_input_info_cb_t>(subscribe_sink_cb),
      &data);
  WaitForOperationCompletion(op);
  pa_threaded_mainloop_unlock(_paMainloop);

  return result;
};

#endif