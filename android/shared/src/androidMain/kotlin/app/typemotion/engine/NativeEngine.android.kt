package app.typemotion.engine

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.asCoroutineDispatcher
import java.util.concurrent.Executors

/** libtypemotion.so, whose native methods engine_jni.cpp registers */
actual object NativeEngine {
    init {
        System.loadLibrary("typemotion")
    }

    @JvmStatic actual external fun init(config: String, callbacks: NativeCallbacks): String
    @JvmStatic actual external fun call(method: String, args: String): String
    @JvmStatic actual external fun respond(id: Long, answer: String)
    /**
     * Copy the newest video frame into [buffer] (direct, RGBA) at most
     * [maxWidth] by [maxHeight], or 0 for any size. Returns
     * {width, height, serial, copied}; with a null or small buffer nothing
     * is copied and the size says what is needed. Any thread may call it.
     */
    @JvmStatic external fun copyFrame(buffer: java.nio.ByteBuffer?, maxWidth: Int, maxHeight: Int): String
    @JvmStatic actual external fun cancel()
    @JvmStatic actual external fun shutdown()
}

actual fun engineCallDispatcher(): CoroutineDispatcher =
    Executors.newSingleThreadExecutor { runnable ->
        Thread(runnable, "TypeMotion calls").apply { isDaemon = true }
    }.asCoroutineDispatcher()
