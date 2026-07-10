package com.readingmusic.app.domain

import androidx.annotation.StringRes
import com.readingmusic.app.R

enum class MusicStyle(
    val id: Int,
    @StringRes val titleRes: Int,
    @StringRes val descriptionRes: Int
) {
    FANTASY(
        id = 0,
        titleRes = R.string.style_fantasy,
        descriptionRes = R.string.style_fantasy_desc
    ),
    SCI_FI(
        id = 1,
        titleRes = R.string.style_scifi,
        descriptionRes = R.string.style_scifi_desc
    ),
    NOIR(
        id = 2,
        titleRes = R.string.style_noir,
        descriptionRes = R.string.style_noir_desc
    ),
    CLASSICAL(
        id = 3,
        titleRes = R.string.style_classical,
        descriptionRes = R.string.style_classical_desc
    ),
    NATURE(
        id = 4,
        titleRes = R.string.style_nature,
        descriptionRes = R.string.style_nature_desc
    ),
    LOFI(
        id = 5,
        titleRes = R.string.style_lofi,
        descriptionRes = R.string.style_lofi_desc
    ),
    MEDITATION(
        id = 6,
        titleRes = R.string.style_meditation,
        descriptionRes = R.string.style_meditation_desc
    );

    companion object {
        val all = entries.toList()

        fun fromId(id: Int): MusicStyle =
            entries.firstOrNull { it.id == id } ?: FANTASY
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
