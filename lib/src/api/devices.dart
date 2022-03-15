import 'dart:io';

import 'package:flutter/services.dart';

import '/src/model/constraints.dart';
import '/src/model/device.dart';
import '/src/platform/native/media_stream_track.dart';
import 'channel.dart';

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
  Future<List<MediaDeviceInfo>> mdInfo;

  if (Platform.isAndroid || Platform.isIOS) {
    mdInfo = _enumerateDevicesChannel();
  } else {
    mdInfo = _enumerateDevicesFFI();
  }

  return mdInfo;
}

Future<List<MediaDeviceInfo>> _enumerateDevicesChannel() async {
  List<dynamic> res =
      await _mediaDevicesMethodChannel.invokeMethod('enumerateDevices');
  return res.map((i) => MediaDeviceInfo.fromMap(i)).toList();
}

Future<List<MediaDeviceInfo>> _enumerateDevicesFFI() async {
  // TODO: implement enumerateDevices
  throw UnimplementedError();
}

/// Returns list of local audio and video [NativeMediaStreamTrack]s based on
/// the provided [DeviceConstraints].
Future<List<NativeMediaStreamTrack>> getUserMedia(
    DeviceConstraints constraints) async {
  Future<List<NativeMediaStreamTrack>> nativeTrack;

  if (Platform.isAndroid || Platform.isIOS) {
    nativeTrack = _getUserMediaChannel(constraints);
  } else {
    nativeTrack = _getUserMediaFFI(constraints);
  }

  return nativeTrack;
}

Future<List<NativeMediaStreamTrack>> _getUserMediaChannel(
    DeviceConstraints constraints) async {
  try {
    List<dynamic> res = await _mediaDevicesMethodChannel
        .invokeMethod('getUserMedia', {'constraints': constraints.toMap()});
    return res.map((t) => NativeMediaStreamTrack.fromMap(t)).toList();
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
  // TODO: implement getUserMedia
  throw UnimplementedError();
}

/// Returns list of local display [NativeMediaStreamTrack]s based on the
/// provided [DisplayConstraints].
Future<List<NativeMediaStreamTrack>> getDisplayMedia(
    DisplayConstraints constraints) async {
  Future<List<NativeMediaStreamTrack>> nativeTrack;

  if (Platform.isAndroid || Platform.isIOS) {
    nativeTrack = _getDisplayMediaChannel(constraints);
  } else {
    nativeTrack = _getDisplayMediaFFI(constraints);
  }

  return nativeTrack;
}

Future<List<NativeMediaStreamTrack>> _getDisplayMediaChannel(
    DisplayConstraints constraints) async {
  List<dynamic> res = await _mediaDevicesMethodChannel
      .invokeMethod('getDisplayMedia', {'constraints': constraints.toMap()});
  return res.map((t) => NativeMediaStreamTrack.fromMap(t)).toList();
}

Future<List<NativeMediaStreamTrack>> _getDisplayMediaFFI(
    DisplayConstraints constraints) async {
  // TODO: implement getDisplayMedia
  throw UnimplementedError();
}

/// Switches the current output audio device to the provided [deviceId].
///
/// List of output audio devices may be obtained via [enumerateDevices].
Future<void> setOutputAudioId(String deviceId) async {
  if (Platform.isAndroid || Platform.isIOS) {
    _setOutputAudioIdChannel(deviceId);
  } else {
    _setOutputAudioIdFFI(deviceId);
  }
}

Future<void> _setOutputAudioIdChannel(String deviceId) async {
  await _mediaDevicesMethodChannel
      .invokeMethod('setOutputAudioId', {'deviceId': deviceId});
}

Future<void> _setOutputAudioIdFFI(String deviceId) async {
  // TODO: implement setOutputAudioId
  throw UnimplementedError();
}
