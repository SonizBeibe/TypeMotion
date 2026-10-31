package app.typemotion.platform

import platform.Foundation.NSDate
import platform.Foundation.NSDateFormatter
import platform.Foundation.NSDateFormatterMediumStyle
import platform.Foundation.NSDateFormatterShortStyle
import platform.Foundation.NSString
import platform.Foundation.dateWithTimeIntervalSince1970
import platform.Foundation.localizedStandardCompare

@Suppress("CAST_NEVER_SUCCEEDS")
actual fun textComparator(): Comparator<String> =
    Comparator { a, b -> (a as NSString).localizedStandardCompare(b).toInt() }

actual fun formatDateTime(millis: Long): String {
    val formatter = NSDateFormatter()
    formatter.dateStyle = NSDateFormatterMediumStyle
    formatter.timeStyle = NSDateFormatterShortStyle
    return formatter.stringFromDate(NSDate.dateWithTimeIntervalSince1970(millis / 1000.0))
}
