import 'package:flutter/services.dart';

import 'package:medea_flutter_webrtc/src/model/track.dart';
import '../model/capability.dart';
import 'bridge/api.dart' as ffi;
import 'channel.dart';
import 'peer.dart';

/// [MethodChannel] used for the messaging with a native side.
final _peerConnectionFactoryMethodChannel =
    methodChannel('PeerConnectionFactory', 0);

/// [RTCRtpReceiver][1] implementation.
///
/// [1]: https://www.w3.org/TR/webrtc/#dom-rtcrtpreceiver
abstract class RtpReceiver {
  /// [RtpCapabilities] of an RTP receiver of the specified [MediaKind].
  static Future<RtpCapabilities> getCapabilities(MediaKind kind) {
    if (isDesktop) {
      return _RtpReceiverFFI.getCapabilities(kind);
    } else {
      return _RtpReceiverChannel.getCapabilities(kind);
    }
  }
}

/// [MethodChannel]-based implementation of a [RtpReceiver].
class _RtpReceiverChannel extends RtpReceiver {
  /// [RtpCapabilities] of an RTP receiver of the specified [MediaKind].
  static Future<RtpCapabilities> getCapabilities(MediaKind kind) async {
    var map = await _peerConnectionFactoryMethodChannel
        .invokeMethod('getRtpReceiverCapabilities', {'kind': kind.index});
    return RtpCapabilities.fromMap(map);
  }
}

/// FFI-based implementation of a [RtpReceiver].
class _RtpReceiverFFI extends RtpReceiver {
  /// [RtpCapabilities] of an RTP receiver of the specified [MediaKind].
  static Future<RtpCapabilities> getCapabilities(MediaKind kind) async {
    return RtpCapabilities.fromFFI(await ffi.getRtpReceiverCapabilities(
        kind: ffi.MediaType.values[kind.index]));
  }
}
