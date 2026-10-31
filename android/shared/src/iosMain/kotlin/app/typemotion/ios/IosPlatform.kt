@file:OptIn(ExperimentalForeignApi::class)

package app.typemotion.ios

import app.typemotion.io.File
import app.typemotion.platform.Folders
import app.typemotion.platform.Platform
import kotlinx.cinterop.ExperimentalForeignApi
import platform.Foundation.NSBundle
import platform.Foundation.NSCachesDirectory
import platform.Foundation.NSDocumentDirectory
import platform.Foundation.NSLibraryDirectory
import platform.Foundation.NSLocale
import platform.Foundation.NSSearchPathForDirectoriesInDomains
import platform.Foundation.NSUserDefaults
import platform.Foundation.NSUserDomainMask
import platform.Foundation.currentLocale
import platform.Foundation.localeIdentifier
import platform.UIKit.UIDevice
import platform.UIKit.UIPasteboard

/**
 * The iPhone: the app's own folders, and its Documents folder as the
 * TypeMotion folder, which the Files app shows as "En mi iPhone › TypeMotion"
 * for the user to put subtitles, videos, scripts and fonts in
 */
class IosPlatform : Platform {
    private fun dir(kind: ULong): String =
        NSSearchPathForDirectoriesInDomains(kind, NSUserDomainMask, true).firstOrNull() as? String ?: "/tmp"

    private val library = dir(NSLibraryDirectory)

    override val folders = Folders(
        data = File(library, "TypeMotion/data"),
        user = File(library, "TypeMotion/user"),
        cache = File(dir(NSCachesDirectory)),
        storage = File(dir(NSDocumentDirectory)),
    )

    private val version: String
        get() = NSBundle.mainBundle.objectForInfoDictionaryKey("CFBundleShortVersionString") as? String ?: "?"

    private val build: String
        get() = NSBundle.mainBundle.objectForInfoDictionaryKey("CFBundleVersion") as? String ?: "?"

    override fun installData() {
        // The official scripts and the translations come in the app; copy
        // them again after an update
        val stamp = "$version ($build)"
        val marker = File(folders.data, ".installed")
        if (marker.exists() && marker.readText() == stamp) return

        val resources = NSBundle.mainBundle.resourcePath ?: return
        for (folder in listOf("automation", "locale")) {
            val target = File(folders.data, folder)
            target.deleteRecursively()
            copyTree(File(resources, folder), target)
        }
        folders.data.mkdirs()
        marker.writeText(stamp)
    }

    private fun copyTree(source: File, target: File) {
        if (!source.isDirectory) {
            if (source.exists()) source.copyTo(target, overwrite = true)
            return
        }
        target.mkdirs()
        for (child in source.listFiles() ?: emptyArray()) copyTree(child, File(target, child.name))
    }

    // The app's own Documents folder is always there to use
    override fun hasStorageAccess(): Boolean = true

    override fun requestStorageAccess() {
    }

    override fun storageRoots(): List<Pair<String, File>> = listOf("TypeMotion" to folders.storage)

    override fun getClipboardText(): String = UIPasteboard.generalPasteboard.string ?: ""

    override fun setClipboardText(text: String) {
        UIPasteboard.generalPasteboard.string = text
    }

    override val locale: String
        get() = NSLocale.currentLocale.localeIdentifier.substringBefore('@')

    override val systemFontDirs = listOf("/System/Library/Fonts")

    private val defaults get() = NSUserDefaults.standardUserDefaults

    override fun getSetting(key: String, default: String): String = defaults.stringForKey("typemotion.$key") ?: default

    override fun setSetting(key: String, value: String) {
        defaults.setObject(value, "typemotion.$key")
    }

    // iPhone apps don't close themselves; the user leaves them
    override fun closeApp() {
    }

    override val crashReportFile = File(library, "TypeMotion/crash-report.txt")

    override val crashReportHeader: String by lazy {
        val device = UIDevice.currentDevice
        "TypeMotion $version ($build)\n" +
            "${device.model}, ${device.systemName} ${device.systemVersion}\n"
    }
}
