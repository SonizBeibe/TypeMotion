package app.typemotion.model

/**
 * The engine counts positions in text in bytes of UTF-8, like the desktop
 * text control, and Compose in UTF-16 code units.
 */
object Utf8 {
    /** The UTF-8 byte offset of a UTF-16 index */
    fun byteOffset(text: String, index: Int): Int {
        var bytes = 0
        var i = 0
        val end = index.coerceIn(0, text.length)
        while (i < end) {
            val c = text[i]
            when {
                c.code < 0x80 -> bytes += 1
                c.code < 0x800 -> bytes += 2
                Character.isHighSurrogate(c) && i + 1 < text.length && Character.isLowSurrogate(text[i + 1]) -> {
                    bytes += 4
                    i++
                }
                else -> bytes += 3
            }
            i++
        }
        return bytes
    }

    /** The UTF-16 index of a UTF-8 byte offset */
    fun charIndex(text: String, offset: Int): Int {
        var bytes = 0
        var i = 0
        while (i < text.length && bytes < offset) {
            val c = text[i]
            when {
                c.code < 0x80 -> bytes += 1
                c.code < 0x800 -> bytes += 2
                Character.isHighSurrogate(c) && i + 1 < text.length && Character.isLowSurrogate(text[i + 1]) -> {
                    bytes += 4
                    i++
                }
                else -> bytes += 3
            }
            i++
        }
        return i
    }
}
