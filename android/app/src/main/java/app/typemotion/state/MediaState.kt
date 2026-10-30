package app.typemotion.state

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.ImageBitmap
import app.typemotion.engine.EngineError
import app.typemotion.engine.NativeEngine
import app.typemotion.engine.bool
import app.typemotion.engine.double
import app.typemotion.engine.int
import app.typemotion.engine.ints
import app.typemotion.engine.str
import app.typemotion.platform.FrameImage
import app.typemotion.platform.Platform
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.nio.ByteBuffer

/** The open video, as video_state describes it */
data class VideoInfo(
    val path: String,
    val width: Int,
    val height: Int,
    val displayWidth: Int,
    val displayHeight: Int,
    val frames: Int,
    val fps: Double,
    val keyframes: List<Int>,
) {
    val name: String get() = File(path).name

    /** The width over the height the video is shown at */
    val aspectRatio: Float
        get() = if (displayWidth > 0 && displayHeight > 0) displayWidth.toFloat() / displayHeight
            else if (width > 0 && height > 0) width.toFloat() / height
            else 16f / 9f

    companion object {
        fun from(o: JSONObject?) = o?.let {
            VideoInfo(
                path = it.str("path"),
                width = it.int("width"),
                height = it.int("height"),
                displayWidth = it.int("display_width"),
                displayHeight = it.int("display_height"),
                frames = it.int("frames"),
                fps = it.double("fps"),
                keyframes = it.ints("keyframes"),
            )
        }
    }
}

/** The open audio */
data class AudioInfo(val path: String, val duration: Int, val sampleRate: Int, val channels: Int) {
    val name: String get() = File(path).name

    companion object {
        fun from(o: JSONObject?) = o?.let {
            AudioInfo(it.str("path"), it.int("duration"), it.int("sample_rate"), it.int("channels"))
        }
    }
}

/**
 * The video and audio of the open file: the video panel's frames and where
 * playback is. The core opens, decodes and plays them as the desktop
 * program does; this shows its frames and sends it the panel's buttons.
 */
class MediaState(private val app: AppState) {
    var video by mutableStateOf<VideoInfo?>(null)
        private set
    var audio by mutableStateOf<AudioInfo?>(null)
        private set
    var frame by mutableIntStateOf(0)
        private set
    /** The time of [frame] in milliseconds */
    var time by mutableIntStateOf(0)
        private set
    var playing by mutableStateOf(false)
        private set
    /** Where audio without video plays, in milliseconds, or -1 */
    var audioPosition by mutableIntStateOf(-1)
        private set
    var audioPlaying by mutableStateOf(false)
        private set

    /** The newest frame with the subtitles on it */
    var image by mutableStateOf<ImageBitmap?>(null)
        private set

    /** Whether the panel is folded away to leave room for the lines */
    var hidden by mutableStateOf(false)

    private fun onState(state: JSONObject) {
        val newVideo = VideoInfo.from(state.optJSONObject("video"))
        if (newVideo?.path != video?.path) hidden = false
        video = newVideo
        audio = AudioInfo.from(state.optJSONObject("audio"))
        state.optJSONObject("video")?.let { v ->
            frame = v.int("frame")
            time = v.int("time")
            updatePlaying(v.bool("playing"))
        }
        if (newVideo == null) {
            image = null
            frame = 0
            time = 0
            updatePlaying(false)
        }
    }

    /** Read the state again, when the app starts or comes back */
    suspend fun refresh() {
        onState(app.engine.call("video_state"))
        if (video != null) onFrameReady()
    }

    fun onEvent(kind: String, data: JSONObject) {
        when (kind) {
            "video", "audio" -> onState(data)
            "video_seek" -> {
                frame = data.int("frame")
                time = data.int("time")
                updatePlaying(data.bool("playing"))
            }
            "video_frame" -> onFrameReady()
            "audio_position" -> {
                audioPosition = data.int("time")
                updateAudioPlaying(true)
            }
            "audio_stop" -> {
                audioPosition = -1
                updateAudioPlaying(false)
            }
        }
    }

    // Opening and closing

    fun pickVideo() {
        app.filePicker = FilePickerRequest(
            title = Strings.openVideo,
            save = false,
            startDir = startFolder(),
            filters = FileFilter.parse(Strings.videoWildcard),
        ) { paths ->
            app.filePicker = null
            paths?.firstOrNull()?.let(::openVideo)
        }
    }

    fun openVideo(path: String) = app.run { onState(app.call("open_video", "path" to path)) }

    fun closeVideo() = app.run { onState(app.call("close_video")) }

    fun pickAudio() {
        app.filePicker = FilePickerRequest(
            title = Strings.openAudio,
            save = false,
            startDir = startFolder(),
            filters = FileFilter.parse(Strings.audioWildcard),
        ) { paths ->
            app.filePicker = null
            paths?.firstOrNull()?.let { path -> app.run { onState(app.call("open_audio", "path" to path)) } }
        }
    }

    fun openVideoAudio() = app.run { onState(app.call("open_audio", "from_video" to true)) }

    fun closeAudio() = app.run { onState(app.call("close_audio")) }

    private fun startFolder(): File? =
        listOfNotNull(video?.path, audio?.path, app.fileInfo.path.takeIf { it.isNotEmpty() })
            .firstNotNullOfOrNull { File(it).parentFile }
            ?: app.platform.folders.storage

    // The buttons

    fun seekFrame(frame: Int) = app.run { onState(app.call("video_seek", "frame" to frame)) }

    fun step(frames: Int) = app.run { onState(app.call("video_seek", "step" to frames)) }

    private var scrubbing = false
    private var scrubTo = -1

    /**
     * Go to the frame the seek bar is dragged to. It moves faster than the
     * engine seeks, so only the newest place is sent.
     */
    fun scrub(frame: Int) {
        scrubTo = frame
        if (scrubbing) return
        scrubbing = true
        app.run {
            try {
                while (scrubTo >= 0) {
                    val target = scrubTo
                    scrubTo = -1
                    onState(app.call("video_seek", "frame" to target))
                }
            } finally {
                scrubbing = false
            }
        }
    }

    fun jumpToStart() = seekFrame(0)

    fun jumpToEnd() = seekFrame((video?.frames ?: 1) - 1)

    /** Play from here, or stop if it plays */
    fun play() = app.run { onState(app.call("video_play")) }

    /** Play the active line, or with only audio, play or stop it */
    fun playLine() = app.run { onState(app.call("video_play", "line" to true)) }

    fun stop() = app.run {
        onState(app.call("video_stop"))
        audioPosition = -1
        updateAudioPlaying(false)
    }

    // Playback stops at the video's or line's end without an event, so its
    // state is checked while it plays, as the desktop program's buttons do

    private var watchJob: Job? = null

    private fun updatePlaying(value: Boolean) {
        playing = value
        watchPlayback()
    }

    private fun updateAudioPlaying(value: Boolean) {
        audioPlaying = value
        watchPlayback()
    }

    private fun watchPlayback() {
        if (!playing && !audioPlaying) return
        if (watchJob?.isActive == true) return
        watchJob = app.scope.launch {
            while (isActive && (playing || audioPlaying)) {
                delay(250)
                try {
                    val position = app.engine.call("video_position")
                    playing = position.bool("playing")
                    if (!position.bool("audio_playing") && audioPlaying) {
                        audioPlaying = false
                        audioPosition = -1
                    }
                } catch (_: EngineError) {
                    break
                }
            }
        }
    }

    // The frames

    /** The largest frame worth copying, from the panel's size on the screen */
    @Volatile
    var maxFrameWidth = 1280
        private set
    @Volatile
    var maxFrameHeight = 720
        private set

    fun setPanelSize(width: Int, height: Int) {
        if (width <= 0 || height <= 0) return
        if (width == maxFrameWidth && height == maxFrameHeight) return
        maxFrameWidth = width
        maxFrameHeight = height
        if (video != null) onFrameReady()
    }

    private var copying = false
    private var copyAgain = false
    private val frames = FrameCopier(app.platform)

    /**
     * Copy the engine's newest frame. Frames come faster than they can be
     * copied while the video plays, so only the newest one is copied.
     */
    private fun onFrameReady() {
        if (copying) {
            copyAgain = true
            return
        }
        copying = true
        app.scope.launch {
            try {
                do {
                    copyAgain = false
                    val copied = withContext(Dispatchers.Default) { frames.copy(maxFrameWidth, maxFrameHeight) }
                    if (copied != null && video != null) image = copied
                } while (copyAgain)
            } finally {
                copying = false
            }
        }
    }
}

/**
 * Copies frames into a few images in turn, so that the one on the screen
 * isn't written to
 */
private class FrameCopier(private val platform: Platform) {
    private var buffer: ByteBuffer? = null
    private val images = arrayOfNulls<FrameImage>(3)
    private var next = 0

    fun copy(maxWidth: Int, maxHeight: Int): ImageBitmap? {
        val size = JSONObject(NativeEngine.copyFrame(null, maxWidth, maxHeight))
        val needed = size.int("width") * size.int("height") * 4
        if (needed <= 0) return null

        val buffer = buffer?.takeIf { it.capacity() >= needed } ?: ByteBuffer.allocateDirect(needed).also { buffer = it }
        buffer.clear()
        val result = JSONObject(NativeEngine.copyFrame(buffer, maxWidth, maxHeight))
        // The video may have changed size since the first call
        if (!result.bool("copied")) return null
        val width = result.int("width")
        val height = result.int("height")

        val index = next
        next = (next + 1) % images.size
        val image = images[index]?.takeIf { it.width == width && it.height == height }
            ?: platform.createFrameImage(width, height).also { images[index] = it }
        buffer.position(0)
        buffer.limit(width * height * 4)
        image.write(buffer)
        return image.image()
    }
}
