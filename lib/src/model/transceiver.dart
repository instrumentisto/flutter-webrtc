import '/src/api/bridge.g.dart' as ffi;
import '/src/api/peer.dart';

/// Direction of an `RtpTransceiver`.
enum TransceiverDirection {
  /// Indicates that the transceiver is both sending to and receiving from the
  /// remote peer connection.
  sendRecv,

  /// Indicates that the transceiver is sending to the remote peer, but is not
  /// receiving any media from the remote peer.
  sendOnly,

  /// Indicates that the transceiver is receiving from the remote peer, but is
  /// not sending any media to the remote peer.
  recvOnly,

  /// Indicates that the transceiver is inactive, neither sending nor receiving
  /// any media data.
  inactive,

  /// The transceiver will neither send, nor receive RTP. It will generate a
  /// zero port in the offer.
  stopped,
}

/// Init config for an `RtpTransceiver` creation.
class RtpTransceiverInit {
  /// Creates a new [RtpTransceiverInit] config with the provided
  /// [TransceiverDirection].
  RtpTransceiverInit(this.direction);

  /// Direction of an `RtpTransceiver` which will be created from this config.
  late TransceiverDirection direction;

  /// [SendEncodingParameters] of an `RtpTransceiver` which will be created from
  /// this config.
  late List<SendEncodingParameters> sendEncodings = List.empty(growable: true);

  /// Converts this model to the [Map] expected by Flutter.
  Map<String, dynamic> toMap() {
    return {
      'direction': direction.index,
      'sendEncodings':
          sendEncodings.map((encoding) => encoding.toMap()).toList()
    };
  }
}

/// Encoding describing a single configuration of a codec for an RTCRtpSender.
class SendEncodingParameters {
  /// String which, if set, specifies an RTP stream ID (RID) to be sent using
  /// the RID header extension.
  SendEncodingParameters(this.rid, this.active, {this.maxBitrate, this.maxFramerate, this.scalabilityMode, this.scaleResolutionDownBy});

  /// String which, if set, specifies an RTP stream ID (RID) to be sent using
  /// the RID header extension.
  late String rid;

  /// If true, the described encoding is currently actively being used.
  late bool active;

  /// Indicator of the maximum number of bits per second to allow for this
  /// encoding.
  int? maxBitrate;

  /// Value specifying the maximum number of frames per second to allow for
  /// this encoding.
  double? maxFramerate;

  /// Double-precision floating-point value specifying a factor by which to
  /// scale down the video during encoding.
  double? scaleResolutionDownBy;

  /// Scalability mode describes layers within the media stream.
  String? scalabilityMode;

  static SendEncodingParameters fromFFI(ffi.RtcRtpEncodingParameters e) {
    return _SendEncodingParametersFFI(e.parameters, e.rid, e.active,
          maxBitrate: e.maxBitrate,
          maxFramerate: e.maxFramerate,
          scalabilityMode: e.scalabilityMode,
          scaleResolutionDownBy: e.scaleResolutionDownBy);
  }

  /// Converts this model to the [Map] expected by Flutter.
  Map<String, dynamic> toMap() {
    return {
      'rid': rid,
      'active': active,
      'maxBitrate': maxBitrate,
      'maxFramerate': maxFramerate,
      'scaleResolutionDownBy': scaleResolutionDownBy,
      'scalabilityMode': scalabilityMode
    };
  }

  Future<ffi.ArcRtpEncodingParameters?> toFFI() async {
    return null; // no-op
  }
}

class _SendEncodingParametersFFI extends SendEncodingParameters {
  _SendEncodingParametersFFI(this._encoding, String rid, bool active,
      {int? maxBitrate,
      double? maxFramerate,
      String? scalabilityMode,
      double? scaleResolutionDownBy})
      : super(rid, active,
            maxBitrate: maxBitrate,
            maxFramerate: maxFramerate,
            scalabilityMode: scalabilityMode,
            scaleResolutionDownBy: scaleResolutionDownBy);

  final ffi.ArcRtpEncodingParameters _encoding;

  @override
  Future<ffi.ArcRtpEncodingParameters?> toFFI() async {
    await api!.updateEncodingParameters(encoding: _encoding, active: super.active, maxBitrate: super.maxBitrate, maxFramerate: super.maxFramerate, scalabilityMode: super.scalabilityMode, scaleResolutionDownBy: super.scaleResolutionDownBy);

    return _encoding;
  }
}
