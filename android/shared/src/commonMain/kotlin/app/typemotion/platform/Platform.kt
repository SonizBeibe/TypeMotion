package app.typemotion.platform

import androidx.compose.runtime.staticCompositionLocalOf
import app.typemotion.io.File

/**
 * What the app needs from the system it runs on. The Android app implements
 * it in app.typemotion.android and the iPhone app in app.typemotion.ios; the
 * rest of the app doesn't use either system's classes, so that the same
 * screens run on both.
 */
interface Platform {
    /** The app's private folders, and the TypeMotion folder in the phone's storage */
    val folders: Folders

    /**
     * Copy the files which come with the app into [Folders.data] if this
     * version hasn't yet. Called on a background thread before the engine
     * starts.
     */
    fun installData()

    /** Whether the app can read and write any file in the phone's storage */
    fun hasStorageAccess(): Boolean

    /** Ask the user for access to the phone's storage */
    fun requestStorageAccess()

    /** Places to start browsing for files: name and folder */
    fun storageRoots(): List<Pair<String, File>>

    fun getClipboardText(): String
    fun setClipboardText(text: String)

    /** The phone's language, such as es_MX */
    val locale: String

    /** Folders with the phone's own fonts */
    val systemFontDirs: List<String>

    fun getSetting(key: String, default: String): String
    fun setSetting(key: String, value: String)

    /** Leave the app, as the desktop program's Exit does */
    fun closeApp()

    /**
     * Where what made the app close is written down when it crashes, for
     * the app to show the next time it opens
     */
    val crashReportFile: File

    /** The first lines of a crash report: the app's version and the phone */
    val crashReportHeader: String
}

class Folders(
    /** Files which come with the app, such as the official scripts (?data) */
    val data: File,
    /** The app's private files: configuration, recent files, autosaves (?user) */
    val user: File,
    val cache: File,
    /** The TypeMotion folder with the user's own scripts and fonts */
    val storage: File,
)

val LocalPlatform = staticCompositionLocalOf<Platform> { error("No platform") }
