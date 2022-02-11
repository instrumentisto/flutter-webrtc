#include <cstdint>
#include <memory>
#include <string>

#include "api/video/i420_buffer.h"
#include "libwebrtc-sys/include/bridge.h"
#include "libyuv.h"
#include "modules/audio_device/include/audio_device_factory.h"
#include "libwebrtc-sys/src/bridge.rs.h"

namespace bridge {

// Calls `AudioDeviceModule->Create()`.
std::unique_ptr<AudioDeviceModule> create_audio_device_module(
    AudioLayer audio_layer,
    TaskQueueFactory& task_queue_factory) {
  auto adm =
      webrtc::AudioDeviceModule::Create(audio_layer, &task_queue_factory);

  if (adm == nullptr) {
    return nullptr;
  }

  return std::make_unique<AudioDeviceModule>(adm);
}

// Calls `AudioDeviceModule->Init()`.
int32_t init_audio_device_module(const AudioDeviceModule& audio_device_module) {
  return audio_device_module->Init();
}

// Calls `AudioDeviceModule->PlayoutDevices()`.
int16_t playout_devices(const AudioDeviceModule& audio_device_module) {
  return audio_device_module->PlayoutDevices();
}

// Calls `AudioDeviceModule->RecordingDevices()`.
int16_t recording_devices(const AudioDeviceModule& audio_device_module) {
  return audio_device_module->RecordingDevices();
}

// Calls `AudioDeviceModule->PlayoutDeviceName()` with the provided arguments.
int32_t playout_device_name(const AudioDeviceModule& audio_device_module,
                            int16_t index,
                            rust::String& name,
                            rust::String& guid) {
  char name_buff[webrtc::kAdmMaxDeviceNameSize];
  char guid_buff[webrtc::kAdmMaxGuidSize];

  const int32_t result =
      audio_device_module->PlayoutDeviceName(index, name_buff, guid_buff);
  name = name_buff;
  guid = guid_buff;

  return result;
}

// Calls `AudioDeviceModule->RecordingDeviceName()` with the provided arguments.
int32_t recording_device_name(const AudioDeviceModule& audio_device_module,
                              int16_t index,
                              rust::String& name,
                              rust::String& guid) {
  char name_buff[webrtc::kAdmMaxDeviceNameSize];
  char guid_buff[webrtc::kAdmMaxGuidSize];

  const int32_t result =
      audio_device_module->RecordingDeviceName(index, name_buff, guid_buff);

  name = name_buff;
  guid = guid_buff;

  return result;
}

// Calls `AudioDeviceModule->SetRecordingDevice()` with the provided arguments.
int32_t set_audio_recording_device(const AudioDeviceModule& audio_device_module,
                                   uint16_t index) {
  return audio_device_module->SetRecordingDevice(index);
}

// Calls `VideoCaptureFactory->CreateDeviceInfo()`.
std::unique_ptr<VideoDeviceInfo> create_video_device_info() {
  std::unique_ptr<VideoDeviceInfo> ptr(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());

  return ptr;
}

// Calls `VideoDeviceInfo->GetDeviceName()` with the provided arguments.
int32_t video_device_name(VideoDeviceInfo& device_info,
                          uint32_t index,
                          rust::String& name,
                          rust::String& guid) {
  char name_buff[256];
  char guid_buff[256];

  const int32_t size =
      device_info.GetDeviceName(index, name_buff, 256, guid_buff, 256);

  name = name_buff;
  guid = guid_buff;

  return size;
}

// Calls `Thread->Create()`.
std::unique_ptr<rtc::Thread> create_thread() {
  return rtc::Thread::Create();
}

// Creates a new `DeviceVideoCapturer` with the specified constraints and
// calls `CreateVideoTrackSourceProxy()`.
std::unique_ptr<VideoTrackSourceInterface> create_video_source(
    Thread& worker_thread,
    Thread& signaling_thread,
    size_t width,
    size_t height,
    size_t fps,
    uint32_t device) {
  auto src = webrtc::CreateVideoTrackSourceProxy(
      &signaling_thread, &worker_thread,
      DeviceVideoCapturer::Create(width, height, fps, device));

  if (src == nullptr) {
    return nullptr;
  }

  return std::make_unique<VideoTrackSourceInterface>(src);
}

// Calls `PeerConnectionFactoryInterface->CreateAudioSource()` with empty
// `AudioOptions`.
std::unique_ptr<AudioSourceInterface> create_audio_source(
    const PeerConnectionFactoryInterface& peer_connection_factory) {
  auto src = peer_connection_factory->CreateAudioSource(
      cricket::AudioOptions());

  if (src == nullptr) {
    return nullptr;
  }

  return std::make_unique<AudioSourceInterface>(src);
}

// Calls `PeerConnectionFactoryInterface->CreateVideoTrack`.
std::unique_ptr<VideoTrackInterface> create_video_track(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    rust::String id,
    const VideoTrackSourceInterface& video_source) {
  auto track = peer_connection_factory->CreateVideoTrack(
      std::string(id), video_source.ptr());

  if (track == nullptr) {
    return nullptr;
  }

  return std::make_unique<VideoTrackInterface>(track);
}

// Calls `PeerConnectionFactoryInterface->CreateAudioTrack`.
std::unique_ptr<AudioTrackInterface> create_audio_track(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    rust::String id,
    const AudioSourceInterface& audio_source) {
  auto track = peer_connection_factory->CreateAudioTrack(
      std::string(id), audio_source.ptr());

  if (track == nullptr) {
    return nullptr;
  }

  return std::make_unique<AudioTrackInterface>(track);
}

// Calls `MediaStreamInterface->CreateLocalMediaStream`.
std::unique_ptr<MediaStreamInterface> create_local_media_stream(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    rust::String id) {
  auto
      stream = peer_connection_factory->CreateLocalMediaStream(std::string(id));

  if (stream == nullptr) {
    return nullptr;
  }

  return std::make_unique<MediaStreamInterface>(stream);
}

// Calls `MediaStreamInterface->AddTrack`.
bool add_video_track(const MediaStreamInterface& media_stream,
                     const VideoTrackInterface& track) {
  return media_stream->AddTrack(track.ptr());
}

// Calls `MediaStreamInterface->AddTrack`.
bool add_audio_track(const MediaStreamInterface& media_stream,
                     const AudioTrackInterface& track) {
  return media_stream->AddTrack(track.ptr());
}

// Calls `MediaStreamInterface->RemoveTrack`.
bool remove_video_track(const MediaStreamInterface& media_stream,
                        const VideoTrackInterface& track) {
  return media_stream->RemoveTrack(track.ptr());
}

// Calls `MediaStreamInterface->RemoveTrack`.
bool remove_audio_track(const MediaStreamInterface& media_stream,
                        const AudioTrackInterface& track) {
  return media_stream->RemoveTrack(track.ptr());
}

// Registers the provided video `sink` for the given `track`.
//
// Used to connect the given `track` to the underlying video engine.
void add_or_update_video_sink(const VideoTrackInterface& track,
                              VideoSinkInterface& sink) {
  track->AddOrUpdateSink(&sink, rtc::VideoSinkWants());
}

// Detaches the provided video `sink` from the given `track`.
void remove_video_sink(const VideoTrackInterface& track,
                       VideoSinkInterface& sink) {
  track->RemoveSink(&sink);
}

// Creates a new `ForwardingVideoSink`.
std::unique_ptr<VideoSinkInterface> create_forwarding_video_sink(
    rust::Box<DynOnFrameCallback> cb) {
  return std::make_unique<video_sink::ForwardingVideoSink>(std::move(cb));
}

// Converts the provided `webrtc::VideoFrame` pixels to the ABGR scheme and
// writes the result to the provided `dst_abgr`.
void video_frame_to_abgr(const webrtc::VideoFrame& frame,
                         uint8_t* dst_abgr) {
  rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
      frame.video_frame_buffer()->ToI420());

  libyuv::I420ToABGR(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
                     buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                     dst_abgr, buffer->width() * 4, buffer->width(),
                     buffer->height());
}

// Creates a new `PeerConnectionFactoryInterface`.
std::unique_ptr<PeerConnectionFactoryInterface> create_peer_connection_factory(
    const std::unique_ptr<Thread>& network_thread,
    const std::unique_ptr<Thread>& worker_thread,
    const std::unique_ptr<Thread>& signaling_thread,
    const std::unique_ptr<AudioDeviceModule>& default_adm) {
  auto default_adm_ =
      default_adm.get() == nullptr ? nullptr : default_adm.get()->ptr();
  if (default_adm_ != nullptr) {
    default_adm_->AddRef(); // TODO: recheck that we really need this
  }

  auto factory = webrtc::CreatePeerConnectionFactory(
      network_thread.get(),
      worker_thread.get(),
      signaling_thread.get(),
      default_adm_,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(),
      nullptr,
      nullptr);

  if (factory == nullptr) {
    return nullptr;
  }
  return std::make_unique<PeerConnectionFactoryInterface>(factory);
}

// Calls `PeerConnectionFactoryInterface->CreatePeerConnectionOrError`.
std::unique_ptr<PeerConnectionInterface> create_peer_connection_or_error(
    PeerConnectionFactoryInterface& peer_connection_factory,
    const RTCConfiguration& configuration,
    std::unique_ptr<PeerConnectionDependencies> dependencies,
    rust::String& error) {
  auto pc = peer_connection_factory->CreatePeerConnectionOrError(
      configuration, std::move(*dependencies));

  if (pc.ok()) {
    return std::make_unique<PeerConnectionInterface>(pc.MoveValue());
  }

  error = rust::String(pc.MoveError().message());
  return nullptr;
}

// Creates a new default `RTCConfiguration`.
std::unique_ptr<RTCConfiguration> create_default_rtc_configuration() {
  return std::make_unique<RTCConfiguration>();
}

// Creates a new `PeerConnectionObserver`.
std::unique_ptr<PeerConnectionObserver> create_peer_connection_observer(
    rust::Box<bridge::DynPeerConnectionOnEvent> cb) {
  return std::make_unique<PeerConnectionObserver>(PeerConnectionObserver(std::move(cb)));
}

// Creates a new `PeerConnectionDependencies`.
std::unique_ptr<PeerConnectionDependencies> create_peer_connection_dependencies(
    const std::unique_ptr<PeerConnectionObserver>& observer) {
  PeerConnectionDependencies pcd(observer.get());
  return std::make_unique<PeerConnectionDependencies>(std::move(pcd));
}

// Creates a new `RTCOfferAnswerOptions`.
std::unique_ptr<RTCOfferAnswerOptions> create_default_rtc_offer_answer_options() {
  return std::make_unique<RTCOfferAnswerOptions>();
}

// Creates a new `RTCOfferAnswerOptions`.
std::unique_ptr<RTCOfferAnswerOptions> create_rtc_offer_answer_options(
    int32_t offer_to_receive_video,
    int32_t offer_to_receive_audio,
    bool voice_activity_detection,
    bool ice_restart,
    bool use_rtp_mux) {
  return std::make_unique<RTCOfferAnswerOptions>(offer_to_receive_video,
                                                 offer_to_receive_audio,
                                                 voice_activity_detection,
                                                 ice_restart, use_rtp_mux);
}

// Creates a new `CreateSessionDescriptionObserver` from the provided
// `bridge::DynCreateSdpCallback`.
std::unique_ptr<CreateSessionDescriptionObserver>
create_create_session_observer(
    rust::Box<bridge::DynCreateSdpCallback> cb) {
  return std::make_unique<CreateSessionDescriptionObserver>(std::move(cb));
}

// Creates a new `SetLocalDescriptionObserverInterface` from the provided
// `bridge::DynSetDescriptionCallback`.
std::unique_ptr<SetLocalDescriptionObserver>
create_set_local_description_observer(
    rust::Box<bridge::DynSetDescriptionCallback> cb) {
  return std::make_unique<SetLocalDescriptionObserver>(std::move(cb));
}

// Creates a new `SetRemoteDescriptionObserverInterface` from the provided
// `bridge::DynSetDescriptionCallback`.
std::unique_ptr<SetRemoteDescriptionObserver>
create_set_remote_description_observer(
    rust::Box<bridge::DynSetDescriptionCallback> cb) {
  return std::make_unique<SetRemoteDescriptionObserver>(std::move(cb));
}

// Calls `PeerConnectionInterface->CreateOffer`.
void create_offer(PeerConnectionInterface& peer_connection_interface,
                  const RTCOfferAnswerOptions& options,
                  std::unique_ptr<CreateSessionDescriptionObserver> obs) {
  peer_connection_interface->CreateOffer(obs.release(), options);
}

// Calls `PeerConnectionInterface->CreateAnswer`.
void create_answer(PeerConnectionInterface& peer_connection_interface,
                   const RTCOfferAnswerOptions& options,
                   std::unique_ptr<CreateSessionDescriptionObserver> obs) {
  peer_connection_interface->CreateAnswer(obs.release(), options);
}

// Calls `PeerConnectionInterface->SetLocalDescription`.
void set_local_description(PeerConnectionInterface& peer_connection_interface,
                           std::unique_ptr<SessionDescriptionInterface> desc,
                           std::unique_ptr<SetLocalDescriptionObserver> obs) {
  auto observer =
      rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>(
          obs.release());
  peer_connection_interface->SetLocalDescription(std::move(desc),
                                                 observer);
}

// Calls `PeerConnectionInterface->SetRemoteDescription`.
void set_remote_description(PeerConnectionInterface& peer_connection_interface,
                            std::unique_ptr<SessionDescriptionInterface> desc,
                            std::unique_ptr<SetRemoteDescriptionObserver> obs) {
  auto observer =
      rtc::scoped_refptr<SetRemoteDescriptionObserver>(obs.release());
  peer_connection_interface->SetRemoteDescription(std::move(desc),
                                                  observer);
}

// Calls `IceCandidateInterface->ToString` and wraps result in `std::unqiue_ptr`.
std::unique_ptr<std::string> ice_candidate_interface_to_string(const IceCandidateInterface* candidate) {
    std::string out;
    candidate->ToString(&out);
    return std::make_unique<std::string>(out);
};

// Calls `Candidate->ToString` and wraps result in `std::unqiue_ptr`.
std::unique_ptr<std::string> candidate_to_string(const Candidate& candidate) {
  return std::make_unique<std::string>(candidate.ToString());
};

// Gets `CandidatePairChangeEvent.candidate_pair`.
const CandidatePair& get_candidate_pair(const CandidatePairChangeEvent& event) {
  return event.selected_candidate_pair;
};

// Gets `CandidatePairChangeEvent.last_data_received_ms`.
int64_t get_last_data_received_ms(const CandidatePairChangeEvent& event) {
  return event.last_data_received_ms;
}

// Gets `CandidatePairChangeEvent.reason` and wraps result in `std::unqiue_ptr`.
std::unique_ptr<std::string> get_reason(const CandidatePairChangeEvent& event) {
  return std::make_unique<std::string>(event.reason);
}

// Gets `CandidatePairChangeEvent.estimated_disconnected_time_ms`.
int64_t get_estimated_disconnected_time_ms(const CandidatePairChangeEvent& event) {
  return event.estimated_disconnected_time_ms;
}

// Calls `CandidatePair->local_candidate`.
const Candidate& get_local_candidate(const CandidatePair& pair) {
  return pair.local_candidate();
};

// Calls `CandidatePair->remote_candidate`.
const Candidate& get_remote_candidate(const CandidatePair& pair) {
  return pair.remote_candidate();
};






// RtpTransceiverInterface

// todo
std::unique_ptr<RtpReceiverInterface> rtp_transceiver_interface_get_receiver(
    const RtpTransceiverInterface& transceiver) {
      return std::make_unique<RtpReceiverInterface>(transceiver->receiver());
    }

std::unique_ptr<std::string> rtp_transceiver_interface_get_mid(
    const RtpTransceiverInterface& transceiver) {
      return std::make_unique<std::string>(transceiver->mid().value());
    }

RtpTransceiverDirection rtp_transceiver_interface_get_direction(
    const RtpTransceiverInterface& transceiver) {
      return transceiver->direction();
    }

std::unique_ptr<RtpSenderInterface> rtp_transceiver_interface_get_sender(
    const RtpTransceiverInterface& transceiver) {
      return std::make_unique<RtpSenderInterface> (transceiver->sender());
    }

// End RtpTransceiverInterface




// RtpSenderInterface

std::unique_ptr<std::string> rtp_sender_interface_get_id(
    const RtpSenderInterface& sender) {
      return std::make_unique<std::string>(sender->id());
    }

std::unique_ptr<DtmfSenderInterface> rtp_sender_interface_get_dtmf(
    const RtpSenderInterface& sender) {
      return std::make_unique<DtmfSenderInterface>(sender->GetDtmfSender());
    }

std::unique_ptr<RtpParameters> rtp_sender_interface_get_parameters(
    const RtpSenderInterface& sender) {
      return std::make_unique<RtpParameters>(sender->GetParameters());
    }

std::unique_ptr<MediaStreamTrackInterface> rtp_sender_interface_get_track(
    const RtpSenderInterface& sender) {
      return std::make_unique<MediaStreamTrackInterface>(sender->track());
    }

// End RtpSenderInterface




// DtmfSenderInterface

int32_t dtmf_sender_interface_get_duration(
    const DtmfSenderInterface& dtmf) {
      return dtmf->duration();
    }

int32_t dtmf_sender_interface_get_inter_tone_gap(
    const DtmfSenderInterface& dtmf) {
      return dtmf->inter_tone_gap();
    }

// End DtmfSenderInterface





// RtpReceiverInterface

// todo
std::unique_ptr<std::string> rtp_receiver_interface_get_id(
    const RtpReceiverInterface& receiver) {
      return std::make_unique<std::string>(receiver->id());
    }

// todo
std::unique_ptr<std::vector<MediaStreamInterface>> rtp_receiver_interface_get_streams(
    const RtpReceiverInterface& receiver) {
      auto streams = receiver->streams();
      std::vector<MediaStreamInterface> result;
      for (int i = 0; i<streams.size(); ++i) {
        result.push_back(streams[i]);
      }
      return std::make_unique<std::vector<MediaStreamInterface>>(result);
    }


// todo
std::unique_ptr<MediaStreamTrackInterface> rtp_receiver_interface_get_track(
    const RtpReceiverInterface& receiver) {
      return std::make_unique<MediaStreamTrackInterface>(receiver->track());
}

// todo 
std::unique_ptr<std::vector<std::string>> rtp_receiver_interface_get_stream_ids(
    const RtpReceiverInterface& receiver) {
      return std::make_unique<std::vector<std::string>>(receiver->stream_ids());
    }

// todo
std::unique_ptr<RtpParameters> rtp_receiver_interface_get_parameters(
    const RtpReceiverInterface& receiver) {
      return std::make_unique<RtpParameters>(receiver->GetParameters());
    }

// End RtpReceiverInterface




// RtpParameters 

// todo
std::unique_ptr<std::string> rtp_parameters_get_transaction_id(
    const RtpParameters& parameters) {
      return std::make_unique<std::string>(parameters.transaction_id);
    }

// todo
std::unique_ptr<std::string> rtp_parameters_get_mid(
    const RtpParameters& parameters) {
      return std::make_unique<std::string>(parameters.mid);
    }

// todo
std::unique_ptr<std::vector<RtpCodecParameters>> rtp_parameters_get_codecs(
    const RtpParameters& parameters) {
      return std::make_unique<std::vector<RtpCodecParameters>>(parameters.codecs);
    }

// todo
std::unique_ptr<std::vector<RtpExtension>> rtp_parameters_get_header_extensions(
    const RtpParameters& parameters) {
      return std::make_unique<std::vector<RtpExtension>>(parameters.header_extensions);
    }

// todo
std::unique_ptr<std::vector<RtpEncodingParameters>> rtp_parameters_get_encodings(
    const RtpParameters& parameters) {
      return std::make_unique<std::vector<RtpEncodingParameters>>(parameters.encodings);
    }

// todo
std::unique_ptr<RtcpParameters> rtp_parameters_get_rtcp(
    const RtpParameters& parameters) {
      return std::make_unique<RtcpParameters>(parameters.rtcp);
    }

// End RtpParameters



// RtcpParameters

// todo
std::unique_ptr<std::string> rtcp_parameters_get_cname(
    const RtcpParameters& rtcp) {
      return std::make_unique<std::string> (rtcp.cname);
    }

// todo refact
bool rtcp_parameters_get_reduced_size(
    const RtcpParameters& rtcp) {
      return rtcp.reduced_size;
    }


// End RtcpParameters



// RtpCodecParameters

// todo 
std::unique_ptr<std::string> rtp_codec_parameters_get_name(
    const RtpCodecParameters& codec) {
      return std::make_unique<std::string>(codec.name);
    }

 // todo 
 int32_t rtp_codec_parameters_get_payload_type(
     const RtpCodecParameters& codec) {
       return codec.payload_type;
     }

 // todo optinoanl
 int32_t rtp_codec_parameters_get_clock_rate(
     const RtpCodecParameters& codec) {
       return codec.clock_rate.value();
     }

 // todo optinoanl
 int32_t rtp_codec_parameters_get_num_channels(
     const RtpCodecParameters& codec) {
       return codec.num_channels.value();
     }

// todo
std::unique_ptr<std::vector<bridge::StringPair>> rtp_codec_parameters_get_parameters(
    const RtpCodecParameters& codec) {
      std::vector<StringPair> result;
       for (std::pair<std::string, std::string> element : codec.parameters) {
         auto pair = new_string_pair(element.first, element.second);
         result.push_back(pair);
       }
      return nullptr;
    }

// todo
MediaType rtp_codec_parameters_get_kind(
    const RtpCodecParameters& codec) {
      return codec.kind;
    }

// End RtpCodecParameters




// RtpExtension

// todo
std::unique_ptr<std::string> rtp_extension_get_uri(
    const RtpExtension& extension) {
      return std::make_unique<std::string>(extension.uri);
    }

int32_t rtp_extension_get_id(
    const RtpExtension& extension) {
      return extension.id;
    }

bool rtp_extension_get_encrypt(
    const RtpExtension& extension) {
      return extension.encrypt;
    }

// End RtpExtension



// RtpEncodingParameters

// todo
bool rtp_encoding_parameters_get_active(
    const RtpEncodingParameters& encoding) {
      return encoding.active; 
    }

int32_t rtp_encoding_parameters_get_maxBitrate(
    const RtpEncodingParameters& encoding) {
      return encoding.max_bitrate_bps.value();
    }

int32_t rtp_encoding_parameters_get_minBitrate(
    const RtpEncodingParameters& encoding) {
      return encoding.min_bitrate_bps.value();
    }

double rtp_encoding_parameters_get_maxFramerate(
    const RtpEncodingParameters& encoding) {
      return encoding.max_framerate.value();
    }

int64_t rtp_encoding_parameters_get_ssrc(
    const RtpEncodingParameters& encoding) {
      return encoding.ssrc.value();
    }

double rtp_encoding_parameters_get_scale_resolution_down_by(
    const RtpEncodingParameters& encoding) {
      return encoding.scale_resolution_down_by.value();
    }

// End RtpEncodingParameters




// MediaStreamInterface

// todo 
std::unique_ptr<std::string> media_stream_interface_get_id(
  const MediaStreamInterface& stream) {
  return std::make_unique<std::string>(stream->id());
}

// todo
std::unique_ptr<std::vector<AudioTrackInterface>> media_stream_interface_get_audio_tracks(
    const MediaStreamInterface& stream) {
      auto tracks = stream->GetAudioTracks();
      std::vector<AudioTrackInterface> result;
      for (int i = 0; i < tracks.size(); ++i) {
        result.push_back(tracks[i]);
      }
      return std::make_unique<std::vector<AudioTrackInterface>>(result);
    }

// todo
std::unique_ptr<std::vector<VideoTrackInterface>> media_stream_interface_get_video_tracks(
    const MediaStreamInterface& stream) {
      auto tracks = stream->GetVideoTracks();
      std::vector<VideoTrackInterface> result;
      for (int i = 0; i < tracks.size(); ++i) {
        result.push_back(tracks[i]);
      }
      return std::make_unique<std::vector<VideoTrackInterface>>(result);
    }


// End MediaStreamInterface




// MediaStreamTrackInterface

// todo
std::unique_ptr<std::string> media_stream_track_interface_get_kind(
    const MediaStreamTrackInterface& track) {
      return std::make_unique<std::string>(track->kind());
    }

// todo
std::unique_ptr<std::string> media_stream_track_interface_get_id(
    const MediaStreamTrackInterface& track) {
      return std::make_unique<std::string>(track->id());
    }

// todo
TrackState media_stream_track_interface_get_state(
    const MediaStreamTrackInterface& track) {
      return track->state();
    }

// todo
bool media_stream_track_interface_get_enabled(
    const MediaStreamTrackInterface& track) {
      return track->enabled();
    }

// todo recheck
std::unique_ptr<VideoTrackInterface> media_stream_track_interface_downcast_video_track(
  MediaStreamTrackInterface& track) {
    return std::make_unique<VideoTrackInterface>(static_cast<webrtc::VideoTrackInterface*>(track.ptr()));
  }

// todo recheck
std::unique_ptr<AudioTrackInterface> media_stream_track_interface_downcast_audio_track(
  MediaStreamTrackInterface& track) {
    return std::make_unique<AudioTrackInterface>(static_cast<webrtc::AudioTrackInterface*>(track.ptr()));
  }

// End MediaStreamTrackInterface





const MediaStreamTrackInterface& video_track_truncation(
    const VideoTrackInterface& track) {
      return MediaStreamTrackInterface(track.ptr());
    }

const MediaStreamTrackInterface& audio_track_truncation(
    const AudioTrackInterface& track) {
      return MediaStreamTrackInterface(track.ptr());
    }

// todo
std::unique_ptr<AudioSourceInterface> audio_track_get_sourse(
     const AudioTrackInterface& track) {
       return std::make_unique<AudioSourceInterface>(track->GetSource());
     }

std::unique_ptr<VideoTrackSourceInterface> video_track_get_sourse(
    const VideoTrackInterface& track) {
      return std::make_unique<VideoTrackSourceInterface>(track->GetSource());
    }

}  // namespace bridge
