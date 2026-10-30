package app.typemotion.engine

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.util.concurrent.Executors

/** An API method failed; the message is the engine's */
class EngineError(message: String) : Exception(message)

/** A question the engine is waiting for the answer to */
class EngineRequest(val id: Long, val kind: String, val data: JSONObject)

/**
 * Calls the engine from coroutines, and hands its events and requests to
 * the listener on the main thread, in the order they were sent.
 */
class Engine(private val listener: Listener) {
    interface Listener {
        fun onEvent(kind: String, data: JSONObject)
        fun onRequest(request: EngineRequest)
    }

    private val main = CoroutineScope(SupervisorJob() + Dispatchers.Main)

    /** The calls block, so they run one after another on their own thread */
    private val callThread = Executors.newSingleThreadExecutor { runnable ->
        Thread(runnable, "TypeMotion calls").apply { isDaemon = true }
    }.asCoroutineDispatcher()

    private val callbacks = object : NativeCallbacks {
        override fun onEvent(kind: String, json: String) {
            val data = parse(json)
            main.launch { listener.onEvent(kind, data) }
        }

        override fun onRequest(id: Long, kind: String, json: String) {
            val request = EngineRequest(id, kind, parse(json))
            main.launch { listener.onRequest(request) }
        }
    }

    var running = false
        private set

    /** Start the engine with the app's folders */
    suspend fun start(config: JSONObject) = withContext(callThread) {
        val result = parse(NativeEngine.init(config.toString(), callbacks))
        if (result.has("error"))
            throw EngineError(result.str("error"))
        running = true
    }

    suspend fun call(method: String, args: JSONObject = JSONObject()): JSONObject = withContext(callThread) {
        val result = parse(NativeEngine.call(method, args.toString()))
        if (result.has("error"))
            throw EngineError(result.str("error"))
        result
    }

    suspend fun call(method: String, vararg args: Pair<String, Any?>): JSONObject = call(method, jsonOf(*args))

    /** Answer a request; this doesn't wait for anything */
    fun respond(request: EngineRequest, answer: JSONObject = JSONObject()) {
        NativeEngine.respond(request.id, answer.toString())
    }

    /** Ask the running automation script to stop */
    fun cancel() {
        NativeEngine.cancel()
    }

    suspend fun shutdown() = withContext(callThread) {
        if (running) {
            NativeEngine.shutdown()
            running = false
        }
    }

    private fun parse(json: String): JSONObject =
        try {
            JSONObject(json)
        } catch (e: Exception) {
            JSONObject()
        }
}
