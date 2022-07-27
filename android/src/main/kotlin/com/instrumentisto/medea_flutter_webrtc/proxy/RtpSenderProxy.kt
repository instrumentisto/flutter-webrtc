package com.instrumentisto.medea_flutter_webrtc.proxy

import com.instrumentisto.medea_flutter_webrtc.exception.ReplaceSenderTrackException
import org.webrtc.RtpSender

/**
 * Wrapper around an [RtpSender].
 *
 * @param sender Actual underlying [RtpSender].
 */
class RtpSenderProxy(sender: RtpSender) : Proxy<RtpSender>(sender) {

  /** [MediaStreamTrackProxy] of this [RtpReceiverProxy]. */
  private var track: MediaStreamTrackProxy? = null

  /** The disposed state of [obj]. */
  private var disposed: Boolean = false

  init {
    syncMediaStreamTrack()
    addOnSyncListener { syncMediaStreamTrack() }
  }

  /** Sets [disposed] to `true`. */
  fun setDisposed() {
    disposed = true
  }

  /**
   * Replaces [MediaStreamTrackProxy] of the underlying [RtpSender] with the provided one.
   *
   * @param t [MediaStreamTrackProxy] which will be set to the underlying [RtpSender].
   */
  fun replaceTrack(t: MediaStreamTrackProxy?) {
    if (!disposed) {
      track = t
      val isSuccessful = obj.setTrack(t?.obj, false)
      if (!isSuccessful) {
        throw ReplaceSenderTrackException()
      }
    }
  }

  /**
   * Synchronizes the [MediaStreamTrackProxy] of this [RtpSenderProxy] with the underlying
   * [RtpSender].
   */
  private fun syncMediaStreamTrack() {
    val newSenderTrack = obj.track()
    if (newSenderTrack == null) {
      track = null
    } else {
      if (track == null) {
        track = MediaStreamTrackProxy(newSenderTrack)
      } else {
        track!!.replace(newSenderTrack)
      }
    }
  }
}
