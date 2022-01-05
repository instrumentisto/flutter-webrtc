#pragma once

#include <iostream>
#include <memory>
#include <string>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/peer_connection_interface.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_track_source_proxy_factory.h"
#include "custom_video_renderer.h"
#include "device_video_capturer.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/video_capture/video_capture_factory.h"
#include "native_win_rndrr.h"
#include "pc/audio_track.h"
#include "pc/local_audio_source.h"
#include "pc/video_track_source.h"
#include "rust/cxx.h"
#include "screen_video_capturer.h"

namespace bridge {

// Smart pointer designed to wrap WebRTC's `rtc::scoped_refptr`.
//
// `rtc::scoped_refptr` can't be used with `std::uniqueptr` since it has private
// destructor. `rc` unwraps raw pointer from the provided `rtc::scoped_refptr`
// and calls `Release()` in its destructor therefore this allows wrapping `rc`
// into a `std::uniqueptr`.
template <class T>
class rc {
 public:
  typedef T element_type;

  // Unwraps the actual pointer from the provided `rtc::scoped_refptr`.
  rc(rtc::scoped_refptr<T> p) : ptr_(p.release()) {}

  // Calls `RefCountInterface::Release()` on the underlying pointer.
  ~rc() { ptr_->Release(); }

  // Returns a pointer to the managed object.
  T* ptr() const { return ptr_; }

  // Returns a pointer to the managed object.
  T* operator->() const { return ptr_; }

 protected:
  // Pointer to the managed object.
  T* ptr_;
};

using TaskQueueFactory = webrtc::TaskQueueFactory;
using AudioDeviceModule = rc<webrtc::AudioDeviceModule>;
using VideoDeviceInfo = webrtc::VideoCaptureModule::DeviceInfo;
using AudioLayer = webrtc::AudioDeviceModule::AudioLayer;
using Thread = rtc::Thread;
using PeerConnectionFactoryInterface =
    rc<webrtc::PeerConnectionFactoryInterface>;
using VideoTrackSourceInterface = rc<webrtc::VideoTrackSourceInterface>;
using AudioSourceInterface = rc<webrtc::AudioSourceInterface>;
using VideoTrackInterface = rc<webrtc::VideoTrackInterface>;
using AudioTrackInterface = rc<webrtc::AudioTrackInterface>;
using MediaStreamInterface = rc<webrtc::MediaStreamInterface>;
using VideoFrame = webrtc::VideoFrame;

// Creates a new `AudioDeviceModule` for the given `AudioLayer`.
std::unique_ptr<AudioDeviceModule> create_audio_device_module(
    AudioLayer audio_layer,
    TaskQueueFactory& task_queue_factory);

// Initializes the native audio parts required for each platform.
int32_t init_audio_device_module(const AudioDeviceModule& audio_device_module);

// Returns count of the available playout audio devices.
int16_t playout_devices(const AudioDeviceModule& audio_device_module);

// Returns count of the available recording audio devices.
int16_t recording_devices(const AudioDeviceModule& audio_device_module);

// Obtains information regarding the specified audio playout device.
int32_t playout_device_name(const AudioDeviceModule& audio_device_module,
                            int16_t index,
                            rust::String& name,
                            rust::String& guid);

// Obtains information regarding the specified audio recording device.
int32_t recording_device_name(const AudioDeviceModule& audio_device_module,
                              int16_t index,
                              rust::String& name,
                              rust::String& guid);

// Creates a new `VideoDeviceInfo`.
std::unique_ptr<VideoDeviceInfo> create_video_device_info();

// Obtains information regarding the specified video recording device.
int32_t video_device_name(VideoDeviceInfo& device_info,
                          uint32_t index,
                          rust::String& name,
                          rust::String& guid);

/// Creates a new thread.
std::unique_ptr<rtc::Thread> create_thread();

/// Starts the thread.
bool start_thread(rtc::Thread& thread);

/// Creates a new Peer Connection Factory.
std::unique_ptr<PeerConnectionFactoryInterface> create_peer_connection_factory(
    Thread& worker_thread,
    Thread& signaling_thread);

/// Creates a new video source.
std::unique_ptr<VideoTrackSourceInterface> create_video_source(
    Thread& worker_thread,
    Thread& signaling_thread,
    size_t width,
    size_t height,
    size_t fps);

/// Creates a new audio source.
std::unique_ptr<AudioSourceInterface> create_audio_source(
    const PeerConnectionFactoryInterface& peer_connection_factory);

/// Creates a new video track.
std::unique_ptr<VideoTrackInterface> create_video_track(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    const VideoTrackSourceInterface& video_source);

/// Creates a new audio track.
std::unique_ptr<AudioTrackInterface> create_audio_track(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    const AudioSourceInterface& audio_source);

/// Creates a new Local Media Stream.
std::unique_ptr<MediaStreamInterface> create_local_media_stream(
    const PeerConnectionFactoryInterface& peer_connection_factory);

/// Adds the video track to media stream.
bool add_video_track(const MediaStreamInterface& media_stream,
                     const VideoTrackInterface& track);

/// Adds the audio track to media stream.
bool add_audio_track(const MediaStreamInterface& media_stream,
                     const AudioTrackInterface& track);

/// Removes the video track from media stream.
bool remove_video_track(const MediaStreamInterface& media_stream,
                        const VideoTrackInterface& track);

/// Removes the audio track from media stream.
bool remove_audio_track(const MediaStreamInterface& media_stream,
                        const AudioTrackInterface& track);

int32_t frame_width(const std::unique_ptr<VideoFrame>& frame);

int32_t frame_height(const std::unique_ptr<VideoFrame>& frame);

int32_t frame_rotation(const std::unique_ptr<VideoFrame>& frame);

rust::Vec<uint8_t> convert_to_argb(const std::unique_ptr<VideoFrame>& frame,
                                   int32_t buffer_size);

std::unique_ptr<VideoRenderer> get_video_renderer(
    rust::Fn<void(std::unique_ptr<VideoFrame>, size_t)> cb,
    size_t flutter_cb_ptr,
    const std::unique_ptr<VideoTrackInterface>& track_to_render);

void set_renderer_no_track(
    const std::unique_ptr<VideoRenderer>& video_renderer);

std::unique_ptr<VideoTrackSourceInterface> create_screen_source(
    Thread& worker_thread,
    Thread& signaling_thread,
    size_t width,
    size_t height,
    size_t fps);

void test();
}  // namespace bridge
