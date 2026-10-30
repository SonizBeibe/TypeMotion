package app.typemotion.android

import android.Manifest
import android.app.Activity
import android.app.Application
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings
import androidx.core.app.ActivityCompat
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.core.content.ContextCompat
import app.typemotion.platform.Folders
import app.typemotion.platform.FrameImage
import app.typemotion.platform.Platform
import java.io.File
import java.nio.ByteBuffer
import java.util.Locale

class AndroidPlatform(private val context: Application) : Platform {
    /** The activity on screen, which asks for permissions and can be closed */
    var activity: Activity? = null

    override val folders = Folders(
        data = File(context.filesDir, "data"),
        user = File(context.filesDir, "user"),
        cache = context.cacheDir,
        storage = File(Environment.getExternalStorageDirectory(), "TypeMotion"),
    )

    private val preferences = context.getSharedPreferences("typemotion", Context.MODE_PRIVATE)

    override fun installData() {
        // The official scripts and the translations come in the APK; copy
        // them again after an update
        @Suppress("DEPRECATION")
        val info = context.packageManager.getPackageInfo(context.packageName, 0)
        val stamp = info.lastUpdateTime.toString()
        val marker = File(folders.data, ".installed")
        if (marker.exists() && marker.readText() == stamp) return

        for (folder in listOf("automation", "locale")) {
            val target = File(folders.data, folder)
            target.deleteRecursively()
            copyAssets(folder, target)
        }
        marker.writeText(stamp)
    }

    private fun copyAssets(path: String, target: File) {
        val children = context.assets.list(path) ?: emptyArray()
        if (children.isEmpty()) {
            target.parentFile?.mkdirs()
            context.assets.open(path).use { input -> target.outputStream().use { input.copyTo(it) } }
            return
        }
        target.mkdirs()
        for (child in children) copyAssets("$path/$child", File(target, child))
    }

    override fun hasStorageAccess(): Boolean =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) Environment.isExternalStorageManager()
        else ContextCompat.checkSelfPermission(context, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED

    override fun requestStorageAccess() {
        val activity = activity ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // The "All files access" page of the system settings, for this app
            val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:${context.packageName}"))
            try {
                activity.startActivity(intent)
            } catch (_: Exception) {
                activity.startActivity(Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION))
            }
        } else {
            ActivityCompat.requestPermissions(
                activity,
                arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE),
                1,
            )
        }
    }

    override fun storageRoots(): List<Pair<String, File>> {
        val phone = Environment.getExternalStorageDirectory()
        val roots = mutableListOf(
            "Teléfono" to phone,
            "TypeMotion" to folders.storage,
            "Descargas" to Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
            "Documentos" to Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS),
        )
        // Memory cards: the folder above the app's own Android/data folder on each
        for (dir in context.getExternalFilesDirs(null).drop(1)) {
            val card = dir?.path?.substringBefore("/Android/data")?.let(::File) ?: continue
            roots.add("Tarjeta ${card.name}" to card)
        }
        return roots.filter { it.second.exists() || it.second == folders.storage }
    }

    private val clipboard get() = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    override fun getClipboardText(): String =
        clipboard.primaryClip?.takeIf { it.itemCount > 0 }?.getItemAt(0)?.coerceToText(context)?.toString() ?: ""

    override fun setClipboardText(text: String) {
        clipboard.setPrimaryClip(ClipData.newPlainText("TypeMotion", text))
    }

    override val locale: String
        get() = Locale.getDefault().let { if (it.country.isEmpty()) it.language else "${it.language}_${it.country}" }

    override val systemFontDirs = listOf("/system/fonts")

    override fun getSetting(key: String, default: String): String = preferences.getString(key, default) ?: default

    override fun setSetting(key: String, value: String) {
        preferences.edit().putString(key, value).apply()
    }

    override fun createFrameImage(width: Int, height: Int): FrameImage = BitmapFrameImage(width, height)

    override val crashReportFile = File(context.filesDir, "crash-report.txt")

    override val crashReportHeader: String by lazy {
        @Suppress("DEPRECATION")
        val version = context.packageManager.getPackageInfo(context.packageName, 0).versionName
        val abi = File(context.applicationInfo.nativeLibraryDir).name
        "TypeMotion $version ($abi)\n" +
            "${Build.MANUFACTURER} ${Build.MODEL}, Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})\n" +
            "ABI del teléfono: ${Build.SUPPORTED_ABIS.joinToString()}\n"
    }

    override fun closeApp() {
        activity?.finishAndRemoveTask()
    }
}

private class BitmapFrameImage(override val width: Int, override val height: Int) : FrameImage {
    private val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)

    // ARGB_8888 bitmaps are RGBA in memory, as the engine copies frames
    override fun write(pixels: ByteBuffer) = bitmap.copyPixelsFromBuffer(pixels)

    override fun image(): ImageBitmap = bitmap.asImageBitmap()
}
