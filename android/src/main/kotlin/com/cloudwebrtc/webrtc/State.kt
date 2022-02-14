package com.cloudwebrtc.webrtc

import android.content.Context
import android.media.AudioManager
import com.cloudwebrtc.webrtc.utils.EglUtils
import org.webrtc.DefaultVideoDecoderFactory
import org.webrtc.DefaultVideoEncoderFactory
import org.webrtc.EglBase
import org.webrtc.PeerConnectionFactory
import org.webrtc.audio.JavaAudioDeviceModule

/**
 * Global context for the flutter_webrtc library.
 *
 * Used for creating tracks, peers, perform gUM requests.
 *
 * @property context Android [Context] used, for example, for gUM requests.
 */
class State(private val context: Context) {
    /**
     * Module for the controlling audio devices in context of libwebrtc.
     */
    private var audioDeviceModule: JavaAudioDeviceModule? = null

    /**
     * Factory for the `PeerConnection`s, `MediaStreamTrack`s.
     *
     * Will be lazily initialized on the first call of [getPeerConnectionFactory].
     */
    private var factory: PeerConnectionFactory? = null

    init {
        PeerConnectionFactory.initialize(
            PeerConnectionFactory.InitializationOptions.builder(context)
                .setEnableInternalTracer(true)
                .createInitializationOptions()
        )
    }

    /**
     * Initializes new [factory].
     */
    private fun initPeerConnectionFactory() {
        val audioModule = JavaAudioDeviceModule.builder(context)
            .setUseHardwareAcousticEchoCanceler(true)
            .setUseHardwareNoiseSuppressor(true)
            .createAudioDeviceModule()
        val eglContext: EglBase.Context = EglUtils.rootEglBaseContext!!
        val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        audioManager.mode = AudioManager.MODE_IN_CALL
        audioManager.isSpeakerphoneOn = true
        factory = PeerConnectionFactory.builder()
            .setOptions(PeerConnectionFactory.Options())
            .setVideoEncoderFactory(
                DefaultVideoEncoderFactory(eglContext, true, true)
            )
            .setVideoDecoderFactory(DefaultVideoDecoderFactory(eglContext))
            .setAudioDeviceModule(audioModule)
            .createPeerConnectionFactory()
        audioModule.setSpeakerMute(false)
        audioDeviceModule = audioModule
    }

    /**
     * Initializes [PeerConnectionFactory] if it wasn't initiaized before.
     *
     * @return current [PeerConnectionFactory] of this [State].
     */
    fun getPeerConnectionFactory(): PeerConnectionFactory {
        if (factory == null) {
            initPeerConnectionFactory()
        }
        return factory!!
    }

    /**
     * @return Android SDK [Context].
     */
    fun getAppContext(): Context {
        return context
    }
}