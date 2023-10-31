package com.instrumentisto.medea_flutter_webrtc.controller

import com.instrumentisto.medea_flutter_webrtc.TrackRepository
import com.instrumentisto.medea_flutter_webrtc.proxy.RtpSenderProxy
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel

/**
 * Controller of [RtpSenderProxy] functional.
 *
 * @param messenger Messenger used for creating new [MethodChannel]s.
 * @property sender Underlying [RtpSenderProxy] to perform [MethodCall]s on.
 */
class RtpSenderController(messenger: BinaryMessenger, private val sender: RtpSenderProxy) :
    MethodChannel.MethodCallHandler, IdentifiableController {
  /** Unique ID of the [MethodChannel] of this controller. */
  private val channelId = nextChannelId()

  /** Channel listened for the [MethodCall]s. */
  private val chan = MethodChannel(messenger, ChannelNameGenerator.name("RtpSender", channelId))

  init {
    chan.setMethodCallHandler(this)
  }

  override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
    when (call.method) {
      "replaceTrack" -> {
        val trackId: String? = call.argument("trackId")
        val track =
            if (trackId != null) {
              TrackRepository.getTrack(trackId)!!
            } else {
              null
            }
        sender.replaceTrack(track)
        result.success(null)
      }
      "encodings" -> {
        val res =
            sender.getParameters().encodings.mapIndexed { index, enc ->
              mapOf(
                  "index" to index,
                  "rid" to enc.rid,
                  "active" to enc.active,
                  "maxBitrate" to enc.maxBitrateBps,
                  "maxFramerate" to enc.maxFramerate,
                  "scaleResolutionDownBy" to enc.scaleResolutionDownBy,
              )
            }

        result.success(res)
      }
      "setParameters" -> {
        val params = sender.getParameters()
        val encodings: List<Map<String, Any>>? = call.argument("encodings")

        encodings?.forEach {
          val enc = params.encodings[it["index"] as Int]
          enc.active = it["active"] as Boolean
          enc.maxBitrateBps = it["maxBitrate"] as Int?
          enc.maxFramerate = it["maxFramerate"] as Int?
          enc.scaleResolutionDownBy = it["scaleResolutionDownBy"] as Double?
        }

        if (encodings != null && !sender.setParameters(params)) {
          result.error("SenderException", "Could not set parameters", null)
        }

        result.success(null)
      }
      "dispose" -> {
        chan.setMethodCallHandler(null)
        result.success(null)
      }
    }
  }

  /**
   * Converts this [RtpSenderController] to the Flutter's method call result.
   *
   * @return [Map] generated from this controller which can be returned to the Flutter side.
   */
  fun asFlutterResult(): Map<String, Any> = mapOf("channelId" to channelId)
}
