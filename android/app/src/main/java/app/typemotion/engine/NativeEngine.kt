package app.typemotion.engine

/**
 * The C++ core, in libtypemotion.so. src/android/android.h in the core
 * describes these. Everything is JSON, and init() and call() block until the
 * engine thread has done the work, so [Engine] calls them on its own thread.
 */
object NativeEngine {
    init {
        System.loadLibrary("typemotion")
    }

    @JvmStatic external fun init(config: String, callbacks: NativeCallbacks): String
    @JvmStatic external fun call(method: String, args: String): String
    @JvmStatic external fun respond(id: Long, answer: String)
    /**
     * Copy the newest video frame into [buffer] (direct, RGBA) at most
     * [maxWidth] by [maxHeight], or 0 for any size. Returns
     * {width, height, serial, copied}; with a null or small buffer nothing
     * is copied and the size says what is needed. Any thread may call it.
     */
    @JvmStatic external fun copyFrame(buffer: java.nio.ByteBuffer?, maxWidth: Int, maxHeight: Int): String
    @JvmStatic external fun cancel()
    @JvmStatic external fun shutdown()
}

/**
 * What the engine tells the app. Called on the engine's threads; requests
 * wait for [NativeEngine.respond].
 */
interface NativeCallbacks {
    fun onEvent(kind: String, json: String)
    fun onRequest(id: Long, kind: String, json: String)
}
