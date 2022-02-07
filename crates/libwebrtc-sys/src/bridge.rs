use std::fmt;

use anyhow::anyhow;
use cxx::{CxxString, UniquePtr};

use crate::{
    create_transceivers, CreateSdpCallback, OnFrameCallback,
    SetDescriptionCallback, Transceivers,
};

/// [`CreateSdpCallback`] transferable to the C++ side.
type DynCreateSdpCallback = Box<dyn CreateSdpCallback>;

/// [`SetDescriptionCallback`] transferable to the C++ side.
type DynSetDescriptionCallback = Box<dyn SetDescriptionCallback>;

/// [`OnFrameCallback`] transferable to the C++ side.
type DynOnFrameCallback = Box<dyn OnFrameCallback>;

#[allow(clippy::expl_impl_clone_on_copy, clippy::items_after_statements)]
#[cxx::bridge(namespace = "bridge")]
pub(crate) mod webrtc {
    /// Possible kinds of audio devices implementation.
    #[repr(i32)]
    #[derive(Debug, Eq, Hash, PartialEq)]
    pub enum AudioLayer {
        kPlatformDefaultAudio = 0,
        kWindowsCoreAudio,
        kWindowsCoreAudio2,
        kLinuxAlsaAudio,
        kLinuxPulseAudio,
        kAndroidJavaAudio,
        kAndroidOpenSLESAudio,
        kAndroidJavaInputAndOpenSLESOutputAudio,
        kAndroidAAudioAudio,
        kAndroidJavaInputAndAAudioOutputAudio,
        kDummyAudio,
    }

    /// [RTCSdpType] representation.
    ///
    /// [RTCSdpType]: https://w3.org/TR/webrtc#dom-rtcsdptype
    #[repr(i32)]
    #[derive(Debug, Eq, Hash, PartialEq)]
    pub enum SdpType {
        /// [RTCSdpType.offer][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcsdptype-offer
        kOffer,

        /// [RTCSdpType.pranswer][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcsdptype-pranswer
        kPrAnswer,

        /// [RTCSdpType.answer][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcsdptype-answer
        kAnswer,

        /// [RTCSdpType.rollback][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcsdptype-rollback
        kRollback,
    }

    #[repr(i32)]
    #[derive(Debug, Eq, Hash, PartialEq)]
    pub enum MediaType {
        MEDIA_TYPE_AUDIO = 0,
        MEDIA_TYPE_VIDEO,
        MEDIA_TYPE_DATA,
        MEDIA_TYPE_UNSUPPORTED,
    }

    #[repr(i32)]
    #[derive(Debug, Eq, Hash, PartialEq)]
    pub enum RtpTransceiverDirection {
        kSendRecv = 0,
        kSendOnly,
        kRecvOnly,
        kInactive,
        kStopped,
    }

    /// Possible variants of a [`VideoFrame`]'s rotation.
    #[repr(i32)]
    #[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
    pub enum VideoRotation {
        kVideoRotation_0 = 0,
        kVideoRotation_90 = 90,
        kVideoRotation_180 = 180,
        kVideoRotation_270 = 270,
    }

    #[rustfmt::skip]
    unsafe extern "C++" {
        include!("libwebrtc-sys/include/bridge.h");

        type PeerConnectionFactoryInterface;
        type TaskQueueFactory;
        type Thread;

        /// Creates a default [`TaskQueueFactory`] based on the current
        /// platform.
        #[namespace = "webrtc"]
        #[cxx_name = "CreateDefaultTaskQueueFactory"]
        pub fn create_default_task_queue_factory()
            -> UniquePtr<TaskQueueFactory>;

        /// Creates a new [`Thread`].
        pub fn create_thread() -> UniquePtr<Thread>;

        /// Starts the current [`Thread`].
        #[cxx_name = "Start"]
        pub fn start_thread(self: Pin<&mut Thread>) -> bool;

        /// Creates a new [`PeerConnectionFactoryInterface`].
        pub fn create_peer_connection_factory(
            network_thread: &UniquePtr<Thread>,
            worker_thread: &UniquePtr<Thread>,
            signaling_thread: &UniquePtr<Thread>,
            default_adm: &UniquePtr<AudioDeviceModule>,
        ) -> UniquePtr<PeerConnectionFactoryInterface>;
    }

    unsafe extern "C++" {
        type AudioDeviceModule;
        type AudioLayer;

        /// Creates a new [`AudioDeviceModule`] for the given [`AudioLayer`].
        pub fn create_audio_device_module(
            audio_layer: AudioLayer,
            task_queue_factory: Pin<&mut TaskQueueFactory>,
        ) -> UniquePtr<AudioDeviceModule>;

        /// Initializes the given [`AudioDeviceModule`].
        pub fn init_audio_device_module(
            audio_device_module: &AudioDeviceModule,
        ) -> i32;

        /// Returns count of available audio playout devices.
        pub fn playout_devices(audio_device_module: &AudioDeviceModule) -> i16;

        /// Returns count of available audio recording devices.
        pub fn recording_devices(
            audio_device_module: &AudioDeviceModule,
        ) -> i16;

        /// Writes device info to the provided `name` and `id` for the given
        /// audio playout device `index`.
        pub fn playout_device_name(
            audio_device_module: &AudioDeviceModule,
            index: i16,
            name: &mut String,
            id: &mut String,
        ) -> i32;

        /// Writes device info to the provided `name` and `id` for the given
        /// audio recording device `index`.
        pub fn recording_device_name(
            audio_device_module: &AudioDeviceModule,
            index: i16,
            name: &mut String,
            id: &mut String,
        ) -> i32;

        /// Specifies which microphone to use for recording audio using an
        /// index retrieved by the corresponding enumeration method which is
        /// [`AudiDeviceModule::RecordingDeviceName`].
        pub fn set_audio_recording_device(
            audio_device_module: &AudioDeviceModule,
            index: u16,
        ) -> i32;
    }

    unsafe extern "C++" {
        type VideoDeviceInfo;

        /// Creates a new [`VideoDeviceInfo`].
        pub fn create_video_device_info() -> UniquePtr<VideoDeviceInfo>;

        /// Returns count of a video recording devices.
        #[namespace = "webrtc"]
        #[cxx_name = "NumberOfDevices"]
        pub fn number_of_video_devices(self: Pin<&mut VideoDeviceInfo>) -> u32;

        /// Writes device info to the provided `name` and `id` for the given
        /// video device `index`.
        pub fn video_device_name(
            device_info: Pin<&mut VideoDeviceInfo>,
            index: u32,
            name: &mut String,
            id: &mut String,
        ) -> i32;
    }

    extern "Rust" {
        type Transceivers;

        /// Adds a new [`RtpTransceiverInterface`].
        pub fn add(
            self: &mut Transceivers,
            transceiver: UniquePtr<RtpTransceiverInterface>,
        );

        /// Creates a new `boxed` [`Transceivers`].
        pub fn create_transceivers() -> Box<Transceivers>;
    }

    #[rustfmt::skip]
    unsafe extern "C++" {
        type CreateSessionDescriptionObserver;
        type PeerConnectionDependencies;
        type PeerConnectionInterface;
        type PeerConnectionObserver;
        type RTCConfiguration;
        type RTCOfferAnswerOptions;
        type SdpType;
        type SessionDescriptionInterface;
        type SetLocalDescriptionObserver;
        type SetRemoteDescriptionObserver;

        type MediaType;
        type RtpTransceiverDirection;
        type RtpTransceiverInterface;

        /// Creates a default [`RTCConfiguration`].
        pub fn create_default_rtc_configuration()
            -> UniquePtr<RTCConfiguration>;

        /// Creates a new [`PeerConnectionInterface`].
        ///
        /// If creation fails then an error will be written to the provided
        /// `error` and the returned [`UniquePtr`] will be `null`.
        pub fn create_peer_connection_or_error(
            peer_connection_factory: Pin<&mut PeerConnectionFactoryInterface>,
            conf: &RTCConfiguration,
            deps: UniquePtr<PeerConnectionDependencies>,
            error: &mut String,
        ) -> UniquePtr<PeerConnectionInterface>;

        /// Creates a new [`PeerConnectionObserver`].
        pub fn create_peer_connection_observer(
        ) -> UniquePtr<PeerConnectionObserver>;

        /// Creates a [`PeerConnectionDependencies`] from the provided
        /// [`PeerConnectionObserver`].
        pub fn create_peer_connection_dependencies(
            observer: UniquePtr<PeerConnectionObserver>,
        ) -> UniquePtr<PeerConnectionDependencies>;

        /// Creates a default [`RTCOfferAnswerOptions`].
        pub fn create_default_rtc_offer_answer_options(
        ) -> UniquePtr<RTCOfferAnswerOptions>;

        /// Creates a new [`RTCOfferAnswerOptions`] from the provided options.
        pub fn create_rtc_offer_answer_options(
            offer_to_receive_video: i32,
            offer_to_receive_audio: i32,
            voice_activity_detection: bool,
            ice_restart: bool,
            use_rtp_mux: bool,
        ) -> UniquePtr<RTCOfferAnswerOptions>;

        /// Creates a new [`CreateSessionDescriptionObserver`] from the
        /// provided [`DynCreateSdpCallback`].
        pub fn create_create_session_observer(
            cb: Box<DynCreateSdpCallback>,
        ) -> UniquePtr<CreateSessionDescriptionObserver>;

        /// Creates a new [`SetLocalDescriptionObserver`] from the provided
        /// [`DynSetDescriptionCallback`].
        pub fn create_set_local_description_observer(
            cb: Box<DynSetDescriptionCallback>,
        ) -> UniquePtr<SetLocalDescriptionObserver>;

        /// Creates a new [`SetRemoteDescriptionObserver`] from the provided
        /// [`DynSetDescriptionCallback`].
        pub fn create_set_remote_description_observer(
            cb: Box<DynSetDescriptionCallback>,
        ) -> UniquePtr<SetRemoteDescriptionObserver>;

        /// Calls the [RTCPeerConnection.createOffer()][1] on the provided
        /// [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcpeerconnection-createoffer
        pub fn create_offer(
            peer: Pin<&mut PeerConnectionInterface>,
            options: &RTCOfferAnswerOptions,
            obs: UniquePtr<CreateSessionDescriptionObserver>,
        );

        /// Calls the [RTCPeerConnection.createAnswer()][1] on the provided
        /// [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcpeerconnection-createanswer
        pub fn create_answer(
            peer: Pin<&mut PeerConnectionInterface>,
            options: &RTCOfferAnswerOptions,
            obs: UniquePtr<CreateSessionDescriptionObserver>,
        );

        /// Calls the [RTCPeerConnection.setLocalDescription()][1] on the
        /// provided [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-peerconnection-setlocaldescription
        pub fn set_local_description(
            peer: Pin<&mut PeerConnectionInterface>,
            desc: UniquePtr<SessionDescriptionInterface>,
            obs: UniquePtr<SetLocalDescriptionObserver>,
        );

        /// Calls the [RTCPeerConnection.setRemoteDescription()][1] on the
        /// provided [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-peerconnection-setremotedescription
        pub fn set_remote_description(
            peer: Pin<&mut PeerConnectionInterface>,
            desc: UniquePtr<SessionDescriptionInterface>,
            obs: UniquePtr<SetRemoteDescriptionObserver>,
        );

        /// Creates a new [`SessionDescriptionInterface`].
        #[namespace = "webrtc"]
        #[cxx_name = "CreateSessionDescription"]
        pub fn create_session_description(
            kind: SdpType,
            sdp: &CxxString,
        ) -> UniquePtr<SessionDescriptionInterface>;

        /// Adds a new [`RTCRtpTransceiver`][1] to some [`PeerConnectionInterface`]. The [`RTCRtpTransceiver`][1]
        /// interface represents a combination of a [`RTCRtpSender`][2] and
        /// a [`RTCRtpReceiver`][3].
        ///
        /// [1]: https://tinyurl.com/2p88ajym
        /// [2]: https://tinyurl.com/mr37vbjy
        /// [3]: https://tinyurl.com/zfmc7ph3
        pub fn add_transceiver(
            peer_connection_interface: Pin<&mut PeerConnectionInterface>,
            media_type: MediaType,
            direction: RtpTransceiverDirection
        ) -> UniquePtr<RtpTransceiverInterface>;

        /// Gets information about [`PeerConnectionInterface`]'s [`RTCRtpTransceiver`][1]s.
        ///
        /// [1]: https://tinyurl.com/2p88ajym
        pub fn get_transceivers(peer_connection_interface: &PeerConnectionInterface) -> Box<Transceivers>;

        /// Gets [`Transceiver`]'s `mid`.
        pub fn get_transceiver_mid(transceiver: &RtpTransceiverInterface) -> String;

        /// Gets the [`Transceiver`]'s `pointer` as `usize`, used to compare [`Transceiver`]s.
        pub fn get_transceiver_ptr(transceiver: &RtpTransceiverInterface) -> usize;

        /// Gets the [`Transceiver`]'s [`RtpTransceiverDirection`].
        pub fn get_transceiver_direction(transceiver: &RtpTransceiverInterface) -> RtpTransceiverDirection;
    }

    unsafe extern "C++" {
        type AudioSourceInterface;
        type AudioTrackInterface;
        type MediaStreamInterface;
        type VideoTrackInterface;
        type VideoTrackSourceInterface;
        #[namespace = "webrtc"]
        type VideoFrame;
        type VideoSinkInterface;
        type VideoRotation;

        /// Creates a new [`VideoTrackSourceInterface`] sourced by a video input
        /// device with provided `device_index`.
        pub fn create_device_video_source(
            worker_thread: Pin<&mut Thread>,
            signaling_thread: Pin<&mut Thread>,
            width: usize,
            height: usize,
            fps: usize,
            device_index: u32,
        ) -> UniquePtr<VideoTrackSourceInterface>;

        /// Creates a new [`VideoTrackSourceInterface`] sourced by a screen
        /// capturing.
        pub fn create_display_video_source(
            worker_thread: Pin<&mut Thread>,
            signaling_thread: Pin<&mut Thread>,
            width: usize,
            height: usize,
            fps: usize,
        ) -> UniquePtr<VideoTrackSourceInterface>;

        /// Creates a new [`AudioSourceInterface`].
        pub fn create_audio_source(
            peer_connection_factory: &PeerConnectionFactoryInterface,
        ) -> UniquePtr<AudioSourceInterface>;

        /// Creates a new [`VideoTrackInterface`].
        pub fn create_video_track(
            peer_connection_factory: &PeerConnectionFactoryInterface,
            id: String,
            video_source: &VideoTrackSourceInterface,
        ) -> UniquePtr<VideoTrackInterface>;

        /// Creates a new [`AudioTrackInterface`].
        pub fn create_audio_track(
            peer_connection_factory: &PeerConnectionFactoryInterface,
            id: String,
            audio_source: &AudioSourceInterface,
        ) -> UniquePtr<AudioTrackInterface>;

        /// Creates a new [`MediaStreamInterface`].
        pub fn create_local_media_stream(
            peer_connection_factory: &PeerConnectionFactoryInterface,
            id: String,
        ) -> UniquePtr<MediaStreamInterface>;

        /// Adds the [`VideoTrackInterface`] to the [`MediaStreamInterface`].
        pub fn add_video_track(
            peer_connection_factory: &MediaStreamInterface,
            track: &VideoTrackInterface,
        ) -> bool;

        /// Adds the [`AudioTrackInterface`] to the [`MediaStreamInterface`].
        pub fn add_audio_track(
            peer_connection_factory: &MediaStreamInterface,
            track: &AudioTrackInterface,
        ) -> bool;

        /// Removes the [`VideoTrackInterface`] from the
        /// [`MediaStreamInterface`].
        pub fn remove_video_track(
            media_stream: &MediaStreamInterface,
            track: &VideoTrackInterface,
        ) -> bool;

        /// Removes the [`AudioTrackInterface`] from the
        /// [`MediaStreamInterface`].
        pub fn remove_audio_track(
            media_stream: &MediaStreamInterface,
            track: &AudioTrackInterface,
        ) -> bool;

        /// Changes the [enabled][1] property of the specified
        /// [`VideoTrackInterface`].
        ///
        /// [1]: https://w3.org/TR/mediacapture-streams#track-enabled
        pub fn set_video_track_enabled(
            track: &VideoTrackInterface,
            enabled: bool,
        );

        /// Changes the [enabled][1] property of the specified
        /// [`AudioTrackInterface`].
        ///
        /// [1]: https://w3.org/TR/mediacapture-streams#track-enabled
        pub fn set_audio_track_enabled(
            track: &AudioTrackInterface,
            enabled: bool,
        );

        /// Registers the provided [`VideoSinkInterface`] for the given
        /// [`VideoTrackInterface`].
        ///
        /// Used to connect the given [`VideoTrackInterface`] to the underlying
        /// video engine.
        pub fn add_or_update_video_sink(
            track: &VideoTrackInterface,
            sink: Pin<&mut VideoSinkInterface>,
        );

        /// Detaches the provided [`VideoSinkInterface`] from the given
        /// [`VideoTrackInterface`].
        pub fn remove_video_sink(
            track: &VideoTrackInterface,
            sink: Pin<&mut VideoSinkInterface>,
        );

        /// Creates a new forwarding [`VideoSinkInterface`] backed by the
        /// provided [`DynOnFrameCallback`].
        pub fn create_forwarding_video_sink(
            handler: Box<DynOnFrameCallback>,
        ) -> UniquePtr<VideoSinkInterface>;

        /// Returns a width of this [`VideoFrame`].
        #[must_use]
        pub fn width(self: &VideoFrame) -> i32;

        /// Returns a height of this [`VideoFrame`].
        #[must_use]
        pub fn height(self: &VideoFrame) -> i32;

        /// Returns a [`VideoRotation`] of this [`VideoFrame`].
        #[must_use]
        pub fn rotation(self: &VideoFrame) -> VideoRotation;

        /// Converts the provided [`webrtc::VideoFrame`] pixels to the `ABGR`
        /// scheme and writes the result to the provided `buffer`.
        pub unsafe fn video_frame_to_abgr(frame: &VideoFrame, buffer: *mut u8);
    }

    extern "Rust" {
        type DynOnFrameCallback;

        /// Forwards the given [`webrtc::VideoFrame`] the the provided
        /// [`DynOnFrameCallback`].
        pub fn on_frame(
            cb: &mut DynOnFrameCallback,
            frame: UniquePtr<VideoFrame>,
        );
    }

    extern "Rust" {
        type DynSetDescriptionCallback;
        type DynCreateSdpCallback;

        /// Successfully completes the provided [`DynSetDescriptionCallback`].
        pub fn create_sdp_success(
            cb: Box<DynCreateSdpCallback>,
            sdp: &CxxString,
            kind: SdpType,
        );

        /// Completes the provided [`DynCreateSdpCallback`] with an error.
        pub fn create_sdp_fail(
            cb: Box<DynCreateSdpCallback>,
            error: &CxxString,
        );

        /// Successfully completes the provided [`DynSetDescriptionCallback`].
        pub fn set_description_success(cb: Box<DynSetDescriptionCallback>);

        /// Completes the provided [`DynSetDescriptionCallback`] with an error.
        pub fn set_description_fail(
            cb: Box<DynSetDescriptionCallback>,
            error: &CxxString,
        );

    }
}

/// Successfully completes the provided [`DynSetDescriptionCallback`].
#[allow(clippy::boxed_local)]
pub fn create_sdp_success(
    mut cb: Box<DynCreateSdpCallback>,
    sdp: &CxxString,
    kind: webrtc::SdpType,
) {
    cb.success(sdp, kind);
}

/// Completes the provided [`DynCreateSdpCallback`] with an error.
#[allow(clippy::boxed_local)]
pub fn create_sdp_fail(mut cb: Box<DynCreateSdpCallback>, error: &CxxString) {
    cb.fail(error);
}

/// Successfully completes the provided [`DynSetDescriptionCallback`].
#[allow(clippy::boxed_local)]
pub fn set_description_success(mut cb: Box<DynSetDescriptionCallback>) {
    cb.success();
}

/// Completes the provided [`DynSetDescriptionCallback`] with the given `error`.
#[allow(clippy::boxed_local)]
pub fn set_description_fail(
    mut cb: Box<DynSetDescriptionCallback>,
    error: &CxxString,
) {
    cb.fail(error);
}

impl TryFrom<&str> for webrtc::SdpType {
    type Error = anyhow::Error;

    fn try_from(val: &str) -> Result<Self, Self::Error> {
        match val {
            "offer" => Ok(Self::kOffer),
            "answer" => Ok(Self::kAnswer),
            "pranswer" => Ok(Self::kPrAnswer),
            "rollback" => Ok(Self::kRollback),
            v => Err(anyhow!("Invalid `SdpType`: {v}")),
        }
    }
}

impl fmt::Display for webrtc::SdpType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            webrtc::SdpType::kOffer => write!(f, "offer"),
            webrtc::SdpType::kAnswer => write!(f, "answer"),
            webrtc::SdpType::kPrAnswer => write!(f, "pranswer"),
            webrtc::SdpType::kRollback => write!(f, "rollback"),
            _ => unreachable!(),
        }
    }
}

/// Forwards the given [`webrtc::VideoFrame`] the the provided
/// [`DynOnFrameCallback`].
fn on_frame(cb: &mut DynOnFrameCallback, frame: UniquePtr<webrtc::VideoFrame>) {
    cb.on_frame(frame);
}
