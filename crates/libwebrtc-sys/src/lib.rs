#[rustfmt::skip]
mod bridge;

use anyhow::{anyhow, bail, Result};
use cxx::UniquePtr;

use bridge::webrtc;

pub use webrtc::AudioLayer;

/// A thread safe task queue factory internally used in `WebRTC` that is
/// capable of creating [Task Queue]s.
///
/// [Task Queue Factory]: https://tinyurl.com/doc-threads
pub struct TaskQueueFactory(UniquePtr<webrtc::TaskQueueFactory>);

impl TaskQueueFactory {
    /// Creates a default [TaskQueueFactory] based on the current platform.
    pub fn create_default_task_queue_factory() -> Self {
        Self(webrtc::create_default_task_queue_factory())
    }
}

/// Available audio devices manager that is responsible for driving input
/// (microphone) and output (speaker) audio in `WebRTC`.
///
/// Backed by WebRTC's [Audio Device Module].
///
/// [Audio Device Module]: https://tinyurl.com/doc-adm
pub struct AudioDeviceModule(UniquePtr<webrtc::AudioDeviceModule>);

impl AudioDeviceModule {
    /// Creates a new [`AudioDeviceModule`] for the given [`AudioLayer`].
    pub fn create(
        audio_layer: AudioLayer,
        task_queue_factory: &mut TaskQueueFactory,
    ) -> Result<Self> {
        let ptr = webrtc::create_audio_device_module(
            audio_layer,
            task_queue_factory.0.pin_mut(),
        );

        if ptr.is_null() {
            bail!("Null pointer returned from AudioDeviceModule::Create()");
        }
        Self(ptr)
    }

    /// Initializes the current [`AudioDeviceModule`].
    pub fn init(&self) -> Result<()> {
        let result = webrtc::init_audio_device_module(&self.0);
        if result != 0 {
            bail!("AudioDeviceModule::Init() failed with `{}` code", result);
        }
        Ok(())
    }

    /// Returns count of available audio playout devices.
    pub fn playout_devices(&self) -> Result<u32> {
        let count = webrtc::playout_devices(&self.0);

        if count < 0 {
            bail!(
                "AudioDeviceModule::PlayoutDevices() failed with `{}` code",
                count
            );
        }
        Ok(count as u32)
    }

    /// Returns count of available audio recording devices.
    pub fn recording_devices(&self) -> Result<u32> {
        let count = webrtc::recording_devices(&self.0);

        if count < 0 {
            bail!(
                "AudioDeviceModule::RecordingDevices() failed with `{}` code",
                count
            );
        }
        Ok(count as u32)
    }

    /// Returns the `(label, id)` tuple for the given audio playout device
    /// `index`.
    pub fn playout_device_name(&self, index: i16) -> Result<(String, String)> {
        let mut name = String::new();
        let mut guid = String::new();

        let result =
            webrtc::playout_device_name(&self.0, index, &mut name, &mut guid);

        if result != 0 {
            bail!(
                "AudioDeviceModule::PlayoutDeviceName() failed with `{}` code",
                result
            );
        }

        Ok((name, guid))
    }

    /// Returns the `(label, id)` tuple for the given audio recording device
    /// `index`.
    pub fn recording_device_name(
        &self,
        index: i16,
    ) -> Result<(String, String)> {
        let mut name = String::new();
        let mut guid = String::new();

        let result =
            webrtc::recording_device_name(&self.0, index, &mut name, &mut guid);

        if result != 0 {
            bail!(
                "AudioDeviceModule::RecordingDeviceName() failed with \
                `{}` code",
                result
            );
        }

        Ok((name, guid))
    }
}

/// Interface for receiving information about available camera devices.
pub struct VideoDeviceInfo(UniquePtr<webrtc::VideoDeviceInfo>);

impl VideoDeviceInfo {
    /// Creates a new [VideoDeviceInfo].
    pub fn create_device_info() -> Result<Self> {
        let ptr = webrtc::create_video_device_info();

        if ptr.is_null() {
            bail!(
                "Null pointer returned from \
                VideoCaptureFactory::CreateDeviceInfo()"
            );
        }
        Self(ptr)
    }

    /// Returns count of a video recording devices.
    pub fn number_of_devices(&mut self) -> u32 {
        self.0.pin_mut().number_of_video_devices()
    }

    /// Returns the `(label, id)` tuple for the given video device `index`.
    pub fn device_name(&mut self, index: u32) -> Result<(String, String)> {
        let mut name = String::new();
        let mut guid = String::new();

        let result = webrtc::video_device_name(
            self.0.pin_mut(),
            index,
            &mut name,
            &mut guid,
        );

        if result != 0 {
            bail!(
                "AudioDeviceModule::GetDeviceName() failed with `{}` code",
                result
            );
        }

        Ok((name, guid))
    }
}
