import 'package:flutter/services.dart';

import 'package:medea_flutter_webrtc/src/api/bridge.g.dart' as ffi;
import '/src/platform/track.dart';
import 'channel.dart';
import 'peer.dart';

/// [RTCSender][1] implementation.
///
/// [1]: https://w3.org/TR/webrtc#dom-rtcrtpsender
abstract class RtpSender {
  /// Creates an [RtpSender] basing on the [Map] received from the native side.
  static RtpSender fromMap(dynamic map) {
    return _RtpSenderChannel.fromMap(map);
  }

  /// Create a new [RtpSender] from the provided [peerId] and [transceiverId].
  static RtpSender fromFFI(
      ffi.ArcPeerConnection peer, ffi.ArcRtpTransceiver transceiver) {
    return _RtpSenderFFI(peer, transceiver);
  }

  /// Current [MediaStreamTrack] of this [RtpSender].
  MediaStreamTrack? _track;

  /// Getter for the [MediaStreamTrack] currently owned by this [RtpSender].
  MediaStreamTrack? get track => _track;

  /// Replaces [MediaStreamTrack] of this [RtpSender].
  Future<void> replaceTrack(MediaStreamTrack? t);

  /// Disposes this [RtpSender].
  Future<void> dispose();
}

/// [MethodChannel]-based implementation of a [RtpSender].
class _RtpSenderChannel extends RtpSender {
  /// Creates an [RtpSender] basing on the [Map] received from the native side.
  _RtpSenderChannel.fromMap(dynamic map) {
    _chan = methodChannel('RtpSender', map['channelId']);
  }

  /// [MethodChannel] used for the messaging with the native side.
  late MethodChannel _chan;

  @override
  Future<void> replaceTrack(MediaStreamTrack? t) async {
    _track = t;
    await _chan.invokeMethod('replaceTrack', {'trackId': t?.id()});
  }

  @override
  Future<void> dispose() async {
    await _chan.invokeMethod('dispose');
  }
}

/// FFI-based implementation of a [RtpSender].
class _RtpSenderFFI extends RtpSender {
  /// RustOpaque of the native side peer.
  final ffi.ArcPeerConnection _peer;

  /// RustOpaque of the native side transceiver.
  final ffi.ArcRtpTransceiver _transceiver;

  _RtpSenderFFI(this._peer, this._transceiver);

  @override
  Future<void> replaceTrack(MediaStreamTrack? t) async {
    await api!.senderReplaceTrack(
        peer: _peer, transceiver: _transceiver, trackId: t?.id());
  }

  @override
  Future<void> dispose() async {
    // no-op for FFI implementation
  }
}
