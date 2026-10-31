package app.typemotion.io

import okio.FileSystem
import okio.Path
import okio.Path.Companion.toPath
import okio.buffer
import okio.use

/**
 * A file or folder by its path, with the parts of java.io.File the app
 * uses, so that the same code runs on Android and on the iPhone
 */
class File(path: String) {
    constructor(parent: File, child: String) : this(join(parent.path, child))
    constructor(parent: String, child: String) : this(join(parent, child))

    val path: String = path.trimEnd('/').ifEmpty { if (path.startsWith("/")) "/" else path }

    private val okioPath: Path get() = path.toPath()

    val name: String get() = path.substringAfterLast('/')

    /** The folder this is in, or null for the root */
    val parent: String?
        get() {
            if (path == "/") return null
            val slash = path.lastIndexOf('/')
            return when {
                slash < 0 -> null
                slash == 0 -> "/"
                else -> path.substring(0, slash)
            }
        }

    val parentFile: File? get() = parent?.let(::File)

    val extension: String get() = name.substringAfterLast('.', "")

    private val metadata get() = try {
        fs.metadataOrNull(okioPath)
    } catch (_: Exception) {
        null
    }

    val isDirectory: Boolean get() = metadata?.isDirectory == true
    val isFile: Boolean get() = metadata?.isRegularFile == true

    fun exists(): Boolean = metadata != null

    fun canRead(): Boolean = exists()

    fun length(): Long = metadata?.size ?: 0

    /** When the file was last changed, in milliseconds since 1970 */
    fun lastModified(): Long = metadata?.lastModifiedAtMillis ?: 0

    /** What's in the folder, or null if it isn't one or can't be read */
    fun listFiles(): Array<File>? = try {
        fs.listOrNull(okioPath)?.map { File(it.toString()) }?.toTypedArray()
    } catch (_: Exception) {
        null
    }

    /** Create the folder and the ones it is in; false if it already existed or failed */
    fun mkdirs(): Boolean {
        if (exists()) return false
        return try {
            fs.createDirectories(okioPath)
            true
        } catch (_: Exception) {
            false
        }
    }

    fun delete(): Boolean = try {
        if (!exists()) false else {
            fs.delete(okioPath)
            true
        }
    } catch (_: Exception) {
        false
    }

    fun deleteRecursively(): Boolean = try {
        fs.deleteRecursively(okioPath)
        true
    } catch (_: Exception) {
        false
    }

    fun readText(): String = fs.read(okioPath) { readUtf8() }

    fun readBytes(): ByteArray = fs.read(okioPath) { readByteArray() }

    fun writeText(text: String) {
        fs.write(okioPath) { writeUtf8(text) }
    }

    fun writeBytes(bytes: ByteArray) {
        fs.write(okioPath) { write(bytes) }
    }

    /** Copy the file to [target], creating its folder */
    fun copyTo(target: File, overwrite: Boolean = false): File {
        if (target.exists() && !overwrite) throw IllegalStateException("${target.path} already exists")
        target.parentFile?.mkdirs()
        fs.source(okioPath).use { source ->
            fs.sink(target.okioPath).buffer().use { it.writeAll(source) }
        }
        return target
    }

    override fun toString(): String = path
    override fun equals(other: Any?): Boolean = other is File && other.path == path
    override fun hashCode(): Int = path.hashCode()

    private companion object {
        val fs: FileSystem get() = FileSystem.SYSTEM

        fun join(parent: String, child: String): String =
            if (parent.isEmpty()) child else parent.trimEnd('/') + "/" + child.trimStart('/')
    }
}
