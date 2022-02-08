import 'package:flutter/services.dart';

import 'package:flutter_webrtc/src/api/utils/channel_name_generator.dart';
import 'package:flutter_webrtc/src/model/media_kind.dart';
import '../media_stream_track.dart';

/// Repesentation of the one media unit.
class NativeMediaStreamTrack extends MediaStreamTrack {
  /// Creates [NativeMediaStreamTrack] based on the [Map] received
  /// from the native side.
  NativeMediaStreamTrack.fromMap(dynamic map) {
    _methodChannel =
        MethodChannel(channelNameWithId('MediaStreamTrack', map['channelId']));
    _id = map['id'];
    _deviceId = map['deviceId'];
    _kind = MediaKind.values[map['kind']];
  }

  /// Indicates that this [NativeMediaStreamTrack] transmits media.
  ///
  /// If it's `false` then blank (black screen for video and 0dB for audio)
  /// media will be transmitted.
  bool _enabled = true;

  /// Unique ID of this [NativeMediaStreamTrack].
  late String _id;

  /// [MediaKind] of this [NativeMediaStreamTrack].
  late MediaKind _kind;

  /// Unique ID of the device from which this [NativeMediaStreamTrack] was
  /// created.
  ///
  /// "remote" - for the remove tracks.
  late String _deviceId;

  /// [MethodChannel] used for the messaging with a native side.
  late MethodChannel _methodChannel;

  @override
  String id() {
    return _id;
  }

  @override
  MediaKind kind() {
    return _kind;
  }

  @override
  String deviceId() {
    return _deviceId;
  }

  @override
  bool isEnabled() {
    return _enabled;
  }

  @override
  Future<void> setEnabled(bool enabled) async {
    await _methodChannel.invokeMethod('setEnabled', {'enabled': enabled});
    _enabled = enabled;
  }

  @override
  Future<void> stop() async {
    await _methodChannel.invokeMethod('stop');
  }

  // TODO(evdokimovs): implement disposing for native side
  @override
  Future<void> dispose() async {}

  @override
  Future<MediaStreamTrack> clone() async {
    return NativeMediaStreamTrack.fromMap(
        await _methodChannel.invokeMethod('clone'));
  }
}
