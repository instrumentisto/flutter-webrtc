package com.cloudwebrtc.webrtc.controller

import com.cloudwebrtc.webrtc.TrackRepository
import com.cloudwebrtc.webrtc.proxy.RtpSenderProxy
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel

/**
 * Controller for the [RtpSenderProxy] functional.
 */
class RtpSenderController(messenger: BinaryMessenger, private val sender: RtpSenderProxy) :
    MethodChannel.MethodCallHandler, IdentifiableController {
    private val channelId = nextChannelId()
    private val methodChannel =
        MethodChannel(messenger, ChannelNameGenerator.withId("RtpSender", channelId))

    init {
        methodChannel.setMethodCallHandler(this);
    }

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "setTrack" -> {
                val trackId: String? = call.argument("trackId")
                val track = if (trackId != null) {
                    TrackRepository.getTrack(trackId)!!
                } else {
                    null
                }
                sender.setTrack(track)
                result.success(null)
            }
            "dispose" -> {
                dispose()
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

    /**
     * Closes method channel of this [RtpSenderController].
     */
    private fun dispose() {
        methodChannel.setMethodCallHandler(null)
    }
}