#![warn(clippy::pedantic)]

mod devices;
mod internal;
mod pc;
mod user_media;
mod video_sink;

use std::{
    collections::HashMap,
    rc::Rc,
    sync::{
        atomic::{AtomicU64, Ordering},
        Arc,
    },
};

use cxx::UniquePtr;
use dashmap::DashMap;
use libwebrtc_sys::{
    AudioLayer, AudioSourceInterface, PeerConnectionFactoryInterface,
    TaskQueueFactory, Thread, VideoDeviceInfo,
};
use threadpool::ThreadPool;

use crate::video_sink::Id as VideoSinkId;

#[doc(inline)]
pub use crate::{
    pc::{PeerConnection, PeerConnectionId},
    user_media::{
        AudioDeviceId, AudioDeviceModule, AudioTrack, AudioTrackId,
        MediaStream, MediaStreamId, VideoDeviceId, VideoSource, VideoTrack,
        VideoTrackId,
    },
    video_sink::{Frame, VideoSink},
};

/// Counter used to generate unique IDs.
static ID_COUNTER: AtomicU64 = AtomicU64::new(1);

/// Returns a next unique ID.
pub(crate) fn next_id() -> u64 {
    ID_COUNTER.fetch_add(1, Ordering::Relaxed)
}

/// The module which describes the bridge to call Rust from C++.
#[allow(clippy::items_after_statements, clippy::expl_impl_clone_on_copy)]
#[cxx::bridge]
pub mod api {
    /// Possible kinds of media devices.
    #[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
    pub enum MediaDeviceKind {
        kAudioInput,
        kAudioOutput,
        kVideoInput,
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
        pub width: usize,

        /// The height, in pixels.
        pub height: usize,

        /// The exact frame rate (frames per second).
        pub frame_rate: usize,
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

    /// Representation of a stream of media content.
    ///
    /// This stream consists of several [`MediaStreamTrack`]s, such as video
    /// and/or audio tracks.
    pub struct MediaStream {
        /// Unique ID of this [`MediaStream`].
        pub stream_id: u64,

        /// [`MediaStreamTrack`]s with [`TrackKind::kVideo`].
        pub video_tracks: Vec<MediaStreamTrack>,

        /// [`MediaStreamTrack`]s with [`TrackKind::kAudio`].
        pub audio_tracks: Vec<MediaStreamTrack>,
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

        /// [`TrackKind`] of the current [`MediaStreamTrack`].
        pub kind: TrackKind,

        /// The `enabled` property on the [`MediaStreamTrack`] interface is a
        /// `enabled` value which is `true` if the track is allowed to render
        /// the source stream or `false` if it is not. This can be used to
        /// intentionally mute a track.
        pub enabled: bool,
    }

    /// Nature of the [`MediaStreamTrack`].
    #[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
    pub enum TrackKind {
        kAudio,
        kVideo,
    }

    /// Representation of a permanent pair of an [`RtcRtpSender`] and an
    /// [RTCRtpReceiver], along with some shared state.
    ///
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

    /// The [`RtcRtpSender`] object allows an application to control how a
    /// given [`MediaStreamTrack`] is encoded and transmitted to a remote peer.
    #[derive(Clone, Debug, Eq, Hash, PartialEq)]
    pub struct RtcRtpSender {
        /// ID of this [`RtcRtpSender`].
        pub id: u64,
    }

    /// [`RtcTrackEvent`] represents the track event, which is sent when a new
    /// [`MediaStreamTrack`] is added to an [`RtcRtpTransceiver`] which is part
    /// of the [`PeerConnection`].
    pub struct RtcTrackEvent {
        pub track: MediaStreamTrack,
        pub transceiver: RtcRtpTransceiver,
    }

    /// Single video frame.
    pub struct VideoFrame {
        /// Vertical count of pixels in this [`VideoFrame`].
        pub height: usize,

        /// Horizontal count of pixels in this [`VideoFrame`].
        pub width: usize,

        /// Rotation of this [`VideoFrame`] in degrees.
        pub rotation: i32,

        /// Size of the bytes buffer required for allocation of the
        /// [`VideoFrame::get_abgr_bytes()`] call.
        pub buffer_size: usize,

        /// Underlying Rust side frame.
        pub frame: Box<Frame>,
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

    extern "C++" {
        type CreateSdpCallbackInterface =
            crate::internal::CreateSdpCallbackInterface;

        type SetDescriptionCallbackInterface =
            crate::internal::SetDescriptionCallbackInterface;

        type PeerConnectionObserverInterface =
            crate::internal::PeerConnectionObserverInterface;

        type OnFrameCallbackInterface =
            crate::internal::OnFrameCallbackInterface;

        type TrackEventInterface = crate::internal::TrackEventInterface;

        type AddIceCandidateCallbackInterface =
            crate::internal::AddIceCandidateCallbackInterface;

        type OnDeviceChangeCallback = crate::internal::OnDeviceChangeCallback;
    }

    extern "Rust" {
        include!("flutter-webrtc-native/include/api.h");

        type Webrtc;
        type Frame;

        /// Creates an instance of [`Webrtc`].
        #[cxx_name = "Init"]
        pub fn init() -> Box<Webrtc>;

        /// Returns a list of all available media input and output devices, such
        /// as microphones, cameras, headsets, and so forth.
        #[cxx_name = "EnumerateDevices"]
        pub fn enumerate_devices(self: &mut Webrtc) -> Vec<MediaDeviceInfo>;

        /// Creates a new [`PeerConnection`] and returns its ID.
        ///
        /// Writes an error to the provided `err`, if any.
        #[cxx_name = "CreatePeerConnection"]
        pub fn create_peer_connection(
            self: &mut Webrtc,
            cb: UniquePtr<PeerConnectionObserverInterface>,
            configuration: RtcConfiguration,
            err: &mut String,
        ) -> u64;

        /// Initiates the creation of a SDP offer for the purpose of starting
        /// a new WebRTC connection to a remote peer.
        ///
        /// Returns an empty [`String`] if operation succeeds or an error
        /// otherwise.
        #[cxx_name = "CreateOffer"]
        pub fn create_offer(
            self: &mut Webrtc,
            peer_id: u64,
            voice_activity_detection: bool,
            ice_restart: bool,
            use_rtp_mux: bool,
            cb: UniquePtr<CreateSdpCallbackInterface>,
        ) -> String;

        /// Creates a SDP answer to an offer received from a remote peer during
        /// the offer/answer negotiation of a WebRTC connection.
        ///
        /// Returns an empty [`String`] in operation succeeds or an error
        /// otherwise.
        #[cxx_name = "CreateAnswer"]
        #[allow(clippy::too_many_arguments)]
        pub fn create_answer(
            self: &mut Webrtc,
            peer_connection_id: u64,
            voice_activity_detection: bool,
            ice_restart: bool,
            use_rtp_mux: bool,
            cb: UniquePtr<CreateSdpCallbackInterface>,
        ) -> String;

        /// Changes the local description associated with the connection.
        ///
        /// Returns an empty [`String`] in operation succeeds or an error
        /// otherwise.
        #[cxx_name = "SetLocalDescription"]
        pub fn set_local_description(
            self: &mut Webrtc,
            peer_connection_id: u64,
            kind: String,
            sdp: String,
            cb: UniquePtr<SetDescriptionCallbackInterface>,
        ) -> String;

        /// Sets the specified session description as the remote peer's current
        /// offer or answer.
        ///
        /// Returns an empty [`String`] in operation succeeds or an error
        /// otherwise.
        #[cxx_name = "SetRemoteDescription"]
        pub fn set_remote_description(
            self: &mut Webrtc,
            peer_connection_id: u64,
            kind: String,
            sdp: String,
            cb: UniquePtr<SetDescriptionCallbackInterface>,
        ) -> String;

        /// Creates a new [`RtcRtpTransceiver`] and adds it to the set of
        /// transceivers of the specified [`PeerConnection`].
        #[cxx_name = "AddTransceiver"]
        pub fn add_transceiver(
            self: &mut Webrtc,
            peer_id: u64,
            media_type: &str,
            direction: &str,
        ) -> RtcRtpTransceiver;

        /// Returns a sequence of [`RtcRtpTransceiver`] objects representing
        /// the RTP transceivers currently attached to the specified
        /// [`PeerConnection`].
        #[cxx_name = "GetTransceivers"]
        pub fn get_transceivers(
            self: &mut Webrtc,
            peer_id: u64,
        ) -> Vec<RtcRtpTransceiver>;

        /// Changes the preferred `direction` of the specified
        /// [`RtcRtpTransceiver`].
        #[cxx_name = "SetTransceiverDirection"]
        pub fn set_transceiver_direction(
            self: &mut Webrtc,
            peer_id: u64,
            transceiver_id: u64,
            direction: &str,
        ) -> String;

        /// Returns the [Negotiated media ID (mid)][1] of the specified
        /// [`RtcRtpTransceiver`].
        ///
        /// [1]: https://w3.org/TR/webrtc#dfn-media-stream-identification-tag
        #[cxx_name = "GetTransceiverMid"]
        pub fn get_transceiver_mid(
            self: &mut Webrtc,
            peer_id: u64,
            transceiver_id: u64,
        ) -> String;

        /// Returns the preferred direction of the specified
        /// [`RtcRtpTransceiver`].
        #[cxx_name = "GetTransceiverDirection"]
        pub fn get_transceiver_direction(
            self: &mut Webrtc,
            peer_id: u64,
            transceiver_id: u64,
        ) -> String;

        /// Irreversibly marks the specified [`RtcRtpTransceiver`] as stopping,
        /// unless it's already stopped.
        ///
        /// This will immediately cause the transceiver's sender to no longer
        /// send, and its receiver to no longer receive.
        #[cxx_name = "StopTransceiver"]
        pub fn stop_transceiver(
            self: &mut Webrtc,
            peer_id: u64,
            transceiver_id: u64,
        ) -> String;

        /// Replaces the specified [`AudioTrack`] (or [`VideoTrack`]) on
        /// the [`sys::Transceiver`]'s `sender`.
        #[cxx_name = "SenderReplaceTrack"]
        pub fn sender_replace_track(
            self: &mut Webrtc,
            peer_id: u64,
            transceiver_id: u64,
            track_id: u64,
        ) -> String;

        /// Adds the new ICE candidate to the given [`PeerConnection`].
        #[cxx_name = "AddIceCandidate"]
        pub fn add_ice_candidate(
            self: &mut Webrtc,
            peer_id: u64,
            candidate: &str,
            sdp_mid: &str,
            sdp_mline_index: i32,
            cb: UniquePtr<AddIceCandidateCallbackInterface>,
        );

        /// Tells the [`PeerConnection`] that ICE should be restarted.
        #[cxx_name = "RestartIce"]
        pub fn restart_ice(self: &mut Webrtc, peer_id: u64);

        /// Closes the [`PeerConnection`].
        #[cxx_name = "DisposePeerConnection"]
        pub fn dispose_peer_connection(self: &mut Webrtc, peer_id: u64);

        /// Creates a [`MediaStream`] with tracks according to provided
        /// [`MediaStreamConstraints`].
        #[cxx_name = "GetMedia"]
        pub fn get_media(
            self: &mut Webrtc,
            constraints: &MediaStreamConstraints,
            is_display: bool,
        ) -> MediaStream;

        /// Disposes the [`MediaStream`] and all contained tracks.
        #[cxx_name = "DisposeStream"]
        pub fn dispose_stream(self: &mut Webrtc, id: u64);

        /// Creates a new [`VideoSink`] attached to the specified media stream
        /// backed by the provided [`OnFrameCallbackInterface`].
        #[cxx_name = "CreateVideoSink"]
        pub fn create_video_sink(
            self: &mut Webrtc,
            sink_id: i64,
            stream_id: u64,
            handler: UniquePtr<OnFrameCallbackInterface>,
        );

        /// Destroys the [`VideoSink`] by the given ID.
        #[cxx_name = "DisposeVideoSink"]
        fn dispose_video_sink(self: &mut Webrtc, sink_id: i64);

        /// Converts this [`api::VideoFrame`] pixel data to `ABGR` scheme and
        /// outputs the result to the provided `buffer`.
        #[cxx_name = "GetABGRBytes"]
        unsafe fn get_abgr_bytes(self: &VideoFrame, buffer: *mut u8);

        /// Changes the [enabled][1] property of the media track by its ID.
        ///
        /// [1]: https://w3.org/TR/mediacapture-streams#track-enabled
        #[cxx_name = "SetTrackEnabled"]
        pub fn set_track_enabled(
            self: &mut Webrtc,
            track_id: u64,
            enabled: bool,
        );

        // todo
        #[cxx_name = "RegisterObserver"]
        pub fn register_observer_track(
            self: &mut Webrtc,
            id: u64,
            cb: UniquePtr<TrackEventInterface>,
        ) -> String;

        // todo
        #[cxx_name = "UnregisterObserver"]
        pub fn unregister_observer_track(self: &mut Webrtc, id: u64) -> String;

        /// Sets the provided [`OnDeviceChangeCallback`] as the callback to be
        /// called whenever a set of available media devices changes.
        ///
        /// Only one callback can be set at a time, so the previous one will be
        /// dropped, if any.
        #[cxx_name = "SetOnDeviceChanged"]
        pub fn set_on_device_changed(
            self: &mut Webrtc,
            cb: UniquePtr<OnDeviceChangeCallback>,
        );
    }
}

/// [`Context`] wrapper that is exposed to the C++ API clients.
pub struct Webrtc(Box<Context>);

/// Application context that manages all dependencies.
#[allow(dead_code)]
pub struct Context {
    task_queue_factory: TaskQueueFactory,
    worker_thread: Thread,
    network_thread: Thread,
    signaling_thread: Thread,
    audio_device_module: AudioDeviceModule,
    video_device_info: VideoDeviceInfo,
    peer_connection_factory: PeerConnectionFactoryInterface,
    video_sources: HashMap<VideoDeviceId, Rc<VideoSource>>,
    video_tracks: Arc<DashMap<VideoTrackId, VideoTrack>>,
    audio_source: Option<Rc<AudioSourceInterface>>,
    audio_tracks: Arc<DashMap<AudioTrackId, AudioTrack>>,
    local_media_streams: HashMap<MediaStreamId, MediaStream>,
    peer_connections: HashMap<PeerConnectionId, PeerConnection>,
    video_sinks: HashMap<VideoSinkId, VideoSink>,
    callback_pool: Arc<ThreadPool>,
}

/// Creates a new instance of [`Webrtc`].
///
/// # Panics
///
/// Panics on any error returned from the `libWebRTC`.
#[must_use]
pub fn init() -> Box<Webrtc> {
    // TODO: Dont panic but propagate errors to API users.
    let mut task_queue_factory =
        TaskQueueFactory::create_default_task_queue_factory();

    let mut network_thread = Thread::create(true).unwrap();
    network_thread.start().unwrap();

    let mut worker_thread = Thread::create(false).unwrap();
    worker_thread.start().unwrap();

    let mut signaling_thread = Thread::create(false).unwrap();
    signaling_thread.start().unwrap();

    let audio_device_module = AudioDeviceModule::new(
        AudioLayer::kPlatformDefaultAudio,
        &mut task_queue_factory,
    )
    .unwrap();

    let peer_connection_factory = PeerConnectionFactoryInterface::create(
        Some(&network_thread),
        Some(&worker_thread),
        Some(&signaling_thread),
        Some(&audio_device_module.inner),
    )
    .unwrap();

    let video_device_info = VideoDeviceInfo::create().unwrap();

    Box::new(Webrtc(Box::new(Context {
        task_queue_factory,
        network_thread,
        worker_thread,
        signaling_thread,
        audio_device_module,
        video_device_info,
        peer_connection_factory,
        video_sources: HashMap::new(),
        video_tracks: Arc::new(DashMap::new()),
        audio_source: None,
        audio_tracks: Arc::new(DashMap::new()),
        local_media_streams: HashMap::new(),
        peer_connections: HashMap::new(),
        video_sinks: HashMap::new(),
        callback_pool: Arc::new(ThreadPool::new(1)),
    })))
}

impl Drop for Webrtc {
    fn drop(&mut self) {
        self.set_on_device_changed(UniquePtr::null());
    }
}
