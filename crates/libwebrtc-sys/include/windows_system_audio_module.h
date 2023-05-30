/*
 * Copyright (c) 2014 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once
#include <windows.h>
#include <iostream>
#include <minwindef.h>
#include <handleapi.h>
#include <avrt.h>
#include <combaseapi.h>
#include <assert.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <RTWorkQ.h>
#include <functional>
#include <audioclientactivationparams.h>
#include <functiondiscoverykeys.h>
#include "system_audio_module.h"
#include <setupapi.h>  
#include <initguid.h>  // Put this in to get rid of linker errors.  
#include <devpkey.h>  // Property keys defined here are now defined inline.

#include <wrl/implements.h>
#include "win-help.h"

#ifdef _WIN32
#include <Unknwn.h>
#endif

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

#define OBS_KSAUDIO_SPEAKER_4POINT1 \
  (KSAUDIO_SPEAKER_SURROUND | SPEAKER_LOW_FREQUENCY)
#define RECONNECT_INTERVAL 3000
#define BUFFER_TIME_100NS (5 * 10000000)
typedef HRESULT(STDAPICALLTYPE* PFN_ActivateAudioInterfaceAsync)(
    LPCWSTR,
    REFIID,
    PROPVARIANT*,
    IActivateAudioInterfaceCompletionHandler*,
    IActivateAudioInterfaceAsyncOperation**);

typedef HRESULT(STDAPICALLTYPE* PFN_RtwqUnlockWorkQueue)(DWORD);
typedef HRESULT(STDAPICALLTYPE* PFN_RtwqLockSharedWorkQueue)(PCWSTR usageClass,
                                                             LONG basePriority,
                                                             DWORD* taskId,
                                                             DWORD* id);
typedef HRESULT(STDAPICALLTYPE* PFN_RtwqCreateAsyncResult)(IUnknown*,
                                                           IRtwqAsyncCallback*,
                                                           IUnknown*,
                                                           IRtwqAsyncResult**);
typedef HRESULT(STDAPICALLTYPE* PFN_RtwqPutWorkItem)(DWORD,
                                                     LONG,
                                                     IRtwqAsyncResult*);
typedef HRESULT(STDAPICALLTYPE* PFN_RtwqPutWaitingWorkItem)(HANDLE,
                                                            LONG,
                                                            IRtwqAsyncResult*,
                                                            RTWQWORKITEM_KEY*);

/* Oh no I have my own com pointer class, the world is ending, how dare you
 * write your own! */

template <class T>
class ComPtr {
 protected:
  T* ptr;

  inline void Kill() {
    if (ptr)
      ptr->Release();
  }

  inline void Replace(T* p) {
    if (ptr != p) {
      if (p)
        p->AddRef();
      if (ptr)
        ptr->Release();
      ptr = p;
    }
  }

 public:
  inline ComPtr() : ptr(nullptr) {}
  inline ComPtr(T* p) : ptr(p) {
    if (ptr)
      ptr->AddRef();
  }
  inline ComPtr(const ComPtr<T>& c) : ptr(c.ptr) {
    if (ptr)
      ptr->AddRef();
  }
  inline ComPtr(ComPtr<T>&& c) noexcept : ptr(c.ptr) { c.ptr = nullptr; }
  template <class U>
  inline ComPtr(ComPtr<U>&& c) noexcept : ptr(c.Detach()) {}
  inline ~ComPtr() { Kill(); }

  inline void Clear() {
    if (ptr) {
      ptr->Release();
      ptr = nullptr;
    }
  }

  inline ComPtr<T>& operator=(T* p) {
    Replace(p);
    return *this;
  }

  inline ComPtr<T>& operator=(const ComPtr<T>& c) {
    Replace(c.ptr);
    return *this;
  }

  inline ComPtr<T>& operator=(ComPtr<T>&& c) noexcept {
    if (&ptr != &c.ptr) {
      Kill();
      ptr = c.ptr;
      c.ptr = nullptr;
    }

    return *this;
  }

  template <class U>
  inline ComPtr<T>& operator=(ComPtr<U>&& c) noexcept {
    Kill();
    ptr = c.Detach();

    return *this;
  }

  inline T* Detach() {
    T* out = ptr;
    ptr = nullptr;
    return out;
  }

  inline void CopyTo(T** out) {
    if (out) {
      if (ptr)
        ptr->AddRef();
      *out = ptr;
    }
  }

  inline ULONG Release() {
    ULONG ref;

    if (!ptr)
      return 0;
    ref = ptr->Release();
    ptr = nullptr;
    return ref;
  }

  inline T** Assign() {
    Clear();
    return &ptr;
  }
  inline void Set(T* p) {
    Kill();
    ptr = p;
  }

  inline T* Get() const { return ptr; }

  inline T** operator&() { return Assign(); }

  inline operator T*() const { return ptr; }
  inline T* operator->() const { return ptr; }

  inline bool operator==(T* p) const { return ptr == p; }
  inline bool operator!=(T* p) const { return ptr != p; }

  inline bool operator!() const { return !ptr; }
};

struct HRError {
  const char* str;
  HRESULT hr;

  inline HRError(const char* str, HRESULT hr) : str(str), hr(hr) {}
};


class WinHandle {
  HANDLE handle = INVALID_HANDLE_VALUE;

  inline void Clear() {
    if (handle && handle != INVALID_HANDLE_VALUE)
      CloseHandle(handle);
  }

 public:
  inline WinHandle() {}
  inline WinHandle(HANDLE handle_) : handle(handle_) {}
  inline ~WinHandle() { Clear(); }

  inline operator HANDLE() const { return handle; }

  inline WinHandle& operator=(HANDLE handle_) {
    if (handle_ != handle) {
      Clear();
      handle = handle_;
    }

    return *this;
  }

  inline HANDLE* operator&() { return &handle; }

  inline bool Valid() const { return handle && handle != INVALID_HANDLE_VALUE; }
};

class WinModule {
  HMODULE handle = NULL;

  inline void Clear() {
    if (handle)
      FreeLibrary(handle);
  }

 public:
  inline WinModule() {}
  inline WinModule(HMODULE handle_) : handle(handle_) {}
  inline ~WinModule() { Clear(); }

  inline operator HMODULE() const { return handle; }

  inline WinModule& operator=(HMODULE handle_) {
    if (handle_ != handle) {
      Clear();
      handle = handle_;
    }

    return *this;
  }

  inline HMODULE* operator&() { return &handle; }

  inline bool Valid() const { return handle != NULL; }
};



#ifdef _WIN32
template <class T>
class ComQIPtr : public ComPtr<T> {
 public:
  inline ComQIPtr(IUnknown* unk) {
    this->ptr = nullptr;
    unk->QueryInterface(__uuidof(T), (void**)&this->ptr);
  }

  inline ComPtr<T>& operator=(IUnknown* unk) {
    ComPtr<T>::Clear();
    unk->QueryInterface(__uuidof(T), (void**)&this->ptr);
    return *this;
  }
};
#endif




enum class SourceType {
  Input,
  DeviceOutput,
  ProcessOutput,
};

enum speaker_layout {
  SPEAKERS_UNKNOWN,     /**< Unknown setting, fallback is stereo. */
  SPEAKERS_MONO,        /**< Channels: MONO */
  SPEAKERS_STEREO,      /**< Channels: FL, FR */
  SPEAKERS_2POINT1,     /**< Channels: FL, FR, LFE */
  SPEAKERS_4POINT0,     /**< Channels: FL, FR, FC, RC */
  SPEAKERS_4POINT1,     /**< Channels: FL, FR, FC, LFE, RC */
  SPEAKERS_5POINT1,     /**< Channels: FL, FR, FC, LFE, RL, RR */
  SPEAKERS_7POINT1 = 8, /**< Channels: FL, FR, FC, LFE, RL, RR, SL, SR */
};

class WASAPIActivateAudioInterfaceCompletionHandler
	: public Microsoft::WRL::RuntimeClass<
		  Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
		  Microsoft::WRL::FtmBase,
		  IActivateAudioInterfaceCompletionHandler> {
	IUnknown *unknown;
	HRESULT activationResult;
	WinHandle activationSignal;

public:
	WASAPIActivateAudioInterfaceCompletionHandler();
	HRESULT GetActivateResult(IAudioClient **client);

private:
	virtual HRESULT STDMETHODCALLTYPE ActivateCompleted(
		IActivateAudioInterfaceAsyncOperation *activateOperation)
		override final;
};

class SystemModule : public SystemModuleInterface {
 public:
  SystemModule();
  ~SystemModule();

  // Initialization and terminate.
  bool Init();
  int32_t Terminate();

  // Settings.
  int32_t SetRecordingDevice(uint16_t index);

  // enumerate source
  std::vector<AudioSourceInfo> SystemModule::EnumerateSystemSource() const;

  void SetRecordingSource(int id);

  // System source.
  rtc::scoped_refptr<AudioSource> CreateSource();
  void ResetSource();
  int32_t StopRecording();
  int32_t StartRecording();
  int32_t RecordingChannels();


  void SetSystemAudioLevel(float level);
  float GetSystemAudioLevel() const;

  void Initialize();
  static void InitFormat(const WAVEFORMATEX* wfex,
                         speaker_layout& speakers,
                         int& format,
                         uint32_t& sampleRate);
  static ComPtr<IAudioClient> InitClient(
      DWORD process_id,
      DWORD& reg_process_id,
      PFN_ActivateAudioInterfaceAsync activate_audio_interface_async,
      speaker_layout& speakers,
      int& format,
      uint32_t& samples_per_sec,
      rtc::scoped_refptr<SystemSource> source,
      HANDLE& ended_wait_handle
      );

  bool ProcessCaptureData();
  static DWORD WINAPI CaptureThread(LPVOID param);
  static DWORD WINAPI ReconnectThread(LPVOID param);
  static speaker_layout SystemModule::ConvertSpeakerLayout(DWORD layout,
                                                           WORD channels);
  static ComPtr<IAudioCaptureClient> InitCapture(IAudioClient* client,
                                                 HANDLE receiveSignal);


  ComPtr<IAudioClient> client;
  ComPtr<IAudioCaptureClient> capture;

  WinModule mmdevapi_module;
  PFN_ActivateAudioInterfaceAsync activate_audio_interface_async = NULL;

  DWORD process_id = 0;
  DWORD reg_process_id = 0;

  bool previouslyFailed = false;

  WinHandle reconnectThread = NULL;
  WinHandle captureThread = NULL;

  WinHandle idleSignal;
  WinHandle stopSignal;
  WinHandle receiveSignal;
  WinHandle restartSignal;
  WinHandle reconnectExitSignal;
  WinHandle exitSignal;
  WinHandle initSignal;
  HANDLE ended_wait_handle = nullptr;

  DWORD reconnectDuration = 0;
  WinHandle reconnectSignal;

  speaker_layout speakers = SPEAKERS_UNKNOWN;
  int format;  // byte size of frame.
  uint32_t sampleRate;

  bool stop = false;

  float audio_multiplier = 1.0;
  std::vector<float> capture_buffer;
  std::vector<int16_t> release_capture_buffer = std::vector<int16_t>(480 * 8);
};