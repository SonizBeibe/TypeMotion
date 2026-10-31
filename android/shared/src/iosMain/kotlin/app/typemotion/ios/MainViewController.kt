@file:OptIn(ExperimentalForeignApi::class)

package app.typemotion.ios

import androidx.compose.ui.window.ComposeUIViewController
import app.typemotion.state.AppState
import app.typemotion.ui.TypeMotionApp
import kotlinx.cinterop.ExperimentalForeignApi
import platform.AVFAudio.AVAudioSession
import platform.AVFAudio.AVAudioSessionCategoryPlayback
import platform.AVFAudio.setActive
import platform.Foundation.NSNotificationCenter
import platform.Foundation.NSOperationQueue
import platform.UIKit.UIApplicationDidBecomeActiveNotification
import platform.UIKit.UIApplicationDidEnterBackgroundNotification
import platform.UIKit.UIViewController

/** Keeps the engine and the app's state for as long as the app runs */
private val app: AppState by lazy {
    val platform = IosPlatform()
    platform.folders.user.mkdirs()
    AppState(platform).also { app ->
        val center = NSNotificationCenter.defaultCenter
        val main = NSOperationQueue.mainQueue
        center.addObserverForName(UIApplicationDidBecomeActiveNotification, null, main) { _ -> app.onResume() }
        // iOS can end the app from here on without telling it
        center.addObserverForName(UIApplicationDidEnterBackgroundNotification, null, main) { _ -> app.flush() }
    }
}

/** The app's screen, which iosApp/TypeMotionApp.swift shows */
@Suppress("FunctionName", "unused")
fun MainViewController(): UIViewController {
    // Play audio even with the phone's silent switch on, as video players do
    try {
        val session = AVAudioSession.sharedInstance()
        session.setCategory(AVAudioSessionCategoryPlayback, null)
        session.setActive(true, null)
    } catch (_: Throwable) {
    }
    app.start()
    return ComposeUIViewController(configure = { enforceStrictPlistSanityCheck = false }) { TypeMotionApp(app) }
}
