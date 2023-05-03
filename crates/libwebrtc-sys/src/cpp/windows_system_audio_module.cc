
#if WEBRTC_WIN
#include "windows_system_audio_module.h"

DWORD GetSpeakerChannelMask(speaker_layout layout) {
  switch (layout) {
    case SPEAKERS_STEREO:
      return KSAUDIO_SPEAKER_STEREO;
    case SPEAKERS_2POINT1:
      return KSAUDIO_SPEAKER_2POINT1;
    case SPEAKERS_4POINT0:
      return KSAUDIO_SPEAKER_SURROUND;
    case SPEAKERS_4POINT1:
      return OBS_KSAUDIO_SPEAKER_4POINT1;
    case SPEAKERS_5POINT1:
      return KSAUDIO_SPEAKER_5POINT1_SURROUND;
    case SPEAKERS_7POINT1:
      return KSAUDIO_SPEAKER_7POINT1_SURROUND;
  }

  return (DWORD)layout;
}

speaker_layout SystemModule::ConvertSpeakerLayout(DWORD layout, WORD channels) {
  switch (layout) {
    case KSAUDIO_SPEAKER_2POINT1:
      return SPEAKERS_2POINT1;
    case KSAUDIO_SPEAKER_SURROUND:
      return SPEAKERS_4POINT0;
    case OBS_KSAUDIO_SPEAKER_4POINT1:
      return SPEAKERS_4POINT1;
    case KSAUDIO_SPEAKER_5POINT1_SURROUND:
      return SPEAKERS_5POINT1;
    case KSAUDIO_SPEAKER_7POINT1_SURROUND:
      return SPEAKERS_7POINT1;
  }

  return (speaker_layout)channels;
}

int GetChannels(speaker_layout layout) {
  return (int)layout;
}

WASAPIActivateAudioInterfaceCompletionHandler::
    WASAPIActivateAudioInterfaceCompletionHandler() {
  activationSignal = CreateEvent(nullptr, false, false, nullptr);
  if (!activationSignal.Valid())
    throw "Could not create receive signal";
}

HRESULT
WASAPIActivateAudioInterfaceCompletionHandler::GetActivateResult(
    IAudioClient** client) {
  WaitForSingleObject(activationSignal, INFINITE);
  *client = static_cast<IAudioClient*>(unknown);
  return activationResult;
}

HRESULT
WASAPIActivateAudioInterfaceCompletionHandler::ActivateCompleted(
    IActivateAudioInterfaceAsyncOperation* activateOperation) {
  HRESULT hr, hr_activate;
  hr = activateOperation->GetActivateResult(&hr_activate, &unknown);
  hr = SUCCEEDED(hr) ? hr_activate : hr;
  activationResult = hr;

  SetEvent(activationSignal);
  return hr;
}

void ReorderingBuffer(std::vector<int16_t>& output,
                      float* data,
                      int channels,
                      float audio_multiplier) {
  for (int i = 0; i < channels; ++i) {
    int k = i;
    for (int j = 480 * i; k < 480 * channels; ++j, k += channels) {
      float inSample = data[k] * audio_multiplier;
      if (inSample > 1.0)
        inSample = 1.0;
      if (inSample < -1.0)
        inSample = -1.0;

      if (inSample >= 0) {
        output[j] = (int16_t)lrintf(inSample * 32767.0);
      } else {
        output[j] = (int16_t)lrintf(inSample * 32768.0);
      }
    }
  }
}

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

SystemModule::SystemModule() {
  mmdevapi_module = LoadLibrary("Mmdevapi");
  if (mmdevapi_module) {
    activate_audio_interface_async =
        (PFN_ActivateAudioInterfaceAsync)GetProcAddress(
            mmdevapi_module, "ActivateAudioInterfaceAsync");
  }

  idleSignal = CreateEvent(nullptr, true, false, nullptr);
  if (!idleSignal.Valid())
    throw "Could not create idle signal";

  stopSignal = CreateEvent(nullptr, true, false, nullptr);
  if (!stopSignal.Valid())
    throw "Could not create stop signal";

  receiveSignal = CreateEvent(nullptr, false, false, nullptr);
  if (!receiveSignal.Valid())
    throw "Could not create receive signal";

  restartSignal = CreateEvent(nullptr, true, false, nullptr);
  if (!restartSignal.Valid())
    throw "Could not create restart signal";

  reconnectExitSignal = CreateEvent(nullptr, true, false, nullptr);
  if (!reconnectExitSignal.Valid())
    throw "Could not create reconnect exit signal";

  exitSignal = CreateEvent(nullptr, true, false, nullptr);
  if (!exitSignal.Valid())
    throw "Could not create exit signal";

  initSignal = CreateEvent(nullptr, false, false, nullptr);
  if (!initSignal.Valid())
    throw "Could not create init signal";

  reconnectSignal = CreateEvent(nullptr, false, false, nullptr);
  if (!reconnectSignal.Valid())
    throw "Could not create reconnect signal";

  captureThread =
      CreateThread(nullptr, 0, SystemModule::CaptureThread, this, 0, nullptr);

  if (!reconnectThread.Valid()) {
    ResetEvent(reconnectExitSignal);
    reconnectThread = CreateThread(nullptr, 0, SystemModule::ReconnectThread,
                                   this, 0, nullptr);
  }

  if (!captureThread.Valid()) {
    throw "Failed to create capture thread";
  }

  StartRecording();
}

SystemModule::~SystemModule() {
  Terminate();
}

ComPtr<IAudioClient> SystemModule::InitClient(
    DWORD process_id,
    PFN_ActivateAudioInterfaceAsync activate_audio_interface_async,
    speaker_layout& channels,
    int& format,
    uint32_t& samples_per_sec) {
  WAVEFORMATEXTENSIBLE wfextensible;
  const WAVEFORMATEX* pFormat;
  HRESULT res;
  ComPtr<IAudioClient> client;

  if (activate_audio_interface_async == NULL) {
    throw "ActivateAudioInterfaceAsync is not available";
  }

  const WORD nChannels = (WORD)channels;
  const DWORD nSamplesPerSec = 48000;
  constexpr WORD wBitsPerSample = 32;
  const WORD nBlockAlign = nChannels * wBitsPerSample / 8;

  WAVEFORMATEX& wf = wfextensible.Format;
  wf.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wf.nChannels = nChannels;
  wf.nSamplesPerSec = nSamplesPerSec;
  wf.nAvgBytesPerSec = nSamplesPerSec * nBlockAlign;
  wf.nBlockAlign = nBlockAlign;
  wf.wBitsPerSample = wBitsPerSample;
  wf.cbSize = sizeof(wfextensible) - sizeof(format);
  wfextensible.Samples.wValidBitsPerSample = wBitsPerSample;
  wfextensible.dwChannelMask =
      GetSpeakerChannelMask(speaker_layout::SPEAKERS_2POINT1);
  wfextensible.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

  AUDIOCLIENT_ACTIVATION_PARAMS audioclientActivationParams;
  audioclientActivationParams.ActivationType =
      AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;

  audioclientActivationParams.ProcessLoopbackParams.TargetProcessId =
      process_id;

  audioclientActivationParams.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
  PROPVARIANT activateParams{};
  activateParams.vt = VT_BLOB;
  activateParams.blob.cbSize = sizeof(audioclientActivationParams);
  activateParams.blob.pBlobData =
      reinterpret_cast<BYTE*>(&audioclientActivationParams);

  {
    Microsoft::WRL::ComPtr<WASAPIActivateAudioInterfaceCompletionHandler>
        handler = Microsoft::WRL::Make<
            WASAPIActivateAudioInterfaceCompletionHandler>();
    ComPtr<IActivateAudioInterfaceAsyncOperation> asyncOp;
    res = activate_audio_interface_async(
        VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
        &activateParams, handler.Get(), &asyncOp);

    if (FAILED(res)) {
      throw HRError("Failed to get activate audio client", res);
    }

    res = handler->GetActivateResult(client.Assign());
    if (FAILED(res)) {
      throw HRError("Async activation failed", res);
    }
  }

  pFormat = &wf;

  InitFormat(pFormat, channels, format, samples_per_sec);

  DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
  res = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, BUFFER_TIME_100NS,
                           0, pFormat, nullptr);
  if (FAILED(res))
    throw HRError("Failed to initialize audio client", res);

  return client;
}

DWORD WINAPI SystemModule::ReconnectThread(LPVOID param) {
  SystemModule* source = (SystemModule*)param;

  const HANDLE sigs[] = {
      source->reconnectExitSignal,
      source->reconnectSignal,
  };

  const HANDLE reconnect_sigs[] = {
      source->reconnectExitSignal,
      source->stopSignal,
  };

  bool exit = false;
  while (!exit) {
    const DWORD ret =
        WaitForMultipleObjects(_countof(sigs), sigs, false, INFINITE);
    switch (ret) {
      case WAIT_OBJECT_0:
        exit = true;
        break;
      default:
        assert(ret == (WAIT_OBJECT_0 + 1));
        if (source->reconnectDuration > 0) {
          WaitForMultipleObjects(_countof(reconnect_sigs), reconnect_sigs,
                                 false, source->reconnectDuration);
        }
        SetEvent(source->initSignal);
    }
  }

  return 0;
}

DWORD WINAPI SystemModule::CaptureThread(LPVOID param) {
  const HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  const bool com_initialized = SUCCEEDED(hr);

  DWORD unused = 0;
  const HANDLE handle = AvSetMmThreadCharacteristics("Audio", &unused);

  SystemModule* source = (SystemModule*)param;

  const HANDLE inactive_sigs[] = {
      source->exitSignal,
      source->stopSignal,
      source->initSignal,
  };

  const HANDLE active_sigs[] = {
      source->exitSignal,
      source->stopSignal,
      source->receiveSignal,
      source->restartSignal,
  };

  DWORD sig_count = _countof(inactive_sigs);
  const HANDLE* sigs = inactive_sigs;

  bool exit = false;
  while (!exit) {
    bool idle = false;
    bool stop = false;
    bool reconnect = false;
    do {
      /* Windows 7 does not seem to wake up for LOOPBACK */
      const DWORD dwMilliseconds = sigs == active_sigs ? 10 : INFINITE;
      const DWORD ret =
          WaitForMultipleObjects(sig_count, sigs, false, dwMilliseconds);
      switch (ret) {
        case WAIT_OBJECT_0: {
          exit = true;
          stop = true;
          idle = true;
          break;
        }

        case WAIT_OBJECT_0 + 1:
          stop = true;
          idle = true;
          break;

        case WAIT_OBJECT_0 + 2:
        case WAIT_TIMEOUT:
          if (sigs == inactive_sigs) {
            assert(ret != WAIT_TIMEOUT);
            if (source->Init()) {
              sig_count = _countof(active_sigs);
              sigs = active_sigs;
            } else {
              stop = true;
              reconnect = true;
              source->reconnectDuration = RECONNECT_INTERVAL;
            }
          } else {
            stop = !source->ProcessCaptureData();
            if (stop) {
              stop = true;
              reconnect = true;
              source->reconnectDuration = RECONNECT_INTERVAL;
            }
          }
          break;

        default:
          assert(sigs == active_sigs);
          assert(ret == WAIT_OBJECT_0 + 3);
          stop = true;
          reconnect = true;
          source->reconnectDuration = 0;
          ResetEvent(source->restartSignal);
      }
    } while (!stop);

    sig_count = _countof(inactive_sigs);
    sigs = inactive_sigs;

    if (source->client) {
      source->client->Stop();

      source->capture.Clear();
      source->client.Clear();
    }

    if (idle) {
      SetEvent(source->idleSignal);
    } else if (reconnect) {
      SetEvent(source->reconnectSignal);
    }
  }

  if (handle) {
    AvRevertMmThreadCharacteristics(handle);
  }

  if (com_initialized) {
    CoUninitialize();
  }

  return 0;
}

bool SystemModule::ProcessCaptureData() {
  if (source) {
    int channels = GetChannels(speakers);
    if (stop) {
      return false;
    }

    HRESULT res;
    LPBYTE buffer;
    UINT32 frames;
    DWORD flags;
    UINT64 pos, ts;
    UINT captureSize = 0;

    while (true) {
      res = capture->GetNextPacketSize(&captureSize);
      if (FAILED(res)) {
        if (res != AUDCLNT_E_DEVICE_INVALIDATED)
          return false;
      }

      if (!captureSize) {
        source->Mute();
        break;
      }

      res = capture->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
      if (FAILED(res)) {
        if (res != AUDCLNT_E_DEVICE_INVALIDATED)
          return false;
      }

      float* data = (float*)buffer;
      for (int i = 0; i < frames * channels; ++i) {
        capture_buffer.push_back(data[i]);

        if (capture_buffer.size() == ((sampleRate / 100) * channels)) {
          ReorderingBuffer(release_capture_buffer, capture_buffer.data(),
                           channels, audio_multiplier);
          source->UpdateFrame((const int16_t*)(release_capture_buffer.data()),
                              480, 48000, channels);
          capture_buffer.clear();
        }
      }

      capture->ReleaseBuffer(frames);
    }

    return true;
  } else {
    return false;
  }
}

void SystemModule::Initialize() {
  ResetEvent(receiveSignal);

  ComPtr<IAudioClient> temp_client = InitClient(
      process_id, activate_audio_interface_async, speakers, format, sampleRate);

  ComPtr<IAudioCaptureClient> temp_capture =
      InitCapture(temp_client, receiveSignal);

  client = std::move(temp_client);
  capture = std::move(temp_capture);
}

void SystemModule::InitFormat(const WAVEFORMATEX* wfex,
                              speaker_layout& channels,
                              int& format,
                              uint32_t& sampleRate) {
  DWORD layout = 0;

  if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    WAVEFORMATEXTENSIBLE* ext = (WAVEFORMATEXTENSIBLE*)wfex;
    layout = ext->dwChannelMask;
  }

  /* WASAPI is always float */
  channels = ConvertSpeakerLayout(layout, wfex->nChannels);
  format = 4;  // float bytes
  sampleRate = wfex->nSamplesPerSec;
}

ComPtr<IAudioCaptureClient> SystemModule::InitCapture(IAudioClient* client,
                                                      HANDLE receiveSignal) {
  ComPtr<IAudioCaptureClient> capture;
  HRESULT res = client->GetService(IID_PPV_ARGS(capture.Assign()));
  if (FAILED(res))
    throw HRError("Failed to create capture context", res);

  res = client->SetEventHandle(receiveSignal);
  if (FAILED(res))
    throw HRError("Failed to set event handle", res);

  res = client->Start();
  if (FAILED(res))
    throw HRError("Failed to start capture client", res);

  return capture;
}

////////////////////////// API

int32_t SystemModule::Terminate() {
  SetEvent(exitSignal);
  return StopRecording();
}

int32_t SystemModule::StartRecording() {
  SetEvent(initSignal);
  return 0;
}

void SystemModule::SetSystemAudioLevel(float level) {
  audio_multiplier = level;
}

float SystemModule::GetSystemAudioLevel() const {
  return audio_multiplier;
}

void SystemModule::SetRecordingSource(int id) {
  const bool restart = process_id != id;
  process_id = id;

  if (restart) {
    SetEvent(restartSignal);
  }
}

bool SystemModule::Init() {
  bool success;
  try {
    Initialize();
    success = true;
  } catch (...) {
    success = false;
  }

  previouslyFailed = !success;
  return success;
}

int SystemModule::StopRecording() {
  return 0;
}

rtc::scoped_refptr<AudioSource> SystemModule::CreateSource() {
  if (!source) {
    source = rtc::scoped_refptr<SystemSource>(new SystemSource(this));
  }
  auto result = source;
  source->Release();
  return result;
}

int32_t SystemModule::RecordingChannels() {
  return GetChannels(speakers);
}

void SystemModule::ResetSource() {
  source = nullptr;
}

std::vector<AudioSourceInfo> SystemModule::EnumerateSystemSource() const {
  return ms_fill_window_list(window_search_mode::INCLUDE_MINIMIZED);
}

#endif