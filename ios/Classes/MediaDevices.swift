import AVFoundation
import WebRTC

/// Processor for `getUserMedia()` requests.
class MediaDevices {
  /// Global state used for creation of new `MediaStreamTrackProxy`s.
  private var state: State

  /// Subscribers for `onDeviceChange` callback of this `MediaDevices`.
  private var onDeviceChange: [() -> Void] = []

  /// Initializes new `MediaDevices` with the provided `State`.
  ///
  /// Subscribes on `AVAudioSession.routeChangeNotification` notifications
  /// for `onDeviceChange` callback firing.
  init(state: State) {
    try! AVAudioSession.sharedInstance().setCategory(
      AVAudioSession.Category.playAndRecord,
      options: AVAudioSession.CategoryOptions.allowBluetooth
    )
    self.state = state
    NotificationCenter.default.addObserver(
      forName: AVAudioSession.routeChangeNotification, object: nil,
      queue: OperationQueue.main,
      using: { (_: Notification) in
        for cb in self.onDeviceChange {
          cb()
        }
      }
    )
  }

  /// Switches input device to the iPhone's microphone.
  func setBuiltInMicAsInput() {
    if let routes = AVAudioSession.sharedInstance().availableInputs {
      for route in routes {
        if route.portType == .builtInMic {
          _ = try? AVAudioSession.sharedInstance()
            .setPreferredInput(route)
          break
        }
      }
    }
  }

  /// Switches audio output device to the device with a provided ID.
  func setOutputAudioId(id: String) {
    let session = AVAudioSession.sharedInstance()
    if id == "speaker" {
      self.setBuiltInMicAsInput()
      try! AVAudioSession.sharedInstance().overrideOutputAudioPort(.speaker)
    } else if id == "ear-piece" {
      self.setBuiltInMicAsInput()
      try! AVAudioSession.sharedInstance().overrideOutputAudioPort(.none)
    } else {
      let selectedInput = AVAudioSession.sharedInstance().availableInputs?
        .first(where: { $0.portName == id })
      if selectedInput != nil {
        try! session.setPreferredInput(selectedInput!)
      }
    }
  }

  /// Subscribes to `onDeviceChange` callback of `MediaDevices`.
  func onDeviceChange(cb: @escaping () -> Void) {
    self.onDeviceChange.append(cb)
  }

  /// Returns a list of `MediaDeviceInfo`s for the currently available devices.
  func enumerateDevices() -> [MediaDeviceInfo] {
    var devices: [MediaDeviceInfo] = []
    devices.append(MediaDeviceInfo(
      deviceId: "speaker",
      label: "Speaker",
      kind: MediaDeviceKind.audioOutput
    ))
    devices.append(MediaDeviceInfo(
      deviceId: "ear-piece",
      label: "Ear-Piece",
      kind: MediaDeviceKind.audioOutput
    ))

    let videoDevices = AVCaptureDevice.devices(for: AVMediaType.video).map {
      device -> MediaDeviceInfo in
      MediaDeviceInfo(
        deviceId: device.uniqueID, label: device.localizedName,
        kind: MediaDeviceKind.videoInput
      )
    }
    devices.append(contentsOf: videoDevices)

    let session = AVAudioSession.sharedInstance()
    guard let availableInputs = session.availableInputs else {
      return devices
    }

    let bluetoothOutput = availableInputs
      .filter { $0.portType == AVAudioSession.Port.bluetoothHFP }.last
    if bluetoothOutput != nil {
      devices.append(MediaDeviceInfo(
        deviceId: bluetoothOutput!.portName,
        label: bluetoothOutput!.portName,
        kind: MediaDeviceKind.audioOutput
      ))
    }

    return devices
  }

  /// Creates local audio and video `MediaStreamTrackProxy`s based on the
  /// provided `Constraints`.
  func getUserMedia(constraints: Constraints) -> [MediaStreamTrackProxy] {
    var tracks: [MediaStreamTrackProxy] = []
    if constraints.audio != nil {
      tracks.append(self.getUserAudio())
    }
    if constraints.video != nil {
      tracks.append(self.getUserVideo(constraints: constraints.video!))
    }

    return tracks
  }

  /// Searches for an `AVCaptureDevice` which fits into the provided
  /// `VideoConstraints`.
  private func findVideoDeviceForConstraints(constraints: VideoConstraints)
    -> AVCaptureDevice?
  {
    var maxScore = 0
    var bestFoundDevice: AVCaptureDevice?
    let discoverySession = AVCaptureDevice.DiscoverySession(
      deviceTypes: [AVCaptureDevice.DeviceType.builtInWideAngleCamera],
      mediaType: AVMediaType.video,
      position: AVCaptureDevice.Position.unspecified
    )
    for device in AVCaptureDevice.devices(for: AVMediaType.video) {
      let deviceScore = constraints.calculateScoreForDevice(device: device)
      if deviceScore != nil {
        if deviceScore! >= maxScore {
          maxScore = deviceScore!
          bestFoundDevice = device
        }
      }
    }
    return bestFoundDevice
  }

  /// Creates an audio `MediaStreamTrackProxy`.
  private func getUserAudio() -> MediaStreamTrackProxy {
    let track = self.state.getPeerFactory().audioTrack(
      withTrackId: LocalTrackIdGenerator.shared.nextId()
    )
    let audioSource = AudioMediaTrackSourceProxy(track: track)
    return audioSource.newTrack()
  }

  /// Creates a video `MediaStreamTrackProxy` for the provided
  /// `VideoConstraints`.
  private func getUserVideo(constraints: VideoConstraints)
    -> MediaStreamTrackProxy
  {
    let videoDevice = self
      .findVideoDeviceForConstraints(constraints: constraints)!
    let selectedFormat = self.selectFormatForDevice(
      device: videoDevice,
      constraints: constraints
    )
    let fps = self.selectFpsForFormat(
      format: selectedFormat,
      constraints: constraints
    )

    let source = self.state.getPeerFactory().videoSource()
    let capturer = RTCCameraVideoCapturer(delegate: source)
    capturer.startCapture(with: videoDevice, format: selectedFormat, fps: fps)
    let videoTrackSource = VideoMediaTrackSourceProxy(
      peerConnectionFactory: self.state.getPeerFactory(), source: source,
      deviceId: videoDevice.uniqueID,
      capturer: capturer
    )
    return videoTrackSource.newTrack()
  }

  /// Selects the most suitable FPS for the provided `AVCaptureDevice.Format`.
  private func selectFpsForFormat(
    format: AVCaptureDevice.Format,
    constraints: VideoConstraints
  )
    -> Int
  {
    var maxSupportedFramerate = 0.0
    for fpsRange in format.videoSupportedFrameRateRanges {
      maxSupportedFramerate = fmax(maxSupportedFramerate, fpsRange.maxFrameRate)
    }
    var targetFps = 30
    if constraints.fps != nil {
      targetFps = constraints.fps!
    }
    return min(Int(maxSupportedFramerate), targetFps)
  }

  /// Selects the most suitable `AVCaptureDevice.Format` for the provided
  /// `AVCaptureDevice` based on the provided `VideoConstraints`.
  private func selectFormatForDevice(
    device: AVCaptureDevice,
    constraints: VideoConstraints
  )
    -> AVCaptureDevice.Format
  {
    var bestFoundFormat: AVCaptureDevice.Format?
    var currentDiff = Int.max
    var targetWidth = 640
    if constraints.width != nil {
      targetWidth = constraints.width!
    }
    var targetHeight = 480
    if constraints.height != nil {
      targetHeight = constraints.height!
    }
    for format in RTCCameraVideoCapturer.supportedFormats(for: device) {
      let dimension = CMVideoFormatDescriptionGetDimensions(format
        .formatDescription)
      let diff = abs(targetWidth - Int(dimension.width)) +
        abs(targetHeight - Int(dimension.height))
      if diff < currentDiff {
        bestFoundFormat = format
        currentDiff = diff
      }
    }
    return bestFoundFormat!
  }
}
