package app.typemotion.android

import android.app.Application
import android.util.Log
import app.typemotion.state.AppState

/** Keeps the engine and the app's state for as long as the process lives */
class TypeMotionApplication : Application() {
    lateinit var platform: AndroidPlatform
        private set
    lateinit var app: AppState
        private set

    override fun onCreate() {
        super.onCreate()
        platform = AndroidPlatform(this)
        writeCrashReports()
        app = AppState(platform)
    }

    /** Write down Java's errors which close the app, as the engine does its own */
    private fun writeCrashReports() {
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            try {
                platform.crashReportFile.writeText(
                    platform.crashReportHeader + "\nError en el hilo ${thread.name}:\n" + Log.getStackTraceString(error)
                )
            } catch (_: Throwable) {
            }
            previous?.uncaughtException(thread, error)
        }
    }
}
