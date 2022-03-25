import 'package:flutter/services.dart';

import '/src/model/constraints.dart';
import '/src/model/device.dart';
import '/src/platform/native/media_stream_track.dart';
import 'bridge.g.dart' as ffi;
import 'channel.dart';
import 'peer.dart';

const DEFAULT_USER_WIDTH = 480;
const DEFAULT_USER_HEIGHT = 640;
const DEFAULT_DISPLAY_WIDTH = 1920;
const DEFAULT_DISPLAY_HEIGHT = 1080;
const DEFAULT_FPS = 30;

typedef OnDeviceChangeHandler = void Function();

class DeviceHandler {
  static final DeviceHandler _instance = DeviceHandler._internal();

  factory DeviceHandler() {
    return _instance;
  }

  DeviceHandler._internal() {
    _listen();
  }

  void _listen() async {
    api.setOnDeviceChanged().listen(
      (event) {
        if (_handler != null) {
          _handler!();
        }
      },
    );
  }

  void setHandler(OnDeviceChangeHandler? handler) {
    _handler = handler;
  }

  OnDeviceChangeHandler? _handler;
}

/// [Exception] thrown if the specified constraints resulted in no candidate
/// devices which met the criteria requested. The error is an object of type
/// [OverconstrainedException], and has a constraint property whose string value
/// is the name of a constraint which was impossible to meet.
class OverconstrainedException implements Exception {
  /// Constructs a new [OverconstrainedException].
  OverconstrainedException();

  @override
  String toString() {
    return 'OverconstrainedException';
  }
}

/// [MethodChannel] used for the messaging with a native side.
final _mediaDevicesMethodChannel = methodChannel('MediaDevices', 0);

/// Returns list of [MediaDeviceInfo]s for the currently available devices.
Future<List<MediaDeviceInfo>> enumerateDevices() async {
  if (IS_DESKTOP) {
    return await _enumerateDevicesFFI();
  } else {
    return await _enumerateDevicesChannel();
  }
}

Future<List<MediaDeviceInfo>> _enumerateDevicesChannel() async {
  return (await _mediaDevicesMethodChannel.invokeMethod('enumerateDevices'))
      .map((i) => MediaDeviceInfo.fromMap(i))
      .toList();
}

Future<List<MediaDeviceInfo>> _enumerateDevicesFFI() async {
  return (await api.enumerateDevices())
      .map((e) => MediaDeviceInfo.fromFFI(e))
      .toList();
}

/// Returns list of local audio and video [NativeMediaStreamTrack]s based on
/// the provided [DeviceConstraints].
Future<List<NativeMediaStreamTrack>> getUserMedia(
    DeviceConstraints constraints) async {
  if (IS_DESKTOP) {
    return _getUserMediaFFI(constraints);
  } else {
    return _getUserMediaChannel(constraints);
  }
}

Future<List<NativeMediaStreamTrack>> _getUserMediaChannel(
    DeviceConstraints constraints) async {
  try {
    List<dynamic> res = await _mediaDevicesMethodChannel
        .invokeMethod('getUserMedia', {'constraints': constraints.toMap()});
    return res.map((t) => NativeMediaStreamTrack.from(t)).toList();
  } on PlatformException catch (e) {
    if (e.code == 'OverconstrainedError') {
      throw OverconstrainedException();
    } else {
      rethrow;
    }
  }
}

Future<List<NativeMediaStreamTrack>> _getUserMediaFFI(
    DeviceConstraints constraints) async {
  var audioConstraints = constraints.audio.mandatory != null
      ? ffi.AudioConstraints(
          deviceId: constraints.audio.mandatory?.deviceId ?? '')
      : null;

  var videoConstraints = constraints.video.mandatory != null
      ? ffi.VideoConstraints(
          deviceId: constraints.video.mandatory?.deviceId ?? '',
          height: constraints.video.mandatory?.height ?? DEFAULT_USER_HEIGHT,
          width: constraints.video.mandatory?.width ?? DEFAULT_USER_WIDTH,
          frameRate: constraints.video.mandatory?.fps ?? DEFAULT_FPS,
          isDisplay: false)
      : null;

  var tracks = await api.getMedia(
      constraints: ffi.MediaStreamConstraints(
          audio: audioConstraints, video: videoConstraints));

  return tracks.map((e) => NativeMediaStreamTrack.from(e)).toList();
}

/// Returns list of local display [NativeMediaStreamTrack]s based on the
/// provided [DisplayConstraints].
Future<List<NativeMediaStreamTrack>> getDisplayMedia(
    DisplayConstraints constraints) async {
  if (IS_DESKTOP) {
    return _getDisplayMediaFFI(constraints);
  } else {
    return _getDisplayMediaChannel(constraints);
  }
}

Future<List<NativeMediaStreamTrack>> _getDisplayMediaChannel(
    DisplayConstraints constraints) async {
  List<dynamic> res = await _mediaDevicesMethodChannel
      .invokeMethod('getDisplayMedia', {'constraints': constraints.toMap()});
  return res.map((t) => NativeMediaStreamTrack.from(t)).toList();
}

Future<List<NativeMediaStreamTrack>> _getDisplayMediaFFI(
    DisplayConstraints constraints) async {
  var audioConstraints = constraints.audio.mandatory != null
      ? ffi.AudioConstraints(
          deviceId: constraints.audio.mandatory?.deviceId ?? '')
      : null;

  var videoConstraints = constraints.video.mandatory != null
      ? ffi.VideoConstraints(
          deviceId: constraints.video.mandatory?.deviceId ?? '',
          height: constraints.video.mandatory?.height ?? DEFAULT_DISPLAY_HEIGHT,
          width: constraints.video.mandatory?.width ?? DEFAULT_DISPLAY_WIDTH,
          frameRate: constraints.video.mandatory?.fps ?? DEFAULT_FPS,
          isDisplay: true)
      : null;

  var tracks = await api.getMedia(
      constraints: ffi.MediaStreamConstraints(
          audio: audioConstraints, video: videoConstraints));

  return tracks.map((e) => NativeMediaStreamTrack.from(e)).toList();
}

/// Switches the current output audio device to the provided [deviceId].
///
/// List of output audio devices may be obtained via [enumerateDevices].
Future<void> setOutputAudioId(String deviceId) async {
  if (IS_DESKTOP) {
    _setOutputAudioIdFFI(deviceId);
  } else {
    _setOutputAudioIdChannel(deviceId);
  }
}

Future<void> _setOutputAudioIdChannel(String deviceId) async {
  await _mediaDevicesMethodChannel
      .invokeMethod('setOutputAudioId', {'deviceId': deviceId});
}

Future<void> _setOutputAudioIdFFI(String deviceId) async {
  await api.setAudioPlayoutDevice(deviceId: deviceId);
}
