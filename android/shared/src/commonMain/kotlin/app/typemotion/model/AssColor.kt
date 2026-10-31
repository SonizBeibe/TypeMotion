package app.typemotion.model

import app.typemotion.platform.hex

/**
 * A colour as ASS writes it, &HAABBGGRR, where the alpha is the
 * transparency: 0 is opaque and 255 invisible.
 */
data class AssColor(val r: Int, val g: Int, val b: Int, val a: Int = 0) {
    /** &HAABBGGRR, the way the engine's styles are written */
    fun toAss(): String = "&H${hex(a)}${hex(b)}${hex(g)}${hex(r)}"

    /** #RRGGBB */
    fun toHtml(): String = "#${hex(r)}${hex(g)}${hex(b)}"

    /** As an ARGB int for drawing, with the alpha turned into opacity */
    fun toArgb(): Long = ((255 - a).toLong() shl 24) or (r.toLong() shl 16) or (g.toLong() shl 8) or b.toLong()

    companion object {
        /**
         * Reads &HAABBGGRR&, &HBBGGRR&, #RRGGBB and #RRGGBBAA, like
         * agi::Color does
         */
        fun parse(text: String): AssColor? {
            val t = text.trim()
            if (t.startsWith("&H", ignoreCase = true)) {
                val hex = t.substring(2).trimEnd('&')
                val value = hex.toLongOrNull(16) ?: return null
                return AssColor(
                    r = (value and 0xFF).toInt(),
                    g = (value shr 8 and 0xFF).toInt(),
                    b = (value shr 16 and 0xFF).toInt(),
                    a = (value shr 24 and 0xFF).toInt(),
                )
            }
            if (t.startsWith("#")) {
                val hex = t.substring(1)
                if (hex.length != 6 && hex.length != 8) return null
                val value = hex.toLongOrNull(16) ?: return null
                return if (hex.length == 6)
                    AssColor((value shr 16 and 0xFF).toInt(), (value shr 8 and 0xFF).toInt(), (value and 0xFF).toInt())
                else
                    AssColor((value shr 24 and 0xFF).toInt(), (value shr 16 and 0xFF).toInt(), (value shr 8 and 0xFF).toInt(), (value and 0xFF).toInt())
            }
            return null
        }
    }
}
