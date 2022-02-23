#pragma once

#include <string>

struct VideoFrame;
struct RtcTrackEvent;

// Completion callback for the `Webrtc::CreateOffer` and `Webrtc::CreateAnswer`
// functions.
class CreateSdpCallbackInterface {
 public:
  // Called when an operation succeeds.
  virtual void OnSuccess(const std::string& sdp, const std::string& kind) = 0;

  // Called when an operation fails with the `error`.
  virtual void OnFail(const std::string& error) = 0;

  virtual ~CreateSdpCallbackInterface() = default;
};

// Completion callback for the `Webrtc::SetLocalDescription` and
// `Webrtc::SetRemoteDescription` functions.
class SetDescriptionCallbackInterface {
 public:
  // Called when an operation succeeds.
  virtual void OnSuccess() = 0;

  // Called when an operation fails with the `error`.
  virtual void OnFail(const std::string& error) = 0;

  virtual ~SetDescriptionCallbackInterface() = default;
};

// Callback for video frames handlers provided to the
// `Webrtc::create_video_sink()` function.
class OnFrameCallbackInterface {
 public:
  // Called when the underlying video engine produces a new video frame.
  //
  // The provided frame is a pointer to the `rust::Box<VideoFrame>`. Its
  // ownership can be transferred back to the Rust side using the
  // `rust::Box::from_raw()`.
  virtual void OnFrame(VideoFrame) = 0;

  virtual ~OnFrameCallbackInterface() = default;
};

// TODO: Add OnTrack event in #30.
// Handler of events firing from a `PeerConnectionInterface`.
class PeerConnectionObserverInterface {
 public:

  // Called when a `connectionstatechange` event occurs.
  //
  // See: https://w3.org/TR/webrtc#event-connectionstatechange
  virtual void OnConnectionStateChange(const std::string& new_state) = 0;

  // Called when an `icecandidate` event occurs.
  //
  // See: https://w3.org/TR/webrtc#event-icecandidate
  virtual void OnIceCandidate(const std::string& candidate) = 0;

  // Called when an `icecandidateerror` event occurs.
  //
  // See: https://www.w3.org/TR/webrtc/#event-icecandidateerror
  virtual void OnIceCandidateError(const std::string& address,
                                   int port,
                                   const std::string& url,
                                   int error_code,
                                   const std::string& error_text) = 0;

  // Called when an `iceconnectionstatechange` event occurs.
  //
  // See: https://w3.org/TR/webrtc#event-iceconnectionstatechange
  virtual void OnIceConnectionStateChange(const std::string& new_state) = 0;

  // Called when an `icegatheringstatechange` event occurs.
  //
  // See: https://w3.org/TR/webrtc#event-icegatheringstatechange
  virtual void OnIceGatheringStateChange(const std::string& new_state) = 0;

  // Called when a `negotiation` event occurs.
  //
  // See: https://w3.org/TR/webrtc#event-negotiation
  virtual void OnNegotiationNeeded() = 0;

  // Called when a `signalingstatechange` event occurs.
  //
  // See: https://w3.org/TR/webrtc#event-signalingstatechange
  virtual void OnSignalingChange(const std::string& new_state) = 0;

  // Called when a `track` event occurs.
  //
  // See: https://w3.org/TR/webrtc/#event-track
  virtual void OnTrack(RtcTrackEvent event) = 0;

  virtual ~PeerConnectionObserverInterface() = default;
};


// Handler of events firing from a `MediaStreamTrackInterface`.
class TrackEventInterface {
 public:

  // Called when a `Ended` event occurs.
  //
  // See: https://w3.org/TR/mediacapture-streams/#event-mediastreamtrack-ended
  virtual void OnEnded() = 0;
  
  // Called when a `Mute` event occurs.
  //
  // See: https://w3.org/TR/mediacapture-streams/#event-mediastreamtrack-mute
  virtual void OnMute() = 0;
  
  // Called when a `Unmute` event occurs.
  //
  // See: https://w3.org/TR/mediacapture-streams/#event-mediastreamtrack-unmute
  virtual void OnUnmute() = 0;

  virtual ~TrackEventInterface() = default;
};
