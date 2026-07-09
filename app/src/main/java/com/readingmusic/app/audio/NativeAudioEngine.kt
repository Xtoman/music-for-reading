package com.readingmusic.app.audio

class NativeAudioEngine {
    external fun nativeInit(): Boolean
    external fun nativeStart(): Boolean
    external fun nativeStop()
    external fun nativeSetStyle(style: Int)
    external fun nativeSetVolume(volume: Float)
    external fun nativeSetFadeSeconds(seconds: Float)
    external fun nativeRequestSleepFade(seconds: Float)
    external fun nativeIsRunning(): Boolean
    external fun nativeRelease()

    fun init(): Boolean = nativeInit()
    fun start(): Boolean = nativeStart()
    fun stop() = nativeStop()
    fun setStyle(style: Int) = nativeSetStyle(style)
    fun setVolume(volume: Float) = nativeSetVolume(volume)
    fun setFadeSeconds(seconds: Float) = nativeSetFadeSeconds(seconds)
    fun requestSleepFade(seconds: Float) = nativeRequestSleepFade(seconds)
    fun isRunning(): Boolean = nativeIsRunning()
    fun release() = nativeRelease()

    companion object {
        init {
            System.loadLibrary("readingmusic")
        }
    }
}
