package app.typemotion.ui.editor

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.Check
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import app.typemotion.engine.EngineError
import app.typemotion.engine.objects
import app.typemotion.engine.str
import app.typemotion.engine.strings
import app.typemotion.state.AppState
import app.typemotion.state.Screen
import org.json.JSONObject
import java.io.File
import java.text.Collator

/** A page of the menus: one of the desktop menus, a recent files list or the automation macros */
private sealed interface MenuPage {
    val title: String

    data class Menu(val name: String, override val title: String) : MenuPage
    data class Recent(val list: String, override val title: String) : MenuPage
    data class Automation(val path: List<String>, override val title: String) : MenuPage
}

private sealed interface MenuEntry {
    data object Separator : MenuEntry
    data class Command(val name: String, val label: String) : MenuEntry
    data class Submenu(val page: MenuPage) : MenuEntry
    data class Disabled(val label: String) : MenuEntry
}

/** Commands which don't work on the phone yet: the windows which haven't been made for it */
private val unavailablePrefixes = listOf("help/", "app/display/")
private val unavailableCommands = setOf(
    "app/new_window", "app/language", "app/toggle/toolbar", "app/updates", "app/log",
    "app/about", "tool/export", "tool/font_collector",
)
private val unavailableMenus = setOf("main/help")

/**
 * The desktop's video and audio commands which the app does itself, since
 * they are about its windows: the video panel and the file picker
 */
private val appCommands = mapOf(
    "video/open" to "Abrir video...",
    "video/close" to "Cerrar video",
    "video/jump/start" to "Ir al inicio del video",
    "video/jump/end" to "Ir al final del video",
    "video/play" to "Reproducir",
    "video/play/line" to "Reproducir la línea",
    "video/stop" to "Detener",
    "video/frame/next" to "Cuadro siguiente",
    "video/frame/prev" to "Cuadro anterior",
    "audio/open" to "Abrir audio...",
    "audio/open/video" to "Abrir el audio del video",
    "audio/close" to "Cerrar audio",
)

private fun runAppCommand(app: AppState, name: String) {
    val media = app.media
    when (name) {
        "video/open" -> media.pickVideo()
        "video/close" -> media.closeVideo()
        "video/jump/start" -> media.jumpToStart()
        "video/jump/end" -> media.jumpToEnd()
        "video/play" -> media.play()
        "video/play/line" -> media.playLine()
        "video/stop" -> media.stop()
        "video/frame/next" -> media.step(1)
        "video/frame/prev" -> media.step(-1)
        "audio/open" -> media.pickAudio()
        "audio/open/video" -> media.openVideoAudio()
        "audio/close" -> media.closeAudio()
    }
}

private fun appCommandValid(app: AppState, name: String): Boolean = when (name) {
    "video/open", "audio/open" -> true
    "audio/open/video" -> app.media.video != null
    "audio/close" -> app.media.audio != null
    else -> app.media.video != null
}

/** The recent files lists of the menus, and their titles */
private val recentLists = mapOf(
    "Subtitle" to "Recientes", "Video" to "Videos recientes", "Audio" to "Audios recientes",
    "Timecodes" to "Timecodes recientes", "Keyframes" to "Keyframes recientes",
)

/** Titles of the menus, which the engine doesn't translate yet */
private val menuTitles = mapOf(
    "&File" to "Archivo", "&Edit" to "Editar", "&Subtitle" to "Subtítulos", "&Timing" to "Tiempos",
    "&Video" to "Video", "&Audio" to "Audio", "&Tools" to "Herramientas", "Vie&w" to "Ver", "&Help" to "Ayuda",
    "&Recent" to "Recientes", "Recent" to "Recientes",
    "&Insert Lines" to "Insertar líneas", "Join Lines" to "Unir líneas", "Sort All Lines" to "Ordenar todas las líneas",
    "Sort Selected Lines" to "Ordenar líneas seleccionadas",
)

private fun menuLabel(text: String) = menuTitles[text] ?: text.replace("&", "")

private fun isAvailable(command: String) =
    command !in unavailableCommands && unavailablePrefixes.none { command.startsWith(it) }

/** The desktop program's menu bar */
@Composable
fun MainMenuSheet(app: AppState, onDismiss: () -> Unit) {
    MenuSheet(app, rootMenu = "main", title = "Menú", onDismiss = onDismiss)
}

/** One of the desktop program's menus, with its submenus, as a sheet */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MenuSheet(app: AppState, rootMenu: String, title: String, onDismiss: () -> Unit) {
    val stack = remember { mutableStateListOf<MenuPage>(MenuPage.Menu(rootMenu, title)) }
    val page = stack.last()
    var recentFiles by remember(page) { mutableStateOf<List<String>>(emptyList()) }
    val entries = remember(page, recentFiles, app.scripts) { menuEntries(app, page, recentFiles) }
    var state by remember(page) { mutableStateOf(JSONObject()) }

    LaunchedEffect(page) {
        try {
            if (page is MenuPage.Recent)
                recentFiles = app.call("recent", "list" to page.list).strings("files")
        } catch (_: EngineError) {
        }
    }
    LaunchedEffect(entries) {
        val names = entries.filterIsInstance<MenuEntry.Command>().map { it.name }
        state = try {
            app.commandState(names)
        } catch (_: EngineError) {
            JSONObject()
        }
    }

    fun run(name: String) {
        onDismiss()
        // The desktop preferences are the app's settings
        if (name == "app/options") app.navigate(Screen.Settings)
        else if (name in appCommands) runAppCommand(app, name)
        else app.runCommand(name)
    }

    ModalBottomSheet(onDismissRequest = onDismiss, sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)) {
        Column(Modifier.fillMaxWidth().padding(bottom = 16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(horizontal = 8.dp)) {
                if (stack.size > 1) {
                    IconButton(onClick = { stack.removeAt(stack.lastIndex) }) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Atrás")
                    }
                } else Spacer(Modifier.width(12.dp))
                Text(page.title, style = MaterialTheme.typography.titleMedium)
            }
            HorizontalDivider()
            LazyColumn(Modifier.fillMaxWidth()) {
                items(entries) { entry ->
                    when (entry) {
                        MenuEntry.Separator -> HorizontalDivider(Modifier.padding(vertical = 4.dp))
                        is MenuEntry.Disabled -> MenuRow(entry.label, enabled = false, onClick = {})
                        is MenuEntry.Submenu -> MenuRow(entry.page.title, submenu = true) { stack.add(entry.page) }
                        is MenuEntry.Command -> {
                            val valid = if (entry.name in appCommands) appCommandValid(app, entry.name)
                                else state.optJSONObject("valid")?.optBoolean(entry.name, true) ?: true
                            val active = state.optJSONObject("active")?.optBoolean(entry.name, false) ?: false
                            val label = state.optJSONObject("menu")?.optString(entry.name)?.takeIf { it.isNotEmpty() }
                                ?.replace("&", "") ?: entry.label
                            val help = app.commands[entry.name]?.help.orEmpty()
                            MenuRow(label, help = help, checked = active, enabled = valid) { run(entry.name) }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun MenuRow(
    label: String,
    help: String = "",
    checked: Boolean = false,
    enabled: Boolean = true,
    submenu: Boolean = false,
    onClick: () -> Unit,
) {
    val alpha = if (enabled) 1f else 0.4f
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = enabled, onClick = onClick)
            .heightIn(min = 48.dp)
            .padding(horizontal = 16.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        if (checked) Icon(Icons.Default.Check, contentDescription = "Activado", modifier = Modifier.size(20.dp))
        else Spacer(Modifier.width(20.dp))
        Column(Modifier.weight(1f)) {
            Text(label, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurface.copy(alpha = alpha))
            if (help.isNotEmpty()) {
                Text(
                    help,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = alpha),
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
        if (submenu) Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, contentDescription = null)
    }
}

private fun menuEntries(app: AppState, page: MenuPage, recentFiles: List<String>): List<MenuEntry> = when (page) {
    is MenuPage.Menu -> jsonMenu(app, page.name)
    is MenuPage.Recent -> recentFiles.mapIndexed { i, path ->
        MenuEntry.Command("recent/${page.list.lowercase()}/$i", "${i + 1}  ${File(path).name}  (${File(path).parent ?: ""})")
    }.ifEmpty { listOf(MenuEntry.Disabled("Vacío")) }
    is MenuPage.Automation -> automationMenu(app, page)
}

private fun jsonMenu(app: AppState, name: String): List<MenuEntry> {
    val array = app.menus.optJSONArray(name) ?: return emptyList()
    val entries = mutableListOf<MenuEntry>()
    for (item in array.objects()) {
        val text = item.str("text")
        when {
            item.length() == 0 -> if (entries.isNotEmpty() && entries.last() != MenuEntry.Separator) entries.add(MenuEntry.Separator)
            item.has("command") -> {
                val command = item.str("command")
                if (!isAvailable(command)) continue
                // Commands of the desktop's windows which the core doesn't have
                val appLabel = appCommands[command]
                if (appLabel == null && command !in app.commands) continue
                val label = appLabel ?: if (text.isNotEmpty()) menuLabel(text)
                    else app.commands[command]?.menu?.replace("&", "") ?: command
                entries.add(MenuEntry.Command(command, label))
            }
            item.has("recent") -> {
                val list = item.str("recent")
                recentLists[list]?.let { entries.add(MenuEntry.Submenu(MenuPage.Recent(list, it))) }
            }
            item.str("special") == "automation" ->
                entries.add(MenuEntry.Submenu(MenuPage.Automation(emptyList(), menuLabel(text))))
            item.has("submenu") -> {
                val submenu = item.str("submenu")
                if (submenu in unavailableMenus) continue
                val sub = jsonMenu(app, submenu)
                if (sub.any { it !is MenuEntry.Separator })
                    entries.add(MenuEntry.Submenu(MenuPage.Menu(submenu, menuLabel(text))))
            }
        }
    }
    while (entries.lastOrNull() == MenuEntry.Separator) entries.removeAt(entries.lastIndex)
    return entries
}

/** The macros, in submenus by the parts of their names, as the desktop Automation menu shows them */
private fun automationMenu(app: AppState, page: MenuPage.Automation): List<MenuEntry> {
    val macros = app.scripts.flatMap { it.macros }
    if (macros.isEmpty()) return listOf(MenuEntry.Disabled("No Automation macros loaded"))
    val collator = Collator.getInstance()
    val entries = mutableListOf<Pair<String, MenuEntry>>()
    val submenus = mutableSetOf<String>()
    for (macro in macros) {
        val parts = macro.name.split('/')
        if (parts.size <= page.path.size || parts.subList(0, page.path.size) != page.path) continue
        val name = parts[page.path.size]
        if (parts.size == page.path.size + 1)
            entries.add(name to MenuEntry.Command(macro.command, name))
        else if (submenus.add(name))
            entries.add(name to MenuEntry.Submenu(MenuPage.Automation(page.path + name, name)))
    }
    return entries.sortedWith { a, b -> collator.compare(a.first, b.first) }.map { it.second }
}
