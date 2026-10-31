package app.typemotion.platform

import kotlin.math.abs
import kotlin.math.roundToLong

/** Sorts text by the phone's language, as java.text.Collator does */
expect fun textComparator(): Comparator<String>

/** A date and time in the phone's language, such as "31 oct 2026, 10:45" */
expect fun formatDateTime(millis: Long): String

/** [value] in decimal with at least [digits] digits, padded with zeros */
fun pad(value: Int, digits: Int): String = value.toString().padStart(digits, '0')

/** [value] in hexadecimal, upper case, with at least [digits] digits */
fun hex(value: Int, digits: Int = 2): String = value.toString(16).uppercase().padStart(digits, '0')

/** [value] with [decimals] digits after the point, as "%.Nf" writes it */
fun fixed(value: Double, decimals: Int): String {
    if (!value.isFinite()) return value.toString()
    var scale = 1L
    repeat(decimals) { scale *= 10 }
    val scaled = (abs(value) * scale).roundToLong()
    val whole = scaled / scale
    val fraction = scaled % scale
    val sign = if (value < 0 && scaled != 0L) "-" else ""
    return if (decimals == 0) "$sign$whole" else "$sign$whole.${fraction.toString().padStart(decimals, '0')}"
}
