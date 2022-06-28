package com.instrumentisto.medea_jason_webrtc.utils

/** Generates unique IDs for local media tracks. */
object LocalTrackIdGenerator {
  /** Last created unique ID. */
  private var lastId: Int = 0

  /** @return New unique ID for the local media track. */
  fun nextId(): String {
    return "local-" + lastId++.toString()
  }
}
