#![warn(clippy::pedantic)]

mod peer_connection;

use std::{cell::RefCell, collections::HashMap, rc::Rc};

use libwebrtc_sys::{
    AudioDeviceModule, AudioLayer, PeerConnectionFactoryInterface,
    TaskQueueFactory, VideoDeviceInfo,
};

use peer_connection::{PeerConnection, PeerConnectionId, PeerConnection_};

use self::ffi::{MediaDeviceInfo, MediaDeviceKind};

/// The module which describes the bridge to call Rust from C++.
#[allow(clippy::items_after_statements, clippy::expl_impl_clone_on_copy)]
#[cxx::bridge]
pub mod ffi {
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

    extern "Rust" {
        type Webrtc;
        type PeerConnection_;

        /// Creates an instance of [Webrtc].
        #[cxx_name = "Init"]
        fn init() -> Box<Webrtc>;

        /// Returns a list of all available media input and output devices, such
        /// as microphones, cameras, headsets, and so forth.
        #[cxx_name = "EnumerateDevices"]
        fn enumerate_devices() -> Vec<MediaDeviceInfo>;

        #[cxx_name = "CreatePeerConnection"]
        fn create_default_peer_connection(self: &mut Webrtc) -> u64;

        #[cxx_name = "GetPeerConnectionFromId"]
        fn get_peer_connection_from_id(
            self: &Webrtc,
            id: u64,
        ) -> Box<PeerConnection_>;

    }
}

/// Returns a list of all available media input and output devices, such as
/// microphones, cameras, headsets, and so forth.
#[must_use]
pub fn enumerate_devices() -> Vec<MediaDeviceInfo> {
    let mut audio = audio_devices_info();
    let mut video = video_devices_info();

    audio.append(&mut video);

    audio
}

/// Returns a list of all available audio input and output devices.
fn audio_devices_info() -> Vec<MediaDeviceInfo> {
    // TODO: Do not unwrap.
    let mut task_queue = TaskQueueFactory::create_default_task_queue_factory();
    let adm = AudioDeviceModule::create(
        AudioLayer::kPlatformDefaultAudio,
        &mut task_queue,
    )
    .unwrap();
    adm.init().unwrap();

    let count_playout = adm.playout_devices().unwrap();
    let count_recording = adm.recording_devices().unwrap();

    #[allow(clippy::cast_sign_loss)]
    let mut result =
        Vec::with_capacity((count_playout + count_recording) as usize);

    for kind in [MediaDeviceKind::kAudioOutput, MediaDeviceKind::kAudioInput] {
        let count = if let MediaDeviceKind::kAudioOutput = kind {
            count_playout
        } else {
            count_recording
        };

        for i in 0..count {
            let (label, device_id) = if let MediaDeviceKind::kAudioOutput = kind
            {
                adm.playout_device_name(i).unwrap()
            } else {
                adm.recording_device_name(i).unwrap()
            };

            result.push(MediaDeviceInfo {
                device_id,
                kind,
                label,
            });
        }
    }

    result
}

/// Returns a list of all available video input devices.
fn video_devices_info() -> Vec<MediaDeviceInfo> {
    // TODO: Do not unwrap.
    let mut vdi = VideoDeviceInfo::create().unwrap();
    let count = vdi.number_of_devices();
    let mut result = Vec::with_capacity(count as usize);

    for i in 0..count {
        let (label, device_id) = vdi.device_name(i).unwrap();

        result.push(MediaDeviceInfo {
            device_id,
            kind: MediaDeviceKind::kVideoInput,
            label,
        });
    }

    result
}

/// Contains all necessary tools for interoperate with [`libWebRTC`].
///
/// [`libWebrtc`]: https://tinyurl.com/54y935zz
pub struct Inner {
    task_queue_factory: TaskQueueFactory,
    peer_connection_factory: PeerConnectionFactoryInterface,
    peer_connections: HashMap<u64, Rc<RefCell<PeerConnection>>>,
}

/// Wraps the [`Inner`] instanse.
/// This struct is intended to be extern and managed outside of the Rust app.
pub struct Webrtc(Box<Inner>);

/// Creates an instanse of [`Webrtc`].
///
/// # Panics
///
/// May panic if `PeerconnectionFactory` is not valiable to be created.
#[must_use]
pub fn init() -> Box<Webrtc> {
    let task_queue_factory =
        TaskQueueFactory::create_default_task_queue_factory();
    let peer_connection_factory =
        PeerConnectionFactoryInterface::create_whith_null();

    Box::new(Webrtc(Box::new(Inner {
        task_queue_factory,
        peer_connection_factory,
        peer_connections: HashMap::new(),
    })))
}
