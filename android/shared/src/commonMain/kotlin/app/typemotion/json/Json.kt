package app.typemotion.json

/**
 * JSON objects and arrays with the parts of org.json's API the app uses, so
 * that the same code runs on Android and on the iPhone, which has no
 * org.json. Numbers are kept as Long or Double, and values are converted
 * the way org.json's opt methods convert them.
 */
class JSONObject() {
    private val map = LinkedHashMap<String, Any>()

    constructor(json: String) : this() {
        val value = JsonParser(json).parseDocument()
        if (value !is JSONObject) throw JSONException("Not a JSON object")
        map.putAll(value.map)
    }

    constructor(values: Map<*, *>) : this() {
        for ((key, value) in values) {
            if (key != null && value != null) map[key.toString()] = wrap(value)
        }
    }

    /** Set [key] to [value], or remove it if [value] is null */
    fun put(key: String, value: Any?): JSONObject {
        if (value == null) map.remove(key) else map[key] = wrap(value)
        return this
    }

    fun has(key: String): Boolean = key in map
    fun isNull(key: String): Boolean = map[key].let { it == null || it === JsonNull }
    fun remove(key: String): Any? = map.remove(key)
    fun length(): Int = map.size
    fun keys(): Iterator<String> = map.keys.toList().iterator()
    fun opt(key: String): Any? = map[key]

    fun optString(key: String, default: String = ""): String = stringOf(map[key]) ?: default
    fun optInt(key: String, default: Int = 0): Int = numberOf(map[key])?.toInt() ?: default
    fun optLong(key: String, default: Long = 0): Long = numberOf(map[key])?.toLong() ?: default
    fun optDouble(key: String, default: Double = Double.NaN): Double = numberOf(map[key])?.toDouble() ?: default
    fun optBoolean(key: String, default: Boolean = false): Boolean = booleanOf(map[key]) ?: default
    fun optJSONObject(key: String): JSONObject? = map[key] as? JSONObject
    fun optJSONArray(key: String): JSONArray? = map[key] as? JSONArray

    fun getString(key: String): String = stringOf(map[key]) ?: throw JSONException("No string $key")
    fun getInt(key: String): Int = numberOf(map[key])?.toInt() ?: throw JSONException("No number $key")
    fun getJSONObject(key: String): JSONObject = optJSONObject(key) ?: throw JSONException("No object $key")
    fun getJSONArray(key: String): JSONArray = optJSONArray(key) ?: throw JSONException("No array $key")

    override fun toString(): String = StringBuilder().also { write(it) }.toString()

    internal fun write(out: StringBuilder) {
        out.append('{')
        var first = true
        for ((key, value) in map) {
            if (!first) out.append(',')
            first = false
            writeString(out, key)
            out.append(':')
            writeValue(out, value)
        }
        out.append('}')
    }

    override fun equals(other: Any?): Boolean = other is JSONObject && other.map == map
    override fun hashCode(): Int = map.hashCode()
}

class JSONArray() {
    private val list = ArrayList<Any>()

    constructor(json: String) : this() {
        val value = JsonParser(json).parseDocument()
        if (value !is JSONArray) throw JSONException("Not a JSON array")
        list.addAll(value.list)
    }

    constructor(values: Collection<*>) : this() {
        for (value in values) list.add(if (value == null) JsonNull else wrap(value))
    }

    fun put(value: Any?): JSONArray {
        list.add(if (value == null) JsonNull else wrap(value))
        return this
    }

    fun length(): Int = list.size
    fun opt(index: Int): Any? = list.getOrNull(index)
    fun isNull(index: Int): Boolean = list.getOrNull(index).let { it == null || it === JsonNull }

    fun optString(index: Int, default: String = ""): String = stringOf(list.getOrNull(index)) ?: default
    fun optInt(index: Int, default: Int = 0): Int = numberOf(list.getOrNull(index))?.toInt() ?: default
    fun optLong(index: Int, default: Long = 0): Long = numberOf(list.getOrNull(index))?.toLong() ?: default
    fun optDouble(index: Int, default: Double = Double.NaN): Double = numberOf(list.getOrNull(index))?.toDouble() ?: default
    fun optBoolean(index: Int, default: Boolean = false): Boolean = booleanOf(list.getOrNull(index)) ?: default
    fun optJSONObject(index: Int): JSONObject? = list.getOrNull(index) as? JSONObject
    fun optJSONArray(index: Int): JSONArray? = list.getOrNull(index) as? JSONArray

    override fun toString(): String = StringBuilder().also { write(it) }.toString()

    internal fun write(out: StringBuilder) {
        out.append('[')
        list.forEachIndexed { i, value ->
            if (i > 0) out.append(',')
            writeValue(out, value)
        }
        out.append(']')
    }

    override fun equals(other: Any?): Boolean = other is JSONArray && other.list == list
    override fun hashCode(): Int = list.hashCode()
}

class JSONException(message: String) : RuntimeException(message)

private object JsonNull {
    override fun toString() = "null"
}

/** Values as they are kept: nested collections become JSON too */
private fun wrap(value: Any): Any = when (value) {
    is JSONObject, is JSONArray, is String, is Boolean, JsonNull -> value
    is Int -> value.toLong()
    is Short -> value.toLong()
    is Byte -> value.toLong()
    is Long -> value
    is Float -> value.toDouble()
    is Double -> value
    is Number -> value.toDouble()
    is Map<*, *> -> JSONObject(value)
    is Collection<*> -> JSONArray(value)
    is Array<*> -> JSONArray(value.toList())
    is IntArray -> JSONArray(value.toList())
    is LongArray -> JSONArray(value.toList())
    is Char -> value.toString()
    is Enum<*> -> value.name
    else -> value.toString()
}

private fun stringOf(value: Any?): String? = when (value) {
    null, JsonNull -> null
    is String -> value
    is Double -> numberText(value)
    else -> value.toString()
}

private fun numberOf(value: Any?): Number? = when (value) {
    is Long -> value
    is Double -> value
    is String -> value.trim().let { it.toLongOrNull() ?: it.toDoubleOrNull() }
    is Boolean -> null
    else -> null
}

private fun booleanOf(value: Any?): Boolean? = when (value) {
    is Boolean -> value
    is String -> when (value.lowercase()) {
        "true" -> true
        "false" -> false
        else -> null
    }
    else -> null
}

/** Whole numbers without ".0", as org.json writes them */
private fun numberText(d: Double): String =
    if (d.isFinite() && d == kotlin.math.floor(d) && kotlin.math.abs(d) < 1e15) d.toLong().toString() else d.toString()

private fun writeValue(out: StringBuilder, value: Any) {
    when (value) {
        is JSONObject -> value.write(out)
        is JSONArray -> value.write(out)
        is String -> writeString(out, value)
        is Long -> out.append(value)
        is Double -> out.append(if (value.isFinite()) numberText(value) else "null")
        is Boolean -> out.append(value)
        else -> out.append("null")
    }
}

private fun writeString(out: StringBuilder, s: String) {
    out.append('"')
    for (c in s) {
        when (c) {
            '"' -> out.append("\\\"")
            '\\' -> out.append("\\\\")
            '\n' -> out.append("\\n")
            '\r' -> out.append("\\r")
            '\t' -> out.append("\\t")
            '\b' -> out.append("\\b")
            '\u000C' -> out.append("\\f")
            else -> if (c < ' ' || c == ' ' || c == ' ') {
                out.append("\\u")
                out.append(c.code.toString(16).padStart(4, '0'))
            } else out.append(c)
        }
    }
    out.append('"')
}

private class JsonParser(private val s: String) {
    private var pos = 0

    fun parseDocument(): Any {
        val value = parseValue()
        skipWhite()
        if (pos != s.length) fail("Text after the value")
        return value
    }

    private fun fail(message: String): Nothing = throw JSONException("$message at $pos")

    private fun skipWhite() {
        while (pos < s.length && (s[pos] == ' ' || s[pos] == '\n' || s[pos] == '\r' || s[pos] == '\t')) pos++
    }

    private fun parseValue(): Any {
        skipWhite()
        if (pos >= s.length) fail("Unexpected end")
        return when (val c = s[pos]) {
            '{' -> parseObject()
            '[' -> parseArray()
            '"' -> parseString()
            't' -> literal("true", true)
            'f' -> literal("false", false)
            'n' -> literal("null", JsonNull)
            else -> if (c == '-' || c in '0'..'9') parseNumber() else fail("Unexpected '$c'")
        }
    }

    private fun literal(text: String, value: Any): Any {
        if (!s.startsWith(text, pos)) fail("Expected $text")
        pos += text.length
        return value
    }

    private fun parseObject(): JSONObject {
        val obj = JSONObject()
        pos++
        skipWhite()
        if (pos < s.length && s[pos] == '}') {
            pos++
            return obj
        }
        while (true) {
            skipWhite()
            if (pos >= s.length || s[pos] != '"') fail("Expected a key")
            val key = parseString()
            skipWhite()
            if (pos >= s.length || s[pos] != ':') fail("Expected ':'")
            pos++
            val value = parseValue()
            obj.put(key, value)
            skipWhite()
            if (pos >= s.length) fail("Unexpected end")
            when (s[pos++]) {
                ',' -> continue
                '}' -> return obj
                else -> fail("Expected ',' or '}'")
            }
        }
    }

    private fun parseArray(): JSONArray {
        val array = JSONArray()
        pos++
        skipWhite()
        if (pos < s.length && s[pos] == ']') {
            pos++
            return array
        }
        while (true) {
            array.put(parseValue())
            skipWhite()
            if (pos >= s.length) fail("Unexpected end")
            when (s[pos++]) {
                ',' -> continue
                ']' -> return array
                else -> fail("Expected ',' or ']'")
            }
        }
    }

    private fun parseString(): String {
        pos++
        val out = StringBuilder()
        while (true) {
            if (pos >= s.length) fail("Unterminated string")
            val c = s[pos++]
            when (c) {
                '"' -> return out.toString()
                '\\' -> {
                    if (pos >= s.length) fail("Unterminated string")
                    when (val e = s[pos++]) {
                        '"' -> out.append('"')
                        '\\' -> out.append('\\')
                        '/' -> out.append('/')
                        'b' -> out.append('\b')
                        'f' -> out.append('\u000C')
                        'n' -> out.append('\n')
                        'r' -> out.append('\r')
                        't' -> out.append('\t')
                        'u' -> {
                            if (pos + 4 > s.length) fail("Bad \\u escape")
                            val code = s.substring(pos, pos + 4).toIntOrNull(16) ?: fail("Bad \\u escape")
                            out.append(code.toChar())
                            pos += 4
                        }
                        else -> fail("Bad escape \\$e")
                    }
                }
                else -> out.append(c)
            }
        }
    }

    private fun parseNumber(): Any {
        val start = pos
        if (s[pos] == '-') pos++
        while (pos < s.length && s[pos] in '0'..'9') pos++
        var integer = true
        if (pos < s.length && s[pos] == '.') {
            integer = false
            pos++
            while (pos < s.length && s[pos] in '0'..'9') pos++
        }
        if (pos < s.length && (s[pos] == 'e' || s[pos] == 'E')) {
            integer = false
            pos++
            if (pos < s.length && (s[pos] == '+' || s[pos] == '-')) pos++
            while (pos < s.length && s[pos] in '0'..'9') pos++
        }
        val text = s.substring(start, pos)
        if (integer) text.toLongOrNull()?.let { return it }
        return text.toDoubleOrNull() ?: fail("Bad number $text")
    }
}
