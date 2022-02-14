use std::fmt;

use anyhow::anyhow;
use cxx::{CxxString, CxxVector, UniquePtr};

use crate::{
    CreateSdpCallback, OnFrameCallback, PeerConnectionEventsHandler,
    SetDescriptionCallback,
};

/// [`CreateSdpCallback`] transferable to the C++ side.
type DynCreateSdpCallback = Box<dyn CreateSdpCallback>;

/// [`SetDescriptionCallback`] transferable to the C++ side.
type DynSetDescriptionCallback = Box<dyn SetDescriptionCallback>;

/// [`OnFrameCallback`] transferable to the C++ side.
type DynOnFrameCallback = Box<dyn OnFrameCallback>;

/// [`PeerConnectionEventsHandler`] transferable to the C++ side.
type DynPeerConnectionEventsHandler = Box<dyn PeerConnectionEventsHandler>;

#[allow(
    clippy::expl_impl_clone_on_copy,
    clippy::items_after_statements,
    clippy::ptr_as_ptr
)]
#[cxx::bridge(namespace = "bridge")]
pub(crate) mod webrtc {

    pub struct StringPair {
        first: String,
        second: String,
    }

    // todo
    #[repr(i32)]
    #[derive(Debug, Eq, Hash, PartialEq)]
    pub enum TrackState {
        kLive,
        kEnded,
    }

    /// Possible kinds of audio devices implementation.
    #[derive(Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
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
    #[derive(Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
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
        /// [1]: https://w3.org/TR/webrtc/#dom-rtcsdptype-rollback
        kRollback,
    }

    /// Possible kinds of an [`RtpTransceiverInterface`].
    #[derive(Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
    pub enum MediaType {
        MEDIA_TYPE_AUDIO = 0,
        MEDIA_TYPE_VIDEO,
        MEDIA_TYPE_DATA,
        MEDIA_TYPE_UNSUPPORTED,
    }

    /// [RTCRtpTransceiverDirection][1] representation.
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtcrtptransceiverdirection
    #[derive(Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
    pub enum RtpTransceiverDirection {
        kSendRecv = 0,
        kSendOnly,
        kRecvOnly,
        kInactive,
        kStopped,
    }

    /// Possible variants of a [`VideoFrame`]'s rotation.
    #[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
    pub enum VideoRotation {
        kVideoRotation_0 = 0,
        kVideoRotation_90 = 90,
        kVideoRotation_180 = 180,
        kVideoRotation_270 = 270,
    }

    // TODO: Remove once `cxx` crate allows using pointers to opaque types in
    //       vectors: https://github.com/dtolnay/cxx/issues/741
    /// Wrapper for an [`RtpTransceiverInterface`] that can be used in Rust/C++
    /// vectors.
    struct TransceiverContainer {
        /// Wrapped [`RtpTransceiverInterface`].
        pub ptr: UniquePtr<RtpTransceiverInterface>,
    }

    /// [RTCSignalingState] representation.
    ///
    /// [RTCSignalingState]: https://w3.org/TR/webrtc#state-definitions
    #[derive(Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
    pub enum SignalingState {
        /// [RTCSignalingState.stable][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcsignalingstate-stable
        kStable,

        /// [RTCSignalingState.have-local-offer][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcsignalingstate-have-local-offer
        kHaveLocalOffer,

        /// [RTCSignalingState.have-local-pranswer][1] representation.
        ///
        /// [1]: https://tinyurl.com/have-local-pranswer
        kHaveLocalPrAnswer,

        /// [RTCSignalingState.have-remote-offer][1] representation.
        ///
        /// [1]: https://tinyurl.com/have-remote-offer
        kHaveRemoteOffer,

        /// [RTCSignalingState.have-remote-pranswer][1] representation.
        ///
        /// [1]: https://tinyurl.com/have-remote-pranswer
        kHaveRemotePrAnswer,

        /// [RTCSignalingState.closed][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcsignalingstate-closed
        kClosed,
    }

    /// [RTCIceGatheringState][1] representation.
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtcicegatheringstate
    #[derive(Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
    pub enum IceGatheringState {
        /// [RTCIceGatheringState.new][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcicegatheringstate-new
        kIceGatheringNew,

        /// [RTCIceGatheringState.gathering][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcicegatheringstate-gathering
        kIceGatheringGathering,

        /// [RTCIceGatheringState.complete][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcicegatheringstate-complete
        kIceGatheringComplete,
    }

    /// [RTCPeerConnectionState][1] representation.
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtcpeerconnectionstate
    #[derive(Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
    pub enum PeerConnectionState {
        /// [RTCPeerConnectionState.new][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcpeerconnectionstate-new
        kNew,

        /// [RTCPeerConnectionState.connecting][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcpeerconnectionstate-connecting
        kConnecting,

        /// [RTCPeerConnectionState.connected][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcpeerconnectionstate-connected
        kConnected,

        /// [RTCPeerConnectionState.disconnected][1] representation.
        ///
        /// [1]: https://tinyurl.com/connectionstate-disconnected
        kDisconnected,

        /// [RTCPeerConnectionState.failed][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcpeerconnectionstate-failed
        kFailed,

        /// [RTCPeerConnectionState.closed][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtcpeerconnectionstate-closed
        kClosed,
    }

    /// [RTCIceConnectionState][1] representation.
    ///
    /// [1]: https://w3.org/TR/webrtc#dom-rtciceconnectionstate
    #[derive(Debug, Eq, Hash, PartialEq)]
    #[repr(i32)]
    pub enum IceConnectionState {
        /// [RTCIceConnectionState.new][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtciceconnectionstate-new
        kIceConnectionNew,

        /// [RTCIceConnectionState.checking][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtciceconnectionstate-checking
        kIceConnectionChecking,

        /// [RTCIceConnectionState.connected][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtciceconnectionstate-connected
        kIceConnectionConnected,

        /// [RTCIceConnectionState.completed][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtciceconnectionstate-completed
        kIceConnectionCompleted,

        /// [RTCIceConnectionState.failed][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtciceconnectionstate-failed
        kIceConnectionFailed,

        /// [RTCIceConnectionState.disconnected][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtciceconnectionstate-disconnected
        kIceConnectionDisconnected,

        /// [RTCIceConnectionState.closed][1] representation.
        ///
        /// [1]: https://w3.org/TR/webrtc#dom-rtciceconnectionstate-closed
        kIceConnectionClosed,

        /// Non-spec-compliant variant.
        ///
        /// [`libwertc` states that it's unreachable][1].
        ///
        /// [1]: https://tinyurl.com/kIceConnectionMax-unreachable
        kIceConnectionMax,
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

    #[rustfmt::skip]
    unsafe extern "C++" {
        include!("libwebrtc-sys/include/media_stream_track_interface_getters.h");
        type MediaStreamTrackInterface;

        #[must_use]
        pub fn get_media_stream_track_kind(
            track: &MediaStreamTrackInterface,
        ) -> UniquePtr<CxxString>;

        #[must_use]
        pub fn get_media_stream_track_id(
            track: &MediaStreamTrackInterface,
        ) -> UniquePtr<CxxString>;

        #[must_use]
        pub fn get_media_stream_track_state(
            track: &MediaStreamTrackInterface,
        ) -> TrackState;

        #[must_use]
        pub fn get_media_stream_track_enabled(
            track: &MediaStreamTrackInterface,
        ) -> bool;
    }

    #[rustfmt::skip]
    unsafe extern "C++" {
        include!("libwebrtc-sys/include/rtp_codec_parameters_getters.h");
        type RtpCodecParameters;

        #[must_use]
        pub fn get_rtp_codec_parameters_name(
            codec: &RtpCodecParameters,
        ) -> UniquePtr<CxxString>;

        #[must_use]
        pub fn get_rtp_codec_parameters_payload_type(
            codec: &RtpCodecParameters,
        ) -> i32;

        // todo optinoanl
        pub fn get_rtp_codec_parameters_clock_rate(
            codec: &RtpCodecParameters,
        ) -> Result<i32>;

        // todo
        pub fn get_rtp_codec_parameters_num_channels(
            codec: &RtpCodecParameters,
        ) -> Result<i32>;

        #[must_use]
        pub fn get_rtp_codec_parameters_parameters(
            codec: &RtpCodecParameters,
        ) -> UniquePtr<CxxVector<StringPair>>;

        #[must_use]
        pub fn get_rtp_codec_parameters_kind(
            codec: &RtpCodecParameters,
        ) -> MediaType;
    }

    #[rustfmt::skip]
    unsafe extern "C++" {
        include!("libwebrtc-sys/include/rtp_receiver_interface_getters.h");

        type RtpReceiverInterface;
        #[must_use]
        pub fn get_rtp_receiver_streams(
            receiver: &RtpReceiverInterface,
        ) -> UniquePtr<CxxVector<MediaStreamInterface>>;

        #[must_use]
        pub fn get_rtp_receiver_id(
            receiver: &RtpReceiverInterface,
        ) -> UniquePtr<CxxString>;

        #[must_use]
        pub fn get_rtp_receiver_track(
            receiver: &RtpReceiverInterface,
        ) -> UniquePtr<MediaStreamTrackInterface>;

        #[must_use]
        pub fn get_rtp_receiver_parameters(
            receiver: &RtpReceiverInterface,
        ) -> UniquePtr<RtpParameters>;
    }

    #[rustfmt::skip]
    unsafe extern "C++" {
        include!("libwebrtc-sys/include/rtp_sender_interface_getters.h");

        type RtpSenderInterface;
        #[must_use]
        pub fn get_rtp_sender_id(
            sender: &RtpSenderInterface,
        ) -> UniquePtr<CxxString>;

        // may be null
        #[must_use]
        pub fn get_rtp_sender_dtmf(
            sender: &RtpSenderInterface,
        ) -> UniquePtr<DtmfSenderInterface>;
    }

    #[rustfmt::skip]
    unsafe extern "C++" {
        include!("libwebrtc-sys/include/rtp_encoding_parameters_getters.h");

        type RtpEncodingParameters;
        #[must_use]
        pub fn get_rtp_encoding_parameters_active(
            encoding: &RtpEncodingParameters,
        ) -> bool;

        pub fn get_rtp_encoding_parameters_maxBitrate(
            encoding: &RtpEncodingParameters,
        ) -> Result<i32>;

        pub fn get_rtp_encoding_parameters_minBitrate(
            encoding: &RtpEncodingParameters,
        ) -> Result<i32>;

        pub fn get_rtp_encoding_parameters_maxFramerate(
            encoding: &RtpEncodingParameters,
        ) -> Result<f64>;

        pub fn get_rtp_encoding_parameters_ssrc(
            encoding: &RtpEncodingParameters,
        ) -> Result<i64>;

        pub fn get_rtp_encoding_parameters_scale_resolution_down_by(
            encoding: &RtpEncodingParameters,
        ) -> Result<f64>;
    }

    #[rustfmt::skip]
    unsafe extern "C++" {
        include!("libwebrtc-sys/include/rtp_parameters_getters.h");

        type RtpParameters;
        #[must_use]
        pub fn get_rtp_parameters_transaction_id(
            parameters: &RtpParameters,
        ) -> UniquePtr<CxxString>;

        #[must_use]
        pub fn get_rtp_parameters_mid(
            parameters: &RtpParameters,
        ) -> UniquePtr<CxxString>;

        #[must_use]
        pub fn get_rtp_parameters_codecs(
            parameters: &RtpParameters,
        ) -> UniquePtr<CxxVector<RtpCodecParameters>>;

        #[must_use]
        pub fn get_rtp_parameters_header_extensions(
            parameters: &RtpParameters,
        ) -> UniquePtr<CxxVector<RtpExtension>>;

        #[must_use]
        pub fn get_rtp_parameters_encodings(
            parameters: &RtpParameters,
        ) -> UniquePtr<CxxVector<RtpEncodingParameters>>;

        #[must_use]
        pub fn get_rtp_parameters_rtcp(
            parameters: &RtpParameters,
        ) -> UniquePtr<RtcpParameters>;
    }


    #[rustfmt::skip]
    unsafe extern "C++" {
        type DtmfSenderInterface;

        #[must_use]
        pub fn get_dtmf_sender_duration(
            dtmf: &DtmfSenderInterface,
        ) -> i32;
    
        #[must_use]
        pub fn get_dtmf_sender_inter_tone_gap(
            dtmf: &DtmfSenderInterface,
        ) -> i32;
    }



    #[rustfmt::skip]
    unsafe extern "C++" {
        #[namespace = "cricket"]
        pub type Candidate;
        #[namespace = "cricket"]
        pub type CandidatePairChangeEvent;
        pub type IceCandidateInterface;
        pub type MediaType;
        #[namespace = "cricket"]
        type CandidatePair;
        type CreateSessionDescriptionObserver;
        type IceConnectionState;
        type IceGatheringState;
        type PeerConnectionDependencies;
        type PeerConnectionInterface;
        type PeerConnectionObserver;
        type PeerConnectionState;
        type RTCConfiguration;
        type RTCOfferAnswerOptions;
        type RtpTransceiverDirection;
        type RtpTransceiverInterface;
        type SdpType;
        type SessionDescriptionInterface;
        type SetLocalDescriptionObserver;
        type SetRemoteDescriptionObserver;
        type SignalingState;

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
            cb: Box<DynPeerConnectionEventsHandler>,
        ) -> UniquePtr<PeerConnectionObserver>;

        /// Creates a [`PeerConnectionDependencies`] from the provided
        /// [`PeerConnectionObserver`].
        pub fn create_peer_connection_dependencies(
            observer: &UniquePtr<PeerConnectionObserver>,
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

        /// Returns the spec-compliant string representation of the provided
        /// [`IceCandidateInterface`].
        ///
        /// # Safety
        ///
        /// `candidate` must be a valid [`IceCandidateInterface`] pointer.
        #[must_use]
        pub unsafe fn ice_candidate_interface_to_string(
            candidate: *const IceCandidateInterface
        ) -> UniquePtr<CxxString>;
    }

    #[rustfmt::skip]
    unsafe extern "C++" {
        
        /// Returns the spec-compliant string representation of the provided
        /// [`Candidate`].
        #[must_use]
        pub fn candidate_to_string(candidate: &Candidate) -> UniquePtr<CxxString>;

        /// Creates a new [`RtpTransceiverInterface`] and adds it to the set of
        /// transceivers of the given [`PeerConnectionInterface`].
        pub fn add_transceiver(
            peer_connection_interface: Pin<&mut PeerConnectionInterface>,
            media_type: MediaType,
            direction: RtpTransceiverDirection
        ) -> UniquePtr<RtpTransceiverInterface>;

        /// Returns a sequence of [`RtpTransceiverInterface`] objects
        /// representing the RTP transceivers currently attached to the given
        /// [`PeerConnectionInterface`] object.
        pub fn get_transceivers(
            peer_connection_interface: &PeerConnectionInterface
        ) -> Vec<TransceiverContainer>;

        /// Returns a `mid` of the given [`RtpTransceiverInterface`].
        ///
        /// If an empty [`String`] is returned, then the given
        /// [`RtpTransceiverInterface`] hasn't been negotiated yet.
        pub fn get_transceiver_mid(
            transceiver: &RtpTransceiverInterface
        ) -> String;

        /// Returns a [`RtpTransceiverDirection`] of the given
        /// [`RtpTransceiverInterface`].
        pub fn get_transceiver_direction(
            transceiver: &RtpTransceiverInterface
        ) -> RtpTransceiverDirection;


        /// Returns a [`RtpSenderInterface`] of the given
        /// [`RtpTransceiverInterface`].
        pub fn get_transceiver_sender(
            transceiver: &RtpTransceiverInterface,
        ) -> UniquePtr<RtpSenderInterface>;


        /// Returns a [`RtpReceiverInterface`] of the given
        /// [`RtpTransceiverInterface`].
        pub fn get_transceiver_receiver(
            transceiver: &RtpTransceiverInterface,
        ) -> UniquePtr<RtpReceiverInterface>;
    }

    unsafe extern "C++" {
        type AudioSourceInterface;
        type AudioTrackInterface;
        type MediaStreamInterface;
        type VideoTrackInterface;
        type VideoTrackSourceInterface;
        #[namespace = "webrtc"]
        pub type VideoFrame;
        type VideoSinkInterface;
        type VideoRotation;
        type RtpExtension;
        type RtcpParameters;

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

        // todo
        #[must_use]
        pub fn get_audio_track_sourse(
            track: &AudioTrackInterface,
        ) -> UniquePtr<AudioSourceInterface>;

        // todo
        #[must_use]
        pub fn get_video_track_sourse(
            track: &VideoTrackInterface,
        ) -> UniquePtr<VideoTrackSourceInterface>;

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

        /// Returns the timestamp of when the last data was received from the
        /// provided [`CandidatePairChangeEvent`].
        #[must_use]
        pub fn get_last_data_received_ms(
            event: &CandidatePairChangeEvent,
        ) -> i64;

        /// Returns the reason causing the provided
        /// [`CandidatePairChangeEvent`].
        #[must_use]
        pub fn get_reason(
            event: &CandidatePairChangeEvent,
        ) -> UniquePtr<CxxString>;

        /// Returns the estimated disconnect time in milliseconds from the
        /// provided [`CandidatePairChangeEvent`].
        #[must_use]
        pub fn get_estimated_disconnected_time_ms(
            event: &CandidatePairChangeEvent,
        ) -> i64;

        /// Returns a `id` of the given [`MediaStreamInterface`].
        #[must_use]
        pub fn get_media_stream_id(
            stream: &MediaStreamInterface,
        ) -> UniquePtr<CxxString>;

        /// Returns a `AudioTrackVector` of the given [`MediaStreamInterface`].
        #[must_use]
        pub fn get_media_stream_audio_tracks(
            stream: &MediaStreamInterface,
        ) -> UniquePtr<CxxVector<AudioTrackInterface>>;

        /// Returns a `VideoTrackVector` of the given [`MediaStreamInterface`].
        #[must_use]
        pub fn get_media_stream_video_tracks(
            stream: &MediaStreamInterface,
        ) -> UniquePtr<CxxVector<VideoTrackInterface>>;

        /// Upcast [`VideoTrackInterface`] to [`MediaStreamTrackInterface`].
        #[must_use]
        pub fn video_track_media_stream_track_upcast(
            track: &VideoTrackInterface,
        ) -> &MediaStreamTrackInterface;

        /// Upcast [`AudioTrackInterface`] to [`MediaStreamTrackInterface`].
        #[must_use]
        pub fn audio_track_media_stream_track_upcast(
            track: &AudioTrackInterface,
        ) -> &MediaStreamTrackInterface;

        #[must_use]
        pub fn media_stream_track_interface_downcast_video_track(
            track: Pin<&mut MediaStreamTrackInterface>,
        ) -> UniquePtr<VideoTrackInterface>;

        #[must_use]
        pub fn media_stream_track_interface_downcast_audio_track(
            track: Pin<&mut MediaStreamTrackInterface>,
        ) -> UniquePtr<AudioTrackInterface>;


        #[must_use]
        pub fn get_rtcp_parameters_cname(
            rtcp: &RtcpParameters,
        ) -> UniquePtr<CxxString>;

        #[must_use]
        pub fn get_rtcp_parameters_reduced_size(rtcp: &RtcpParameters) -> bool;

        #[must_use]
        pub fn  get_rtp_extension_uri(
            extension: &RtpExtension,
        ) -> UniquePtr<CxxString>;

        #[must_use]
        pub fn  get_rtp_extension_id(extension: &RtpExtension) -> i32;

        #[must_use]
        pub fn get_rtp_extension_encrypt(extension: &RtpExtension) -> bool;

        /// Returns the [`CandidatePair`] from the provided
        /// [`CandidatePairChangeEvent`].
        #[must_use]
        pub fn get_candidate_pair(
            event: &CandidatePairChangeEvent,
        ) -> &CandidatePair;

        /// Returns the local [`Candidate`] of the provided [`CandidatePair`].
        #[must_use]
        pub fn local_candidate(self: &CandidatePair) -> &Candidate;

        /// Returns the remote [`Candidate`] of the provided [`CandidatePair`].
        #[must_use]
        pub fn remote_candidate(self: &CandidatePair) -> &Candidate;
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
        type DynPeerConnectionEventsHandler;


        pub fn new_string_pair(f: &CxxString, s: &CxxString) -> StringPair;

        
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

        /// Forwards the new [`SignalingState`] to the provided
        /// [`DynPeerConnectionEventsHandler`] when a
        /// [`signalingstatechange`][1] event occurs in the attached
        /// [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#event-signalingstatechange
        pub fn on_signaling_change(
            cb: &mut DynPeerConnectionEventsHandler,
            state: SignalingState,
        );

        /// Forwards the new [`IceConnectionState`] to the provided
        /// [`DynPeerConnectionEventsHandler`] when an
        /// [`iceconnectionstatechange`][1] event occurs in the attached
        /// [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#event-iceconnectionstatechange
        pub fn on_standardized_ice_connection_change(
            cb: &mut DynPeerConnectionEventsHandler,
            new_state: IceConnectionState,
        );

        /// Forwards the new [`PeerConnectionState`] to the provided
        /// [`DynPeerConnectionEventsHandler`] when a
        /// [`connectionstatechange`][1] event occurs in the attached
        /// [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#event-connectionstatechange
        pub fn on_connection_change(
            cb: &mut DynPeerConnectionEventsHandler,
            new_state: PeerConnectionState,
        );

        /// Forwards the new [`IceGatheringState`] to the provided
        /// [`DynPeerConnectionEventsHandler`] when an
        /// [`icegatheringstatechange`][1] event occurs in the attached
        /// [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#event-icegatheringstatechange
        pub fn on_ice_gathering_change(
            cb: &mut DynPeerConnectionEventsHandler,
            new_state: IceGatheringState,
        );

        /// Forwards a [`negotiation`][1] event to the provided
        /// [`DynPeerConnectionEventsHandler`] when it occurs in the attached
        /// [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#event-negotiation
        pub fn on_negotiation_needed_event(
            cb: &mut DynPeerConnectionEventsHandler,
            event_id: u32,
        );

        /// Forwards an [`icecandidateerror`][1] event's error information to
        /// the provided [`DynPeerConnectionEventsHandler`] when it occurs in
        /// the attached [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#event-icecandidateerror
        pub fn on_ice_candidate_error(
            cb: &mut DynPeerConnectionEventsHandler,
            address: &CxxString,
            port: i32,
            url: &CxxString,
            error_code: i32,
            error_text: &CxxString,
        );

        /// Forwards the new `receiving` status to the provided
        /// [`DynPeerConnectionEventsHandler`] when an ICE connection receiving
        /// status changes in the attached [`PeerConnectionInterface`].
        pub fn on_ice_connection_receiving_change(
            cb: &mut DynPeerConnectionEventsHandler,
            receiving: bool,
        );

        /// Forwards the discovered [`IceCandidateInterface`] to the provided
        /// [`DynPeerConnectionEventsHandler`] when an [`icecandidate`][1] event
        /// occurs in the attached [`PeerConnectionInterface`].
        ///
        /// [1]: https://w3.org/TR/webrtc#event-icecandidate
        pub unsafe fn on_ice_candidate(
            cb: &mut DynPeerConnectionEventsHandler,
            candidate: *const IceCandidateInterface,
        );

        /// Forwards the removed [`Candidate`]s to the given
        /// [`DynPeerConnectionEventsHandler`] when some ICE candidates have
        /// been removed.
        pub fn on_ice_candidates_removed(
            cb: &mut DynPeerConnectionEventsHandler,
            candidates: &CxxVector<Candidate>,
        );

        /// Forwards the selected [`CandidatePairChangeEvent`] to the provided
        /// [`DynPeerConnectionEventsHandler`] when a
        /// [`selectedcandidatepairchange`][1] event occurs in the attached
        /// [`PeerConnectionInterface`].
        ///
        /// [1]: https://tinyurl.com/w3-selectedcandidatepairchange
        pub fn on_ice_selected_candidate_pair_changed(
            cb: &mut DynPeerConnectionEventsHandler,
            event: &CandidatePairChangeEvent,
        );

        // todo
        pub fn on_track(
            cb: &mut DynPeerConnectionEventsHandler,
            event: &RtpTransceiverInterface,
        );

        // todo
        pub fn on_remove_track(
            cb: &mut DynPeerConnectionEventsHandler,
            event: &RtpReceiverInterface,
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

/// Forwards the given [`webrtc::VideoFrame`] the the provided
/// [`DynOnFrameCallback`].
fn on_frame(cb: &mut DynOnFrameCallback, frame: UniquePtr<webrtc::VideoFrame>) {
    cb.on_frame(frame);
}

/// Forwards the new [`SignalingState`] to the provided
/// [`DynPeerConnectionEventsHandler`] when a [`signalingstatechange`][1] event
/// occurs in the attached [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
/// [`SignalingState`]: webrtc::SignalingState
/// [1]: https://w3.org/TR/webrtc#event-signalingstatechange
pub fn on_signaling_change(
    cb: &mut DynPeerConnectionEventsHandler,
    state: webrtc::SignalingState,
) {
    cb.on_signaling_change(state);
}

/// Forwards the new [`IceConnectionState`] to the provided
/// [`DynPeerConnectionEventsHandler`] when an [`iceconnectionstatechange`][1]
/// event occurs in the attached [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
/// [`IceConnectionState`]: webrtc::IceConnectionState
/// [1]: https://w3.org/TR/webrtc#event-iceconnectionstatechange
pub fn on_standardized_ice_connection_change(
    cb: &mut DynPeerConnectionEventsHandler,
    new_state: webrtc::IceConnectionState,
) {
    cb.on_standardized_ice_connection_change(new_state);
}

/// Forwards the new [`PeerConnectionState`] to the provided
/// [`DynPeerConnectionEventsHandler`] when a [`connectionstatechange`][1] event
/// occurs in the attached [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
/// [`PeerConnectionState`]: webrtc::PeerConnectionState
/// [1]: https://w3.org/TR/webrtc#event-connectionstatechange
pub fn on_connection_change(
    cb: &mut DynPeerConnectionEventsHandler,
    new_state: webrtc::PeerConnectionState,
) {
    cb.on_connection_change(new_state);
}

/// Forwards the new [`IceGatheringState`] to the provided
/// [`DynPeerConnectionEventsHandler`] when an [`icegatheringstatechange`][1]
/// event occurs in the attached [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
/// [`IceGatheringState`]: webrtc::IceGatheringState
/// [1]: https://w3.org/TR/webrtc#event-icegatheringstatechange
pub fn on_ice_gathering_change(
    cb: &mut DynPeerConnectionEventsHandler,
    new_state: webrtc::IceGatheringState,
) {
    cb.on_ice_gathering_change(new_state);
}

/// Forwards a [`negotiation`][1] event to the provided
/// [`DynPeerConnectionEventsHandler`] when it occurs in the attached
/// [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
/// [1]: https://w3.org/TR/webrtc#event-negotiation
pub fn on_negotiation_needed_event(
    cb: &mut DynPeerConnectionEventsHandler,
    event_id: u32,
) {
    cb.on_negotiation_needed_event(event_id);
}

/// Forwards an [`icecandidateerror`][1] event's error information to the
/// provided [`DynPeerConnectionEventsHandler`] when it occurs in the attached
/// [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
/// [1]: https://w3.org/TR/webrtc#event-icecandidateerror
pub fn on_ice_candidate_error(
    cb: &mut DynPeerConnectionEventsHandler,
    address: &CxxString,
    port: i32,
    url: &CxxString,
    error_code: i32,
    error_text: &CxxString,
) {
    cb.on_ice_candidate_error(address, port, url, error_code, error_text);
}

/// Forwards the new `receiving` status to the provided
/// [`DynPeerConnectionEventsHandler`] when an ICE connection receiving status
/// changes in the attached [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
pub fn on_ice_connection_receiving_change(
    cb: &mut DynPeerConnectionEventsHandler,
    receiving: bool,
) {
    cb.on_ice_connection_receiving_change(receiving);
}

/// Forwards the discovered [`IceCandidateInterface`] to the provided
/// [`DynPeerConnectionEventsHandler`] when an [`icecandidate`][1] event occurs
/// in the attached [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
/// [`IceCandidateInterface`]: webrtc::IceCandidateInterface
/// [1]: https://w3.org/TR/webrtc#event-icecandidate
pub fn on_ice_candidate(
    cb: &mut DynPeerConnectionEventsHandler,
    candidate: *const webrtc::IceCandidateInterface,
) {
    cb.on_ice_candidate(candidate);
}

/// Forwards the removed [`Candidate`]s to the given
/// [`DynPeerConnectionEventsHandler`] when some ICE candidates have been
/// removed.
///
/// [`Candidate`]: webrtc::Candidate
pub fn on_ice_candidates_removed(
    cb: &mut DynPeerConnectionEventsHandler,
    candidates: &CxxVector<webrtc::Candidate>,
) {
    cb.on_ice_candidates_removed(candidates);
}

/// Called when a [`selectedcandidatepairchange`][1] event occurs in the
/// attached [`PeerConnectionInterface`]. Forwards the selected
/// [`CandidatePairChangeEvent`] to the given
/// [`DynPeerConnectionEventsHandler`].
///
/// Forwards the selected [`CandidatePairChangeEvent`] to the provided
/// [`DynPeerConnectionEventsHandler`] when a [`selectedcandidatepairchange`][1]
/// event occurs in the attached [`PeerConnectionInterface`].
///
/// [`PeerConnectionInterface`]: webrtc::PeerConnectionInterface
/// [`CandidatePairChangeEvent`]: webrtc::CandidatePairChangeEvent
/// [1]: https://tinyurl.com/w3-selectedcandidatepairchange
pub fn on_ice_selected_candidate_pair_changed(
    cb: &mut DynPeerConnectionEventsHandler,
    event: &webrtc::CandidatePairChangeEvent,
) {
    cb.on_ice_selected_candidate_pair_changed(event);
}

// todo
pub fn on_track(
    cb: &mut DynPeerConnectionOnEvent,
    event: &webrtc::RtpTransceiverInterface,
) {
    cb.on_track(event);
}

// todo
pub fn on_remove_track(
    cb: &mut DynPeerConnectionOnEvent,
    event: &webrtc::RtpReceiverInterface,
) {
    cb.on_remove_track(event);
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

impl TryFrom<&str> for webrtc::MediaType {
    type Error = anyhow::Error;

    fn try_from(val: &str) -> Result<Self, Self::Error> {
        match val {
            "audio" => Ok(Self::MEDIA_TYPE_AUDIO),
            "video" => Ok(Self::MEDIA_TYPE_VIDEO),
            "data" => Ok(Self::MEDIA_TYPE_DATA),
            "unsupported" => Ok(Self::MEDIA_TYPE_UNSUPPORTED),
            v => Err(anyhow!("Invalid `MediaType`: {v}")),
        }
    }
}

impl TryFrom<&str> for webrtc::RtpTransceiverDirection {
    type Error = anyhow::Error;

    fn try_from(val: &str) -> Result<Self, Self::Error> {
        match val {
            "sendrecv" => Ok(Self::kSendRecv),
            "sendonly" => Ok(Self::kSendOnly),
            "recvonly" => Ok(Self::kRecvOnly),
            "stopped" => Ok(Self::kStopped),
            "inactive" => Ok(Self::kInactive),
            v => Err(anyhow!("Invalid `RtpTransceiverDirection`: {v}")),
        }
    }
}

impl fmt::Display for webrtc::SdpType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use webrtc::SdpType as ST;

        match *self {
            ST::kOffer => write!(f, "offer"),
            ST::kAnswer => write!(f, "answer"),
            ST::kPrAnswer => write!(f, "pranswer"),
            ST::kRollback => write!(f, "rollback"),
            _ => unreachable!(),
        }
    }
}

impl fmt::Display for webrtc::RtpTransceiverDirection {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use webrtc::RtpTransceiverDirection as D;

        match *self {
            D::kSendRecv => write!(f, "sendrecv"),
            D::kSendOnly => write!(f, "sendonly"),
            D::kRecvOnly => write!(f, "recvonly"),
            D::kInactive => write!(f, "inactive"),
            D::kStopped => write!(f, "stopped"),
            _ => unreachable!(),
        }
    }
}

impl fmt::Display for webrtc::SignalingState {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use webrtc::SignalingState as S;

        match *self {
            S::kStable => write!(f, "stable"),
            S::kHaveLocalOffer => write!(f, "have-local-offer"),
            S::kHaveLocalPrAnswer => write!(f, "have-local-pranswer"),
            S::kHaveRemoteOffer => write!(f, "have-remote-offer"),
            S::kHaveRemotePrAnswer => write!(f, "have-remote-pranswer"),
            S::kClosed => write!(f, "closed"),
            _ => unreachable!(),
        }
    }
}

impl fmt::Display for webrtc::IceGatheringState {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use webrtc::IceGatheringState as S;

        match *self {
            S::kIceGatheringNew => write!(f, "new"),
            S::kIceGatheringGathering => write!(f, "gathering"),
            S::kIceGatheringComplete => write!(f, "complete"),
            _ => unreachable!(),
        }
    }
}

impl fmt::Display for webrtc::IceConnectionState {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use webrtc::IceConnectionState as S;

        match *self {
            S::kIceConnectionNew => write!(f, "new"),
            S::kIceConnectionChecking => write!(f, "checking"),
            S::kIceConnectionConnected => write!(f, "connected"),
            S::kIceConnectionCompleted => write!(f, "completed"),
            S::kIceConnectionFailed => write!(f, "failed"),
            S::kIceConnectionDisconnected => write!(f, "disconnected"),
            S::kIceConnectionClosed => write!(f, "closed"),
            _ => unreachable!(),
        }
    }
}

impl ToString for webrtc::TrackState {
    fn to_string(&self) -> String {
        match *self {
            webrtc::TrackState::kLive => "live".to_owned(),
            webrtc::TrackState::kEnded => "ended".to_owned(),
            _ => unreachable!(),
        }
    }
}

impl ToString for webrtc::MediaType {
    fn to_string(&self) -> String {
        match *self {
            webrtc::MediaType::MEDIA_TYPE_AUDIO => "audio".to_owned(),
            webrtc::MediaType::MEDIA_TYPE_VIDEO => "video".to_owned(),
            webrtc::MediaType::MEDIA_TYPE_DATA => "data".to_owned(),

            // todo recheck
            webrtc::MediaType::MEDIA_TYPE_UNSUPPORTED => "unsupport".to_owned(),
            _ => unreachable!(),
        }
    }
}

impl ToString for webrtc::RtpTransceiverDirection {
    fn to_string(&self) -> String {
        match *self {
            webrtc::RtpTransceiverDirection::kSendRecv => "sendrecv".to_owned(),
            webrtc::RtpTransceiverDirection::kSendOnly => "sendonly".to_owned(),
            webrtc::RtpTransceiverDirection::kRecvOnly => "recvonly".to_owned(),
            webrtc::RtpTransceiverDirection::kInactive => "inactive".to_owned(),
            webrtc::RtpTransceiverDirection::kStopped => "stopped".to_owned(),
            _ => unreachable!(),
        }
    }
}

fn new_string_pair(f: &CxxString, s: &CxxString) -> webrtc::StringPair {
    webrtc::StringPair {
        first: f.to_string(),
        second: s.to_string(),
    }
}

/// Forwards the given [`webrtc::VideoFrame`] the the provided
/// [`DynOnFrameCallback`].
fn on_frame(cb: &mut DynOnFrameCallback, frame: UniquePtr<webrtc::VideoFrame>) {
    cb.on_frame(frame);
}


impl fmt::Display for webrtc::PeerConnectionState {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use webrtc::PeerConnectionState as S;

        match *self {
            S::kNew => write!(f, "new"),
            S::kConnecting => write!(f, "connecting"),
            S::kConnected => write!(f, "connected"),
            S::kDisconnected => write!(f, "disconnected"),
            S::kFailed => write!(f, "failed"),
            S::kClosed => write!(f, "closed"),
            _ => unreachable!(),
        }
    }
}
