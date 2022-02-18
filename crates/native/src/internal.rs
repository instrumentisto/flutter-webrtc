pub use cpp_api_bindings::*;

#[allow(clippy::items_after_statements)]
#[cxx::bridge]
mod cpp_api_bindings {
    unsafe extern "C++" {
        include!("flutter-webrtc-native/include/api.h");
        include!("flutter-webrtc-native/src/lib.rs.h");

        pub type CreateSdpCallbackInterface;
        pub type SetDescriptionCallbackInterface;
        pub type OnFrameCallbackInterface;
        pub type PeerConnectionObserverInterface;
        pub type TrackEventInterface;

        type VideoFrame = crate::api::VideoFrame;
        type OnTrackSerialized = crate::api::OnTrackSerialized;

        /// Calls C++ side `CreateSdpCallbackInterface->OnSuccess`.
        #[cxx_name = "OnSuccess"]
        pub fn on_create_sdp_success(
            self: Pin<&mut CreateSdpCallbackInterface>,
            sdp: &CxxString,
            kind: &CxxString,
        );

        /// Calls C++ side `CreateSdpCallbackInterface->OnFail`.
        #[cxx_name = "OnFail"]
        pub fn on_create_sdp_fail(
            self: Pin<&mut CreateSdpCallbackInterface>,
            error: &CxxString,
        );

        /// Calls C++ side `SetDescriptionCallbackInterface->OnSuccess`.
        #[cxx_name = "OnSuccess"]
        pub fn on_set_description_sucess(
            self: Pin<&mut SetDescriptionCallbackInterface>,
        );

        /// Calls C++ side `SetDescriptionCallbackInterface->OnFail`.
        #[cxx_name = "OnFail"]
        pub fn on_set_description_fail(
            self: Pin<&mut SetDescriptionCallbackInterface>,
            error: &CxxString,
        );

        /// Calls C++ side `OnFrameCallbackInterface->OnFrame`.
        #[cxx_name = "OnFrame"]
        pub fn on_frame(
            self: Pin<&mut OnFrameCallbackInterface>,
            frame: VideoFrame,
        );

        /// Calls C++ side `PeerConnectionObserverInterface->OnSignalingChange`.
        #[cxx_name = "OnSignalingChange"]
        pub fn on_signaling_change(
            self: Pin<&mut PeerConnectionObserverInterface>,
            new_state: &CxxString,
        );

        /// Calls C++ side
        /// `PeerConnectionObserverInterface->OnIceConnectionStateChange`.
        #[cxx_name = "OnIceConnectionStateChange"]
        pub fn on_ice_connection_state_change(
            self: Pin<&mut PeerConnectionObserverInterface>,
            new_state: &CxxString,
        );

        /// Calls C++ side
        /// `PeerConnectionObserverInterface->OnConnectionStateChange`.
        #[cxx_name = "OnConnectionStateChange"]
        pub fn on_connection_state_change(
            self: Pin<&mut PeerConnectionObserverInterface>,
            new_state: &CxxString,
        );

        /// Calls C++ side
        /// `PeerConnectionObserverInterface->OnIceGatheringStateChange`.
        #[cxx_name = "OnIceGatheringStateChange"]
        pub fn on_ice_gathering_change(
            self: Pin<&mut PeerConnectionObserverInterface>,
            new_state: &CxxString,
        );

        /// Calls C++ side
        /// `PeerConnectionObserverInterface->OnNegotiationNeeded`.
        #[cxx_name = "OnNegotiationNeeded"]
        pub fn on_negotiation_needed(
            self: Pin<&mut PeerConnectionObserverInterface>,
        );

        /// Calls C++ side
        /// `PeerConnectionObserverInterface->OnIceCandidateError`.
        #[cxx_name = "OnIceCandidateError"]
        pub fn on_ice_candidate_error(
            self: Pin<&mut PeerConnectionObserverInterface>,
            address: &CxxString,
            port: i32,
            url: &CxxString,
            error_code: i32,
            error_text: &CxxString,
        );

        /// Calls C++ side `PeerConnectionObserverInterface->OnIceCandidate`.
        #[cxx_name = "OnIceCandidate"]
        pub fn on_ice_candidate(
            self: Pin<&mut PeerConnectionObserverInterface>,
            candidate: &CxxString,
        );

        /// Calls C++ side `PeerConnectionObserverInterface->OnTrack`.
        #[cxx_name = "OnTrack"]
        pub fn on_track(
            self: Pin<&mut PeerConnectionObserverInterface>,
            event: OnTrackSerialized,
        );

        /// Calls C++ side `TrackEventInterface->OnEnded`.
        #[cxx_name = "OnEnded"]
        pub fn on_ended(
            self: Pin<&mut TrackEventInterface>,
        );

    }

    // This will trigger `cxx` to generate `UniquePtrTarget` trait for the
    // mentioned types.
    extern "Rust" {
        fn _touch_create_sdp_callback(i: UniquePtr<CreateSdpCallbackInterface>);
        fn _touch_set_description_callback(
            i: UniquePtr<SetDescriptionCallbackInterface>,
        );
        fn _touch_unique_ptr_on_frame_handler(
            i: UniquePtr<OnFrameCallbackInterface>,
        );
        fn _touch_unique_ptr_peer_connection_on_event_interface(
            i: UniquePtr<PeerConnectionObserverInterface>,
        );

        fn _touch_track_event(i: UniquePtr<TrackEventInterface>);
    }
}

fn _touch_track_event(_: cxx::UniquePtr<TrackEventInterface>) {}
fn _touch_create_sdp_callback(_: cxx::UniquePtr<CreateSdpCallbackInterface>) {}

fn _touch_set_description_callback(
    _: cxx::UniquePtr<SetDescriptionCallbackInterface>,
) {
}

fn _touch_unique_ptr_on_frame_handler(
    _: cxx::UniquePtr<OnFrameCallbackInterface>,
) {
}

fn _touch_unique_ptr_peer_connection_on_event_interface(
    _: cxx::UniquePtr<PeerConnectionObserverInterface>,
) {
}
