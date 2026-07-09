package com.readingmusic.app

import android.app.Application
import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.readingmusic.app.domain.MusicStyle
import com.readingmusic.app.domain.PlaybackViewModel
import com.readingmusic.app.domain.SleepTimerMinutes
import com.readingmusic.app.service.MusicPlaybackService
import com.readingmusic.app.ui.HomeScreen
import com.readingmusic.app.ui.PlayerScreen
import com.readingmusic.app.ui.theme.ReadingMusicTheme
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {

    private val viewModel: PlaybackViewModel by viewModels()
    private var sleepTimerJob: Job? = null

    private val notificationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestNotificationPermissionIfNeeded()
        enableEdgeToEdge()
        viewModel.controller.ensureInitialized()

        val initialStyleId = intent?.getIntExtra(MusicPlaybackService.EXTRA_STYLE_ID, -1) ?: -1
        if (initialStyleId >= 0) {
            viewModel.setStyle(MusicStyle.fromId(initialStyleId))
        }

        setContent {
            val navController = rememberNavController()
            val controller = viewModel.controller
            val isPlaying by controller.isPlaying.collectAsState()
            val currentStyle by controller.currentStyle.collectAsState()
            val volume by controller.volume.collectAsState()
            val sleepTimer by controller.sleepTimer.collectAsState()

            ReadingMusicTheme {
                NavHost(
                    navController = navController,
                    startDestination = "home"
                ) {
                    composable("home") {
                        HomeScreen(
                            currentStyle = currentStyle,
                            isPlaying = isPlaying,
                            onStyleSelected = { style ->
                                viewModel.setStyle(style)
                                MusicPlaybackService.start(this@MainActivity, style.id)
                                navController.navigate("player")
                            }
                        )
                    }
                    composable("player") {
                        PlayerScreen(
                            style = currentStyle,
                            isPlaying = isPlaying,
                            volume = volume,
                            sleepTimer = sleepTimer,
                            onBack = { navController.popBackStack() },
                            onTogglePlay = {
                                if (isPlaying) {
                                    viewModel.pause()
                                    MusicPlaybackService.stop(this@MainActivity)
                                } else {
                                    viewModel.play()
                                    MusicPlaybackService.start(
                                        this@MainActivity,
                                        currentStyle.id
                                    )
                                }
                            },
                            onVolumeChange = controller::setVolume,
                            onSleepTimerChange = { timer ->
                                controller.setSleepTimer(timer)
                                scheduleSleepTimer(timer)
                            }
                        )
                    }
                }
            }
        }
    }

    private fun scheduleSleepTimer(timer: SleepTimerMinutes) {
        sleepTimerJob?.cancel()
        val minutes = timer.minutes ?: return
        sleepTimerJob = lifecycleScope.launch {
            delay(minutes * 60_000L)
            viewModel.controller.applySleepFadeIfNeeded()
            delay(6_000L)
            viewModel.pause()
            MusicPlaybackService.stop(this@MainActivity)
        }
    }

    private fun requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.POST_NOTIFICATIONS
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
    }
}

class ReadingMusicApp : Application()
