package com.readingmusic.app.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.IBinder
import android.support.v4.media.session.MediaSessionCompat
import androidx.core.app.NotificationCompat
import androidx.media.app.NotificationCompat.MediaStyle
import com.readingmusic.app.MainActivity
import com.readingmusic.app.R
import com.readingmusic.app.domain.MusicStyle

class MusicPlaybackService : Service() {

    private var mediaSession: MediaSessionCompat? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        mediaSession = MediaSessionCompat(this, "ReadingMusicSession").apply {
            isActive = true
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
                return START_NOT_STICKY
            }
            ACTION_START, null -> {
                val styleId = intent?.getIntExtra(EXTRA_STYLE_ID, 0) ?: 0
                val style = MusicStyle.fromId(styleId)
                startForeground(NOTIFICATION_ID, buildNotification(style))
            }
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        mediaSession?.isActive = false
        mediaSession?.release()
        mediaSession = null
        super.onDestroy()
    }

    private fun buildNotification(style: MusicStyle): Notification {
        val openIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java).apply {
                putExtra(EXTRA_STYLE_ID, style.id)
                flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
            },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val stopIntent = PendingIntent.getService(
            this,
            1,
            Intent(this, MusicPlaybackService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.notification_title))
            .setContentText(getString(style.titleRes))
            .setSmallIcon(R.drawable.ic_notification)
            .setContentIntent(openIntent)
            .addAction(
                R.drawable.ic_notification,
                getString(R.string.stop),
                stopIntent
            )
            .setOngoing(true)
            .setSilent(true)
            .setStyle(
                MediaStyle()
                    .setMediaSession(mediaSession?.sessionToken)
                    .setShowActionsInCompactView(0)
            )
            .build()
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            getString(R.string.notification_channel_name),
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = getString(R.string.notification_channel_desc)
            setSound(null, null)
        }
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(channel)
    }

    companion object {
        const val CHANNEL_ID = "reading_music_playback"
        const val NOTIFICATION_ID = 1001
        const val EXTRA_STYLE_ID = "extra_style_id"
        private const val ACTION_START = "com.readingmusic.app.action.START"
        private const val ACTION_STOP = "com.readingmusic.app.action.STOP"

        fun start(context: Context, styleId: Int) {
            val intent = Intent(context, MusicPlaybackService::class.java).apply {
                action = ACTION_START
                putExtra(EXTRA_STYLE_ID, styleId)
            }
            context.startForegroundService(intent)
        }

        fun stop(context: Context) {
            val intent = Intent(context, MusicPlaybackService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }
}
