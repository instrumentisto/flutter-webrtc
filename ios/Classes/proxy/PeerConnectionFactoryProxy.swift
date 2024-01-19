import VideoToolbox
import WebRTC

/// Creator of new `PeerConnectionProxy`s.
class PeerConnectionFactoryProxy {
  /// Counter for generating new [PeerConnectionProxy] IDs.
  private var lastPeerConnectionId: Int = 0

  /// All the `PeerObserver`s created by this `PeerConnectionFactoryProxy`.
  ///
  /// `PeerObserver`s will be removed on a `PeerConnectionProxy` disposal.
  private var peerObservers: [Int: PeerObserver] = [:]

  /// Underlying native factory object of this factory.
  private var factory: RTCPeerConnectionFactory

  /// Initializes a new `PeerConnectionFactoryProxy` based on the provided
  /// `State`.
  init(state: State) {
    self.factory = state.getPeerFactory()
  }

  /// Creates a new `PeerConnectionProxy` based on the provided
  /// `PeerConnectionConfiguration`.
  func create(conf: PeerConnectionConfiguration) -> PeerConnectionProxy {
    let id = self.nextId()

    let config = conf.intoWebRtc()
    let peerObserver = PeerObserver()
    let peer = self.factory.peerConnection(
      with: config,
      constraints: RTCMediaConstraints(
        mandatoryConstraints: [:],
        optionalConstraints: [:]
      ),
      delegate: peerObserver
    )
    let peerProxy = PeerConnectionProxy(id: id, peer: peer!)
    peerObserver.setPeer(peer: peerProxy)

    self.peerObservers[id] = peerObserver

    return peerProxy
  }

  /// Returns list containing information about video encoders for the different video codecs.
  func videoEncoders() -> [VideoCodecInfo] {
    [
      VideoCodecInfo(
        isHardwareAccelerated: false,
        codec: VideoCodec.VP8
      ),
      VideoCodecInfo(
        isHardwareAccelerated: VTIsHardwareEncodeSupported(kCMVideoCodecType_VP9),
        kind: VideoCodec.VP9,
        mymeType: "video/VP9"
      ),
      VideoCodecInfo(
        isHardwareAccelerated: false,
        codec: VideoCodec.AV1
      ),
      VideoCodecInfo(
        isHardwareAccelerated: VTIsHardwareEncodeSupported(kCMVideoCodecType_H264),
        kind: VideoCodec.H264,
        mymeType: "video/H264"
      ),
    ]
  }

  /// Returns list containing information about video encoders for the different video codecs.
  func audioEncoders() -> [VideoCodecInfo] {
    [
      VideoCodecInfo(
        isHardwareAccelerated: false,
        codec: VideoCodec.VP8
      ),
      VideoCodecInfo(
        isHardwareAccelerated: VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9),
        codec: VideoCodec.VP9
      ),
      VideoCodecInfo(
        isHardwareAccelerated: false,
        codec: VideoCodec.AV1
      ),
      VideoCodecInfo(
        isHardwareAccelerated: VTIsHardwareDecodeSupported(kCMVideoCodecType_H264),
        codec: VideoCodec.H264
      ),
    ]
  }

  /// Removes the specified [PeerObserver] from the [peerObservers].
  private func remotePeerObserver(id: Int) {
    self.peerObservers.removeValue(forKey: id)
  }

  /// Generates the next track ID.
  private func nextId() -> Int {
    self.lastPeerConnectionId += 1
    return self.lastPeerConnectionId
  }
}
