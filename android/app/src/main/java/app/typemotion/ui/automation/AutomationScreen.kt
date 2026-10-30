package app.typemotion.ui.automation

import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import app.typemotion.engine.EngineError
import app.typemotion.model.AutomationScript
import app.typemotion.model.Macro
import app.typemotion.state.AppState
import app.typemotion.state.Screen
import app.typemotion.ui.common.Panel
import app.typemotion.ui.common.ScreenTopBar
import app.typemotion.ui.theme.TypeMotionTheme
import org.json.JSONObject
import java.text.Collator

private enum class ScriptFilter(val label: String) {
    All("Todas"),
    Official("Oficiales"),
    Mine("Mías"),
    File("Del archivo"),
}

private fun AutomationScript.matches(filter: ScriptFilter) = when (filter) {
    ScriptFilter.All -> true
    ScriptFilter.Official -> !custom
    ScriptFilter.Mine -> custom && !local
    ScriptFilter.File -> local
}

/**
 * The desktop Automation menu: every macro of the loaded scripts, which
 * runs on the selected lines like it does there
 */
@Composable
fun AutomationScreen(app: AppState) {
    var query by remember { mutableStateOf("") }
    var filter by remember { mutableStateOf(ScriptFilter.All) }
    var valid by remember { mutableStateOf(JSONObject()) }

    LaunchedEffect(app.scripts, app.selection) {
        valid = try {
            app.call("validate_macros").optJSONObject("valid") ?: JSONObject()
        } catch (_: EngineError) {
            JSONObject()
        }
    }

    val collator = remember { Collator.getInstance() }
    val groups = remember(app.scripts, query, filter) {
        val q = query.trim()
        app.scripts
            .filter { it.matches(filter) && it.macros.isNotEmpty() }
            .map { script ->
                script to script.macros.filter {
                    q.isEmpty() || it.name.contains(q, ignoreCase = true) || it.help.contains(q, ignoreCase = true) ||
                        script.name.contains(q, ignoreCase = true)
                }.sortedWith { a, b -> collator.compare(a.name, b.name) }
            }
            .filter { it.second.isNotEmpty() }
            .sortedWith { a, b -> collator.compare(a.first.name, b.first.name) }
    }
    val total = app.scripts.sumOf { it.macros.size }

    Scaffold(
        topBar = {
            ScreenTopBar(
                title = "Automatización",
                subtitle = "$total macros • ${app.selection.size} líneas seleccionadas",
                onBack = { app.back() },
                actions = {
                    IconButton(onClick = { app.reloadScripts() }) {
                        Icon(Icons.Default.Refresh, contentDescription = "Recargar scripts")
                    }
                    IconButton(onClick = { app.navigate(Screen.MyAutomations) }) {
                        Icon(Icons.Default.Settings, contentDescription = "Administrador de automatización")
                    }
                },
            )
        },
        containerColor = MaterialTheme.colorScheme.background,
    ) { padding ->
        Column(Modifier.fillMaxSize().padding(padding)) {
            OutlinedTextField(
                query,
                { query = it },
                placeholder = { Text("Buscar macros") },
                leadingIcon = { Icon(Icons.Default.Search, contentDescription = null) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 6.dp),
            )
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp),
            ) {
                for (f in ScriptFilter.entries) {
                    FilterChip(selected = filter == f, onClick = { filter = f }, label = { Text(f.label) })
                }
            }
            if (groups.isEmpty()) {
                Text(
                    if (total == 0) "No hay macros de automatización cargadas." else "Ninguna macro coincide.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(16.dp),
                )
            }
            LazyColumn(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(8.dp),
                contentPadding = androidx.compose.foundation.layout.PaddingValues(12.dp),
            ) {
                items(groups, key = { it.first.path }) { (script, macros) ->
                    ScriptMacros(script, macros, valid) { macro ->
                        // Back to the lines, then run it on them
                        app.back()
                        app.runCommand(macro.command)
                    }
                }
            }
        }
    }
}

@Composable
private fun ScriptMacros(script: AutomationScript, macros: List<Macro>, valid: JSONObject, onRun: (Macro) -> Unit) {
    Panel(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(vertical = 8.dp)) {
            Row(Modifier.padding(horizontal = 12.dp)) {
                Column(Modifier.weight(1f)) {
                    Text(script.name.ifEmpty { script.file }, style = MaterialTheme.typography.titleSmall)
                    val by = listOfNotNull(
                        script.author.takeIf { it.isNotEmpty() },
                        script.version.takeIf { it.isNotEmpty() }?.let { "v$it" },
                        when {
                            script.local -> "del archivo"
                            script.custom -> "tuya"
                            else -> "oficial"
                        },
                    ).joinToString(" • ")
                    Text(by, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.secondary)
                }
            }
            for (macro in macros) {
                val enabled = valid.optBoolean(macro.command, true)
                val alpha = if (enabled) 1f else 0.4f
                Column(
                    Modifier
                        .fillMaxWidth()
                        .clickable(enabled = enabled) { onRun(macro) }
                        .heightIn(min = 48.dp)
                        .padding(horizontal = 12.dp, vertical = 8.dp),
                ) {
                    Text(
                        macro.name,
                        style = MaterialTheme.typography.bodyMedium,
                        color = TypeMotionTheme.colors.assTagHighlight.copy(alpha = alpha),
                    )
                    if (macro.help.isNotEmpty()) {
                        Text(
                            macro.help,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = alpha),
                            maxLines = 2,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
            }
        }
    }
}
