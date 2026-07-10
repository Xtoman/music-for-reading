package com.readingmusic.app.domain

import com.readingmusic.app.audio.NativeAudioEngine
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class PlaybackController(
    private val engine: NativeAudioEngine = NativeAudioEngine()
) {
    private val _isPlaying = MutableStateFlow(false)
    val isPlaying: StateFlow<Boolean> = _isPlaying.asStateFlow()

    private val _currentStyle = MutableStateFlow(MusicStyle.FANTASY)
    val currentStyle: StateFlow<MusicStyle> = _currentStyle.asStateFlow()

    private val _volume = MutableStateFlow(0.85f)
    val volume: StateFlow<Float> = _volume.asStateFlow()

    private val _sleepTimer = MutableStateFlow(SleepTimerMinutes.OFF)
    val sleepTimer: StateFlow<SleepTimerMinutes> = _sleepTimer.asStateFlow()

    private var initialized = false

    fun ensureInitialized(): Boolean {
        if (!initialized) {
            initialized = engine.init()
        }
        return initialized
    }

    fun play(style: MusicStyle? = null) {
        if (!ensureInitialized()) return
        style?.let {
            _currentStyle.value = it
            engine.setStyle(it.id)
        }
        engine.setVolume(_volume.value)
        engine.setFadeSeconds(3f)
        if (engine.start()) {
            _isPlaying.value = true
        }
    }

    fun pause() {
        engine.stop()
        _isPlaying.value = false
    }

    fun toggle(style: MusicStyle? = null) {
        if (_isPlaying.value) pause() else play(style)
    }

    fun setStyle(style: MusicStyle) {
        _currentStyle.value = style
        if (ensureInitialized()) {
            engine.setStyle(style.id)
            if (!_isPlaying.value) {
                play(style)
            }
        }
    }

    fun setVolume(value: Float) {
        val clamped = value.coerceIn(0f, 1f)
        _volume.value = clamped
        if (ensureInitialized()) {
            engine.setVolume(clamped)
        }
    }

    fun setSleepTimer(timer: SleepTimerMinutes) {
        _sleepTimer.value = timer
    }

    fun applySleepFadeIfNeeded() {
        if (ensureInitialized()) {
            engine.requestSleepFade(5f)
        }
    }

    fun release() {
        engine.release()
        initialized = false
        _isPlaying.value = false
    }
}
