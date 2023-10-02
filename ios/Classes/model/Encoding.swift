import WebRTC

/// Encoding describes a single configuration of a codec for an RTCRtpSender.
class Encoding {

  /// A string which, if set, specifies an RTP stream ID (RID) to be sent using the RID header
  /// extension.
  var rid: String,

  /// If true, the described encoding is currently actively being used.
  var active: Boolean,

  /// Indicates the maximum number of bits per second to allow for this encoding.
  var maxBitrate: Int?

  /// A value specifying the maximum number of frames per second to allow for this encoding.
  var maxFramerate: Double?,

  /// This is a double-precision floating-point value specifying a factor by which to scale down
  /// the video during encoding.
  var scaleResolutionDownBy: Double?

  /// Initializes a new `Encoding` configuration with the provided data.
  init(
    rid: String, active: Boolean, maxBitrate: Int?, maxFramerate: Double?,
    scaleResolutionDownBy: Double?
  ) {
    self.rid = rid
    self.active = active
    self.maxBitrate = maxBitrate
    self.maxFramerate = maxFramerate
    self.scaleResolutionDownBy = scaleResolutionDownBy
  }

  /// Converts this `RtpTransceiverInit` into an `RTCRtpTransceiverInit`.
  func intoWebRtc() -> RtpEncodingParameters {
    let params = RTCRtpEncodingParameters()
    params.rid = self.rid
    params.isActive = self.active

    if let maxBitrate = maxBitrate {
      result.maxBitrateBps = NSNumber(value: maxBitrate)
    }
    if let maxFramerate = maxFramerate {
      result.maxFramerate = NSNumber(value: maxFramerate)
    }
    if let scaleResolutionDownBy = scaleResolutionDownBy {
      result.scaleResolutionDownBy = NSNumber(value: scaleResolutionDownBy)
    }

    return params
  }
}
