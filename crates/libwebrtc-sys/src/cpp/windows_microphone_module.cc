/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if WEBRTC_WIN
#pragma warning(disable : 4995)  // name was marked as #pragma deprecated

#if (_MSC_VER >= 1310) && (_MSC_VER < 1400)
// Reports the major and minor versions of the compiler.
// For example, 1310 for Microsoft Visual C++ .NET 2003. 1310 represents version
// 13 and a 1.0 point release. The Visual C++ 2005 compiler version is 1400.
// Type cl /? at the command line to see the major and minor versions of your
// compiler along with the build number.
#pragma message(">> INFO: Windows Core Audio is not supported in VS 2003")
#endif

#include "modules/audio_device/audio_device_config.h"

#ifdef WEBRTC_WINDOWS_CORE_AUDIO_BUILD

// clang-format off
// To get Windows includes in the right order, this must come before the Windows
// includes below.
#include "windows_microphone_module.h"
// clang-format on

#include <string.h>

#include <comdef.h>
#include <dmo.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmsystem.h>
#include <strsafe.h>
#include <uuids.h>
#include <windows.h>

#include <iomanip>

#include "api/make_ref_counted.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/sleep.h"

// Macro that calls a COM method returning HRESULT value.
#define EXIT_ON_ERROR(hres) \
  do {                      \
    if (FAILED(hres))       \
      goto Exit;            \
  } while (0)

// Macro that continues to a COM error.
#define CONTINUE_ON_ERROR(hres) \
  do {                          \
    if (FAILED(hres))           \
      goto Next;                \
  } while (0)

// Macro that releases a COM object if not NULL.
#define SAFE_RELEASE(p) \
  do {                  \
    if ((p)) {          \
      (p)->Release();   \
      (p) = NULL;       \
    }                   \
  } while (0)

#define ROUND(x) ((x) >= 0 ? (int)((x) + 0.5) : (int)((x)-0.5))

// REFERENCE_TIME time units per millisecond
#define REFTIMES_PER_MILLISEC 10000

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;      // must be 0x1000
  LPCSTR szName;     // pointer to name (in user addr space)
  DWORD dwThreadID;  // thread ID (-1=caller thread)
  DWORD dwFlags;     // reserved for future use, must be zero
} THREADNAME_INFO;

namespace webrtc {
namespace {

enum { COM_THREADING_MODEL = COINIT_MULTITHREADED };

enum { kAecCaptureStreamIndex = 0, kAecRenderStreamIndex = 1 };

// An implementation of IMediaBuffer, as required for
// IMediaObject::ProcessOutput(). After consuming data provided by
// ProcessOutput(), call SetLength() to update the buffer availability.
//
// Example implementation:
// http://msdn.microsoft.com/en-us/library/dd376684(v=vs.85).aspx
class MediaBufferImpl final : public IMediaBuffer {
 public:
  explicit MediaBufferImpl(DWORD maxLength)
      : _data(new BYTE[maxLength]),
        _length(0),
        _maxLength(maxLength),
        _refCount(0) {}

  // IMediaBuffer methods.
  STDMETHOD(GetBufferAndLength(BYTE** ppBuffer, DWORD* pcbLength)) {
    if (!ppBuffer || !pcbLength) {
      return E_POINTER;
    }

    *ppBuffer = _data;
    *pcbLength = _length;

    return S_OK;
  }

  STDMETHOD(GetMaxLength(DWORD* pcbMaxLength)) {
    if (!pcbMaxLength) {
      return E_POINTER;
    }

    *pcbMaxLength = _maxLength;
    return S_OK;
  }

  STDMETHOD(SetLength(DWORD cbLength)) {
    if (cbLength > _maxLength) {
      return E_INVALIDARG;
    }

    _length = cbLength;
    return S_OK;
  }

  // IUnknown methods.
  STDMETHOD_(ULONG, AddRef()) { return InterlockedIncrement(&_refCount); }

  STDMETHOD(QueryInterface(REFIID riid, void** ppv)) {
    if (!ppv) {
      return E_POINTER;
    } else if (riid != IID_IMediaBuffer && riid != IID_IUnknown) {
      return E_NOINTERFACE;
    }

    *ppv = static_cast<IMediaBuffer*>(this);
    AddRef();
    return S_OK;
  }

  STDMETHOD_(ULONG, Release()) {
    LONG refCount = InterlockedDecrement(&_refCount);
    if (refCount == 0) {
      delete this;
    }

    return refCount;
  }

 private:
  ~MediaBufferImpl() { delete[] _data; }

  BYTE* _data;
  DWORD _length;
  const DWORD _maxLength;
  LONG _refCount;
};
}  // namespace
}  // namespace webrtc

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

void MicrophoneModule::ResetSource() {
  webrtc::MutexLock lock(&mutex_);
  source = nullptr;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::SetRecordingDevice(uint16_t index) {
  bool start = _recIsInitialized;
  StopRecording();

  if (_recIsInitialized) {
    return -1;
  }

  // Get current number of available capture endpoint devices and refresh the
  // capture collection.
  UINT nDevices = RecordingDevices();

  if (index < 0 || index > (nDevices - 1)) {
    RTC_LOG(LS_ERROR) << "device index is out of range [0," << (nDevices - 1)
                      << "]";
    return -1;
  }

  webrtc::MutexLock lock(&mutex_);

  HRESULT hr(S_OK);

  RTC_DCHECK(_ptrCaptureCollection);

  // Select an endpoint capture device given the specified index
  SAFE_RELEASE(_ptrDeviceIn);
  hr = _ptrCaptureCollection->Item(index, &_ptrDeviceIn);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    SAFE_RELEASE(_ptrDeviceIn);
    return -1;
  }

  WCHAR szDeviceName[MAX_PATH];
  const int bufferLen = sizeof(szDeviceName) / sizeof(szDeviceName)[0];

  // Get the endpoint device's friendly-name
  if (_GetDeviceName(_ptrDeviceIn, szDeviceName, bufferLen) == 0) {
    RTC_LOG(LS_VERBOSE) << "friendly name: \"" << szDeviceName << "\"";
  }

  _usingInputDeviceIndex = true;
  _inputDeviceIndex = index;

  if (start) {
    InitRecording();
    StartRecording();
  }
  return 0;
}

int32_t MicrophoneModule::RecordingChannels() {
  return _recChannels;
}

// ----------------------------------------------------------------------------
//  DoCaptureThread
// ----------------------------------------------------------------------------

DWORD MicrophoneModule::DoCaptureThread() {
  bool keepRecording = true;
  HANDLE waitArray[2] = {_hShutdownCaptureEvent, _hCaptureSamplesReadyEvent};
  HRESULT hr = S_OK;

  LARGE_INTEGER t1;

  BYTE* syncBuffer = NULL;
  UINT32 syncBufIndex = 0;

  _readSamples = 0;

  // Initialize COM as MTA in this thread.
  webrtc::ScopedCOMInitializer comInit(webrtc::ScopedCOMInitializer::kMTA);
  if (!comInit.Succeeded()) {
    RTC_LOG(LS_ERROR) << "failed to initialize COM in capture thread";
    return 1;
  }

  hr = InitCaptureThreadPriority();
  if (FAILED(hr)) {
    return hr;
  }

  mutex_.Lock();

  // Get size of capturing buffer (length is expressed as the number of audio
  // frames the buffer can hold). This value is fixed during the capturing
  // session.
  //
  UINT32 bufferLength = 0;
  if (_ptrClientIn == NULL) {
    RTC_LOG(LS_ERROR)
        << "input state has been modified before capture loop starts.";
    return 1;
  }
  hr = _ptrClientIn->GetBufferSize(&bufferLength);
  EXIT_ON_ERROR(hr);
  RTC_LOG(LS_VERBOSE) << "[CAPT] size of buffer       : " << bufferLength;

  // Allocate memory for sync buffer.
  // It is used for compensation between native 44.1 and internal 44.0 and
  // for cases when the capture buffer is larger than 10ms.
  //
  const UINT32 syncBufferSize = 2 * (bufferLength * _recAudioFrameSize);
  syncBuffer = new BYTE[syncBufferSize];
  if (syncBuffer == NULL) {
    return (DWORD)E_POINTER;
  }
  RTC_LOG(LS_VERBOSE) << "[CAPT] size of sync buffer  : " << syncBufferSize
                      << " [bytes]";

  // Get maximum latency for the current stream (will not change for the
  // lifetime of the IAudioClient object).
  //
  REFERENCE_TIME latency;
  _ptrClientIn->GetStreamLatency(&latency);
  RTC_LOG(LS_VERBOSE) << "[CAPT] max stream latency   : " << (DWORD)latency
                      << " (" << (double)(latency / 10000.0) << " ms)";

  // Get the length of the periodic interval separating successive processing
  // passes by the audio engine on the data in the endpoint buffer.
  //
  REFERENCE_TIME devPeriod = 0;
  REFERENCE_TIME devPeriodMin = 0;
  _ptrClientIn->GetDevicePeriod(&devPeriod, &devPeriodMin);
  RTC_LOG(LS_VERBOSE) << "[CAPT] device period        : " << (DWORD)devPeriod
                      << " (" << (double)(devPeriod / 10000.0) << " ms)";

  double extraDelayMS = (double)((latency + devPeriod) / 10000.0);
  RTC_LOG(LS_VERBOSE) << "[CAPT] extraDelayMS         : " << extraDelayMS;

  double endpointBufferSizeMS =
      10.0 * ((double)bufferLength / (double)_recBlockSize);
  RTC_LOG(LS_VERBOSE) << "[CAPT] endpointBufferSizeMS : "
                      << endpointBufferSizeMS;

  // Start up the capturing stream.
  //
  hr = _ptrClientIn->Start();
  EXIT_ON_ERROR(hr);

  mutex_.Unlock();

  // Set event which will ensure that the calling thread modifies the recording
  // state to true.
  //
  SetEvent(_hCaptureStartedEvent);

  // >> ---------------------------- THREAD LOOP ----------------------------

  while (keepRecording) {
    // Wait for a capture notification event or a shutdown event
    DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, 500);
    switch (waitResult) {
      case WAIT_OBJECT_0 + 0:  // _hShutdownCaptureEvent
        keepRecording = false;
        break;
      case WAIT_OBJECT_0 + 1:  // _hCaptureSamplesReadyEvent
        break;
      case WAIT_TIMEOUT:  // timeout notification
        RTC_LOG(LS_WARNING) << "capture event timed out after 0.5 seconds";
        goto Exit;
      default:  // unexpected error
        RTC_LOG(LS_WARNING) << "unknown wait termination on capture side";
        goto Exit;
    }

    while (keepRecording) {
      BYTE* pData = 0;
      UINT32 framesAvailable = 0;
      DWORD flags = 0;
      UINT64 recTime = 0;
      UINT64 recPos = 0;

      mutex_.Lock();

      // Sanity check to ensure that essential states are not modified
      // during the unlocked period.
      if (_ptrCaptureClient == NULL || _ptrClientIn == NULL) {
        mutex_.Unlock();
        RTC_LOG(LS_ERROR)
            << "input state has been modified during unlocked period";
        goto Exit;
      }

      //  Find out how much capture data is available
      //
      hr = _ptrCaptureClient->GetBuffer(
          &pData,            // packet which is ready to be read by used
          &framesAvailable,  // #frames in the captured packet (can be zero)
          &flags,            // support flags (check)
          &recPos,    // device position of first audio frame in data packet
          &recTime);  // value of performance counter at the time of recording
                      // the first audio frame

      if (SUCCEEDED(hr)) {
        if (AUDCLNT_S_BUFFER_EMPTY == hr) {
          // Buffer was empty => start waiting for a new capture notification
          // event
          mutex_.Unlock();
          break;
        }

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
          // Treat all of the data in the packet as silence and ignore the
          // actual data values.
          RTC_LOG(LS_WARNING) << "AUDCLNT_BUFFERFLAGS_SILENT";
          pData = NULL;
        }

        RTC_DCHECK_NE(framesAvailable, 0);

        if (pData) {
          CopyMemory(&syncBuffer[syncBufIndex * _recAudioFrameSize], pData,
                     framesAvailable * _recAudioFrameSize);
        } else {
          ZeroMemory(&syncBuffer[syncBufIndex * _recAudioFrameSize],
                     framesAvailable * _recAudioFrameSize);
        }
        RTC_DCHECK_GE(syncBufferSize, (syncBufIndex * _recAudioFrameSize) +
                                          framesAvailable * _recAudioFrameSize);

        // Release the capture buffer
        //
        hr = _ptrCaptureClient->ReleaseBuffer(framesAvailable);
        EXIT_ON_ERROR(hr);

        _readSamples += framesAvailable;
        syncBufIndex += framesAvailable;

        QueryPerformanceCounter(&t1);

        // Get the current recording and playout delay.
        uint32_t sndCardRecDelay =
            (uint32_t)(((((UINT64)t1.QuadPart * _perfCounterFactor) - recTime) /
                        10000) +
                       (10 * syncBufIndex) / _recBlockSize - 10);
        uint32_t sndCardPlayDelay = static_cast<uint32_t>(_sndCardPlayDelay);

        while (syncBufIndex >= _recBlockSize) {
          if (source) {
            source->UpdateFrame((const int16_t*)syncBuffer, _recBlockSize,
                                _recSampleRate, _recChannels);
          }

          // store remaining data which was not able to deliver as 10ms segment
          MoveMemory(&syncBuffer[0],
                     &syncBuffer[_recBlockSize * _recAudioFrameSize],
                     (syncBufIndex - _recBlockSize) * _recAudioFrameSize);
          syncBufIndex -= _recBlockSize;
          sndCardRecDelay -= 10;
        }
      } else {
        // If GetBuffer returns AUDCLNT_E_BUFFER_ERROR, the thread consuming the
        // audio samples must wait for the next processing pass. The client
        // might benefit from keeping a count of the failed GetBuffer calls. If
        // GetBuffer returns this error repeatedly, the client can start a new
        // processing loop after shutting down the current client by calling
        // IAudioClient::Stop, IAudioClient::Reset, and releasing the audio
        // client.
        RTC_LOG(LS_ERROR) << "IAudioCaptureClient::GetBuffer returned"
                             " AUDCLNT_E_BUFFER_ERROR, hr = 0x"
                          << rtc::ToHex(hr);
        goto Exit;
      }

      mutex_.Unlock();
    }
  }

  // ---------------------------- THREAD LOOP ---------------------------- <<

  if (_ptrClientIn) {
    hr = _ptrClientIn->Stop();
  }

Exit:
  if (FAILED(hr)) {
    _ptrClientIn->Stop();
    mutex_.Unlock();
    _TraceCOMError(hr);
  }

  RevertCaptureThreadPriority();

  mutex_.Lock();

  if (keepRecording) {
    if (_ptrClientIn != NULL) {
      hr = _ptrClientIn->Stop();
      if (FAILED(hr)) {
        _TraceCOMError(hr);
      }
      hr = _ptrClientIn->Reset();
      if (FAILED(hr)) {
        _TraceCOMError(hr);
      }
    }

    RTC_LOG(LS_ERROR)
        << "Recording error: capturing thread has ended pre-maturely";
  } else {
    RTC_LOG(LS_VERBOSE) << "_Capturing thread is now terminated properly";
  }

  SAFE_RELEASE(_ptrClientIn);
  SAFE_RELEASE(_ptrCaptureClient);

  mutex_.Unlock();

  if (syncBuffer) {
    delete[] syncBuffer;
  }

  return (DWORD)hr;
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

DWORD MicrophoneModule::DoCaptureThreadPollDMO() {
  RTC_DCHECK(_mediaBuffer);
  bool keepRecording = true;

  // Initialize COM as MTA in this thread.
  webrtc::ScopedCOMInitializer comInit(webrtc::ScopedCOMInitializer::kMTA);
  if (!comInit.Succeeded()) {
    RTC_LOG(LS_ERROR) << "failed to initialize COM in polling DMO thread";
    return 1;
  }

  HRESULT hr = InitCaptureThreadPriority();
  if (FAILED(hr)) {
    return hr;
  }

  // Set event which will ensure that the calling thread modifies the
  // recording state to true.
  SetEvent(_hCaptureStartedEvent);

  // >> ---------------------------- THREAD LOOP ----------------------------
  while (keepRecording) {
    // Poll the DMO every 5 ms.
    // (The same interval used in the Wave implementation.)
    DWORD waitResult = WaitForSingleObject(_hShutdownCaptureEvent, 5);
    switch (waitResult) {
      case WAIT_OBJECT_0:  // _hShutdownCaptureEvent
        keepRecording = false;
        break;
      case WAIT_TIMEOUT:  // timeout notification
        break;
      default:  // unexpected error
        RTC_LOG(LS_WARNING) << "Unknown wait termination on capture side";
        hr = -1;  // To signal an error callback.
        keepRecording = false;
        break;
    }

    while (keepRecording) {
      webrtc::MutexLock lockScoped(&mutex_);

      DWORD dwStatus = 0;
      {
        DMO_OUTPUT_DATA_BUFFER dmoBuffer = {0};
        dmoBuffer.pBuffer = _mediaBuffer.get();
        dmoBuffer.pBuffer->AddRef();

        // Poll the DMO for AEC processed capture data. The DMO will
        // copy available data to `dmoBuffer`, and should only return
        // 10 ms frames. The value of `dwStatus` should be ignored.
        hr = _dmo->ProcessOutput(0, 1, &dmoBuffer, &dwStatus);
        SAFE_RELEASE(dmoBuffer.pBuffer);
        dwStatus = dmoBuffer.dwStatus;
      }
      if (FAILED(hr)) {
        _TraceCOMError(hr);
        keepRecording = false;
        RTC_DCHECK_NOTREACHED();
        break;
      }

      ULONG bytesProduced = 0;
      BYTE* data;
      // Get a pointer to the data buffer. This should be valid until
      // the next call to ProcessOutput.
      hr = _mediaBuffer->GetBufferAndLength(&data, &bytesProduced);
      if (FAILED(hr)) {
        _TraceCOMError(hr);
        keepRecording = false;
        RTC_DCHECK_NOTREACHED();
        break;
      }

      if (bytesProduced > 0) {
        const int kSamplesProduced = bytesProduced / _recAudioFrameSize;
        // TODO(andrew): verify that this is always satisfied. It might
        // be that ProcessOutput will try to return more than 10 ms if
        // we fail to call it frequently enough.
        RTC_DCHECK_EQ(kSamplesProduced, static_cast<int>(_recBlockSize));
        RTC_DCHECK_EQ(sizeof(BYTE), sizeof(int8_t));

        if (source) {
          source->UpdateFrame((const int16_t*)data, kSamplesProduced,
                              _recSampleRate, _recChannels);
        }

        // mutex_.Unlock();  // Release lock while making the callback.
        // _ptrAudioBuffer->DeliverRecordedData();
        // mutex_.Lock();
      }

      // Reset length to indicate buffer availability.
      hr = _mediaBuffer->SetLength(0);
      if (FAILED(hr)) {
        _TraceCOMError(hr);
        keepRecording = false;
        RTC_DCHECK_NOTREACHED();
        break;
      }

      if (!(dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE)) {
        // The DMO cannot currently produce more data. This is the
        // normal case; otherwise it means the DMO had more than 10 ms
        // of data available and ProcessOutput should be called again.
        break;
      }
    }
  }
}

// ----------------------------------------------------------------------------
//  Everything below has been copied unchanged from audio_device_core_win.сс
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
//  Terminate
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::Terminate() {
  webrtc::MutexLock lock(&mutex_);

  if (!_initialized) {
    return 0;
  }

  _initialized = false;
  _speakerIsInitialized = false;
  _microphoneIsInitialized = false;
  _playing = false;
  _recording = false;

  SAFE_RELEASE(_ptrRenderCollection);
  SAFE_RELEASE(_ptrCaptureCollection);
  SAFE_RELEASE(_ptrDeviceOut);
  SAFE_RELEASE(_ptrDeviceIn);
  SAFE_RELEASE(_ptrClientOut);
  SAFE_RELEASE(_ptrClientIn);
  SAFE_RELEASE(_ptrRenderClient);
  SAFE_RELEASE(_ptrCaptureClient);
  SAFE_RELEASE(_ptrCaptureVolume);
  SAFE_RELEASE(_ptrRenderSimpleVolume);

  return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneModule() - ctor
// ----------------------------------------------------------------------------

MicrophoneModule::MicrophoneModule()
    : _avrtLibrary(nullptr),
      _winSupportAvrt(false),
      _comInit(webrtc::ScopedCOMInitializer::kMTA),
      _ptrAudioBuffer(nullptr),
      _ptrEnumerator(nullptr),
      _ptrRenderCollection(nullptr),
      _ptrCaptureCollection(nullptr),
      _ptrDeviceOut(nullptr),
      _ptrDeviceIn(nullptr),
      _ptrClientOut(nullptr),
      _ptrClientIn(nullptr),
      _ptrRenderClient(nullptr),
      _ptrCaptureClient(nullptr),
      _ptrCaptureVolume(nullptr),
      _ptrRenderSimpleVolume(nullptr),
      _dmo(nullptr),
      _mediaBuffer(nullptr),
      _builtInAecEnabled(false),
      _hRenderSamplesReadyEvent(nullptr),
      _hPlayThread(nullptr),
      _hRenderStartedEvent(nullptr),
      _hShutdownRenderEvent(nullptr),
      _hCaptureSamplesReadyEvent(nullptr),
      _hRecThread(nullptr),
      _hCaptureStartedEvent(nullptr),
      _hShutdownCaptureEvent(nullptr),
      _hMmTask(nullptr),
      _playAudioFrameSize(0),
      _playSampleRate(0),
      _playBlockSize(0),
      _playChannels(2),
      _sndCardPlayDelay(0),
      _writtenSamples(0),
      _readSamples(0),
      _recAudioFrameSize(0),
      _recSampleRate(0),
      _recBlockSize(0),
      _recChannels(2),
      _initialized(false),
      _recording(false),
      _playing(false),
      _recIsInitialized(false),
      _playIsInitialized(false),
      _speakerIsInitialized(false),
      _microphoneIsInitialized(false),
      _usingInputDeviceIndex(false),
      _usingOutputDeviceIndex(false),
      _inputDevice(webrtc::AudioDeviceModule::kDefaultCommunicationDevice),
      _outputDevice(webrtc::AudioDeviceModule::kDefaultCommunicationDevice),
      _inputDeviceIndex(0),
      _outputDeviceIndex(0) {
  RTC_DLOG(LS_INFO) << __FUNCTION__ << " created";
  RTC_DCHECK(_comInit.Succeeded());

  // Try to load the Avrt DLL
  if (!_avrtLibrary) {
    // Get handle to the Avrt DLL module.
    _avrtLibrary = LoadLibrary(TEXT("Avrt.dll"));
    if (_avrtLibrary) {
      // Handle is valid (should only happen if OS larger than vista & win7).
      // Try to get the function addresses.
      RTC_LOG(LS_VERBOSE) << "AudioDeviceWindowsCore::AudioDeviceWindowsCore()"
                             " The Avrt DLL module is now loaded";

      _PAvRevertMmThreadCharacteristics =
          (PAvRevertMmThreadCharacteristics)GetProcAddress(
              _avrtLibrary, "AvRevertMmThreadCharacteristics");
      _PAvSetMmThreadCharacteristicsA =
          (PAvSetMmThreadCharacteristicsA)GetProcAddress(
              _avrtLibrary, "AvSetMmThreadCharacteristicsA");
      _PAvSetMmThreadPriority = (PAvSetMmThreadPriority)GetProcAddress(
          _avrtLibrary, "AvSetMmThreadPriority");

      if (_PAvRevertMmThreadCharacteristics &&
          _PAvSetMmThreadCharacteristicsA && _PAvSetMmThreadPriority) {
        RTC_LOG(LS_VERBOSE)
            << "AudioDeviceWindowsCore::AudioDeviceWindowsCore()"
               " AvRevertMmThreadCharacteristics() is OK";
        RTC_LOG(LS_VERBOSE)
            << "AudioDeviceWindowsCore::AudioDeviceWindowsCore()"
               " AvSetMmThreadCharacteristicsA() is OK";
        RTC_LOG(LS_VERBOSE)
            << "AudioDeviceWindowsCore::AudioDeviceWindowsCore()"
               " AvSetMmThreadPriority() is OK";
        _winSupportAvrt = true;
      }
    }
  }

  // Create our samples ready events - we want auto reset events that start in
  // the not-signaled state. The state of an auto-reset event object remains
  // signaled until a single waiting thread is released, at which time the
  // system automatically sets the state to nonsignaled. If no threads are
  // waiting, the event object's state remains signaled. (Except for
  // _hShutdownCaptureEvent, which is used to shutdown multiple threads).
  _hRenderSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  _hCaptureSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  _hShutdownRenderEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  _hShutdownCaptureEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  _hRenderStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  _hCaptureStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  _perfCounterFreq.QuadPart = 1;
  _perfCounterFactor = 0.0;

  // list of number of channels to use on recording side
  _recChannelsPrioList[0] = 2;  // stereo is prio 1
  _recChannelsPrioList[1] = 1;  // mono is prio 2
  _recChannelsPrioList[2] = 4;  // quad is prio 3

  // list of number of channels to use on playout side
  _playChannelsPrioList[0] = 2;  // stereo is prio 1
  _playChannelsPrioList[1] = 1;  // mono is prio 2

  HRESULT hr;

  // We know that this API will work since it has already been verified in
  // CoreAudioIsSupported, hence no need to check for errors here as well.

  // Retrive the IMMDeviceEnumerator API (should load the MMDevAPI.dll)
  // TODO(henrika): we should probably move this allocation to Init() instead
  // and deallocate in Terminate() to make the implementation more symmetric.
  CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                   __uuidof(IMMDeviceEnumerator),
                   reinterpret_cast<void**>(&_ptrEnumerator));
  RTC_DCHECK(_ptrEnumerator);

  // DMO initialization for built-in WASAPI AEC.
  {
    IMediaObject* ptrDMO = NULL;
    hr = CoCreateInstance(CLSID_CWMAudioAEC, NULL, CLSCTX_INPROC_SERVER,
                          IID_IMediaObject, reinterpret_cast<void**>(&ptrDMO));
    if (FAILED(hr) || ptrDMO == NULL) {
      // Since we check that _dmo is non-NULL in EnableBuiltInAEC(), the
      // feature is prevented from being enabled.
      _builtInAecEnabled = false;
      _TraceCOMError(hr);
    }
    _dmo = ptrDMO;
    SAFE_RELEASE(ptrDMO);
  }
}

// ----------------------------------------------------------------------------
//  MicrophoneModule() - dtor
// ----------------------------------------------------------------------------

MicrophoneModule::~MicrophoneModule() {
  RTC_DLOG(LS_INFO) << __FUNCTION__ << " destroyed";

  Terminate();

  // The IMMDeviceEnumerator is created during construction. Must release
  // it here and not in Terminate() since we don't recreate it in Init().
  SAFE_RELEASE(_ptrEnumerator);

  _ptrAudioBuffer = NULL;

  if (NULL != _hRenderSamplesReadyEvent) {
    CloseHandle(_hRenderSamplesReadyEvent);
    _hRenderSamplesReadyEvent = NULL;
  }

  if (NULL != _hCaptureSamplesReadyEvent) {
    CloseHandle(_hCaptureSamplesReadyEvent);
    _hCaptureSamplesReadyEvent = NULL;
  }

  if (NULL != _hRenderStartedEvent) {
    CloseHandle(_hRenderStartedEvent);
    _hRenderStartedEvent = NULL;
  }

  if (NULL != _hCaptureStartedEvent) {
    CloseHandle(_hCaptureStartedEvent);
    _hCaptureStartedEvent = NULL;
  }

  if (NULL != _hShutdownRenderEvent) {
    CloseHandle(_hShutdownRenderEvent);
    _hShutdownRenderEvent = NULL;
  }

  if (NULL != _hShutdownCaptureEvent) {
    CloseHandle(_hShutdownCaptureEvent);
    _hShutdownCaptureEvent = NULL;
  }

  if (_avrtLibrary) {
    BOOL freeOK = FreeLibrary(_avrtLibrary);
    if (!freeOK) {
      RTC_LOG(LS_WARNING)
          << "AudioDeviceWindowsCore::~AudioDeviceWindowsCore()"
             " failed to free the loaded Avrt DLL module correctly";
    } else {
      RTC_LOG(LS_WARNING) << "AudioDeviceWindowsCore::~AudioDeviceWindowsCore()"
                             " the Avrt DLL module is now unloaded";
    }
  }
}

// ----------------------------------------------------------------------------
//  _EnumerateEndpointDevicesAll
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::_EnumerateEndpointDevicesAll(
    EDataFlow dataFlow) const {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  RTC_DCHECK(_ptrEnumerator);

  HRESULT hr = S_OK;
  IMMDeviceCollection* pCollection = NULL;
  IMMDevice* pEndpoint = NULL;
  IPropertyStore* pProps = NULL;
  IAudioEndpointVolume* pEndpointVolume = NULL;
  LPWSTR pwszID = NULL;

  // Generate a collection of audio endpoint devices in the system.
  // Get states for *all* endpoint devices.
  // Output: IMMDeviceCollection interface.
  hr = _ptrEnumerator->EnumAudioEndpoints(
      dataFlow,  // data-flow direction (input parameter)
      DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED | DEVICE_STATE_UNPLUGGED,
      &pCollection);  // release interface when done

  EXIT_ON_ERROR(hr);

  // use the IMMDeviceCollection interface...

  UINT count = 0;

  // Retrieve a count of the devices in the device collection.
  hr = pCollection->GetCount(&count);
  EXIT_ON_ERROR(hr);
  if (dataFlow == eRender)
    RTC_LOG(LS_VERBOSE) << "#rendering endpoint devices (counting all): "
                        << count;
  else if (dataFlow == eCapture)
    RTC_LOG(LS_VERBOSE) << "#capturing endpoint devices (counting all): "
                        << count;

  if (count == 0) {
    return 0;
  }

  // Each loop prints the name of an endpoint device.
  for (ULONG i = 0; i < count; i++) {
    RTC_LOG(LS_VERBOSE) << "Endpoint " << i << ":";

    // Get pointer to endpoint number i.
    // Output: IMMDevice interface.
    hr = pCollection->Item(i, &pEndpoint);
    CONTINUE_ON_ERROR(hr);

    // use the IMMDevice interface of the specified endpoint device...

    // Get the endpoint ID string (uniquely identifies the device among all
    // audio endpoint devices)
    hr = pEndpoint->GetId(&pwszID);
    CONTINUE_ON_ERROR(hr);
    RTC_LOG(LS_VERBOSE) << "ID string    : " << pwszID;

    // Retrieve an interface to the device's property store.
    // Output: IPropertyStore interface.
    hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
    CONTINUE_ON_ERROR(hr);

    // use the IPropertyStore interface...

    PROPVARIANT varName;
    // Initialize container for property value.
    PropVariantInit(&varName);

    // Get the endpoint's friendly-name property.
    // Example: "Speakers (Realtek High Definition Audio)"
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    CONTINUE_ON_ERROR(hr);
    RTC_LOG(LS_VERBOSE) << "friendly name: \"" << varName.pwszVal << "\"";

    // Get the endpoint's current device state
    DWORD dwState;
    hr = pEndpoint->GetState(&dwState);
    CONTINUE_ON_ERROR(hr);
    if (dwState & DEVICE_STATE_ACTIVE)
      RTC_LOG(LS_VERBOSE) << "state (0x" << rtc::ToHex(dwState)
                          << ")  : *ACTIVE*";
    if (dwState & DEVICE_STATE_DISABLED)
      RTC_LOG(LS_VERBOSE) << "state (0x" << rtc::ToHex(dwState)
                          << ")  : DISABLED";
    if (dwState & DEVICE_STATE_NOTPRESENT)
      RTC_LOG(LS_VERBOSE) << "state (0x" << rtc::ToHex(dwState)
                          << ")  : NOTPRESENT";
    if (dwState & DEVICE_STATE_UNPLUGGED)
      RTC_LOG(LS_VERBOSE) << "state (0x" << rtc::ToHex(dwState)
                          << ")  : UNPLUGGED";

    // Check the hardware volume capabilities.
    DWORD dwHwSupportMask = 0;
    hr = pEndpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,
                             (void**)&pEndpointVolume);
    CONTINUE_ON_ERROR(hr);
    hr = pEndpointVolume->QueryHardwareSupport(&dwHwSupportMask);
    CONTINUE_ON_ERROR(hr);
    if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_VOLUME)
      // The audio endpoint device supports a hardware volume control
      RTC_LOG(LS_VERBOSE) << "hwmask (0x" << rtc::ToHex(dwHwSupportMask)
                          << ") : HARDWARE_SUPPORT_VOLUME";
    if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_MUTE)
      // The audio endpoint device supports a hardware mute control
      RTC_LOG(LS_VERBOSE) << "hwmask (0x" << rtc::ToHex(dwHwSupportMask)
                          << ") : HARDWARE_SUPPORT_MUTE";
    if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_METER)
      // The audio endpoint device supports a hardware peak meter
      RTC_LOG(LS_VERBOSE) << "hwmask (0x" << rtc::ToHex(dwHwSupportMask)
                          << ") : HARDWARE_SUPPORT_METER";

    // Check the channel count (#channels in the audio stream that enters or
    // leaves the audio endpoint device)
    UINT nChannelCount(0);
    hr = pEndpointVolume->GetChannelCount(&nChannelCount);
    CONTINUE_ON_ERROR(hr);
    RTC_LOG(LS_VERBOSE) << "#channels    : " << nChannelCount;

    if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_VOLUME) {
      // Get the volume range.
      float fLevelMinDB(0.0);
      float fLevelMaxDB(0.0);
      float fVolumeIncrementDB(0.0);
      hr = pEndpointVolume->GetVolumeRange(&fLevelMinDB, &fLevelMaxDB,
                                           &fVolumeIncrementDB);
      CONTINUE_ON_ERROR(hr);
      RTC_LOG(LS_VERBOSE) << "volume range : " << fLevelMinDB << " (min), "
                          << fLevelMaxDB << " (max), " << fVolumeIncrementDB
                          << " (inc) [dB]";

      // The volume range from vmin = fLevelMinDB to vmax = fLevelMaxDB is
      // divided into n uniform intervals of size vinc = fVolumeIncrementDB,
      // where n = (vmax ?vmin) / vinc. The values vmin, vmax, and vinc are
      // measured in decibels. The client can set the volume level to one of n +
      // 1 discrete values in the range from vmin to vmax.
      int n = (int)((fLevelMaxDB - fLevelMinDB) / fVolumeIncrementDB);
      RTC_LOG(LS_VERBOSE) << "#intervals   : " << n;

      // Get information about the current step in the volume range.
      // This method represents the volume level of the audio stream that enters
      // or leaves the audio endpoint device as an index or "step" in a range of
      // discrete volume levels. Output value nStepCount is the number of steps
      // in the range. Output value nStep is the step index of the current
      // volume level. If the number of steps is n = nStepCount, then step index
      // nStep can assume values from 0 (minimum volume) to n ?1 (maximum
      // volume).
      UINT nStep(0);
      UINT nStepCount(0);
      hr = pEndpointVolume->GetVolumeStepInfo(&nStep, &nStepCount);
      CONTINUE_ON_ERROR(hr);
      RTC_LOG(LS_VERBOSE) << "volume steps : " << nStep << " (nStep), "
                          << nStepCount << " (nStepCount)";
    }
  Next:
    if (FAILED(hr)) {
      RTC_LOG(LS_VERBOSE) << "Error when logging device information";
    }
    CoTaskMemFree(pwszID);
    pwszID = NULL;
    PropVariantClear(&varName);
    SAFE_RELEASE(pProps);
    SAFE_RELEASE(pEndpoint);
    SAFE_RELEASE(pEndpointVolume);
  }
  SAFE_RELEASE(pCollection);
  return 0;

Exit:
  _TraceCOMError(hr);
  CoTaskMemFree(pwszID);
  pwszID = NULL;
  SAFE_RELEASE(pCollection);
  SAFE_RELEASE(pEndpoint);
  SAFE_RELEASE(pEndpointVolume);
  SAFE_RELEASE(pProps);
  return -1;
}

// ----------------------------------------------------------------------------
//  Init
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::Init() {
  webrtc::MutexLock lock(&mutex_);

  if (_initialized) {
    return 0;
  }

  // Enumerate all audio rendering and capturing endpoint devices.
  // Note that, some of these will not be able to select by the user.
  // The complete collection is for internal use only.
  _EnumerateEndpointDevicesAll(eCapture);

  _initialized = true;

  return 0;
}

// ----------------------------------------------------------------------------
//  _TraceCOMError
// ----------------------------------------------------------------------------

void MicrophoneModule::_TraceCOMError(HRESULT hr) const {
  wchar_t buf[MAXERRORLENGTH];
  wchar_t errorText[MAXERRORLENGTH];

  const DWORD dwFlags =
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD dwLangID = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

  // Gets the system's human readable message string for this HRESULT.
  // All error message in English by default.
  DWORD messageLength = ::FormatMessageW(dwFlags, 0, hr, dwLangID, errorText,
                                         MAXERRORLENGTH, NULL);

  RTC_DCHECK_LE(messageLength, MAXERRORLENGTH);

  // Trims tailing white space (FormatMessage() leaves a trailing cr-lf.).
  for (; messageLength && ::isspace(errorText[messageLength - 1]);
       --messageLength) {
    errorText[messageLength - 1] = '\0';
  }

  RTC_LOG(LS_ERROR) << "Core Audio method failed (hr=" << hr << ")";
  StringCchPrintfW(buf, MAXERRORLENGTH, L"Error details: ");
  StringCchCatW(buf, MAXERRORLENGTH, errorText);
  RTC_LOG(LS_ERROR) << rtc::ToUtf8(buf);
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::InitMicrophone() {
  webrtc::MutexLock lock(&mutex_);
  return InitMicrophoneLocked();
}

int32_t MicrophoneModule::InitMicrophoneLocked() {
  if (_recording) {
    return -1;
  }

  if (_ptrDeviceIn == NULL) {
    return -1;
  }

  if (_usingInputDeviceIndex) {
    int16_t nDevices = RecordingDevicesLocked();
    if (_inputDeviceIndex > (nDevices - 1)) {
      RTC_LOG(LS_ERROR) << "current device selection is invalid => unable to"
                           " initialize";
      return -1;
    }
  }

  int32_t ret(0);

  SAFE_RELEASE(_ptrDeviceIn);
  if (_usingInputDeviceIndex) {
    // Refresh the selected capture endpoint device using current index
    ret = _GetListDevice(eCapture, _inputDeviceIndex, &_ptrDeviceIn);
  } else {
    ERole role;
    (_inputDevice == webrtc::AudioDeviceModule::kDefaultDevice)
        ? role = eConsole
        : role = eCommunications;
    // Refresh the selected capture endpoint device using role
    ret = _GetDefaultDevice(eCapture, role, &_ptrDeviceIn);
  }

  if (ret != 0 || (_ptrDeviceIn == NULL)) {
    RTC_LOG(LS_ERROR) << "failed to initialize the capturing enpoint device";
    SAFE_RELEASE(_ptrDeviceIn);
    return -1;
  }

  SAFE_RELEASE(_ptrCaptureVolume);
  ret = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,
                               reinterpret_cast<void**>(&_ptrCaptureVolume));
  if (ret != 0 || _ptrCaptureVolume == NULL) {
    RTC_LOG(LS_ERROR) << "failed to initialize the capture volume";
    SAFE_RELEASE(_ptrCaptureVolume);
    return -1;
  }

  _microphoneIsInitialized = true;

  return 0;
}

// ----------------------------------------------------------------------------
//  _GetListDevice
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::_GetListDevice(EDataFlow dir,
                                         int index,
                                         IMMDevice** ppDevice) {
  HRESULT hr(S_OK);

  RTC_DCHECK(_ptrEnumerator);

  IMMDeviceCollection* pCollection = NULL;

  hr = _ptrEnumerator->EnumAudioEndpoints(
      dir,
      DEVICE_STATE_ACTIVE,  // only active endpoints are OK
      &pCollection);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    SAFE_RELEASE(pCollection);
    return -1;
  }

  hr = pCollection->Item(index, ppDevice);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    SAFE_RELEASE(pCollection);
    return -1;
  }

  SAFE_RELEASE(pCollection);

  return 0;
}

// ----------------------------------------------------------------------------
//  _DeviceListCount
//
//  Gets a count of the endpoint rendering or capture devices in the
//  current list of such devices.
// ----------------------------------------------------------------------------

int16_t MicrophoneModule::_DeviceListCount(EDataFlow dir) {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  HRESULT hr = S_OK;
  UINT count = 0;
  RTC_DCHECK(eRender == dir || eCapture == dir);

  if (eRender == dir && NULL != _ptrRenderCollection) {
    hr = _ptrRenderCollection->GetCount(&count);
  } else if (NULL != _ptrCaptureCollection) {
    hr = _ptrCaptureCollection->GetCount(&count);
  }

  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }

  return static_cast<int16_t>(count);
}

// ----------------------------------------------------------------------------
//  _RefreshDeviceList
//
//  Creates a new list of endpoint rendering or capture devices after
//  deleting any previously created (and possibly out-of-date) list of
//  such devices.
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::_RefreshDeviceList(EDataFlow dir) {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  HRESULT hr = S_OK;
  IMMDeviceCollection* pCollection = NULL;

  RTC_DCHECK(dir == eRender || dir == eCapture);
  RTC_DCHECK(_ptrEnumerator);

  // Create a fresh list of devices using the specified direction
  hr = _ptrEnumerator->EnumAudioEndpoints(dir, DEVICE_STATE_ACTIVE,
                                          &pCollection);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    SAFE_RELEASE(pCollection);
    return -1;
  }

  if (dir == eRender) {
    SAFE_RELEASE(_ptrRenderCollection);
    _ptrRenderCollection = pCollection;
  } else {
    SAFE_RELEASE(_ptrCaptureCollection);
    _ptrCaptureCollection = pCollection;
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDevices
// ----------------------------------------------------------------------------

int16_t MicrophoneModule::RecordingDevices() {
  webrtc::MutexLock lock(&mutex_);
  return RecordingDevicesLocked();
}

int16_t MicrophoneModule::RecordingDevicesLocked() {
  if (_RefreshDeviceList(eCapture) != -1) {
    return (_DeviceListCount(eCapture));
  }

  return -1;
}

// ----------------------------------------------------------------------------
//  _GetDeviceName
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::_GetDeviceName(IMMDevice* pDevice,
                                         LPWSTR pszBuffer,
                                         int bufferLen) {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  static const WCHAR szDefault[] = L"<Device not available>";

  HRESULT hr = E_FAIL;
  IPropertyStore* pProps = NULL;
  PROPVARIANT varName;

  RTC_DCHECK(pszBuffer);
  RTC_DCHECK_GT(bufferLen, 0);

  if (pDevice != NULL) {
    hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "IMMDevice::OpenPropertyStore failed, hr = 0x"
                        << rtc::ToHex(hr);
    }
  }

  // Initialize container for property value.
  PropVariantInit(&varName);

  if (SUCCEEDED(hr)) {
    // Get the endpoint device's friendly-name property.
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "IPropertyStore::GetValue failed, hr = 0x"
                        << rtc::ToHex(hr);
    }
  }

  if ((SUCCEEDED(hr)) && (VT_EMPTY == varName.vt)) {
    hr = E_FAIL;
    RTC_LOG(LS_ERROR) << "IPropertyStore::GetValue returned no value,"
                         " hr = 0x"
                      << rtc::ToHex(hr);
  }

  if ((SUCCEEDED(hr)) && (VT_LPWSTR != varName.vt)) {
    // The returned value is not a wide null terminated string.
    hr = E_UNEXPECTED;
    RTC_LOG(LS_ERROR) << "IPropertyStore::GetValue returned unexpected"
                         " type, hr = 0x"
                      << rtc::ToHex(hr);
  }

  if (SUCCEEDED(hr) && (varName.pwszVal != NULL)) {
    // Copy the valid device name to the provided ouput buffer.
    wcsncpy_s(pszBuffer, bufferLen, varName.pwszVal, _TRUNCATE);
  } else {
    // Failed to find the device name.
    wcsncpy_s(pszBuffer, bufferLen, szDefault, _TRUNCATE);
  }

  PropVariantClear(&varName);
  SAFE_RELEASE(pProps);

  return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool MicrophoneModule::MicrophoneIsInitialized() {
  return (_microphoneIsInitialized);
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::MicrophoneVolumeIsAvailable(bool* available) {
  webrtc::MutexLock lock(&mutex_);

  if (_ptrDeviceIn == NULL) {
    return -1;
  }

  HRESULT hr = S_OK;
  IAudioEndpointVolume* pVolume = NULL;

  hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,
                              reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  float volume(0.0f);
  hr = pVolume->GetMasterVolumeLevelScalar(&volume);
  if (FAILED(hr)) {
    *available = false;
  }
  *available = true;

  SAFE_RELEASE(pVolume);
  return 0;

Exit:
  _TraceCOMError(hr);
  SAFE_RELEASE(pVolume);
  return -1;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::SetMicrophoneVolume(uint32_t volume) {
  RTC_LOG(LS_VERBOSE) << "AudioDeviceWindowsCore::SetMicrophoneVolume(volume="
                      << volume << ")";

  {
    webrtc::MutexLock lock(&mutex_);

    if (!_microphoneIsInitialized) {
      return -1;
    }

    if (_ptrDeviceIn == NULL) {
      return -1;
    }
  }

  if (volume < static_cast<uint32_t>(webrtc::MIN_CORE_MICROPHONE_VOLUME) ||
      volume > static_cast<uint32_t>(webrtc::MAX_CORE_MICROPHONE_VOLUME)) {
    return -1;
  }

  HRESULT hr = S_OK;
  // scale input volume to valid range (0.0 to 1.0)
  const float fLevel =
      static_cast<float>(volume) / webrtc::MAX_CORE_MICROPHONE_VOLUME;
  volume_mutex_.Lock();
  _ptrCaptureVolume->SetMasterVolumeLevelScalar(fLevel, NULL);
  volume_mutex_.Unlock();
  EXIT_ON_ERROR(hr);

  return 0;

Exit:
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::MicrophoneVolume(uint32_t* volume) const {
  {
    webrtc::MutexLock lock(&mutex_);

    if (!_microphoneIsInitialized) {
      return -1;
    }

    if (_ptrDeviceIn == NULL) {
      return -1;
    }
  }

  HRESULT hr = S_OK;
  float fLevel(0.0f);
  *volume = 0;
  volume_mutex_.Lock();
  hr = _ptrCaptureVolume->GetMasterVolumeLevelScalar(&fLevel);
  volume_mutex_.Unlock();
  EXIT_ON_ERROR(hr);

  // scale input volume range [0.0,1.0] to valid output range
  *volume = static_cast<uint32_t>(fLevel * webrtc::MAX_CORE_MICROPHONE_VOLUME);

  return 0;

Exit:
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
//
//  The internal range for Core Audio is 0.0 to 1.0, where 0.0 indicates
//  silence and 1.0 indicates full volume (no attenuation).
//  We add our (webrtc-internal) own max level to match the Wave API and
//  how it is used today in VoE.
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::MaxMicrophoneVolume(uint32_t* maxVolume) const {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  if (!_microphoneIsInitialized) {
    return -1;
  }

  *maxVolume = static_cast<uint32_t>(webrtc::MAX_CORE_MICROPHONE_VOLUME);

  return 0;
}

// ----------------------------------------------------------------------------
//  MinMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::MinMicrophoneVolume(uint32_t* minVolume) const {
  if (!_microphoneIsInitialized) {
    return -1;
  }

  *minVolume = static_cast<uint32_t>(webrtc::MIN_CORE_MICROPHONE_VOLUME);

  return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::MicrophoneMuteIsAvailable(bool* available) {
  webrtc::MutexLock lock(&mutex_);

  if (_ptrDeviceIn == NULL) {
    return -1;
  }

  HRESULT hr = S_OK;
  IAudioEndpointVolume* pVolume = NULL;

  // Query the microphone system mute state.
  hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,
                              reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  BOOL mute;
  hr = pVolume->GetMute(&mute);
  if (FAILED(hr))
    *available = false;
  else
    *available = true;

  SAFE_RELEASE(pVolume);
  return 0;

Exit:
  _TraceCOMError(hr);
  SAFE_RELEASE(pVolume);
  return -1;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneMute
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::SetMicrophoneMute(bool enable) {
  if (!_microphoneIsInitialized) {
    return -1;
  }

  if (_ptrDeviceIn == NULL) {
    return -1;
  }

  HRESULT hr = S_OK;
  IAudioEndpointVolume* pVolume = NULL;

  // Set the microphone system mute state.
  hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,
                              reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  const BOOL mute(enable);
  hr = pVolume->SetMute(mute, NULL);
  EXIT_ON_ERROR(hr);

  SAFE_RELEASE(pVolume);
  return 0;

Exit:
  _TraceCOMError(hr);
  SAFE_RELEASE(pVolume);
  return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::MicrophoneMute(bool* enabled) const {
  if (!_microphoneIsInitialized) {
    return -1;
  }

  HRESULT hr = S_OK;
  IAudioEndpointVolume* pVolume = NULL;

  // Query the microphone system mute state.
  hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,
                              reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  BOOL mute;
  hr = pVolume->GetMute(&mute);
  EXIT_ON_ERROR(hr);

  *enabled = (mute == TRUE) ? true : false;

  SAFE_RELEASE(pVolume);
  return 0;

Exit:
  _TraceCOMError(hr);
  SAFE_RELEASE(pVolume);
  return -1;
}

// ----------------------------------------------------------------------------
//  InitCaptureThreadPriority
// ----------------------------------------------------------------------------

DWORD MicrophoneModule::InitCaptureThreadPriority() {
  _hMmTask = NULL;

  rtc::SetCurrentThreadName("webrtc_core_audio_capture_thread");

  // Use Multimedia Class Scheduler Service (MMCSS) to boost the thread
  // priority.
  if (_winSupportAvrt) {
    DWORD taskIndex(0);
    _hMmTask = _PAvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
    if (_hMmTask) {
      if (!_PAvSetMmThreadPriority(_hMmTask, AVRT_PRIORITY_CRITICAL)) {
        RTC_LOG(LS_WARNING) << "failed to boost rec-thread using MMCSS";
      }
      RTC_LOG(LS_VERBOSE)
          << "capture thread is now registered with MMCSS (taskIndex="
          << taskIndex << ")";
    } else {
      RTC_LOG(LS_WARNING) << "failed to enable MMCSS on capture thread (err="
                          << GetLastError() << ")";
      _TraceCOMError(GetLastError());
    }
  }

  return S_OK;
}

// ----------------------------------------------------------------------------
//  RevertCaptureThreadPriority
// ----------------------------------------------------------------------------

void MicrophoneModule::RevertCaptureThreadPriority() {
  if (_winSupportAvrt) {
    if (NULL != _hMmTask) {
      _PAvRevertMmThreadCharacteristics(_hMmTask);
    }
  }

  _hMmTask = NULL;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::InitRecording() {
  webrtc::MutexLock lock(&mutex_);

  if (_recording) {
    return -1;
  }

  if (_recIsInitialized) {
    return 0;
  }

  if (QueryPerformanceFrequency(&_perfCounterFreq) == 0) {
    return -1;
  }
  _perfCounterFactor = 10000000.0 / (double)_perfCounterFreq.QuadPart;

  if (_ptrDeviceIn == NULL) {
    return -1;
  }

  // Initialize the microphone (devices might have been added or removed)
  if (InitMicrophoneLocked() == -1) {
    RTC_LOG(LS_WARNING) << "InitMicrophone() failed";
  }

  // Ensure that the updated capturing endpoint device is valid
  if (_ptrDeviceIn == NULL) {
    return -1;
  }

  if (_builtInAecEnabled) {
    // The DMO will configure the capture device.
    return InitRecordingDMO();
  }

  HRESULT hr = S_OK;
  WAVEFORMATEX* pWfxIn = NULL;
  WAVEFORMATEXTENSIBLE Wfx = WAVEFORMATEXTENSIBLE();
  WAVEFORMATEX* pWfxClosestMatch = NULL;

  // Create COM object with IAudioClient interface.
  SAFE_RELEASE(_ptrClientIn);
  hr = _ptrDeviceIn->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
                              (void**)&_ptrClientIn);
  EXIT_ON_ERROR(hr);

  // Retrieve the stream format that the audio engine uses for its internal
  // processing (mixing) of shared-mode streams.
  hr = _ptrClientIn->GetMixFormat(&pWfxIn);
  if (SUCCEEDED(hr)) {
    RTC_LOG(LS_VERBOSE) << "Audio Engine's current capturing mix format:";
    // format type
    RTC_LOG(LS_VERBOSE) << "wFormatTag     : 0x"
                        << rtc::ToHex(pWfxIn->wFormatTag) << " ("
                        << pWfxIn->wFormatTag << ")";
    // number of channels (i.e. mono, stereo...)
    RTC_LOG(LS_VERBOSE) << "nChannels      : " << pWfxIn->nChannels;
    // sample rate
    RTC_LOG(LS_VERBOSE) << "nSamplesPerSec : " << pWfxIn->nSamplesPerSec;
    // for buffer estimation
    RTC_LOG(LS_VERBOSE) << "nAvgBytesPerSec: " << pWfxIn->nAvgBytesPerSec;
    // block size of data
    RTC_LOG(LS_VERBOSE) << "nBlockAlign    : " << pWfxIn->nBlockAlign;
    // number of bits per sample of mono data
    RTC_LOG(LS_VERBOSE) << "wBitsPerSample : " << pWfxIn->wBitsPerSample;
    RTC_LOG(LS_VERBOSE) << "cbSize         : " << pWfxIn->cbSize;
  }

  // Set wave format
  Wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  Wfx.Format.wBitsPerSample = 16;
  Wfx.Format.cbSize = 22;
  Wfx.dwChannelMask = 0;
  Wfx.Samples.wValidBitsPerSample = Wfx.Format.wBitsPerSample;
  Wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  const int freqs[6] = {48000, 44100, 16000, 96000, 32000, 8000};
  hr = S_FALSE;

  // Iterate over frequencies and channels, in order of priority
  for (unsigned int freq = 0; freq < sizeof(freqs) / sizeof(freqs[0]); freq++) {
    for (unsigned int chan = 0;
         chan < sizeof(_recChannelsPrioList) / sizeof(_recChannelsPrioList[0]);
         chan++) {
      Wfx.Format.nChannels = _recChannelsPrioList[chan];
      Wfx.Format.nSamplesPerSec = freqs[freq];
      Wfx.Format.nBlockAlign =
          Wfx.Format.nChannels * Wfx.Format.wBitsPerSample / 8;
      Wfx.Format.nAvgBytesPerSec =
          Wfx.Format.nSamplesPerSec * Wfx.Format.nBlockAlign;
      // If the method succeeds and the audio endpoint device supports the
      // specified stream format, it returns S_OK. If the method succeeds and
      // provides a closest match to the specified format, it returns S_FALSE.
      hr = _ptrClientIn->IsFormatSupported(
          AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&Wfx, &pWfxClosestMatch);
      if (hr == S_OK) {
        break;
      } else {
        if (pWfxClosestMatch) {
          RTC_LOG(LS_INFO) << "nChannels=" << Wfx.Format.nChannels
                           << ", nSamplesPerSec=" << Wfx.Format.nSamplesPerSec
                           << " is not supported. Closest match: "
                              "nChannels="
                           << pWfxClosestMatch->nChannels << ", nSamplesPerSec="
                           << pWfxClosestMatch->nSamplesPerSec;
          CoTaskMemFree(pWfxClosestMatch);
          pWfxClosestMatch = NULL;
        } else {
          RTC_LOG(LS_INFO) << "nChannels=" << Wfx.Format.nChannels
                           << ", nSamplesPerSec=" << Wfx.Format.nSamplesPerSec
                           << " is not supported. No closest match.";
        }
      }
    }
    if (hr == S_OK)
      break;
  }

  if (hr == S_OK) {
    _recAudioFrameSize = Wfx.Format.nBlockAlign;
    _recSampleRate = Wfx.Format.nSamplesPerSec;
    _recBlockSize = Wfx.Format.nSamplesPerSec / 100;
    _recChannels = Wfx.Format.nChannels;

    RTC_LOG(LS_VERBOSE) << "VoE selected this capturing format:";
    RTC_LOG(LS_VERBOSE) << "wFormatTag        : 0x"
                        << rtc::ToHex(Wfx.Format.wFormatTag) << " ("
                        << Wfx.Format.wFormatTag << ")";
    RTC_LOG(LS_VERBOSE) << "nChannels         : " << Wfx.Format.nChannels;
    RTC_LOG(LS_VERBOSE) << "nSamplesPerSec    : " << Wfx.Format.nSamplesPerSec;
    RTC_LOG(LS_VERBOSE) << "nAvgBytesPerSec   : " << Wfx.Format.nAvgBytesPerSec;
    RTC_LOG(LS_VERBOSE) << "nBlockAlign       : " << Wfx.Format.nBlockAlign;
    RTC_LOG(LS_VERBOSE) << "wBitsPerSample    : " << Wfx.Format.wBitsPerSample;
    RTC_LOG(LS_VERBOSE) << "cbSize            : " << Wfx.Format.cbSize;
    RTC_LOG(LS_VERBOSE) << "Additional settings:";
    RTC_LOG(LS_VERBOSE) << "_recAudioFrameSize: " << _recAudioFrameSize;
    RTC_LOG(LS_VERBOSE) << "_recBlockSize     : " << _recBlockSize;
    RTC_LOG(LS_VERBOSE) << "_recChannels      : " << _recChannels;
  }

  // Create a capturing stream.
  hr = _ptrClientIn->Initialize(
      AUDCLNT_SHAREMODE_SHARED,  // share Audio Engine with other applications
      AUDCLNT_STREAMFLAGS_EVENTCALLBACK |  // processing of the audio buffer by
                                           // the client will be event driven
          AUDCLNT_STREAMFLAGS_NOPERSIST,   // volume and mute settings for an
                                           // audio session will not persist
                                           // across system restarts
      0,                    // required for event-driven shared mode
      0,                    // periodicity
      (WAVEFORMATEX*)&Wfx,  // selected wave format
      NULL);                // session GUID

  if (hr != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::Initialize() failed:";
  }
  EXIT_ON_ERROR(hr);

  // Get the actual size of the shared (endpoint buffer).
  // Typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
  UINT bufferFrameCount(0);
  hr = _ptrClientIn->GetBufferSize(&bufferFrameCount);
  if (SUCCEEDED(hr)) {
    RTC_LOG(LS_VERBOSE) << "IAudioClient::GetBufferSize() => "
                        << bufferFrameCount << " (<=> "
                        << bufferFrameCount * _recAudioFrameSize << " bytes)";
  }

  // Set the event handle that the system signals when an audio buffer is ready
  // to be processed by the client.
  hr = _ptrClientIn->SetEventHandle(_hCaptureSamplesReadyEvent);
  EXIT_ON_ERROR(hr);

  // Get an IAudioCaptureClient interface.
  SAFE_RELEASE(_ptrCaptureClient);
  hr = _ptrClientIn->GetService(__uuidof(IAudioCaptureClient),
                                (void**)&_ptrCaptureClient);
  EXIT_ON_ERROR(hr);

  // Mark capture side as initialized
  _recIsInitialized = true;

  CoTaskMemFree(pWfxIn);
  CoTaskMemFree(pWfxClosestMatch);

  RTC_LOG(LS_VERBOSE) << "capture side is now initialized";
  return 0;

Exit:
  _TraceCOMError(hr);
  CoTaskMemFree(pWfxIn);
  CoTaskMemFree(pWfxClosestMatch);
  SAFE_RELEASE(_ptrClientIn);
  SAFE_RELEASE(_ptrCaptureClient);
  return -1;
}

int MicrophoneModule::SetVtI4Property(IPropertyStore* ptrPS,
                                      REFPROPERTYKEY key,
                                      LONG value) {
  PROPVARIANT pv;
  PropVariantInit(&pv);
  pv.vt = VT_I4;
  pv.lVal = value;
  HRESULT hr = ptrPS->SetValue(key, pv);
  PropVariantClear(&pv);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }
  return 0;
}

int MicrophoneModule::SetBoolProperty(IPropertyStore* ptrPS,
                                      REFPROPERTYKEY key,
                                      VARIANT_BOOL value) {
  PROPVARIANT pv;
  PropVariantInit(&pv);
  pv.vt = VT_BOOL;
  pv.boolVal = value;
  HRESULT hr = ptrPS->SetValue(key, pv);
  PropVariantClear(&pv);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }
  return 0;
}

int MicrophoneModule::SetDMOProperties() {
  HRESULT hr = S_OK;
  RTC_DCHECK(_dmo);

  rtc::scoped_refptr<IPropertyStore> ps;
  {
    IPropertyStore* ptrPS = NULL;
    hr = _dmo->QueryInterface(IID_IPropertyStore,
                              reinterpret_cast<void**>(&ptrPS));
    if (FAILED(hr) || ptrPS == NULL) {
      _TraceCOMError(hr);
      return -1;
    }
    ps = ptrPS;
    SAFE_RELEASE(ptrPS);
  }

  // Set the AEC system mode.
  // SINGLE_CHANNEL_AEC - AEC processing only.
  if (SetVtI4Property(ps.get(), MFPKEY_WMAAECMA_SYSTEM_MODE,
                      SINGLE_CHANNEL_AEC)) {
    return -1;
  }

  // Set the AEC source mode.
  // VARIANT_TRUE - Source mode (we poll the AEC for captured data).
  if (SetBoolProperty(ps.get(), MFPKEY_WMAAECMA_DMO_SOURCE_MODE,
                      VARIANT_TRUE) == -1) {
    return -1;
  }

  // Enable the feature mode.
  // This lets us override all the default processing settings below.
  if (SetBoolProperty(ps.get(), MFPKEY_WMAAECMA_FEATURE_MODE, VARIANT_TRUE) ==
      -1) {
    return -1;
  }

  // Disable analog AGC (default enabled).
  if (SetBoolProperty(ps.get(), MFPKEY_WMAAECMA_MIC_GAIN_BOUNDER,
                      VARIANT_FALSE) == -1) {
    return -1;
  }

  // Disable noise suppression (default enabled).
  // 0 - Disabled, 1 - Enabled
  if (SetVtI4Property(ps.get(), MFPKEY_WMAAECMA_FEATR_NS, 0) == -1) {
    return -1;
  }

  // Relevant parameters to leave at default settings:
  // MFPKEY_WMAAECMA_FEATR_AGC - Digital AGC (disabled).
  // MFPKEY_WMAAECMA_FEATR_CENTER_CLIP - AEC center clipping (enabled).
  // MFPKEY_WMAAECMA_FEATR_ECHO_LENGTH - Filter length (256 ms).
  // TODO(andrew): investigate decresing the length to 128 ms.
  // MFPKEY_WMAAECMA_FEATR_FRAME_SIZE - Frame size (0).
  //   0 is automatic; defaults to 160 samples (or 10 ms frames at the
  //   selected 16 kHz) as long as mic array processing is disabled.
  // MFPKEY_WMAAECMA_FEATR_NOISE_FILL - Comfort noise (enabled).
  // MFPKEY_WMAAECMA_FEATR_VAD - VAD (disabled).

  // Set the devices selected by VoE. If using a default device, we need to
  // search for the device index.
  int inDevIndex = _inputDeviceIndex;
  int outDevIndex = _outputDeviceIndex;
  if (!_usingInputDeviceIndex) {
    ERole role = eCommunications;
    if (_inputDevice == webrtc::AudioDeviceModule::kDefaultDevice) {
      role = eConsole;
    }

    if (_GetDefaultDeviceIndex(eCapture, role, &inDevIndex) == -1) {
      return -1;
    }
  }

  if (!_usingOutputDeviceIndex) {
    ERole role = eCommunications;
    if (_outputDevice == webrtc::AudioDeviceModule::kDefaultDevice) {
      role = eConsole;
    }

    if (_GetDefaultDeviceIndex(eRender, role, &outDevIndex) == -1) {
      return -1;
    }
  }

  DWORD devIndex = static_cast<uint32_t>(outDevIndex << 16) +
                   static_cast<uint32_t>(0x0000ffff & inDevIndex);
  RTC_LOG(LS_VERBOSE) << "Capture device index: " << inDevIndex
                      << ", render device index: " << outDevIndex;
  if (SetVtI4Property(ps.get(), MFPKEY_WMAAECMA_DEVICE_INDEXES, devIndex) ==
      -1) {
    return -1;
  }

  return 0;
}

int32_t MicrophoneModule::_GetDefaultDeviceIndex(EDataFlow dir,
                                                 ERole role,
                                                 int* index) {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  HRESULT hr = S_OK;
  WCHAR szDefaultDeviceID[MAX_PATH] = {0};
  WCHAR szDeviceID[MAX_PATH] = {0};

  const size_t kDeviceIDLength = sizeof(szDeviceID) / sizeof(szDeviceID[0]);
  RTC_DCHECK_EQ(kDeviceIDLength,
                sizeof(szDefaultDeviceID) / sizeof(szDefaultDeviceID[0]));

  if (_GetDefaultDeviceID(dir, role, szDefaultDeviceID, kDeviceIDLength) ==
      -1) {
    return -1;
  }

  IMMDeviceCollection* collection = _ptrCaptureCollection;
  if (dir == eRender) {
    collection = _ptrRenderCollection;
  }

  if (!collection) {
    RTC_LOG(LS_ERROR) << "Device collection not valid";
    return -1;
  }

  UINT count = 0;
  hr = collection->GetCount(&count);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }

  *index = -1;
  for (UINT i = 0; i < count; i++) {
    memset(szDeviceID, 0, sizeof(szDeviceID));
    rtc::scoped_refptr<IMMDevice> device;
    {
      IMMDevice* ptrDevice = NULL;
      hr = collection->Item(i, &ptrDevice);
      if (FAILED(hr) || ptrDevice == NULL) {
        _TraceCOMError(hr);
        return -1;
      }
      device = ptrDevice;
      SAFE_RELEASE(ptrDevice);
    }

    if (_GetDeviceID(device.get(), szDeviceID, kDeviceIDLength) == -1) {
      return -1;
    }

    if (wcsncmp(szDefaultDeviceID, szDeviceID, kDeviceIDLength) == 0) {
      // Found a match.
      *index = i;
      break;
    }
  }

  if (*index == -1) {
    RTC_LOG(LS_ERROR) << "Unable to find collection index for default device";
    return -1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDeviceID
//
//  Gets the uniqe device ID of an endpoint rendering or capture device
//  given a specified device role.
//
//  Uses: _ptrEnumerator
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::_GetDefaultDeviceID(EDataFlow dir,
                                              ERole role,
                                              LPWSTR szBuffer,
                                              int bufferLen) {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  HRESULT hr = S_OK;
  IMMDevice* pDevice = NULL;

  RTC_DCHECK(dir == eRender || dir == eCapture);
  RTC_DCHECK(role == eConsole || role == eCommunications);
  RTC_DCHECK(_ptrEnumerator);

  hr = _ptrEnumerator->GetDefaultAudioEndpoint(dir, role, &pDevice);

  if (FAILED(hr)) {
    _TraceCOMError(hr);
    SAFE_RELEASE(pDevice);
    return -1;
  }

  int32_t res = _GetDeviceID(pDevice, szBuffer, bufferLen);
  SAFE_RELEASE(pDevice);
  return res;
}

// ----------------------------------------------------------------------------
//  _GetDeviceID
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::_GetDeviceID(IMMDevice* pDevice,
                                       LPWSTR pszBuffer,
                                       int bufferLen) {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  static const WCHAR szDefault[] = L"<Device not available>";

  HRESULT hr = E_FAIL;
  LPWSTR pwszID = NULL;

  RTC_DCHECK(pszBuffer);
  RTC_DCHECK_GT(bufferLen, 0);

  if (pDevice != NULL) {
    hr = pDevice->GetId(&pwszID);
  }

  if (hr == S_OK) {
    // Found the device ID.
    wcsncpy_s(pszBuffer, bufferLen, pwszID, _TRUNCATE);
  } else {
    // Failed to find the device ID.
    wcsncpy_s(pszBuffer, bufferLen, szDefault, _TRUNCATE);
  }

  CoTaskMemFree(pwszID);
  return 0;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDevice
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::_GetDefaultDevice(EDataFlow dir,
                                            ERole role,
                                            IMMDevice** ppDevice) {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  HRESULT hr(S_OK);

  RTC_DCHECK(_ptrEnumerator);

  hr = _ptrEnumerator->GetDefaultAudioEndpoint(dir, role, ppDevice);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }

  return 0;
}

// Capture initialization when the built-in AEC DirectX Media Object (DMO) is
// used. Called from InitRecording(), most of which is skipped over. The DMO
// handles device initialization itself.
// Reference: http://msdn.microsoft.com/en-us/library/ff819492(v=vs.85).aspx
int32_t MicrophoneModule::InitRecordingDMO() {
  RTC_DCHECK(_builtInAecEnabled);
  RTC_DCHECK(_dmo);

  if (SetDMOProperties() == -1) {
    return -1;
  }

  DMO_MEDIA_TYPE mt = {};
  HRESULT hr = MoInitMediaType(&mt, sizeof(WAVEFORMATEX));
  if (FAILED(hr)) {
    MoFreeMediaType(&mt);
    _TraceCOMError(hr);
    return -1;
  }
  mt.majortype = MEDIATYPE_Audio;
  mt.subtype = MEDIASUBTYPE_PCM;
  mt.formattype = FORMAT_WaveFormatEx;

  // Supported formats
  // nChannels: 1 (in AEC-only mode)
  // nSamplesPerSec: 8000, 11025, 16000, 22050
  // wBitsPerSample: 16
  WAVEFORMATEX* ptrWav = reinterpret_cast<WAVEFORMATEX*>(mt.pbFormat);
  ptrWav->wFormatTag = WAVE_FORMAT_PCM;
  ptrWav->nChannels = 1;
  // 16000 is the highest we can support with our resampler.
  ptrWav->nSamplesPerSec = 16000;
  ptrWav->nAvgBytesPerSec = 32000;
  ptrWav->nBlockAlign = 2;
  ptrWav->wBitsPerSample = 16;
  ptrWav->cbSize = 0;

  // Set the VoE format equal to the AEC output format.
  _recAudioFrameSize = ptrWav->nBlockAlign;
  _recSampleRate = ptrWav->nSamplesPerSec;
  _recBlockSize = ptrWav->nSamplesPerSec / 100;
  _recChannels = ptrWav->nChannels;

  // Set the DMO output format parameters.
  hr = _dmo->SetOutputType(webrtc::kAecCaptureStreamIndex, &mt, 0);
  MoFreeMediaType(&mt);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }

  _mediaBuffer = rtc::make_ref_counted<webrtc::MediaBufferImpl>(
      _recBlockSize * _recAudioFrameSize);

  // Optional, but if called, must be after media types are set.
  hr = _dmo->AllocateStreamingResources();
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }

  _recIsInitialized = true;
  RTC_LOG(LS_VERBOSE) << "Capture side is now initialized";

  return 0;
}

// ----------------------------------------------------------------------------
//  [static] WSAPICaptureThread
// ----------------------------------------------------------------------------

DWORD WINAPI MicrophoneModule::WSAPICaptureThread(LPVOID context) {
  return reinterpret_cast<MicrophoneModule*>(context)->DoCaptureThread();
}

// ----------------------------------------------------------------------------
//  StartRecording
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::StartRecording() {
  if (!_recIsInitialized) {
    return -1;
  }

  if (_hRecThread != NULL) {
    return 0;
  }

  if (_recording) {
    return 0;
  }

  {
    webrtc::MutexLock lockScoped(&mutex_);

    // Create thread which will drive the capturing
    LPTHREAD_START_ROUTINE lpStartAddress = WSAPICaptureThread;
    if (_builtInAecEnabled) {
      // Redirect to the DMO polling method.
      lpStartAddress = WSAPICaptureThreadPollDMO;

      if (!_playing) {
        // The DMO won't provide us captured output data unless we
        // give it render data to process.
        RTC_LOG(LS_ERROR)
            << "Playout must be started before recording when using"
               " the built-in AEC";
        return -1;
      }
    }

    RTC_DCHECK(_hRecThread == NULL);
    _hRecThread = CreateThread(NULL, 0, lpStartAddress, this, 0, NULL);
    if (_hRecThread == NULL) {
      RTC_LOG(LS_ERROR) << "failed to create the recording thread";
      return -1;
    }

    // Set thread priority to highest possible
    SetThreadPriority(_hRecThread, THREAD_PRIORITY_TIME_CRITICAL);
  }  // critScoped

  DWORD ret = WaitForSingleObject(_hCaptureStartedEvent, 1000);
  if (ret != WAIT_OBJECT_0) {
    RTC_LOG(LS_VERBOSE) << "capturing did not start up properly";
    return -1;
  }
  RTC_LOG(LS_VERBOSE) << "capture audio stream has now started...";

  _recording = true;

  return 0;
}

DWORD WINAPI MicrophoneModule::WSAPICaptureThreadPollDMO(LPVOID context) {
  return reinterpret_cast<MicrophoneModule*>(context)->DoCaptureThreadPollDMO();
}

// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

int32_t MicrophoneModule::StopRecording() {
  int32_t err = 0;

  if (!_recIsInitialized) {
    return 0;
  }

  mutex_.Lock();

  if (_hRecThread == NULL) {
    RTC_LOG(LS_VERBOSE)
        << "no capturing stream is active => close down WASAPI only";
    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrCaptureClient);
    _recIsInitialized = false;
    _recording = false;
    mutex_.Unlock();
    return 0;
  }

  // Stop the driving thread...
  RTC_LOG(LS_VERBOSE) << "closing down the webrtc_core_audio_capture_thread...";
  // Manual-reset event; it will remain signalled to stop all capture threads.
  SetEvent(_hShutdownCaptureEvent);

  mutex_.Unlock();
  DWORD ret = WaitForSingleObject(_hRecThread, 2000);
  if (ret != WAIT_OBJECT_0) {
    RTC_LOG(LS_ERROR)
        << "failed to close down webrtc_core_audio_capture_thread";
    err = -1;
  } else {
    RTC_LOG(LS_VERBOSE) << "webrtc_core_audio_capture_thread is now closed";
  }
  mutex_.Lock();

  ResetEvent(_hShutdownCaptureEvent);  // Must be manually reset.
  // Ensure that the thread has released these interfaces properly.
  RTC_DCHECK(err == -1 || _ptrClientIn == NULL);
  RTC_DCHECK(err == -1 || _ptrCaptureClient == NULL);

  _recIsInitialized = false;
  _recording = false;

  // These will create thread leaks in the result of an error,
  // but we can at least resume the call.
  CloseHandle(_hRecThread);
  _hRecThread = NULL;

  if (_builtInAecEnabled) {
    RTC_DCHECK(_dmo);
    // This is necessary. Otherwise the DMO can generate garbage render
    // audio even after rendering has stopped.
    HRESULT hr = _dmo->FreeStreamingResources();
    if (FAILED(hr)) {
      _TraceCOMError(hr);
      err = -1;
    }
  }

  mutex_.Unlock();

  return err;
}

#endif WEBRTC_WINDOWS_CORE_AUDIO_BUILD
#endif