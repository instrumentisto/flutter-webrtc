#include <cstdint>
#include <memory>
#include <string>

#include "modules/audio_device/include/audio_device_factory.h"

#include "libwebrtc-sys/include/bridge.h"

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
};

// Calls `AudioDeviceModule->Init()`.
int32_t init_audio_device_module(const AudioDeviceModule& audio_device_module) {
  return audio_device_module->Init();
}

// Calls `AudioDeviceModule->PlayoutDevices()`.
int16_t playout_devices(const AudioDeviceModule& audio_device_module) {
  return audio_device_module->PlayoutDevices();
};

// Calls `AudioDeviceModule->RecordingDevices()`.
int16_t recording_devices(const AudioDeviceModule& audio_device_module) {
  return audio_device_module->RecordingDevices();
};

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
};

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
};

// Calls `AudioDeviceModule->SetRecordingDevice()` with the provided arguments.
int32_t set_audio_recording_device(const AudioDeviceModule& audio_device_module,
                                   uint16_t index) {
  return audio_device_module.ptr()->SetRecordingDevice(index);
}

// Calls `VideoCaptureFactory->CreateDeviceInfo()`.
std::unique_ptr<VideoDeviceInfo> create_video_device_info() {
  std::unique_ptr<VideoDeviceInfo> ptr(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());

  return ptr;
};

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
};

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
  auto src =
      peer_connection_factory->CreateAudioSource(cricket::AudioOptions());

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
  auto track = peer_connection_factory->CreateVideoTrack(std::string(id),
                                                         video_source.ptr());

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
  auto track = peer_connection_factory->CreateAudioTrack(std::string(id),
                                                         audio_source.ptr());

  if (track == nullptr) {
    return nullptr;
  }

  return std::make_unique<AudioTrackInterface>(track);
}

// Calls `MediaStreamInterface->CreateLocalMediaStream`.
std::unique_ptr<MediaStreamInterface> create_local_media_stream(
    const PeerConnectionFactoryInterface& peer_connection_factory,
    rust::String id) {
  auto stream =
      peer_connection_factory->CreateLocalMediaStream(std::string(id));

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

// Creates `PeerConnectionFactoryInterface`.
std::unique_ptr<PeerConnectionFactoryInterface> create_peer_connection_factory(
    const std::unique_ptr<Thread>& network_thread,
    const std::unique_ptr<Thread>& worker_thread,
    const std::unique_ptr<Thread>& signaling_thread,
    const std::unique_ptr<AudioDeviceModule>& default_adm) {
  auto default_adm_ =
      default_adm.get() == nullptr ? nullptr : default_adm.get()->ptr();
  if (default_adm_ != nullptr) {
    default_adm_->AddRef();  // TODO: recheck that we really need this
  }

  auto factory = webrtc::CreatePeerConnectionFactory(
      network_thread.get(), worker_thread.get(), signaling_thread.get(),
      default_adm_, webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr, nullptr);

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
  PeerConnectionDependencies pcd = std::move(*(dependencies.release()));
  auto pc = peer_connection_factory.ptr()->CreatePeerConnectionOrError(
      configuration, std::move(pcd));

  if (pc.ok()) {
    auto ptr = pc.MoveValue();
    return std::make_unique<PeerConnectionInterface>(std::move(ptr));
  }
  error = rust::String(pc.MoveError().message());
  return std::unique_ptr<PeerConnectionInterface>();
}

// Creates default `RTCConfiguration`.
std::unique_ptr<RTCConfiguration> create_default_rtc_configuration() {
  RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  return std::make_unique<RTCConfiguration>(config);
}

// Creates `PeerConnectionObserver`.
std::unique_ptr<PeerConnectionObserver> create_peer_connection_observer() {
  PeerConnectionObserver obs;
  return std::make_unique<PeerConnectionObserver>(obs);
}

// Creates `PeerConnectionDependencies`.
std::unique_ptr<PeerConnectionDependencies> create_peer_connection_dependencies(
    std::unique_ptr<PeerConnectionObserver> observer) {
  PeerConnectionDependencies pcd(observer.release());
  return std::make_unique<PeerConnectionDependencies>(std::move(pcd));
}

// Creates `RTCOfferAnswerOptions`.
std::unique_ptr<RTCOfferAnswerOptions>
create_default_rtc_offer_answer_options() {
  return std::make_unique<RTCOfferAnswerOptions>(RTCOfferAnswerOptions());
}

// Creates `RTCOfferAnswerOptions`.
std::unique_ptr<RTCOfferAnswerOptions> create_rtc_offer_answer_options(
    int32_t offer_to_receive_video,
    int32_t offer_to_receive_audio,
    bool voice_activity_detection,
    bool ice_restart,
    bool use_rtp_mux) {
  return std::make_unique<RTCOfferAnswerOptions>(RTCOfferAnswerOptions(
      offer_to_receive_video, offer_to_receive_audio, voice_activity_detection,
      ice_restart, use_rtp_mux));
}

// Creates `CreateSessionDescriptionObserver`.
std::unique_ptr<CreateSessionDescriptionObserver>
create_create_session_observer(rust::Box<bridge::DynCreateSdpCallback> cb) {
  return std::make_unique<CreateSessionDescriptionObserver>(
      CreateSessionDescriptionObserver(std::move(cb)));
}

// Creates `SetLocalDescriptionObserver`.
std::unique_ptr<SetLocalDescriptionObserver>
create_set_local_description_observer(
    rust::Box<bridge::DynSetDescriptionCallback> cb) {
  return std::make_unique<SetLocalDescriptionObserver>(
      SetLocalDescriptionObserver(std::move(cb)));
}

// Creates `SetRemoteDescriptionObserver`.
std::unique_ptr<SetRemoteDescriptionObserver>
create_set_remote_description_observer(
    rust::Box<bridge::DynSetDescriptionCallback> cb) {
  return std::make_unique<SetRemoteDescriptionObserver>(
      SetRemoteDescriptionObserver(std::move(cb)));
}

// Calls `PeerConnectionInterface->CreateOffer`.
void create_offer(PeerConnectionInterface& peer_connection_interface,
                  const RTCOfferAnswerOptions& options,
                  std::unique_ptr<CreateSessionDescriptionObserver> obs) {
  peer_connection_interface.ptr()->CreateOffer(obs.release(), options);
}

// Calls `PeerConnectionInterface->CreateAnswer`.
void create_answer(PeerConnectionInterface& peer_connection_interface,
                   const RTCOfferAnswerOptions& options,
                   std::unique_ptr<CreateSessionDescriptionObserver> obs) {
  peer_connection_interface.ptr()->CreateAnswer(obs.release(), options);
}

// Calls `PeerConnectionInterface->SetLocalDescription`.
void set_local_description(PeerConnectionInterface& peer_connection_interface,
                           std::unique_ptr<SessionDescriptionInterface> desc,
                           std::unique_ptr<SetLocalDescriptionObserver> obs) {
  auto observer =
      rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>(
          obs.release());
  peer_connection_interface.ptr()->SetLocalDescription(std::move(desc),
                                                       observer);
}

// Calls `PeerConnectionInterface->SetRemoteDescription`.
void set_remote_description(PeerConnectionInterface& peer_connection_interface,
                            std::unique_ptr<SessionDescriptionInterface> desc,
                            std::unique_ptr<SetRemoteDescriptionObserver> obs) {
  auto observer =
      rtc::scoped_refptr<SetRemoteDescriptionObserver>(obs.release());
  peer_connection_interface.ptr()->SetRemoteDescription(std::move(desc),
                                                        observer);
}

void add_transceiver(PeerConnectionInterface& peer_connection_interface,
                     MediaType media_type,
                     RtpTransceiverDirection direction) {
  auto transceiver_init = webrtc::RtpTransceiverInit();
  transceiver_init.direction = direction;

  peer_connection_interface->AddTransceiver(media_type, transceiver_init);
}

std::unique_ptr<std::vector<RtpTransceiverInterface>> get_transceivers(
    const PeerConnectionInterface& peer_connection_interface) {
  std::vector<RtpTransceiverInterface> transceivers;

  for (RtpTransceiverInterface transceiver :
       peer_connection_interface->GetTransceivers()) {
    transceivers.push_back(transceiver);
  }

  return std::make_unique<std::vector<RtpTransceiverInterface>>(transceivers);
}

rust::Box<Transceivers> get_rust_transceivers(
    const PeerConnectionInterface& peer_connection_interface) {
  auto rust_trans = create_transceivers();

  for (RtpTransceiverInterface transceiver :
       peer_connection_interface->GetTransceivers()) {
    rust_trans->add(transceiver->direction(),
                    transceiver->mid().value_or("228"));
  }

  return rust_trans;
}

bool get_transceiver_mid(const RtpTransceiverInterface& transceiver,
                         rust::String& mid) {
  mid = "322";
  return true;
  // auto raw_mid = transceiver->mid();

  // if (raw_mid.has_value()) {
  //   mid = raw_mid.value();

  //   return true;
  // }
  // return false;
}

void ustest(const PeerConnectionInterface& peer_connection_interface) {
  // for (RtpTransceiverInterface trans :
  //      peer_connection_interface->GetTransceivers()) {
  //   auto direction = trans->direction();
  //   auto mid = trans->mid();

  //   auto sender = trans->sender();
  //   auto sid = sender->id();
  //   auto strack = sender->track();
  //   auto srtpparams = sender->GetParameters();

  //   auto receiver = trans->receiver();
  //   auto rid = receiver->id();
  //   auto rtrack = receiver->track();
  //   auto rrtpparams = receiver->GetParameters();
  // }

  auto ddpd = std::string();

  peer_connection_interface->local_description()->ToString(&ddpd);

  printf("testik %s\n=========================\n\n", ddpd.c_str());
}

}  // namespace bridge
