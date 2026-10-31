@file:OptIn(ExperimentalForeignApi::class)

package app.typemotion.platform

import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.toComposeImageBitmap
import app.typemotion.engine.NativeEngine
import app.typemotion.engine.bool
import app.typemotion.engine.int
import app.typemotion.json.JSONObject
import app.typemotion.native.tm_copy_frame
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import org.jetbrains.skia.ColorAlphaType
import org.jetbrains.skia.ColorType
import org.jetbrains.skia.Image
import org.jetbrains.skia.ImageInfo

actual class FrameCopier actual constructor(platform: Platform) {
    private var buffer = ByteArray(0)

    actual fun copy(maxWidth: Int, maxHeight: Int): ImageBitmap? {
        val size = JSONObject(NativeEngine.take(tm_copy_frame(null, 0uL, maxWidth, maxHeight)))
        val needed = size.int("width") * size.int("height") * 4
        if (needed <= 0) return null
        if (buffer.size < needed) buffer = ByteArray(needed)

        val result = buffer.usePinned { pinned ->
            JSONObject(NativeEngine.take(tm_copy_frame(pinned.addressOf(0), buffer.size.toULong(), maxWidth, maxHeight)))
        }
        // The video may have changed size since the first call
        if (!result.bool("copied")) return null
        val width = result.int("width")
        val height = result.int("height")

        // Skia copies the pixels, so the buffer can be written again at once
        val info = ImageInfo(width, height, ColorType.RGBA_8888, ColorAlphaType.OPAQUE)
        return Image.makeRaster(info, buffer, width * 4).toComposeImageBitmap()
    }
}
