import 'package:flutter/services.dart';

import 'package:medea_flutter_webrtc/src/api/bridge.g.dart';
import 'bridge.g.dart' as ffi;
import 'send_encoding_parameters.dart';

/// [RtpParameters][1] implementation.
///
/// [1]: https://w3.org/TR/webrtc/#dom-rtcrtpparameters
abstract class RtpParameters {
  /// Create a new [RtpParameters] from the provided [ArcRtpParameters].
  static fromFFI(ffi.RtcRtpSendParameters params) {
    return _RtpParametersFFI(params);
  }

  /// Create a new [RtpParameters] from the provided [MethodChannel].
  static fromMap(dynamic map) {
    return _RtpParametersChannel.fromMap(map);
  }

  /// The [SendEncodingParameters] which has been set.
  late List<SendEncodingParameters> encodings;

  /// Tries to convert this [RtpParameters] into [ffi.ArcRtpParameters].
  ffi.RtcRtpSendParameters toFFI();

  /// Converts this [RtpParameters] to the [Map] expected by Flutter.
  Map<String, dynamic> toMap();
}

/// [MethodChannel]-based implementation of a [RtpParameters].
class _RtpParametersChannel extends RtpParameters {
  _RtpParametersChannel.fromMap(dynamic map) {
    encodings = List.unmodifiable(map!['encodings']
        .map((e) => SendEncodingParameters.fromMap(e))
        .toList());
  }

  @override
  Map<String, dynamic> toMap() {
    return {
      'encodings': encodings.map((e) => e.toMap()).toList(),
    };
  }

  @override
  ffi.RtcRtpSendParameters toFFI() {
    throw UnimplementedError();
  }
}

/// FFI-based implementation of a [RtpParameters].
class _RtpParametersFFI extends RtpParameters {
  _RtpParametersFFI(ffi.RtcRtpSendParameters params) {
    _inner = params.inner;
    encodings = List.unmodifiable(params.encodings
        .map((e) => SendEncodingParameters.fromFFI(e.$1, e.$2))
        .toList());
  }

  /// Reference to the Rust side [RtpParameters].
  late ArcRtpParameters _inner;

  @override
  ffi.RtcRtpSendParameters toFFI() {
    return ffi.RtcRtpSendParameters(
        encodings: encodings.map((e) {
          var r = e.toFFI();
          return (r.$1, r.$2!);
        }).toList(),
        inner: _inner);
  }

  @override
  Map<String, dynamic> toMap() {
    throw UnimplementedError();
  }
}
