package app.typemotion.platform

import androidx.compose.ui.graphics.ImageBitmap

/**
 * Copies the engine's newest video frame into an image to draw. It keeps a
 * few images and fills them in turn, so that the one on the screen isn't
 * written to. Called on a background thread.
 */
expect class FrameCopier(platform: Platform) {
    /** The newest frame at most [maxWidth] by [maxHeight], or null if there is none */
    fun copy(maxWidth: Int, maxHeight: Int): ImageBitmap?
}
