package app.typemotion.ui.editor

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.ExpandLess
import androidx.compose.material.icons.filled.ExpandMore
import androidx.compose.material.icons.filled.MusicNote
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material.icons.filled.Subtitles
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.min
import app.typemotion.platform.pad
import app.typemotion.state.AppState
import app.typemotion.state.MediaState
import app.typemotion.state.VideoInfo
import app.typemotion.ui.theme.TypeMotionTheme

/**
 * The desktop program's video box: the video with the subtitles on it, the
 * seek bar and the playback buttons. With only audio open it is a bar to
 * play the lines.
 */
@Composable
fun MediaPanel(
    app: AppState,
    modifier: Modifier = Modifier,
    maxVideoHeight: Dp = Dp.Unspecified,
    /** Give the video all the height the buttons leave */
    fillHeight: Boolean = false,
) {
    val media = app.media
    val video = media.video
    when {
        video != null -> VideoPanel(app, modifier, maxVideoHeight, fillHeight)
        media.audio != null -> AudioBar(app, modifier)
    }
}

@Composable
private fun VideoPanel(app: AppState, modifier: Modifier, maxVideoHeight: Dp, fillHeight: Boolean) {
    val media = app.media
    val video = media.video ?: return
    Surface(modifier = modifier, color = TypeMotionTheme.colors.panelBackground) {
        BoxWithConstraints {
            if (media.hidden && maxWidth >= 600.dp) {
                // Folded on a wide screen: the seek bar and the buttons on one row
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    SeekBar(media, Modifier.weight(1f))
                    VideoControls(app, wrap = false)
                }
            } else {
                Column(if (fillHeight) Modifier.fillMaxHeight() else Modifier) {
                    if (!media.hidden) VideoPicture(media, video, maxVideoHeight, fillHeight)
                    SeekBar(media)
                    // Beside the video there is room for two rows of buttons,
                    // but under it one row leaves more room for the lines
                    VideoControls(app, wrap = fillHeight, Modifier.fillMaxWidth())
                }
            }
        }
    }
}

/** The frame with the subtitles on it, on black */
@Composable
private fun ColumnScope.VideoPicture(media: MediaState, video: VideoInfo, maxVideoHeight: Dp, fillHeight: Boolean) {
    BoxWithConstraints(
        Modifier
            .fillMaxWidth()
            .let { if (fillHeight) it.weight(1f) else it }
            .background(Color.Black),
        contentAlignment = Alignment.Center,
    ) {
        // As large as fits the width and the room it was given, in the
        // video's shape
        val ratio = video.aspectRatio.coerceIn(0.25f, 4f)
        val limit = if (fillHeight) maxHeight else maxVideoHeight
        val height = if (limit != Dp.Unspecified) min(maxWidth / ratio, limit) else maxWidth / ratio
        Box(
            Modifier
                .height(height)
                .width(height * ratio)
                .onSizeChanged { media.setPanelSize(it.width, it.height) },
            contentAlignment = Alignment.Center,
        ) {
            val image = media.image
            if (image != null)
                Image(image, contentDescription = video.name, contentScale = ContentScale.FillBounds, modifier = Modifier.fillMaxSize())
            else
                CircularProgressIndicator(Modifier.size(32.dp))
        }
    }
}

/**
 * Where the video is, as the desktop video box shows it, and the seek bar;
 * dragging it shows the frames on the way
 */
@Composable
private fun SeekBar(media: MediaState, modifier: Modifier = Modifier.fillMaxWidth()) {
    val video = media.video ?: return
    val last = (video.frames - 1).coerceAtLeast(0)
    var dragging by remember { mutableStateOf<Float?>(null) }
    Row(
        modifier.padding(start = 10.dp, end = 4.dp, top = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.widthIn(min = 84.dp)) {
            Text(
                formatVideoTime(media.time),
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurface,
                maxLines = 1,
                softWrap = false,
            )
            // Keyframes in the colour of the desktop's keyframe marks
            val keyframe = media.frame in video.keyframes
            Text(
                "Cuadro ${media.frame}",
                style = MaterialTheme.typography.labelSmall,
                color = if (keyframe) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
                softWrap = false,
            )
        }
        Slider(
            value = dragging ?: media.frame.toFloat(),
            onValueChange = {
                dragging = it
                media.scrub(it.toInt())
            },
            onValueChangeFinished = {
                dragging?.let { media.scrub(it.toInt()) }
                dragging = null
            },
            valueRange = 0f..last.toFloat().coerceAtLeast(1f),
            enabled = last > 0,
            modifier = Modifier
                .weight(1f)
                .height(28.dp)
                .padding(horizontal = 8.dp),
        )
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun VideoControls(app: AppState, wrap: Boolean, modifier: Modifier = Modifier) {
    val media = app.media
    val active = app.editBox.state.active
    val playback: @Composable RowScope.() -> Unit = {
        ControlButton(Icons.AutoMirrored.Filled.KeyboardArrowLeft, "Cuadro anterior") { media.step(-1) }
        ControlButton(
            if (media.playing) Icons.Default.Pause else Icons.Default.PlayArrow,
            if (media.playing) "Pausa" else "Reproducir",
        ) { media.play() }
        ControlButton(Icons.Default.Subtitles, "Reproducir la línea", enabled = active) { media.playLine() }
        ControlButton(Icons.AutoMirrored.Filled.KeyboardArrowRight, "Cuadro siguiente") { media.step(1) }
    }
    val timing: @Composable RowScope.() -> Unit = {
        // The desktop's "set start/end to the video's frame" buttons
        TextButtonSmall("Inicio ⇤", enabled = active) { app.runCommand("time/snap/start_video") }
        TextButtonSmall("⇥ Fin", enabled = active) { app.runCommand("time/snap/end_video") }

        ControlButton(
            if (media.hidden) Icons.Default.ExpandMore else Icons.Default.ExpandLess,
            if (media.hidden) "Mostrar el video" else "Ocultar el video",
        ) { media.hidden = !media.hidden }
    }

    if (wrap) {
        // In two groups, which go on two rows when the panel is narrow
        FlowRow(
            modifier = modifier.padding(horizontal = 4.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            ControlGroup(playback)
            ControlGroup(timing)
        }
    } else {
        Row(
            modifier = modifier
                .horizontalScroll(rememberScrollState())
                .padding(horizontal = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(2.dp),
        ) {
            playback()
            timing()
        }
    }
}

@Composable
private fun ControlGroup(content: @Composable RowScope.() -> Unit) {
    Row(
        modifier = Modifier.height(40.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(2.dp),
        content = content,
    )
}

/** Audio without video: play the active line, as the audio box's button does */
@Composable
private fun AudioBar(app: AppState, modifier: Modifier) {
    val media = app.media
    val audio = media.audio ?: return
    Surface(modifier = modifier, color = TypeMotionTheme.colors.panelBackground) {
        Row(
            Modifier
                .fillMaxWidth()
                .padding(horizontal = 8.dp, vertical = 2.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Icon(Icons.Default.MusicNote, contentDescription = null, tint = MaterialTheme.colorScheme.secondary, modifier = Modifier.size(18.dp))
            Text(
                audio.name,
                style = MaterialTheme.typography.labelMedium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.weight(1f),
            )
            if (media.audioPosition >= 0) {
                Text(formatVideoTime(media.audioPosition), style = MaterialTheme.typography.labelSmall, maxLines = 1)
            }
            ControlButton(
                if (media.audioPlaying) Icons.Default.Stop else Icons.Default.PlayArrow,
                if (media.audioPlaying) "Detener" else "Reproducir la línea",
                enabled = media.audioPlaying || app.editBox.state.active,
            ) { if (media.audioPlaying) media.stop() else media.playLine() }
        }
    }
}

@Composable
private fun ControlButton(icon: ImageVector, description: String, enabled: Boolean = true, onClick: () -> Unit) {
    IconButton(onClick = onClick, enabled = enabled, modifier = Modifier.size(40.dp)) {
        Icon(icon, contentDescription = description, modifier = Modifier.size(24.dp))
    }
}

@Composable
private fun TextButtonSmall(label: String, enabled: Boolean, onClick: () -> Unit) {
    Surface(
        onClick = onClick,
        enabled = enabled,
        color = MaterialTheme.colorScheme.surfaceVariant,
        shape = RoundedCornerShape(6.dp),
        modifier = Modifier.height(32.dp),
    ) {
        Box(contentAlignment = Alignment.Center, modifier = Modifier.padding(horizontal = 10.dp)) {
            Text(
                label,
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = if (enabled) 1f else 0.4f),
                maxLines = 1,
            )
        }
    }
}

/** H:MM:SS.mmm, as the desktop video box shows the time */
private fun formatVideoTime(ms: Int): String {
    val t = ms.coerceAtLeast(0)
    return "${t / 3600000}:${pad(t / 60000 % 60, 2)}:${pad(t / 1000 % 60, 2)}.${pad(t % 1000, 3)}"
}
