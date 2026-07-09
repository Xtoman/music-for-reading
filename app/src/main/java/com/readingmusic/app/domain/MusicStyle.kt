package com.readingmusic.app.domain

import androidx.annotation.StringRes
import com.readingmusic.app.R

enum class MusicStyle(
    val id: Int,
    @StringRes val titleRes: Int,
    @StringRes val descriptionRes: Int
) {
    DEEP_AMBIENT(
        id = 0,
        titleRes = R.string.style_deep_ambient,
        descriptionRes = R.string.style_deep_ambient_desc
    ),
    SOFT_PIANO(
        id = 1,
        titleRes = R.string.style_soft_piano,
        descriptionRes = R.string.style_soft_piano_desc
    ),
    RAIN_PAD(
        id = 2,
        titleRes = R.string.style_rain_pad,
        descriptionRes = R.string.style_rain_pad_desc
    ),
    ZEN_GARDEN(
        id = 3,
        titleRes = R.string.style_zen_garden,
        descriptionRes = R.string.style_zen_garden_desc
    ),
    LOFI_HAZE(
        id = 4,
        titleRes = R.string.style_lofi_haze,
        descriptionRes = R.string.style_lofi_haze_desc
    ),
    NIGHT_FOREST(
        id = 5,
        titleRes = R.string.style_night_forest,
        descriptionRes = R.string.style_night_forest_desc
    );

    companion object {
        val all = entries.toList()

        fun fromId(id: Int): MusicStyle =
            entries.firstOrNull { it.id == id } ?: DEEP_AMBIENT
    }
}

enum class SleepTimerMinutes(val minutes: Int?, @StringRes val labelRes: Int) {
    OFF(null, R.string.timer_off),
    MIN_15(15, R.string.timer_15),
    MIN_30(30, R.string.timer_30),
    MIN_60(60, R.string.timer_60),
    MIN_90(90, R.string.timer_90);

    companion object {
        val options = entries.toList()
    }
}
