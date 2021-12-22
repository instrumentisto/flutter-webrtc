#include <Windows.h>
#include <sstream>

#include "flutter_webrtc.h"

#include <flutter_webrtc_native.h>
#include "flutter_webrtc/flutter_web_r_t_c_plugin.h"

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#define DEFAULT_FPS 30

namespace flutter_webrtc_plugin {

template<typename T>
inline bool TypeIs(const EncodableValue val) {
  return std::holds_alternative<T>(val);
}

template<typename T>
inline const T GetValue(EncodableValue val) {
  return std::get<T>(val);
}

inline EncodableMap findMap(const EncodableMap &map, const std::string &key) {
  auto it = map.find(EncodableValue(key));
  if (it != map.end() && TypeIs<EncodableMap>(it->second))
    return GetValue<EncodableMap>(it->second);
  return EncodableMap();
}

inline std::string findString(const EncodableMap &map, const std::string &key) {
  auto it = map.find(EncodableValue(key));
  if (it != map.end() && TypeIs<std::string>(it->second))
    return GetValue<std::string>(it->second);
  return std::string();
}

FlutterWebRTC::FlutterWebRTC(FlutterWebRTCPlugin *plugin) {}

FlutterWebRTC::~FlutterWebRTC() {}

void FlutterWebRTC::HandleMethodCall(
    const flutter::MethodCall<EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
  const std::string &method = method_call.method_name();

  if (method.compare("createPeerConnection") == 0) {
  } else if (method.compare("getSources") == 0) {
    rust::Vec<MediaDeviceInfo> devices = webrtc->EnumerateDevices();

    EncodableList sources;

    for (size_t i = 0; i < devices.size(); ++i) {
      std::string kind;
      switch (devices[i].kind) {
        case MediaDeviceKind::kAudioInput:kind = "audioinput";
          break;

        case MediaDeviceKind::kAudioOutput:kind = "audiooutput";
          break;

        case MediaDeviceKind::kVideoInput:kind = "videoinput";
          break;

        default:throw std::exception("Invalid MediaDeviceKind");
      }

      EncodableMap info;
      info[EncodableValue("label")] =
          EncodableValue(std::string(devices[i].label));
      info[EncodableValue("deviceId")] =
          EncodableValue(std::string(devices[i].device_id));
      info[EncodableValue("kind")] = EncodableValue(kind);
      info[EncodableValue("groupId")] = EncodableValue(std::string(""));

      sources.push_back(EncodableValue(info));
    }

    EncodableMap params;
    params[EncodableValue("sources")] = EncodableValue(sources);

    result->Success(EncodableValue(params));
  } else if (method.compare("getUserMedia") == 0) {
    if (!method_call.arguments()) {
      result->Error("Bad Arguments", "Null constraints arguments received");
      return;
    }

    auto args = GetValue<EncodableMap>(*method_call.arguments());
    auto constraints_arg = findMap(args, "constraints");
    auto audio_arg = constraints_arg.find(EncodableValue("audio"))->second;
    auto video_arg = constraints_arg.find(EncodableValue("video"));

    EncodableMap video_mandatory;

    EncodableMap video_map = GetValue<EncodableMap>(video_arg->second);
    video_mandatory = GetValue<EncodableMap>(
        video_map.find(EncodableValue("mandatory"))->second);
    EncodableValue
        width = video_mandatory.find(EncodableValue("minWidth"))->second;
    EncodableValue height =
        video_mandatory.find(EncodableValue("minHeight"))->second;
    EncodableValue
        fps = video_mandatory.find(EncodableValue("minFrameRate"))->second;

    MediaStreamConstraints constraints;
    VideoConstraints video_constraints;

    if ((TypeIs<bool>(audio_arg) &&
        GetValue<bool>(audio_arg)) ||
        TypeIs<EncodableMap>(audio_arg)) {
      constraints.audio = true;
    } else {
      constraints.audio = false;
    }

    video_constraints.min_width = rust::String(GetValue<std::string>(width));
    video_constraints.min_height = rust::String(GetValue<std::string>(height));
    video_constraints.min_fps = rust::String(GetValue<std::string>(fps));
    constraints.video = video_constraints;

    MediaStream user_media = GetUserMedia(webrtc, constraints);

    EncodableMap params;
    params[EncodableValue("streamId")] =
        EncodableValue(user_media.stream_id.c_str());

    EncodableList video_tracks;
    if (user_media.video_tracks.size() == 0) {
      params[EncodableValue("videoTracks")] = EncodableValue(EncodableList());
    } else {
      for (size_t i = 0; i < user_media.video_tracks.size(); ++i) {
        EncodableMap info;
        info[EncodableValue("id")] =
            EncodableValue(user_media.video_tracks[i].id.c_str());
        info[EncodableValue("label")] =
            EncodableValue(user_media.video_tracks[i].label.c_str());
        info[EncodableValue("kind")] = EncodableValue(
            user_media.video_tracks[i].kind == TrackKind::Video ? "video"
                                                                : "audio");
        info[EncodableValue("enabled")] =
            EncodableValue(user_media.video_tracks[i].enabled);

        video_tracks.push_back(EncodableValue(info));
      }
    }
    params[EncodableValue("videoTracks")] = EncodableValue(video_tracks);

    EncodableList audio_tracks;
    if (user_media.audio_tracks.size() == 0) {
      params[EncodableValue("audioTracks")] = EncodableValue(EncodableList());
    } else {
      for (size_t i = 0; i < user_media.audio_tracks.size(); ++i) {
        EncodableMap info;
        info[EncodableValue("id")] =
            EncodableValue(user_media.audio_tracks[i].id.c_str());
        info[EncodableValue("label")] =
            EncodableValue(user_media.audio_tracks[i].label.c_str());
        info[EncodableValue("kind")] = EncodableValue(
            user_media.audio_tracks[i].kind == TrackKind::Video ? "video"
                                                                : "audio");
        info[EncodableValue("enabled")] =
            EncodableValue(user_media.audio_tracks[i].enabled);

        audio_tracks.push_back(EncodableValue(info));
      }
    }
    params[EncodableValue("audioTracks")] = EncodableValue(audio_tracks);

    result->Success(EncodableValue(params));
  } else if (method.compare("getDisplayMedia") == 0) {
  } else if (method.compare("mediaStreamGetTracks") == 0) {
  } else if (method.compare("createOffer") == 0) {
  } else if (method.compare("createAnswer") == 0) {
  } else if (method.compare("addStream") == 0) {
  } else if (method.compare("removeStream") == 0) {
  } else if (method.compare("setLocalDescription") == 0) {
  } else if (method.compare("setRemoteDescription") == 0) {
  } else if (method.compare("addCandidate") == 0) {
  } else if (method.compare("getStats") == 0) {
  } else if (method.compare("createDataChannel") == 0) {
  } else if (method.compare("dataChannelSend") == 0) {
  } else if (method.compare("dataChannelClose") == 0) {
  } else if (method.compare("streamDispose") == 0) {
    const EncodableMap
        params = GetValue<EncodableMap>(*method_call.arguments());
    const std::string stream_id = findString(params, "streamId");
    DisposeStream(webrtc, rust::String(stream_id));
    result->Success();
  } else if (method.compare("mediaStreamTrackSetEnable") == 0) {
  } else if (method.compare("trackDispose") == 0) {
  } else if (method.compare("peerConnectionClose") == 0) {
  } else if (method.compare("peerConnectionDispose") == 0) {
  } else if (method.compare("createVideoRenderer") == 0) {
  } else if (method.compare("videoRendererDispose") == 0) {
  } else if (method.compare("videoRendererSetSrcObject") == 0) {
  } else if (method.compare("setVolume") == 0) {
  } else if (method.compare("getLocalDescription") == 0) {
  } else if (method.compare("getRemoteDescription") == 0) {
  } else if (method.compare("mediaStreamAddTrack") == 0) {
  } else if (method.compare("mediaStreamRemoveTrack") == 0) {
  } else if (method.compare("addTrack") == 0) {
  } else if (method.compare("removeTrack") == 0) {
  } else if (method.compare("addTransceiver") == 0) {
  } else if (method.compare("getTransceivers") == 0) {
  } else if (method.compare("getReceivers") == 0) {
  } else if (method.compare("getSenders") == 0) {
  } else if (method.compare("rtpSenderDispose") == 0) {
  } else if (method.compare("rtpSenderSetTrack") == 0) {
  } else if (method.compare("rtpSenderReplaceTrack") == 0) {
  } else if (method.compare("rtpSenderSetParameters") == 0) {
  } else if (method.compare("rtpTransceiverStop") == 0) {
  } else if (method.compare("rtpTransceiverSetDirection") == 0) {
  } else if (method.compare("setConfiguration") == 0) {
  } else if (method.compare("captureFrame") == 0) {
  } else {
    result->NotImplemented();
  }
}

}  // namespace flutter_webrtc_plugin
