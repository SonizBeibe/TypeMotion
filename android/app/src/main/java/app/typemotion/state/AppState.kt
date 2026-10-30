package app.typemotion.state

import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import app.typemotion.engine.Engine
import app.typemotion.engine.EngineError
import app.typemotion.engine.EngineRequest
import app.typemotion.engine.bool
import app.typemotion.engine.int
import app.typemotion.engine.ints
import app.typemotion.engine.jsonOf
import app.typemotion.engine.objects
import app.typemotion.engine.str
import app.typemotion.engine.strings
import app.typemotion.model.AutomationScript
import app.typemotion.model.Commit
import app.typemotion.model.CommandInfo
import app.typemotion.model.FileInfo
import app.typemotion.model.Line
import app.typemotion.platform.Platform
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.yield
import org.json.JSONObject
import java.io.File

/** The app's screens */
sealed interface Screen {
    data object Home : Screen
    data object Editor : Screen
    data object Automation : Screen
    data object MyAutomations : Screen
    data class Styles(val editStyle: String? = null) : Screen
    data object Properties : Screen
    data object ShiftTimes : Screen
    data object SelectLines : Screen
    data class Search(val replace: Boolean) : Screen
    data object Settings : Screen
}

enum class EngineStatus { Starting, Running, Failed }

/** One of the filters of a file dialog, from a wxWidgets wildcard */
data class FileFilter(val name: String, val patterns: List<String>) {
    fun matches(name: String): Boolean = patterns.any { pattern ->
        val regex = Regex(
            pattern.split('*').joinToString(".*") { Regex.escape(it) }.replace("\\?", "."),
            RegexOption.IGNORE_CASE,
        )
        regex.matches(name)
    }

    companion object {
        /** "ASS files (*.ass)|*.ass|All files (*.*)|*.*" */
        fun parse(wildcard: String): List<FileFilter> {
            val parts = wildcard.split('|')
            if (parts.size < 2) return listOf(FileFilter("*.*", listOf("*")))
            return parts.chunked(2).filter { it.size == 2 }.map { (name, patterns) ->
                FileFilter(name, patterns.split(';').map { it.trim() }.map { if (it == "*.*") "*" else it })
            }
        }
    }
}

/** The file picker, shown for an open_file or save_file request or for the app's own buttons */
class FilePickerRequest(
    val title: String,
    val save: Boolean,
    val startDir: File?,
    val fileName: String = "",
    val filters: List<FileFilter> = listOf(FileFilter("*.*", listOf("*"))),
    val multiple: Boolean = false,
    val promptOverwrite: Boolean = true,
    /** The chosen paths, or null if cancelled */
    val onResult: (List<String>?) -> Unit,
)

/** The progress of a running automation script */
class ProgressState(title: String, message: String) {
    var title by mutableStateOf(title)
    var message by mutableStateOf(message)
    var percent by mutableIntStateOf(0)
    var indeterminate by mutableStateOf(true)
    val log = mutableStateListOf<String>()
    /** The script ended, and the dialog stays to show what it wrote */
    var finished by mutableStateOf(false)
    var cancelling by mutableStateOf(false)
}

/**
 * Everything the app shows, kept up to date from the engine's events. It
 * lives as long as the app's process, like the engine.
 */
class AppState(val platform: Platform) : Engine.Listener {
    val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    val engine = Engine(this)

    var status by mutableStateOf(EngineStatus.Starting)
        private set
    var startError by mutableStateOf("")
        private set

    /**
     * Why the app closed the last time, if it crashed. The engine doesn't
     * start until it's been seen, since starting may close the app again.
     */
    var crashReport by mutableStateOf(readCrashReport())
        private set

    private fun readCrashReport(): String? =
        try {
            platform.crashReportFile.takeIf { it.exists() }?.readText()?.takeIf { it.isNotBlank() }
        } catch (e: Exception) {
            null
        }

    /** Forget the crash report and start */
    fun dismissCrashReport() {
        platform.crashReportFile.delete()
        crashReport = null
        start()
    }

    // Navigation
    val backStack = mutableStateListOf<Screen>(Screen.Home)
    val screen: Screen get() = backStack.last()

    fun navigate(screen: Screen) {
        if (this.screen != screen) backStack.add(screen)
    }

    fun back(): Boolean {
        if (backStack.size <= 1) return false
        backStack.removeAt(backStack.lastIndex)
        return true
    }

    fun showEditor() {
        while (backStack.size > 1) backStack.removeAt(backStack.lastIndex)
        backStack.add(Screen.Editor)
    }

    // The open file
    var fileInfo by mutableStateOf(FileInfo())
        private set
    val lines = mutableStateListOf<Line>()
    var selection by mutableStateOf(emptySet<Int>())
        private set
    var activeRow by mutableIntStateOf(-1)
        private set
    val editBox = EditBoxController(this)
    val media = MediaState(this)
    /** How the grid shows override tags: 0 shown, 1 replaced by [tagChar], 2 hidden */
    var tagMode by mutableIntStateOf(1)
        private set
    var tagChar by mutableStateOf("☀")
        private set
    /** Bumped when the styles change, so that screens showing them reload */
    var stylesVersion by mutableIntStateOf(0)
        private set
    val scrollRequests = MutableSharedFlow<Int>(extraBufferCapacity = 16)

    // The program
    var recent by mutableStateOf(listOf<String>())
        private set
    var commands by mutableStateOf(mapOf<String, CommandInfo>())
        private set
    var menus by mutableStateOf(JSONObject())
        private set
    var scripts by mutableStateOf(listOf<AutomationScript>())
        private set

    // What the engine asks and tells
    var busy by mutableStateOf(false)
        private set
    val requests = mutableStateListOf<EngineRequest>()
    var progress by mutableStateOf<ProgressState?>(null)
    var filePicker by mutableStateOf<FilePickerRequest?>(null)
    var errorMessage by mutableStateOf<String?>(null)
    val snackbar = SnackbarHostState()

    /** Whether the app can use the phone's storage, checked again when the app comes back */
    var storageAccess by mutableStateOf(platform.hasStorageAccess())
        private set

    /** The app is shown again, maybe after the user gave it access to the storage */
    fun onResume() {
        val access = platform.hasStorageAccess()
        if (access && !storageAccess && status == EngineStatus.Running) {
            // The engine couldn't make the TypeMotion folder without access
            run {
                withContext(Dispatchers.IO) {
                    userAutomationFolder.mkdirs()
                    userIncludeFolder.mkdirs()
                    File(platform.folders.storage, "fonts").mkdirs()
                }
                scripts = AutomationScript.list(engine.call("reload_scripts"))
            }
        }
        storageAccess = access
        if (status == EngineStatus.Running) run { refreshRecent() }
    }

    /** "system", "dark" or "light" */
    var theme by mutableStateOf(platform.getSetting("theme", "dark"))
        private set

    fun setThemeSetting(value: String) {
        theme = value
        platform.setSetting("theme", value)
    }

    private var startJob: Job? = null

    fun start() {
        if (crashReport != null) return
        if (status == EngineStatus.Running || startJob?.isActive == true) return
        status = EngineStatus.Starting
        startJob = scope.launch {
            try {
                val folders = platform.folders
                withContext(Dispatchers.IO) {
                    platform.installData()
                    for (dir in listOf(folders.user, folders.cache, folders.storage))
                        dir.mkdirs()
                }
                engine.start(jsonOf(
                    "data_dir" to folders.data.path,
                    "user_dir" to folders.user.path,
                    "cache_dir" to folders.cache.path,
                    "storage_dir" to folders.storage.path,
                    "font_dirs" to platform.systemFontDirs,
                    "locale" to platform.locale,
                    "crash_report" to platform.crashReportFile.path,
                    "crash_header" to platform.crashReportHeader,
                ))
                commands = engine.call("commands").objects("commands")
                    .map(CommandInfo::from).associateBy { it.name }
                menus = engine.call("menus").optJSONObject("menus") ?: JSONObject()
                status = EngineStatus.Running
                refreshAll()
                refreshRecent()
                refreshScripts()
            } catch (e: Throwable) {
                startError = e.message ?: e.toString()
                status = EngineStatus.Failed
            }
        }
    }

    /** Run something which calls the engine, showing its errors */
    fun run(block: suspend () -> Unit): Job = scope.launch {
        try {
            block()
        } catch (e: EngineError) {
            errorMessage = e.message
        }
    }

    suspend fun call(method: String, vararg args: Pair<String, Any?>): JSONObject = engine.call(method, jsonOf(*args))

    /** Run one of the program's commands, as its menu item or button does */
    fun runCommand(name: String) = run {
        fileInfo = FileInfo.from(engine.call("run_command", "name" to name))
        if (name.startsWith("grid/tags/")) refreshTagMode()
    }

    suspend fun commandState(names: List<String>): JSONObject = engine.call("command_state", "names" to names)

    // Files

    fun openFile(path: String) = run {
        val result = engine.call("open", "path" to path)
        if (result.bool("cancelled")) return@run
        fileInfo = FileInfo.from(result)
        if (result.bool("opened")) showEditor()
    }

    fun newFile() = run {
        val result = engine.call("new")
        if (result.bool("cancelled")) return@run
        fileInfo = FileInfo.from(result)
        showEditor()
    }

    /**
     * Save, asking for a name if the file has none yet, as subtitle/save
     * does. The desktop greys its button out when nothing changed; here it
     * says so.
     */
    fun save() {
        if (!fileInfo.modified) showMessage(Strings.nothingToSave)
        else runCommand("subtitle/save")
    }

    fun undo() = run { fileInfo = FileInfo.from(engine.call("undo")) }
    fun redo() = run { fileInfo = FileInfo.from(engine.call("redo")) }

    suspend fun refreshRecent() {
        recent = engine.call("recent", "list" to "Subtitle").strings("files")
    }

    suspend fun refreshScripts() {
        scripts = AutomationScript.list(engine.call("macros"))
    }

    private suspend fun refreshTagMode() {
        tagMode = engine.call("get_option", "name" to "Subtitle/Grid/Hide Overrides").int("value", 1)
        tagChar = engine.call("get_option", "name" to "Subtitle/Grid/Hide Overrides Char").str("value", "☀")
    }

    private suspend fun refreshAll() {
        fileInfo = FileInfo.from(engine.call("file_info"))
        val all = engine.call("lines").objects("lines").map(Line::from)
        lines.clear()
        lines.addAll(all)
        val sel = engine.call("selection")
        selection = sel.ints("rows").toSet()
        activeRow = sel.int("active", -1)
        editBox.load()
        refreshTagMode()
        media.refresh()
    }

    // Selecting lines

    fun select(rows: Set<Int>, active: Int) = run {
        val result = engine.call("select", "rows" to rows.toList(), "active" to active)
        selection = result.ints("rows").toSet()
        activeRow = result.int("active", -1)
    }

    /** A tap on a line, which selects only it like a click in the desktop grid */
    fun selectOnly(row: Int) = select(setOf(row), row)

    /** Add a line to the selection or take it out, like Ctrl+click */
    fun toggleSelected(row: Int) {
        val rows = if (row in selection) selection - row else selection + row
        val active = when {
            row in rows -> row
            activeRow in rows -> activeRow
            else -> rows.minOrNull() ?: row
        }
        select(rows.ifEmpty { setOf(row) }, active)
    }

    /** Select the lines from the active one to this one, like Shift+click */
    fun selectRange(row: Int) {
        val from = if (activeRow >= 0) activeRow else row
        val rows = (minOf(from, row)..maxOf(from, row)).toSet()
        select(rows, from)
    }

    fun toggleFold(row: Int) = run { engine.call("toggle_fold", "row" to row) }

    // Keeping up with changes

    private var refreshJob: Job? = null
    private var refreshAllLines = false
    private val refreshRows = mutableSetOf<Int>()

    private fun onCommit(type: Int, row: Int?) {
        val lineOnly = type != Commit.NEW && type and Commit.DIAG_FULL == type && row != null
        if (lineOnly) refreshRows.add(row!!) else refreshAllLines = true
        if (type == Commit.NEW || type and Commit.STYLES != 0) stylesVersion++
        // Many commits in a row, from a script, are read once
        if (refreshJob?.isActive == true) return
        refreshJob = scope.launch {
            yield()
            try {
                // More commits can come while reading: read until none is left
                while (refreshAllLines || refreshRows.isNotEmpty()) {
                    if (refreshAllLines) {
                        refreshAllLines = false
                        refreshRows.clear()
                        val all = engine.call("lines").objects("lines").map(Line::from)
                        lines.clear()
                        lines.addAll(all)
                    }
                    else {
                        val rows = refreshRows.toList()
                        refreshRows.clear()
                        for (r in rows) {
                            val line = engine.call("lines", "start" to r, "count" to 1).objects("lines").firstOrNull()
                                ?.let(Line::from) ?: continue
                            if (r < lines.size) lines[r] = line
                        }
                    }
                    fileInfo = FileInfo.from(engine.call("file_info"))
                }
            } catch (e: EngineError) {
                errorMessage = e.message
            }
        }
    }

    override fun onEvent(kind: String, data: JSONObject) {
        when (kind) {
            "commit" -> onCommit(data.int("type"), if (data.has("row")) data.int("row") else null)
            "selection" -> {
                selection = data.ints("rows").toSet()
                activeRow = data.int("active", -1)
            }
            "edit_box" -> editBox.onState(data)
            "text_selection" -> editBox.onTextSelection(data)
            "file_open" -> {
                run {
                    fileInfo = FileInfo.from(engine.call("file_info"))
                    refreshRecent()
                    refreshScripts()
                }
                if (screen == Screen.Home) showEditor()
            }
            "file_save" -> run {
                fileInfo = FileInfo.from(engine.call("file_info"))
                refreshRecent()
            }
            "log" -> {
                val message = data.str("message")
                if (data.str("level") == "error") errorMessage = message
                else showMessage(message)
            }
            "status" -> showMessage(data.str("text"))
            "busy" -> busy = data.bool("busy")
            "progress" -> onProgress(data)
            "progress_log" -> progress?.log?.add(data.str("text"))
            "clipboard_set" -> platform.setClipboardText(data.str("text"))
            "scroll_to" -> scrollRequests.tryEmit(data.int("row"))
            "open_dialog" -> openDialog(data.str("name"), data)
            "close_window" -> platform.closeApp()
            "scripts_changed" -> run { refreshScripts() }
            "video", "audio", "video_seek", "video_frame", "audio_position", "audio_stop" -> media.onEvent(kind, data)
        }
    }

    override fun onRequest(request: EngineRequest) {
        when (request.kind) {
            "clipboard_get" -> engine.respond(request, jsonOf("text" to platform.getClipboardText()))
            "open_file", "save_file" -> showFilePickerFor(request)
            else -> requests.add(request)
        }
    }

    /** Answer the request the dialogs show, and take it off the list */
    fun answer(request: EngineRequest, answer: JSONObject = JSONObject()) {
        requests.remove(request)
        engine.respond(request, answer)
    }

    fun showMessage(text: String) {
        if (text.isBlank()) return
        scope.launch { snackbar.showSnackbar(text) }
    }

    private fun onProgress(data: JSONObject) {
        when (data.str("state")) {
            "start" -> progress = ProgressState(data.str("title"), data.str("message"))
            "update" -> progress?.apply {
                title = data.str("title", title)
                message = data.str("message")
                indeterminate = data.bool("indeterminate")
                percent = data.int("percent")
            }
            "end" -> progress?.let { p ->
                // Like the desktop progress dialog, stay open to show what the
                // script wrote
                if (p.log.isEmpty()) progress = null
                else {
                    p.finished = true
                    p.percent = 100
                    p.indeterminate = false
                }
            }
        }
    }

    private fun openDialog(name: String, data: JSONObject) {
        when (name) {
            "style_manager" -> navigate(Screen.Styles())
            "properties" -> navigate(Screen.Properties)
            "shift_times" -> navigate(Screen.ShiftTimes)
            "select_lines" -> navigate(Screen.SelectLines)
            "search_replace" -> navigate(Screen.Search(data.bool("replace")))
            "automation" -> navigate(Screen.MyAutomations)
            else -> showMessage(Strings.notAvailable)
        }
    }

    private fun showFilePickerFor(request: EngineRequest) {
        val data = request.data
        val save = request.kind == "save_file"
        val dir = data.str("dir").takeIf { it.isNotEmpty() }?.let(::File)
        filePicker = FilePickerRequest(
            title = data.str("message"),
            save = save,
            startDir = dir,
            fileName = data.str("file"),
            filters = FileFilter.parse(data.str("wildcard")),
            multiple = data.bool("multiple"),
            promptOverwrite = data.bool("prompt_overwrite", true),
        ) { paths ->
            filePicker = null
            engine.respond(request, when {
                paths.isNullOrEmpty() -> JSONObject()
                save -> jsonOf("path" to paths.first())
                else -> jsonOf("paths" to paths)
            })
        }
    }

    /** Pick a file to open with the app's file picker */
    fun pickFileToOpen() {
        filePicker = FilePickerRequest(
            title = Strings.openSubtitles,
            save = false,
            startDir = recent.firstOrNull()?.let { File(it).parentFile } ?: platform.folders.storage,
            filters = FileFilter.parse(Strings.subtitleWildcard),
        ) { paths ->
            filePicker = null
            paths?.firstOrNull()?.let(::openFile)
        }
    }

    // Folders

    val userAutomationFolder: File get() = File(platform.folders.storage, "automation/autoload")
    val userIncludeFolder: File get() = File(platform.folders.storage, "automation/include")

    /** Copy a script into the user's automation folder and load it */
    fun importScript(path: String) = run {
        val source = File(path)
        val target = File(userAutomationFolder, source.name)
        withContext(Dispatchers.IO) {
            userAutomationFolder.mkdirs()
            source.copyTo(target, overwrite = true)
        }
        scripts = AutomationScript.list(engine.call("reload_scripts"))
        showMessage(Strings.scriptImported(source.name))
    }

    fun reloadScripts() = run {
        scripts = AutomationScript.list(engine.call("reload_scripts"))
        showMessage(Strings.scriptsReloaded)
    }

    /** The Automation Manager's Add: scripts for the open file only */
    fun addLocalScripts(paths: List<String>) = run {
        scripts = AutomationScript.list(engine.call("add_scripts", "paths" to paths))
    }

    fun removeScript(path: String) = run {
        scripts = AutomationScript.list(engine.call("remove_script", "path" to path))
    }

    fun reloadScript(path: String) = run {
        scripts = AutomationScript.list(engine.call("reload_script", "path" to path))
    }

    fun flush() {
        if (status != EngineStatus.Running) return
        scope.launch {
            try {
                engine.call("flush")
            } catch (_: EngineError) {
            }
        }
    }
}
