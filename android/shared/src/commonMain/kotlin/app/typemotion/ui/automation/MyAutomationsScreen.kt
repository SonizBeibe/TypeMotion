package app.typemotion.ui.automation

import androidx.compose.animation.animateContentSize
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import app.typemotion.engine.EngineError
import app.typemotion.engine.objects
import app.typemotion.engine.str
import app.typemotion.model.AutomationScript
import app.typemotion.state.AppState
import app.typemotion.state.FileFilter
import app.typemotion.state.FilePickerRequest
import app.typemotion.ui.common.Panel
import app.typemotion.ui.common.SectionTitle
import app.typemotion.ui.common.ToolScreen
import app.typemotion.ui.theme.TypeMotionTheme
import app.typemotion.json.JSONObject
import app.typemotion.io.File

/**
 * The Automation Manager, and the TypeMotion folder where the user's own
 * scripts are loaded from
 */
@Composable
fun MyAutomationsScreen(app: AppState) {
    var info by remember { mutableStateOf(JSONObject()) }
    var expanded by remember { mutableStateOf<String?>(null) }
    var infoText by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(Unit) {
        info = try {
            app.call("automation_info")
        } catch (_: EngineError) {
            JSONObject()
        }
    }
    val filters = FileFilter.parse(info.str("wildcard", "Lua scripts (*.lua,*.moon)|*.lua;*.moon|All Files (*.*)|*.*"))

    fun pickScripts(title: String, multiple: Boolean, onPicked: (List<String>) -> Unit) {
        val last = info.str("last_dir").takeIf { it.isNotEmpty() }?.let(::File)
        app.filePicker = FilePickerRequest(
            title = title,
            save = false,
            startDir = last ?: app.platform.folders.storage,
            filters = filters,
            multiple = multiple,
        ) { paths ->
            app.filePicker = null
            if (!paths.isNullOrEmpty()) onPicked(paths)
        }
    }

    val global = app.scripts.filter { !it.local }
    val local = app.scripts.filter { it.local }

    ToolScreen(
        title = "Administrador de automatización",
        subtitle = "${app.scripts.size} scripts",
        onBack = { app.back() },
        actions = {
            IconButton(onClick = { infoText = managerInfo(app, info, null) }) {
                Icon(Icons.Default.Info, contentDescription = "Información")
            }
        },
    ) {
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionTitle("Tu carpeta TypeMotion")
                Text(
                    "Los scripts .lua y .moon que pongas en esta carpeta se cargan siempre, igual que la carpeta autoload del escritorio:",
                    style = MaterialTheme.typography.bodySmall,
                )
                FolderPath(app.userAutomationFolder.path)
                Text("Los módulos para include van en:", style = MaterialTheme.typography.bodySmall)
                FolderPath(app.userIncludeFolder.path)
                if (!app.storageAccess) {
                    Text(
                        "TypeMotion necesita acceso a los archivos para usar esta carpeta.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.error,
                    )
                    TextButton(onClick = { app.platform.requestStorageAccess() }) { Text("Dar acceso") }
                }
                ManagerButtons(
                    onImport = { pickScripts("Copiar script a tu carpeta", multiple = false) { app.importScript(it.first()) } },
                    onAdd = { pickScripts("Agregar script de automatización", multiple = true) { app.addLocalScripts(it) } },
                    onRescan = { app.reloadScripts() },
                    canAdd = app.fileInfo.path.isNotEmpty() || app.fileInfo.lines > 0,
                )
            }
        }

        if (local.isNotEmpty()) {
            SectionTitle("Scripts de este archivo")
            for (script in local) {
                ScriptCard(app, script, expanded == script.path, { expanded = if (expanded == script.path) null else script.path }) {
                    infoText = managerInfo(app, info, script)
                }
            }
        }
        SectionTitle("Scripts globales")
        if (global.isEmpty()) {
            Text("No hay scripts cargados.", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        for (script in global.sortedWith(compareBy({ !it.custom }, { it.name.lowercase() }))) {
            ScriptCard(app, script, expanded == script.path, { expanded = if (expanded == script.path) null else script.path }) {
                infoText = managerInfo(app, info, script)
            }
        }
    }

    infoText?.let { text ->
        AlertDialog(
            onDismissRequest = { infoText = null },
            title = { Text("Información del script de automatización") },
            text = {
                Text(
                    text,
                    style = MaterialTheme.typography.bodySmall,
                    modifier = Modifier.verticalScroll(rememberScrollState()),
                )
            },
            confirmButton = { TextButton(onClick = { infoText = null }) { Text("Aceptar") } },
        )
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ManagerButtons(onImport: () -> Unit, onAdd: () -> Unit, onRescan: () -> Unit, canAdd: Boolean) {
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Button(onClick = onImport) { Text("Importar a mi carpeta") }
        FilledTonalButton(onClick = onAdd, enabled = canAdd) { Text("Agregar al archivo") }
        OutlinedButton(onClick = onRescan) { Text("Volver a escanear") }
    }
}

@Composable
private fun FolderPath(path: String) {
    Surface(color = MaterialTheme.colorScheme.surfaceVariant, shape = RoundedCornerShape(6.dp), modifier = Modifier.fillMaxWidth()) {
        Text(
            path,
            style = MaterialTheme.typography.labelSmall.copy(fontFamily = FontFamily.Monospace),
            color = TypeMotionTheme.colors.assTagHighlight,
            modifier = Modifier.padding(8.dp),
        )
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ScriptCard(app: AppState, script: AutomationScript, expanded: Boolean, onToggle: () -> Unit, onInfo: () -> Unit) {
    val (state, color) = when {
        !script.loaded -> "Error al cargar" to TypeMotionTheme.colors.statusError
        script.warnings.isNotEmpty() -> "Con advertencias" to Color(0xFFE0A030)
        else -> "Cargado" to TypeMotionTheme.colors.statusSuccess
    }
    Panel(Modifier.fillMaxWidth().animateContentSize()) {
        Column(Modifier.clickable(onClick = onToggle).padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(script.name.ifEmpty { script.file }, style = MaterialTheme.typography.titleSmall)
                    Text(script.file, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.secondary)
                }
                Text(state, style = MaterialTheme.typography.labelSmall, color = color)
            }
            if (script.description.isNotEmpty()) {
                Text(script.description, style = MaterialTheme.typography.bodySmall, maxLines = if (expanded) Int.MAX_VALUE else 2)
            }
            if (expanded) {
                if (script.author.isNotEmpty()) Text("Autor: ${script.author}", style = MaterialTheme.typography.bodySmall)
                if (script.version.isNotEmpty()) Text("Versión: ${script.version}", style = MaterialTheme.typography.bodySmall)
                Text(script.path, style = MaterialTheme.typography.labelSmall.copy(fontFamily = FontFamily.Monospace))
                for (warning in script.warnings) {
                    Text(warning, style = MaterialTheme.typography.bodySmall, color = TypeMotionTheme.colors.statusError)
                }
                if (script.macros.isNotEmpty()) {
                    Text("Macros:", style = MaterialTheme.typography.labelMedium)
                    for (macro in script.macros) Text("• ${macro.name}", style = MaterialTheme.typography.bodySmall)
                }
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { app.reloadScript(script.path) }) { Text("Recargar") }
                    OutlinedButton(onClick = onInfo) { Text("Información") }
                    // Like the desktop manager, only the open file's scripts can be taken out
                    if (script.local) OutlinedButton(onClick = { app.removeScript(script.path) }) { Text("Quitar") }
                }
            }
        }
    }
}

/** The desktop manager's Show Info text */
private fun managerInfo(app: AppState, info: JSONObject, script: AutomationScript?): String {
    val global = app.scripts.count { !it.local }
    val local = app.scripts.count { it.local }
    val lines = mutableListOf(
        "Scripts cargados en total: ${global + local}\nScripts globales cargados: $global\nScripts locales cargados: $local\n",
        "Motores de scripts instalados:",
    )
    for (engine in info.objects("engines")) lines.add("- ${engine.str("name")} (${engine.str("pattern")})")
    if (script != null) {
        val state = when {
            !script.loaded -> "Error al cargar"
            script.warnings.isNotEmpty() -> "Cargado con advertencias"
            else -> "Cargado correctamente"
        }
        lines.add(
            "\nInformación del script:\nNombre: ${script.name}\nDescripción: ${script.description}\n" +
                "Autor: ${script.author}\nVersión: ${script.version}\nRuta completa: ${script.path}\nEstado: $state\n",
        )
        for (warning in script.warnings) lines.add("Advertencia: $warning\n")
        lines.add("Funciones del script:\n")
        for (macro in script.macros) lines.add("    Macro: ${macro.name} (${macro.command})")
    }
    return lines.joinToString("\n")
}
