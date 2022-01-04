import 'dart:async';
import 'dart:js_util' as jsutil;

import 'package:flutter/services.dart';

import '../interface/enums.dart';
import '../interface/rtc_rtp_parameters.dart';
import '../interface/rtc_rtp_receiver.dart';
import '../interface/rtc_rtp_sender.dart';
import '../interface/rtc_rtp_transceiver.dart';
import 'rtc_rtp_receiver_impl.dart';
import 'rtc_rtp_sender_impl.dart';

List<RTCRtpEncoding> listToRtpEncodings(List<Map<String, dynamic>> list) {
  return list.map((e) => RTCRtpEncoding.fromMap(e)).toList();
}

@Deprecated('RTCRtpTransceiverInitWeb isn\'t referenced from anywhere.')
class RTCRtpTransceiverInitWeb extends RTCRtpTransceiverInit {
  RTCRtpTransceiverInitWeb(TransceiverDirection direction, List<RTCRtpEncoding> sendEncodings)
      : super(
            direction: direction,
            sendEncodings: sendEncodings);

  factory RTCRtpTransceiverInitWeb.fromMap(Map<dynamic, dynamic> map) {
    if (map['direction'] == null) {
      throw Exception('You must provide a direction');
    }
    if (map['streams'] == null) {
      throw Exception('You must provide the streams');
    }

    return RTCRtpTransceiverInitWeb(
        typeStringToRtpTransceiverDirection[map['direction']]!,
        listToRtpEncodings(map['sendEncodings']));
  }

  Map<String, dynamic> toMap() => {
        'direction': typeRtpTransceiverDirectionToString[direction],
        if (sendEncodings != null)
          'sendEncodings': sendEncodings!.map((e) => e.toMap()).toList(),
      };
}

extension RTCRtpTransceiverInitWebExt on RTCRtpTransceiverInit {
  dynamic toJsObject() => jsutil.jsify({
        'direction': typeRtpTransceiverDirectionToString[direction],
        if (sendEncodings != null)
          'sendEncodings': sendEncodings!.map((e) => e.toMap()).toList(),
      });
}

class RTCRtpTransceiverWeb extends RTCRtpTransceiver {
  RTCRtpTransceiverWeb(this._jsTransceiver, _peerConnectionId);

  factory RTCRtpTransceiverWeb.fromJsObject(Object jsTransceiver,
      {String? peerConnectionId}) {
    var transceiver = RTCRtpTransceiverWeb(jsTransceiver, peerConnectionId);
    return transceiver;
  }

  Object _jsTransceiver;

  @override
  Future<TransceiverDirection?> getCurrentDirection() async =>
      typeStringToRtpTransceiverDirection[
          jsutil.getProperty(_jsTransceiver, 'currentDirection')];

  @override
  Future<TransceiverDirection> getDirection() async =>
      typeStringToRtpTransceiverDirection[
          jsutil.getProperty(_jsTransceiver, 'direction')]!;

  @override
  String get mid => jsutil.getProperty(_jsTransceiver, 'mid');

  @override
  RTCRtpSender get sender => RTCRtpSenderWeb.fromJsSender(
      jsutil.getProperty(_jsTransceiver, 'sender'));

  @override
  RTCRtpReceiver get receiver =>
      RTCRtpReceiverWeb(jsutil.getProperty(_jsTransceiver, 'receiver'));

  @override
  bool get stoped => jsutil.getProperty(_jsTransceiver, 'stopped');

  @override
  Future<void> setDirection(TransceiverDirection direction) async {
    try {
      jsutil.setProperty(_jsTransceiver, 'direction',
          typeRtpTransceiverDirectionToString[direction]);
    } on PlatformException catch (e) {
      throw 'Unable to RTCRtpTransceiver::setDirection: ${e.message}';
    }
  }

  @override
  Future<void> sync() async {
    // Web implementation doesn't need sync method.
  }

  @override
  Future<void> stop() async {
    try {
      jsutil.callMethod(_jsTransceiver, 'stop', []);
    } on PlatformException catch (e) {
      throw 'Unable to RTCRtpTransceiver::stop: ${e.message}';
    }
  }
}
