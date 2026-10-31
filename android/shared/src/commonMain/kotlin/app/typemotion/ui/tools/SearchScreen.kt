package app.typemotion.ui.tools

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import app.typemotion.engine.bool
import app.typemotion.engine.int
import app.typemotion.engine.strings
import app.typemotion.model.AssTime
import app.typemotion.state.AppState
import app.typemotion.ui.common.CheckRow
import app.typemotion.ui.common.ComboField
import app.typemotion.ui.common.Panel
import app.typemotion.ui.common.RadioGroup
import app.typemotion.ui.common.SectionTitle
import app.typemotion.ui.common.ToolScreen

/**
 * Find and Replace. The desktop dialog stays open over the grid; here the
 * line it finds is shown under the options, with what matched marked.
 */
@Composable
fun SearchScreen(app: AppState, replace: Boolean) {
    var loaded by remember { mutableStateOf(false) }
    var find by remember { mutableStateOf("") }
    var replaceWith by remember { mutableStateOf("") }
    var findHistory by remember { mutableStateOf(emptyList<String>()) }
    var replaceHistory by remember { mutableStateOf(emptyList<String>()) }
    var field by remember { mutableStateOf(0) }
    var limit by remember { mutableStateOf(0) }
    var matchCase by remember { mutableStateOf(false) }
    var regex by remember { mutableStateOf(false) }
    var skipComments by remember { mutableStateOf(false) }
    var skipTags by remember { mutableStateOf(false) }
    var searched by remember { mutableStateOf(false) }

    suspend fun loadState(first: Boolean) {
        val state = app.call("search_state")
        findHistory = state.strings("find_history")
        replaceHistory = state.strings("replace_history")
        if (first) {
            find = findHistory.firstOrNull() ?: ""
            replaceWith = replaceHistory.firstOrNull() ?: ""
            field = state.int("field")
            limit = state.int("limit")
            matchCase = state.bool("match_case")
            regex = state.bool("regex")
            skipComments = state.bool("skip_comments")
            skipTags = state.bool("skip_tags")
        }
    }

    LaunchedEffect(Unit) {
        try {
            loadState(first = true)
            loaded = true
        } catch (e: Exception) {
            app.errorMessage = e.message
        }
    }

    fun search(action: String) = app.run {
        val args = mutableListOf<Pair<String, Any?>>(
            "find" to find, "field" to field, "limit" to limit, "match_case" to matchCase,
            "regex" to regex, "skip_comments" to skipComments, "skip_tags" to skipTags, "action" to action,
        )
        if (replace) args.add("replace" to replaceWith)
        val result = app.call("search", *args.toTypedArray())
        searched = true
        if (!result.bool("found") && action == "find_next") app.showMessage("No se encontró nada")
        loadState(first = false)
    }

    ToolScreen(
        title = if (replace) "Reemplazar" else "Buscar",
        onBack = { app.back() },
        buttons = {
            TextButton(onClick = { app.showEditor() }) { Text("Ver en el editor") }
            if (replace) {
                OutlinedButton(onClick = { search("replace_all") }, enabled = loaded && find.isNotEmpty()) { Text("Todo") }
                FilledTonalButton(onClick = { search("replace_next") }, enabled = loaded && find.isNotEmpty()) { Text("Reemplazar") }
            }
            Button(onClick = { search("find_next") }, enabled = loaded && find.isNotEmpty()) { Text("Siguiente") }
        },
    ) {
        ComboField("Buscar", find, findHistory, Modifier.fillMaxWidth(), onType = { find = it }, onPick = { find = it })
        if (replace) {
            ComboField(
                "Reemplazar con",
                replaceWith,
                replaceHistory,
                Modifier.fillMaxWidth(),
                onType = { replaceWith = it },
                onPick = { replaceWith = it },
            )
        }

        if (searched) FoundLine(app)

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                CheckRow("Distinguir mayúsculas", matchCase) { matchCase = it }
                CheckRow("Usar expresiones regulares", regex) { regex = it }
                CheckRow("Saltar comentarios", skipComments) { skipComments = it }
                CheckRow("Saltar etiquetas de formato", skipTags) { skipTags = it }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Panel(Modifier.weight(1f)) {
                Column(Modifier.padding(12.dp)) {
                    SectionTitle("En el campo")
                    RadioGroup(listOf("Texto", "Estilo", "Actor", "Efecto"), field) { field = it }
                }
            }
            Panel(Modifier.weight(1f)) {
                Column(Modifier.padding(12.dp)) {
                    SectionTitle("Limitar a")
                    RadioGroup(listOf("Todas las filas", "Filas seleccionadas"), limit) { limit = it }
                }
            }
        }
    }
}

/** The active line, where the search stopped, with the match marked */
@Composable
private fun FoundLine(app: AppState) {
    val line = app.lines.getOrNull(app.activeRow) ?: return
    val value = app.editBox.value
    val highlight = SpanStyle(background = MaterialTheme.colorScheme.primary, color = MaterialTheme.colorScheme.onPrimary)
    val text = buildAnnotatedString {
        val sel = value.selection
        if (!sel.collapsed && sel.max <= value.text.length) {
            append(value.text.substring(0, sel.min))
            withStyle(highlight) { append(value.text.substring(sel.min, sel.max)) }
            append(value.text.substring(sel.max))
        } else append(line.text)
    }
    Panel(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(
                "Línea ${line.row + 1} • ${AssTime.format(line.start)} → ${AssTime.format(line.end)} • ${line.style}",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.secondary,
            )
            Text(text, style = MaterialTheme.typography.bodyMedium)
        }
    }
}
