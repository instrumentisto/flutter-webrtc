#pragma once

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/peer_connection_interface.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "libwebrtc-sys/include/peer_connection_observer.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_track_source_proxy_factory.h"
#include "device_video_capturer.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/video_capture/video_capture_factory.h"
#include "pc/audio_track.h"
#include "pc/local_audio_source.h"
#include "pc/video_track_source.h"
#include "peer_connection_observer.h"
#include "rust/cxx.h"
#include "screen_video_capturer.h"
#include "video_sink.h"

namespace bridge {

using Thread = rtc::Thread;
using VideoSinkInterface = rtc::VideoSinkInterface<webrtc::VideoFrame>;

using AudioLayer = webrtc::AudioDeviceModule::AudioLayer;
using PeerConnectionDependencies = webrtc::PeerConnectionDependencies;
using RTCConfiguration = webrtc::PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions =
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions;
using SdpType = webrtc::SdpType;
using SessionDescriptionInterface = webrtc::SessionDescriptionInterface;
using TaskQueueFactory = webrtc::TaskQueueFactory;
using VideoDeviceInfo = webrtc::VideoCaptureModule::DeviceInfo;
using VideoRotation = webrtc::VideoRotation;

using AudioDeviceModule = rtc::scoped_refptr<webrtc::AudioDeviceModule>;
using AudioSourceInterface = rtc::scoped_refptr<webrtc::AudioSourceInterface>;
using AudioTrackInterface = rtc::scoped_refptr<webrtc::AudioTrackInterface>;
using MediaStreamInterface = rtc::scoped_refptr<webrtc::MediaStreamInterface>;
using PeerConnectionFactoryInterface =
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>;
using PeerConnectionInterface =
    rtc::scoped_refptr<webrtc::PeerConnectionInterface>;
using VideoTrackInterface = rtc::scoped_refptr<webrtc::VideoTrackInterface>;
using VideoTrackSourceInterface =
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>;

using CreateSessionDescriptionObserver =
    observer::CreateSessionDescriptionObserver;
using PeerConnectionObserver = observer::PeerConnectionObserver;
using SetLocalDescriptionObserver = observer::SetLocalDescriptionObserver;
using SetRemoteDescriptionObserver = observer::SetRemoteDescriptionObserver;

using MediaType = cricket::MediaType;
using RtpTransceiverDirection = webrtc::RtpTransceiverDirection;
using RtpTransceiverInterface =
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface>;

struct Transceivers;

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

// Specifies which microphone to use for recording audio using an index
// retrieved by the corresponding enumeration method which is
// `AudiDeviceModule::RecordingDeviceName`.
int32_t set_audio_recording_device(const AudioDeviceModule& audio_device_module,
                                   uint16_t index);

// Creates a new `VideoDeviceInfo`.
std::unique_ptr<VideoDeviceInfo> create_video_device_info();

// Obtains information regarding the specified video recording device.
int32_t video_device_name(VideoDeviceInfo& device_info,
                          uint32_t index,
                          rust::String& name,
                          rust::String& guid);

// Creates a new `Thread`.
std::unique_ptr<rtc::Thread> create_thread();

// Creates a new `VideoTrackSourceInterface` from the specified video input
// device according to the specified constraints.
std::unique_ptr<VideoTrackSourceInterface> create_device_video_source(
    Thread& worker_thread,
    Thread& signaling_thread,
    size_t width,
    size_t height,
    size_t fps,
    uint32_t device_index);

// Starts screen capturing and creates a new `VideoTrackSourceInterface`
// according to the specified constraints.
std::unique_ptr<VideoTrackSourceInterface> create_display_video_source(
    Thread& worker_thread,
    Thread& signaling_thread,
    size_t width,
    size_t height,
    size_t fps);

// Creates a new `AudioSourceInterface`.
std::unique_ptr<AudioSourceInterface> create_audio_source(
    const PeerConnectionFactoryInterface& peer_connection_factory);

// Creates a new `VideoTrackInterface`.
std::unique_ptr<VideoTrackInterface> create_video_track(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    rust::String id,
    const VideoTrackSourceInterface& video_source);

// Creates a new `AudioTrackInterface`.
std::unique_ptr<AudioTrackInterface> create_audio_track(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    rust::String id,
    const AudioSourceInterface& audio_source);

// Creates a new `MediaStreamInterface`.
std::unique_ptr<MediaStreamInterface> create_local_media_stream(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    rust::String id);

// Adds the provided `VideoTrackInterface` to the specified
// `MediaStreamInterface`.
bool add_video_track(const MediaStreamInterface& media_stream,
                     const VideoTrackInterface& track);

// Adds the provided `AudioTrackInterface` to the specified
// `MediaStreamInterface`.
bool add_audio_track(const MediaStreamInterface& media_stream,
                     const AudioTrackInterface& track);

// Removes the provided `VideoTrackInterface` to the specified
// `MediaStreamInterface`.
bool remove_video_track(const MediaStreamInterface& media_stream,
                        const VideoTrackInterface& track);

// Removes the provided `AudioTrackInterface` to the specified
// `MediaStreamInterface`.
bool remove_audio_track(const MediaStreamInterface& media_stream,
                        const AudioTrackInterface& track);

// Changes the `enabled` property of the provided `VideoTrackInterface`.
void set_video_track_enabled(const VideoTrackInterface& track, bool enabled);

// Changes the `enabled` property of the provided `AudioTrackInterface`.
void set_audio_track_enabled(const AudioTrackInterface& track, bool enabled);

// Registers the provided video `sink` for the given `track`.
//
// Used to connect the given `track` to the underlying video engine.
void add_or_update_video_sink(const VideoTrackInterface& track,
                              VideoSinkInterface& sink);

// Detaches the provided video `sink` from the given `track`.
void remove_video_sink(const VideoTrackInterface& track,
                       VideoSinkInterface& sink);

// Creates a new `ForwardingVideoSink`.
std::unique_ptr<VideoSinkInterface> create_forwarding_video_sink(
    rust::Box<DynOnFrameCallback> handler);

// Converts the provided `webrtc::VideoFrame` pixels to the ABGR scheme and
// writes the result to the provided `dst_abgr`.
void video_frame_to_abgr(const webrtc::VideoFrame& frame, uint8_t* dst_abgr);

// Creates a new `PeerConnectionFactoryInterface`.
std::unique_ptr<PeerConnectionFactoryInterface> create_peer_connection_factory(
    const std::unique_ptr<Thread>& network_thread,
    const std::unique_ptr<Thread>& worker_thread,
    const std::unique_ptr<Thread>& signaling_thread,
    const std::unique_ptr<AudioDeviceModule>& default_adm);

// Creates a new `PeerConnectionInterface`.
std::unique_ptr<PeerConnectionInterface> create_peer_connection_or_error(
    PeerConnectionFactoryInterface& peer_connection_factory,
    const RTCConfiguration& configuration,
    std::unique_ptr<PeerConnectionDependencies> dependencies,
    rust::String& error);

// Creates a new default `RTCConfiguration`.
std::unique_ptr<RTCConfiguration> create_default_rtc_configuration();

// Creates a new `PeerConnectionObserver`.
std::unique_ptr<PeerConnectionObserver> create_peer_connection_observer();

// Creates a new `PeerConnectionDependencies`.
std::unique_ptr<PeerConnectionDependencies> create_peer_connection_dependencies(
    std::unique_ptr<PeerConnectionObserver> observer);

// Creates a new `RTCOfferAnswerOptions`.
std::unique_ptr<RTCOfferAnswerOptions>
create_default_rtc_offer_answer_options();

// Creates a new `RTCOfferAnswerOptions`.
std::unique_ptr<RTCOfferAnswerOptions> create_rtc_offer_answer_options(
    int32_t offer_to_receive_video,
    int32_t offer_to_receive_audio,
    bool voice_activity_detection,
    bool ice_restart,
    bool use_rtp_mux);

// Creates a new `CreateSessionDescriptionObserver` from the provided
// `bridge::DynCreateSdpCallback`.
std::unique_ptr<CreateSessionDescriptionObserver>
create_create_session_observer(rust::Box<bridge::DynCreateSdpCallback> cb);

// Creates a new `SetLocalDescriptionObserverInterface` from the provided
// `bridge::DynSetDescriptionCallback`.
std::unique_ptr<SetLocalDescriptionObserver>
create_set_local_description_observer(
    rust::Box<bridge::DynSetDescriptionCallback> cb);

// Creates a new `SetRemoteDescriptionObserverInterface` from the provided
// `bridge::DynSetDescriptionCallback`.
std::unique_ptr<SetRemoteDescriptionObserver>
create_set_remote_description_observer(
    rust::Box<bridge::DynSetDescriptionCallback> cb);

// Calls `PeerConnectionInterface->CreateOffer`.
void create_offer(PeerConnectionInterface& peer,
                  const RTCOfferAnswerOptions& options,
                  std::unique_ptr<CreateSessionDescriptionObserver> obs);

// Calls `PeerConnectionInterface->CreateAnswer`.
void create_answer(PeerConnectionInterface& peer,
                   const RTCOfferAnswerOptions& options,
                   std::unique_ptr<CreateSessionDescriptionObserver> obs);

// Calls `PeerConnectionInterface->SetLocalDescription`.
void set_local_description(PeerConnectionInterface& peer,
                           std::unique_ptr<SessionDescriptionInterface> desc,
                           std::unique_ptr<SetLocalDescriptionObserver> obs);

// Calls `PeerConnectionInterface->SetRemoteDescription`.
void set_remote_description(PeerConnectionInterface& peer,
                            std::unique_ptr<SessionDescriptionInterface> desc,
                            std::unique_ptr<SetRemoteDescriptionObserver> obs);

// Adds a `RtpTransceiver` to the `PeerConnectionInterface`.
std::unique_ptr<RtpTransceiverInterface> add_transceiver(
    PeerConnectionInterface& peer,
    MediaType media_type,
    RtpTransceiverDirection direction);

// Gets the `PeerConnection`'s `RtpTransceiver`s info to Rust `Transceivers`.
rust::Box<Transceivers> get_transceivers(const PeerConnectionInterface& peer);

// Gets the `Transceiver`'s `mid`.
rust::String get_transceiver_mid(const RtpTransceiverInterface& transceiver);

// Gets the `pointer` of the `Transceiver`'s `scoped_refpt`.
size_t get_transceiver_ptr(const RtpTransceiverInterface& transceiver);

// Gets the `Transceiver`'s `direction`.
RtpTransceiverDirection get_transceiver_direction(
    const RtpTransceiverInterface& transceiver);

}  // namespace bridge
