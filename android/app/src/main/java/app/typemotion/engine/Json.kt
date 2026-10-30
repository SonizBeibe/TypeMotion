package app.typemotion.engine

import org.json.JSONArray
import org.json.JSONObject

/** Builds an object from keys and values, leaving out null values */
fun jsonOf(vararg entries: Pair<String, Any?>): JSONObject {
    val obj = JSONObject()
    for ((key, value) in entries) {
        when (value) {
            null -> Unit
            is Collection<*> -> obj.put(key, JSONArray(value))
            is Map<*, *> -> obj.put(key, JSONObject(value))
            else -> obj.put(key, value)
        }
    }
    return obj
}

fun JSONObject.str(key: String, default: String = ""): String =
    if (has(key) && !isNull(key)) optString(key, default) else default

fun JSONObject.int(key: String, default: Int = 0): Int = optInt(key, default)

fun JSONObject.long(key: String, default: Long = 0): Long = optLong(key, default)

fun JSONObject.double(key: String, default: Double = 0.0): Double = optDouble(key, default)

fun JSONObject.bool(key: String, default: Boolean = false): Boolean = optBoolean(key, default)

fun JSONObject.objects(key: String): List<JSONObject> = optJSONArray(key)?.objects() ?: emptyList()

fun JSONObject.strings(key: String): List<String> = optJSONArray(key)?.strings() ?: emptyList()

fun JSONObject.ints(key: String): List<Int> = optJSONArray(key)?.let { array ->
    (0 until array.length()).map { array.optInt(it) }
} ?: emptyList()

fun JSONArray.objects(): List<JSONObject> = (0 until length()).mapNotNull { optJSONObject(it) }

fun JSONArray.strings(): List<String> = (0 until length()).map { optString(it) }
