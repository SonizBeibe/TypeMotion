package app.typemotion.platform

import android.graphics.Bitmap
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import app.typemotion.engine.NativeEngine
import app.typemotion.engine.bool
import app.typemotion.engine.int
import app.typemotion.json.JSONObject
import java.nio.ByteBuffer

actual class FrameCopier actual constructor(platform: Platform) {
    private var buffer: ByteBuffer? = null
    private val images = arrayOfNulls<BitmapFrameImage>(3)
    private var next = 0

    actual fun copy(maxWidth: Int, maxHeight: Int): ImageBitmap? {
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
            ?: BitmapFrameImage(width, height).also { images[index] = it }
        buffer.position(0)
        buffer.limit(width * height * 4)
        image.write(buffer)
        return image.image()
    }
}

/** An image which video frames are copied into */
private class BitmapFrameImage(val width: Int, val height: Int) {
    private val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)

    // ARGB_8888 bitmaps are RGBA in memory, as the engine copies frames
    fun write(pixels: ByteBuffer) = bitmap.copyPixelsFromBuffer(pixels)

    /** The image to draw, made after each [write] so that it is drawn again */
    fun image(): ImageBitmap = bitmap.asImageBitmap()
}
