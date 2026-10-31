package app.typemotion.platform

import java.text.Collator
import java.text.DateFormat
import java.util.Date

actual fun textComparator(): Comparator<String> {
    val collator = Collator.getInstance()
    return Comparator { a, b -> collator.compare(a, b) }
}

actual fun formatDateTime(millis: Long): String =
    DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT).format(Date(millis))
