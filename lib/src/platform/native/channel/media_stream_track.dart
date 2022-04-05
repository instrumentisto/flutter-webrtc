import 'dart:async';

import 'package:flutter/services.dart';

import '../../../../flutter_webrtc.dart';
import '/src/api/channel/channel.dart';

/// Representation of a single media unit.
class NativeMediaStreamTrack extends MediaStreamTrack {
  /// Creates a [NativeMediaStreamTrack] basing on the [Map] received from the
  /// native side.
  NativeMediaStreamTrack.from(dynamic map) {
    var channelId = map['channelId'];
    _chan = methodChannel('MediaStreamTrack', channelId);
    _eventChan = eventChannel('MediaStreamTrackEvent', channelId);
    _eventSub = _eventChan.receiveBroadcastStream().listen(eventListener);
    _id = map['id'];
    _deviceId = map['deviceId'];
    _kind = MediaKind.values[map['kind']];
  }

  /// [MethodChannel] used for the messaging with a native side.
  late MethodChannel _chan;

  /// [EventChannel] to receive all the [PeerConnection] events from.
  late EventChannel _eventChan;

  /// Indicates whether this [NativeMediaStreamTrack] transmits media.
  ///
  /// If it's `false` then blank (black screen for video and `0dB` for audio)
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

  /// [ended][1] event subscriber.
  ///
  /// [1]: https://w3.org/TR/mediacapture-streams#event-mediastreamtrack-ended
  OnEndedCallback? _onEnded;

  /// [_eventChan] subscription to the [PeerConnection] events.
  late StreamSubscription<dynamic>? _eventSub;

  /// Listener for all the [MediaStreamTrack] events received from the native
  /// side.
  void eventListener(dynamic event) {
    final dynamic e = event;
    switch (e['event']) {
      case 'onEnded':
        _onEnded?.call();
        break;
    }
  }

  @override
  void onEnded(OnEndedCallback cb) {
    _onEnded = cb;
  }

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
    await _chan.invokeMethod('setEnabled', {'enabled': enabled});
    _enabled = enabled;
  }

  @override
  Future<void> stop() async {
    await _chan.invokeMethod('stop');
  }

  @override
  Future<void> dispose() async {
    await _eventSub?.cancel();
  }

  @override
  Future<MediaStreamTrack> clone() async {
    return NativeMediaStreamTrack.from(await _chan.invokeMethod('clone'));
  }
}
