import 'package:flutter/services.dart';

import '/src/api/utils/channel_name_generator.dart';
import '/src/model/device_constraints.dart';
import '/src/model/display_constraints.dart';
import '/src/model/media_device_info.dart';
import '/src/universal/native/media_stream_track.dart';

/// Thrown if the specified constraints resulted in no candidate devices
/// which met the criteria requested. The error is an object of type
/// [OverconstrainedException], and has a constraint property whose string
/// value is the name of a constraint which was impossible to meet.
class OverconstrainedException implements Exception {
  /// Constructs new [OverconstrainedException].
  OverconstrainedException();

  @override
  String toString() {
    return 'OverconstrainedException';
  }
}

/// [MethodChannel] used for the messaging with a native side.
const _mediaDevicesMethodChannel = MethodChannel('$channelTag/MediaDevices');

/// Returns list of [MediaDeviceInfo]s for the currently available devices.
Future<List<MediaDeviceInfo>> enumerateDevices() async {
  List<dynamic> res =
      await _mediaDevicesMethodChannel.invokeMethod('enumerateDevices');
  return res.map((i) => MediaDeviceInfo.fromMap(i)).toList();
}

/// Returns list of local audio and video [NativeMediaStreamTrack]s based on
/// the provided [DeviceConstraints].
Future<List<NativeMediaStreamTrack>> getUserMedia(
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

/// Returns list of local display [NativeMediaStreamTrack]s based on
/// the provided [DisplayConstraints].
Future<List<NativeMediaStreamTrack>> getDisplayMedia(
    DisplayConstraints constraints) async {
  List<dynamic> res = await _mediaDevicesMethodChannel
      .invokeMethod('getDisplayMedia', {'constraints': constraints.toMap()});
  return res.map((t) => NativeMediaStreamTrack.fromMap(t)).toList();
}
