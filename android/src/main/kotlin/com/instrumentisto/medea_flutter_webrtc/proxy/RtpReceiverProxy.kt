package com.instrumentisto.medea_flutter_webrtc.proxy

import org.webrtc.RtpReceiver

/**
 * Wrapper around an [RtpReceiver].
 *
 * @param receiver Underlying [RtpReceiver].
 */
class RtpReceiverProxy(receiver: RtpReceiver) : Proxy<RtpReceiver>(receiver) {
  /** [MediaStreamTrackProxy] of this [RtpReceiverProxy]. */
  private var track: MediaStreamTrackProxy = MediaStreamTrackProxy(obj.track()!!)

  /** TODO */
  private lateinit var id: String

  init {
    id = obj.id()

    track.replace(obj.track()!!)
    addOnSyncListener { track.replace(obj.track()!!) }
  }

  /** TODO */
  fun setDisposed() {
    notifyRemoved()
    track.setDisposed()
  }

  /** @return Unique ID of the underlying [RtpReceiver]. */
  fun id(): String {
    return id
  }

  /** @return [MediaStreamTrackProxy] of this [RtpReceiverProxy]. */
  fun getTrack(): MediaStreamTrackProxy {
    return track
  }

  /**
   * Notifies [RtpReceiverProxy] about its [MediaStreamTrackProxy] being removed from the receiver.
   */
  fun notifyRemoved() {
    track.stop()
    track.observableEventBroadcaster().onEnded()
  }
}
