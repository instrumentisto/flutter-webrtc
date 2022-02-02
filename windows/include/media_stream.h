#pragma once

using namespace rust::cxxbridge1;
using namespace flutter;

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#define DEFAULT_FPS 30

namespace flutter_webrtc_plugin {

  // Calls Rust `EnumerateDevices()` and converts the received Rust vector of
  // `MediaDeviceInfo` info for Dart.
  void EnumerateDevice(
      rust::Box<Webrtc>& webrtc,
      std::unique_ptr<MethodResult<EncodableValue>> result);

  // Parses the received constraints from Dart and passes them to Rust
  // `GetUserMedia()`, then converts the backed `MediaStream` info for Dart.
  void GetUserMedia(
      const flutter::MethodCall<EncodableValue>& method_call,
      Box<Webrtc>& webrtc,
      std::unique_ptr<MethodResult<EncodableValue>> result);

  // Disposes some media stream calling Rust `DisposeStream`.
  void DisposeStream(
      const flutter::MethodCall<EncodableValue>& method_call,
      Box<Webrtc>& webrtc,
      std::unique_ptr<MethodResult<EncodableValue>> result);

  // Parses video constraints received from Dart to Rust `VideoConstraints`.
  VideoConstraints ParseVideoConstraints(const EncodableValue video_arg);

  // Parses audio constraints received from Dart to Rust `AudioConstraints`.
  AudioConstraints ParseAudioConstraints(const EncodableValue audio_arg);

  // Converts Rust `VideoConstraints` or `AudioConstraints` to `EncodableList`
  // for passing to Dart according to `TrackKind`.
  EncodableList GetParams(TrackKind type, MediaStream& user_media);

}  // namespace flutter_webrtc_plugin
