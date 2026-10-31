@file:OptIn(ExperimentalForeignApi::class)

package app.typemotion.engine

import app.typemotion.native.tm_call
import app.typemotion.native.tm_cancel
import app.typemotion.native.tm_free
import app.typemotion.native.tm_init
import app.typemotion.native.tm_respond
import app.typemotion.native.tm_shutdown
import kotlinx.cinterop.COpaquePointer
import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ByteVar
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.StableRef
import kotlinx.cinterop.asStableRef
import kotlinx.cinterop.staticCFunction
import kotlinx.cinterop.toKString
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.newSingleThreadContext

/** The C functions of src/ios/typemotion_ios.h, linked into the app */
actual object NativeEngine {
    private var callbacksRef: StableRef<NativeCallbacks>? = null

    actual fun init(config: String, callbacks: NativeCallbacks): String {
        callbacksRef?.dispose()
        val ref = StableRef.create(callbacks)
        callbacksRef = ref
        return take(tm_init(config, onEvent, onRequest, ref.asCPointer()))
    }

    actual fun call(method: String, args: String): String = take(tm_call(method, args))

    actual fun respond(id: Long, answer: String) = tm_respond(id, answer)

    actual fun cancel() = tm_cancel()

    actual fun shutdown() = tm_shutdown()

    /** A string the engine returned, which is ours to free */
    internal fun take(str: CPointer<ByteVar>?): String {
        if (str == null) return "{}"
        val text = str.toKString()
        tm_free(str)
        return text
    }

    private val onEvent = staticCFunction { user: COpaquePointer?, kind: CPointer<ByteVar>?, json: CPointer<ByteVar>? ->
        user?.asStableRef<NativeCallbacks>()?.get()?.onEvent(kind?.toKString() ?: "", json?.toKString() ?: "{}")
        Unit
    }

    private val onRequest = staticCFunction { user: COpaquePointer?, id: Long, kind: CPointer<ByteVar>?, json: CPointer<ByteVar>? ->
        user?.asStableRef<NativeCallbacks>()?.get()?.onRequest(id, kind?.toKString() ?: "", json?.toKString() ?: "{}")
        Unit
    }
}

@OptIn(DelicateCoroutinesApi::class)
actual fun engineCallDispatcher(): CoroutineDispatcher = newSingleThreadContext("TypeMotion calls")
