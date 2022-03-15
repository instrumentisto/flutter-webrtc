use crate::{cpp_api::OnFrameCallbackInterface, Frame};
use anyhow::anyhow;
use cxx::UniquePtr;
use flutter_rust_bridge::StreamSink;
use std::sync::mpsc::Sender;

/// Possible kinds of media devices.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum MediaDeviceKind {
    AudioInput,
    AudioOutput,
    VideoInput,
}

/// [RTCRtpTransceiverDirection][1] representation.
///
/// [1]: https://w3.org/TR/webrtc#dom-rtcrtptransceiverdirection
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum RtpTransceiverDirection {
    SendRecv,
    SendOnly,
    RecvOnly,
    Inactive,
    Stopped,
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum MediaType {
    Audio,
    Video,
}

/// Information describing a single media input or output device.
#[derive(Debug)]
pub struct MediaDeviceInfo {
    /// Unique identifier for the represented device.
    pub device_id: String,

    /// Kind of the represented device.
    pub kind: MediaDeviceKind,

    /// Label describing the represented device.
    pub label: String,
}

/// The [MediaStreamConstraints] is used to instruct what sort of
/// [`MediaStreamTrack`]s to include in the [`MediaStream`] returned by
/// [`Webrtc::get_users_media()`].
pub struct MediaStreamConstraints {
    /// Specifies the nature and settings of the video [`MediaStreamTrack`].
    pub audio: AudioConstraints,
    /// Specifies the nature and settings of the audio [`MediaStreamTrack`].
    pub video: VideoConstraints,
}

/// Specifies the nature and settings of the video [`MediaStreamTrack`]
/// returned by [`Webrtc::get_users_media()`].
pub struct VideoConstraints {
    /// Indicates whether [`Webrtc::get_users_media()`] should obtain video
    /// track. All other args will be ignored if `required` is set to
    /// `false`.
    pub required: bool,

    /// The identifier of the device generating the content of the
    /// [`MediaStreamTrack`]. First device will be chosen if empty
    /// [`String`] is provided.
    pub device_id: String,

    /// The width, in pixels.
    pub width: u32,

    /// The height, in pixels.
    pub height: u32,

    /// The exact frame rate (frames per second).
    pub frame_rate: u32,
}

/// Specifies the nature and settings of the audio [`MediaStreamTrack`]
/// returned by [`Webrtc::get_users_media()`].
pub struct AudioConstraints {
    /// Indicates whether [`Webrtc::get_users_media()`] should obtain video
    /// track. All other args will be ignored if `required` is set to
    /// `false`.
    pub required: bool,

    /// The identifier of the device generating the content of the
    /// [`MediaStreamTrack`]. First device will be chosen if empty
    /// [`String`] is provided.
    ///
    /// __NOTE__: There can be only one active recording device at a time,
    /// so changing device will affect all previously obtained audio tracks.
    pub device_id: String,
}

/// Representation of a single media track within a [`MediaStream`].
///
/// Typically, these are audio or video tracks, but other track types may
/// exist as well.
pub struct MediaStreamTrack {
    /// Unique identifier (GUID) for the track
    pub id: u64,

    /// Label that identifies the track source, as in "internal microphone".
    pub label: String,

    /// [`MediaType`] of the current [`MediaStreamTrack`].
    pub kind: MediaType,

    /// The `enabled` property on the [`MediaStreamTrack`] interface is a
    /// `enabled` value which is `true` if the track is allowed to render
    /// the source stream or `false` if it is not. This can be used to
    /// intentionally mute a track.
    pub enabled: bool,
}

/// Representation of a permanent pair of an [RTCRtpSender] and an
/// [RTCRtpReceiver], along with some shared state.
///
/// [RTCRtpSender]: https://w3.org/TR/webrtc#dom-rtcrtpsender
/// [RTCRtpReceiver]: https://w3.org/TR/webrtc#dom-rtcrtpreceiver
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct RtcRtpTransceiver {
    /// ID of this [`RtcRtpTransceiver`].
    ///
    /// It's not unique across all possible [`RtcRtpTransceiver`]s, but only
    /// within a specific peer.
    pub id: u64,

    /// [Negotiated media ID (mid)][1] which the local and remote peers have
    /// agreed upon to uniquely identify the [`MediaStream`]'s pairing of
    /// sender and receiver.
    ///
    /// [1]: https://w3.org/TR/webrtc#dfn-media-stream-identification-tag
    pub mid: String,

    /// Preferred [`direction`][1] of this [`RtcRtpTransceiver`].
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtcrtptransceiver-direction
    pub direction: String,

    /// [`RtcRtpSender`] responsible for encoding and sending outgoing
    /// media data for the transceiver's stream.
    pub sender: RtcRtpSender,
}

/// [`RtcRtpSender`] object allowing to control how a [`MediaStreamTrack`]
/// is encoded and transmitted to a remote peer.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct RtcRtpSender {
    /// ID of this [`RtcRtpSender`].
    pub id: u64,
}

/// [`RtcTrackEvent`] representing a track event, sent when a new
/// [`MediaStreamTrack`] is added to an [`RtcRtpTransceiver`] as part of a
/// [`PeerConnection`].
pub struct RtcTrackEvent {
    /// [`MediaStreamTrack`] associated with the [RTCRtpReceiver] identified
    /// by the receiver.
    ///
    /// [RTCRtpReceiver]: https://w3.org/TR/webrtc#dom-rtcrtpreceiver
    pub track: MediaStreamTrack,

    /// [`RtcRtpTransceiver`] object associated with the event.
    pub transceiver: RtcRtpTransceiver,
}

/// [`PeerConnection`]'s configuration.
pub struct RtcConfiguration {
    /// [iceTransportPolicy][1] configuration.
    ///
    /// Indicates which candidates the [ICE Agent][2] is allowed to use.
    ///
    /// [1]: https://tinyurl.com/icetransportpolicy
    /// [2]: https://w3.org/TR/webrtc#dfn-ice-agent
    pub ice_transport_policy: String,

    /// [bundlePolicy][1] configuration.
    ///
    /// Indicates which media-bundling policy to use when gathering ICE
    /// candidates.
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtcconfiguration-bundlepolicy
    pub bundle_policy: String,

    /// [iceServers][1] configuration.
    ///
    /// An array of objects describing servers available to be used by ICE,
    /// such as STUN and TURN servers.
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtcconfiguration-iceservers
    pub ice_servers: Vec<RtcIceServer>,
}

/// Describes the STUN and TURN servers that can be used by the
/// [ICE Agent][1] to establish a connection with a peer.
///
/// [1]: https://w3.org/TR/webrtc#dfn-ice-agent
pub struct RtcIceServer {
    /// STUN or TURN URI(s).
    pub urls: Vec<String>,

    /// If this [`RtcIceServer`] object represents a TURN server, then this
    /// attribute specifies the [username][1] to use with that TURN server.
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtciceserver-username
    pub username: String,

    /// If this [`RtcIceServer`] object represents a TURN server, then this
    /// attribute specifies the [credential][1] to use with that TURN
    /// server.
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtciceserver-credential
    pub credential: String,
}

/// Returns a list of all available media input and output devices, such
/// as microphones, cameras, headsets, and so forth.
pub fn enumerate_devices() -> Vec<MediaDeviceInfo> {
    unimplemented!()
}

/// Creates a new [`PeerConnection`] and returns its ID.
///
/// Writes an error to the provided `err`, if any.
pub fn create_peer_connection(
    cb: StreamSink<()>,
    configuration: RtcConfiguration,
) -> anyhow::Result<u64> {
    unimplemented!()
}

/// Initiates the creation of a SDP offer for the purpose of starting
/// a new WebRTC connection to a remote peer.
///
/// Returns an empty [`String`] if operation succeeds or an error
/// otherwise.
pub fn create_offer(
    peer_id: u64,
    voice_activity_detection: bool,
    ice_restart: bool,
    use_rtp_mux: bool,
) -> String {
    unimplemented!()
}

/// Creates a SDP answer to an offer received from a remote peer during
/// the offer/answer negotiation of a WebRTC connection.
///
/// Returns an empty [`String`] in operation succeeds or an error
/// otherwise.
#[allow(clippy::too_many_arguments)]
pub fn create_answer(
    peer_connection_id: u64,
    voice_activity_detection: bool,
    ice_restart: bool,
    use_rtp_mux: bool,
) -> String {
    unimplemented!()
}

/// Changes the local description associated with the connection.
///
/// Returns an empty [`String`] in operation succeeds or an error
/// otherwise.
pub fn set_local_description(peer_connection_id: u64, kind: String, sdp: String) -> String {
    unimplemented!()
}

/// Sets the specified session description as the remote peer's current
/// offer or answer.
///
/// Returns an empty [`String`] in operation succeeds or an error
/// otherwise.
pub fn set_remote_description(peer_connection_id: u64, kind: String, sdp: String) -> String {
    unimplemented!()
}

/// Creates a new [`RtcRtpTransceiver`] and adds it to the set of
/// transceivers of the specified [`PeerConnection`].
pub fn add_transceiver(
    peer_id: u64,
    media_type: MediaType,
    direction: RtpTransceiverDirection,
) -> RtcRtpTransceiver {
    unimplemented!()
}

/// Returns a sequence of [`RtcRtpTransceiver`] objects representing
/// the RTP transceivers currently attached to the specified
/// [`PeerConnection`].
pub fn get_transceivers(peer_id: u64) -> Vec<RtcRtpTransceiver> {
    unimplemented!()
}

/// Changes the preferred `direction` of the specified
/// [`RtcRtpTransceiver`].
pub fn set_transceiver_direction(
    peer_id: u64,
    transceiver_id: u64,
    direction: RtpTransceiverDirection,
) -> String {
    unimplemented!()
}

/// Returns the [Negotiated media ID (mid)][1] of the specified
/// [`RtcRtpTransceiver`].
///
/// [1]: https://w3.org/TR/webrtc#dfn-media-stream-identification-tag
pub fn get_transceiver_mid(peer_id: u64, transceiver_id: u64) -> String {
    unimplemented!()
}

/// Returns the preferred direction of the specified
/// [`RtcRtpTransceiver`].
pub fn get_transceiver_direction(peer_id: u64, transceiver_id: u64) -> String {
    unimplemented!()
}

/// Irreversibly marks the specified [`RtcRtpTransceiver`] as stopping,
/// unless it's already stopped.
///
/// This will immediately cause the transceiver's sender to no longer
/// send, and its receiver to no longer receive.
pub fn stop_transceiver(peer_id: u64, transceiver_id: u64) -> String {
    unimplemented!()
}

/// Replaces the specified [`AudioTrack`] (or [`VideoTrack`]) on
/// the [`sys::Transceiver`]'s `sender`.
pub fn sender_replace_track(peer_id: u64, transceiver_id: u64, track_id: u64) -> String {
    unimplemented!()
}

/// Adds the new ICE candidate to the given [`PeerConnection`].
pub fn add_ice_candidate(
    peer_id: u64,
    candidate: String,
    sdp_mid: String,
    sdp_mline_index: i32,
) {
    unimplemented!()
}

/// Tells the [`PeerConnection`] that ICE should be restarted.
pub fn restart_ice(peer_id: u64) {
    unimplemented!()
}

/// Closes the [`PeerConnection`].
pub fn dispose_peer_connection(peer_id: u64) {
    unimplemented!()
}

/// Creates a [`MediaStream`] with tracks according to provided
/// [`MediaStreamConstraints`].
pub fn get_media(
    constraints: MediaStreamConstraints,
    is_display: bool,
) -> Vec<MediaStreamTrack> {
    unimplemented!()
}

/// Disposes the [`MediaStream`] and all contained tracks.
pub fn dispose_stream(id: u64) {
    unimplemented!()
}

/// Creates a new [`VideoSink`] attached to the specified media stream
/// backed by the provided [`OnFrameCallbackInterface`].
pub fn create_video_sink(
    sink_id: i64,
    stream_id: u64,
    handler: i64, // UniquePtr<OnFrameCallbackInterface>
) {
    unimplemented!()
}

/// Destroys the [`VideoSink`] by the given ID.
fn dispose_video_sink(sink_id: i64) {
    unimplemented!()
}

/// Converts this [`api::VideoFrame`] pixel data to `ABGR` scheme and
/// outputs the result to the provided `buffer`.
unsafe fn get_abgr_bytes(buffer: *mut u8) {
    unimplemented!()
}

/// Changes the [enabled][1] property of the media track by its ID.
///
/// [1]: https://w3.org/TR/mediacapture-streams#track-enabled
pub fn set_track_enabled(track_id: u64, enabled: bool) {
    unimplemented!()
}

/// Registers an observer to the media track events.
pub fn register_track_observer(id: u64, cb: StreamSink<()>) -> String {
    unimplemented!()
}

/// Sets the provided [`OnDeviceChangeCallback`] as the callback to be
/// called whenever a set of available media devices changes.
///
/// Only one callback can be set at a time, so the previous one will be
/// dropped, if any.
pub fn set_on_device_changed(cb: StreamSink<()>) {
    unimplemented!()
}
