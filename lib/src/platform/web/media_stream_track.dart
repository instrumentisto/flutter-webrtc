import 'dart:async';
import 'dart:html' as html;

import '../../model/constraints.dart';
import '/src/model/track.dart';
import '/src/platform/track.dart';

// ignore_for_file: avoid_web_libraries_in_flutter

class WebMediaStreamTrack extends MediaStreamTrack {
  WebMediaStreamTrack(this.jsTrack);

  final html.MediaStreamTrack jsTrack;

  @override
  String deviceId() {
    return jsTrack.getSettings()['deviceId']!;
  }

  @override
  String id() {
    return jsTrack.id!;
  }

  @override
  void onEnded(OnEndedCallback cb) {
    jsTrack.onEnded.listen((event) {
      cb.call();
    });
  }

  @override
  void onAudioLevelChanged(OnAudioLevelChangedCallback? cb) {
    // TODO(review): is it? there is audio level in media-source stats and
    //               audio node and AnalyserNode
    //               https://webrtc.github.io/samples/src/content/getusermedia/volume/
    throw 'onAudioLevelChanged callback is not supported on web';
  }

  @override
  bool isEnabled() {
    return jsTrack.enabled ?? false;
  }

  @override
  MediaKind kind() {
    var jsKind = jsTrack.kind;
    if (jsKind == 'audio') {
      return MediaKind.audio;
    } else {
      return MediaKind.video;
    }
  }

  @override
  Future<MediaStreamTrackState> state() {
    return Future.value(jsTrack.readyState == 'live'
        ? MediaStreamTrackState.live
        : MediaStreamTrackState.ended);
  }

  @override
  Future<void> setEnabled(bool enabled) async {
    jsTrack.enabled = enabled;
  }

  @override
  Future<void> stop() async {
    jsTrack.stop();
  }

  @override
  Future<void> dispose() async {}

  @override
  Future<MediaStreamTrack> clone() async {
    return WebMediaStreamTrack(jsTrack.clone());
  }

  @override
  FacingMode? facingMode() {
    var settings = jsTrack.getSettings();
    String? facingMode = settings['facingMode'];
    if (facingMode != null) {
      return FacingMode.values
          .firstWhere((element) => element.name.toLowerCase() == facingMode);
    }
    return null;
  }

  @override
  Future<int?> height() {
    var settings = jsTrack.getSettings();
    return settings['height'];
  }

  @override
  Future<int?> width() {
    var settings = jsTrack.getSettings();
    return settings['width'];
  }
}
