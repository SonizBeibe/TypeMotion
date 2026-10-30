package app.typemotion.model

/** Times as ASS writes them: H:MM:SS.cc */
object AssTime {
    fun format(ms: Int): String {
        val cs = ms.coerceAtLeast(0) / 10
        val h = cs / 360000
        val m = cs / 6000 % 60
        val s = cs / 100 % 60
        return "%d:%02d:%02d.%02d".format(h, m, s, cs % 100)
    }

    /**
     * Reads a time like the desktop time fields accept it: H:MM:SS.cc, with
     * fewer parts counting from the seconds
     */
    fun parse(text: String): Int? {
        val trimmed = text.trim()
        if (trimmed.isEmpty()) return null
        val negative = trimmed.startsWith("-")
        val parts = trimmed.removePrefix("-").split(':')
        if (parts.size > 3 || parts.any { it.isEmpty() }) return null
        var ms = 0.0
        for (part in parts.dropLast(1)) {
            val value = part.toIntOrNull() ?: return null
            ms = ms * 60 + value
        }
        val seconds = parts.last().replace(',', '.').toDoubleOrNull() ?: return null
        ms = ms * 60 * 1000 + seconds * 1000
        val result = (Math.round(ms / 10.0) * 10).toInt()
        return if (negative) -result else result
    }
}
