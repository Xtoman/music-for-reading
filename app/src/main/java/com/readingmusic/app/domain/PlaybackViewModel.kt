package com.readingmusic.app.domain

import android.app.Application
import androidx.lifecycle.AndroidViewModel

class PlaybackViewModel(application: Application) : AndroidViewModel(application) {
    val controller = PlaybackController()

    private val audioFocusHelper = AudioFocusHelper(
        context = application,
        onFocusLost = { controller.pause() },
        onFocusGained = { /* User can resume manually */ }
    )

    fun play(style: MusicStyle? = null) {
        if (audioFocusHelper.requestFocus()) {
            controller.play(style)
        }
    }

    fun pause() {
        controller.pause()
        audioFocusHelper.abandonFocus()
    }

    fun setStyle(style: MusicStyle) {
        controller.setStyle(style)
        if (controller.isPlaying.value) {
            audioFocusHelper.requestFocus()
        }
    }

    override fun onCleared() {
        controller.release()
        audioFocusHelper.abandonFocus()
        super.onCleared()
    }
}
