import 'dart:async';
import 'dart:html' as html;
import 'dart:js' as js;
import 'dart:js_util' as jsutil;

import '../interface/enums.dart';
import '../interface/media_stream_track.dart';
import '../interface/rtc_ice_candidate.dart';
import '../interface/rtc_peerconnection.dart';
import '../interface/rtc_rtp_receiver.dart';
import '../interface/rtc_rtp_sender.dart';
import '../interface/rtc_rtp_transceiver.dart';
import '../interface/rtc_session_description.dart';
import '../interface/rtc_stats_report.dart';
import '../interface/rtc_track_event.dart';
import 'media_stream_track_impl.dart';
import 'rtc_rtp_receiver_impl.dart';
import 'rtc_rtp_sender_impl.dart';
import 'rtc_rtp_transceiver_impl.dart';

/*
 *  PeerConnection
 */
class RTCPeerConnectionWeb extends RTCPeerConnection {
  RTCPeerConnectionWeb(this._peerConnectionId, this._jsPc) {
    _jsPc.onIceCandidate.listen((iceEvent) {
      if (iceEvent.candidate != null) {
        onIceCandidate?.call(_iceFromJs(iceEvent.candidate!));
      }
    });

    _jsPc.onIceConnectionStateChange.listen((_) {
      _iceConnectionState =
          iceConnectionStateForString(_jsPc.iceConnectionState);
      onIceConnectionState?.call(_iceConnectionState!);
    });

    jsutil.setProperty(_jsPc, 'onicegatheringstatechange', js.allowInterop((_) {
      _iceGatheringState = iceGatheringStateforString(_jsPc.iceGatheringState);
      onIceGatheringState?.call(_iceGatheringState!);
    }));

    _jsPc.onSignalingStateChange.listen((_) {
      _signalingState = signalingStateForString(_jsPc.signalingState);
      onSignalingState?.call(_signalingState!);
    });

    _jsPc.onIceConnectionStateChange.listen((_) {
      _connectionState = peerConnectionStateForString(_jsPc.iceConnectionState);
      onConnectionState?.call(_connectionState!);
    });

    _jsPc.onNegotiationNeeded.listen((_) {
      onRenegotiationNeeded?.call();
    });

    _jsPc.onTrack.listen((trackEvent) {
      if (trackEvent.track != null && trackEvent.receiver != null) {
        onTrack?.call(
          RTCTrackEvent(
            track: MediaStreamTrackWeb(trackEvent.track!),
            receiver: RTCRtpReceiverWeb(trackEvent.receiver!),
            transceiver: RTCRtpTransceiverWeb.fromJsObject(
                jsutil.getProperty(trackEvent, 'transceiver')),
          ),
        );
      }
    });
  }

  final String _peerConnectionId;
  late final html.RtcPeerConnection _jsPc;
  final _configuration = <String, dynamic>{};

  RTCSignalingState? _signalingState;
  RTCIceGatheringState? _iceGatheringState;
  RTCIceConnectionState? _iceConnectionState;
  RTCPeerConnectionState? _connectionState;

  @override
  RTCSignalingState? get signalingState => _signalingState;

  @override
  RTCIceGatheringState? get iceGatheringState => _iceGatheringState;

  @override
  RTCIceConnectionState? get iceConnectionState => _iceConnectionState;

  @override
  RTCPeerConnectionState? get connectionState => _connectionState;

  @override
  Future<void> dispose() {
    _jsPc.close();
    return Future.value();
  }

  @override
  Map<String, dynamic> get getConfiguration => _configuration;

  @override
  Future<void> setConfiguration(Map<String, dynamic> configuration) {
    _configuration.addAll(configuration);

    _jsPc.setConfiguration(configuration);
    return Future.value();
  }

  @override
  Future<RTCSessionDescription> createOffer(
      [Map<String, dynamic>? constraints]) async {
    final args = constraints != null ? [jsutil.jsify(constraints)] : [];
    final desc = await jsutil.promiseToFuture<dynamic>(
        jsutil.callMethod(_jsPc, 'createOffer', args));
    return RTCSessionDescription(
        jsutil.getProperty(desc, 'sdp'), jsutil.getProperty(desc, 'type'));
  }

  @override
  Future<RTCSessionDescription> createAnswer(
      [Map<String, dynamic>? constraints]) async {
    final args = constraints != null ? [jsutil.jsify(constraints)] : [];
    final desc = await jsutil.promiseToFuture<dynamic>(
        jsutil.callMethod(_jsPc, 'createAnswer', args));
    return RTCSessionDescription(
        jsutil.getProperty(desc, 'sdp'), jsutil.getProperty(desc, 'type'));
  }

  @override
  Future<void> setLocalDescription(RTCSessionDescription description) async {
    await _jsPc.setLocalDescription(description.toMap());
  }

  @override
  Future<void> setRemoteDescription(RTCSessionDescription description) async {
    await _jsPc.setRemoteDescription(description.toMap());
  }

  @override
  Future<RTCSessionDescription?> getLocalDescription() async {
    if (null == _jsPc.localDescription) {
      return null;
    }
    return _sessionFromJs(_jsPc.localDescription);
  }

  @override
  Future<RTCSessionDescription?> getRemoteDescription() async {
    if (null == _jsPc.remoteDescription) {
      return null;
    }
    return _sessionFromJs(_jsPc.remoteDescription);
  }

  @override
  Future<void> addCandidate(RTCIceCandidate candidate) async {
    try {
      Completer completer = Completer<void>();
      var success = js.allowInterop(() => completer.complete());
      var failure = js.allowInterop((e) => completer.completeError(e));
      jsutil.callMethod(
          _jsPc, 'addIceCandidate', [_iceToJs(candidate), success, failure]);

      return completer.future;
    } catch (e) {
      print(e.toString());
    }
  }

  @override
  Future<List<StatsReport>> getStats([MediaStreamTrack? track]) async {
    var stats;
    if (track != null) {
      var jsTrack = (track as MediaStreamTrackWeb).jsTrack;
      stats = await jsutil.promiseToFuture<dynamic>(
          jsutil.callMethod(_jsPc, 'getStats', [jsTrack]));
    } else {
      stats = await _jsPc.getStats();
    }

    var report = <StatsReport>[];
    stats.forEach((key, value) {
      report.add(
          StatsReport(value['id'], value['type'], value['timestamp'], value));
    });
    return report;
  }

  @override
  Future<void> close() async {
    _jsPc.close();
    return Future.value();
  }

  //
  // utility section
  //

  RTCIceCandidate _iceFromJs(html.RtcIceCandidate candidate) => RTCIceCandidate(
        candidate.candidate,
        candidate.sdpMid,
        candidate.sdpMLineIndex,
      );

  html.RtcIceCandidate _iceToJs(RTCIceCandidate c) =>
      html.RtcIceCandidate(c.toMap());

  RTCSessionDescription _sessionFromJs(html.RtcSessionDescription? sd) =>
      RTCSessionDescription(sd?.sdp, sd?.type);

  @override
  Future<List<RTCRtpSender>> getSenders() async {
    var senders = jsutil.callMethod(_jsPc, 'getSenders', []);
    var list = <RTCRtpSender>[];
    senders.forEach((e) {
      list.add(RTCRtpSenderWeb.fromJsSender(e));
    });
    return list;
  }

  @override
  Future<List<RTCRtpReceiver>> getReceivers() async {
    var receivers = jsutil.callMethod(_jsPc, 'getReceivers', []);

    var list = <RTCRtpReceiver>[];
    receivers.forEach((e) {
      list.add(RTCRtpReceiverWeb(e));
    });

    return list;
  }

  @override
  Future<List<RTCRtpTransceiver>> getTransceivers() async {
    var transceivers = jsutil.callMethod(_jsPc, 'getTransceivers', []);

    var list = <RTCRtpTransceiver>[];
    transceivers.forEach((e) {
      list.add(RTCRtpTransceiverWeb.fromJsObject(e));
    });

    return list;
  }

  //'audio|video', { 'direction': 'recvonly|sendonly|sendrecv' }
  //
  // https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/addTransceiver
  //
  @override
  Future<RTCRtpTransceiver> addTransceiver({
    MediaStreamTrack? track,
    RTCRtpMediaType? kind,
    RTCRtpTransceiverInit? init,
  }) async {
    final jsTrack = track is MediaStreamTrackWeb ? track.jsTrack : null;
    final kindString = kind != null ? typeRTCRtpMediaTypetoString[kind] : null;
    final trackOrKind = jsTrack ?? kindString;
    assert(trackOrKind != null, 'track or kind must not be null');

    final transceiver = jsutil.callMethod(
      _jsPc,
      'addTransceiver',
      [
        trackOrKind,
        if (init != null) init.toJsObject(),
      ],
    );

    return RTCRtpTransceiverWeb.fromJsObject(
      transceiver,
      peerConnectionId: _peerConnectionId,
    );
  }
}
