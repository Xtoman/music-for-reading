package com.readingmusic.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.GraphicEq
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.readingmusic.app.domain.MusicStyle

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HomeScreen(
    currentStyle: MusicStyle,
    isPlaying: Boolean,
    onStyleSelected: (MusicStyle) -> Unit,
    modifier: Modifier = Modifier
) {
    Scaffold(
        modifier = modifier.fillMaxSize(),
        containerColor = MaterialTheme.colorScheme.surface,
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text(
                            text = stringResource(id = com.readingmusic.app.R.string.app_name),
                            style = MaterialTheme.typography.titleLarge
                        )
                        Text(
                            text = stringResource(id = com.readingmusic.app.R.string.home_subtitle),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface,
                    titleContentColor = MaterialTheme.colorScheme.onSurface
                )
            )
        }
    ) { innerPadding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding),
            contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            items(MusicStyle.all, key = { it.id }) { style ->
                StyleListItem(
                    style = style,
                    isSelected = style == currentStyle,
                    isPlaying = isPlaying && style == currentStyle,
                    onClick = { onStyleSelected(style) }
                )
            }
            item { Spacer(modifier = Modifier.height(8.dp)) }
        }
    }
}

@Composable
private fun StyleListItem(
    style: MusicStyle,
    isSelected: Boolean,
    isPlaying: Boolean,
    onClick: () -> Unit
) {
    val containerColor = if (isSelected) {
        MaterialTheme.colorScheme.secondaryContainer
    } else {
        MaterialTheme.colorScheme.surfaceContainerLow
    }
    val contentColor = if (isSelected) {
        MaterialTheme.colorScheme.onSecondaryContainer
    } else {
        MaterialTheme.colorScheme.onSurface
    }

    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        color = containerColor,
        tonalElevation = if (isSelected) 1.dp else 0.dp
    ) {
        ListItem(
            headlineContent = {
                Text(
                    text = stringResource(id = style.titleRes),
                    style = MaterialTheme.typography.titleMedium,
                    color = contentColor
                )
            },
            supportingContent = {
                Text(
                    text = when {
                        isPlaying -> stringResource(id = com.readingmusic.app.R.string.now_playing)
                        isSelected -> stringResource(id = com.readingmusic.app.R.string.selected)
                        else -> stringResource(id = style.descriptionRes)
                    },
                    style = MaterialTheme.typography.bodyMedium,
                    color = if (isSelected) {
                        MaterialTheme.colorScheme.onSecondaryContainer.copy(alpha = 0.8f)
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    }
                )
            },
            leadingContent = {
                Icon(
                    imageVector = Icons.Default.GraphicEq,
                    contentDescription = null,
                    tint = if (isSelected) {
                        MaterialTheme.colorScheme.primary
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    }
                )
            },
            trailingContent = if (isSelected) {
                {
                    Icon(
                        imageVector = Icons.Default.Check,
                        contentDescription = stringResource(id = com.readingmusic.app.R.string.selected),
                        tint = MaterialTheme.colorScheme.primary
                    )
                }
            } else {
                null
            },
            colors = ListItemDefaults.colors(containerColor = androidx.compose.ui.graphics.Color.Transparent)
        )
    }
}
