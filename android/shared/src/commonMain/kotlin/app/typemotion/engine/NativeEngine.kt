package app.typemotion.engine

import kotlinx.coroutines.CoroutineDispatcher

/**
 * The C++ core: libtypemotion.so through JNI on Android, the C functions of
 * src/ios/typemotion_ios.h on the iPhone. src/android/android.h in the core
 * describes these. Everything is JSON, and init() and call() block until the
 * engine thread has done the work, so [Engine] calls them on its own thread.
 * Video frames are copied by [app.typemotion.platform.FrameCopier].
 */
expect object NativeEngine {
    fun init(config: String, callbacks: NativeCallbacks): String
    fun call(method: String, args: String): String
    fun respond(id: Long, answer: String)
    fun cancel()
    fun shutdown()
}

/**
 * What the engine tells the app. Called on the engine's threads; requests
 * wait for [NativeEngine.respond].
 */
interface NativeCallbacks {
    fun onEvent(kind: String, json: String)
    fun onRequest(id: Long, kind: String, json: String)
}

/** One thread for the engine's calls, which block, to run one after another */
expect fun engineCallDispatcher(): CoroutineDispatcher
