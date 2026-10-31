package app.typemotion.ui.tools

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import app.typemotion.engine.bool
import app.typemotion.engine.int
import app.typemotion.engine.str
import app.typemotion.state.AppState
import app.typemotion.ui.common.CheckRow
import app.typemotion.ui.common.Panel
import app.typemotion.ui.common.RadioGroup
import app.typemotion.ui.common.SectionTitle
import app.typemotion.ui.common.ToolScreen

/** Select Lines, with the desktop dialog's options */
@Composable
fun SelectLinesScreen(app: AppState) {
    var loaded by remember { mutableStateOf(false) }
    var text by remember { mutableStateOf("") }
    var invert by remember { mutableStateOf(false) }
    var matchCase by remember { mutableStateOf(false) }
    var mode by remember { mutableStateOf(1) }
    var field by remember { mutableStateOf(0) }
    var dialogue by remember { mutableStateOf(true) }
    var comments by remember { mutableStateOf(false) }
    var action by remember { mutableStateOf(0) }

    LaunchedEffect(Unit) {
        try {
            val state = app.call("select_lines_state")
            text = state.str("text")
            invert = state.bool("invert")
            matchCase = state.bool("match_case")
            mode = state.int("mode", 1)
            field = state.int("field")
            dialogue = state.bool("dialogue", true)
            comments = state.bool("comments")
            action = state.int("action")
            loaded = true
        } catch (e: Exception) {
            app.errorMessage = e.message
        }
    }

    /** Select the lines; OK then closes, as on the desktop, even when nothing matched */
    fun apply(close: Boolean) = app.run {
        app.call(
            "select_lines",
            "text" to text, "invert" to invert, "match_case" to matchCase, "mode" to mode,
            "field" to field, "dialogue" to dialogue, "comments" to comments, "action" to action,
        )
        if (close) app.back()
    }

    ToolScreen(
        title = "Seleccionar",
        onBack = { app.back() },
        buttons = {
            TextButton(onClick = { app.back() }) { Text("Cancelar") }
            OutlinedButton(onClick = { apply(close = false) }, enabled = loaded) { Text("Aplicar") }
            Button(onClick = { apply(close = true) }, enabled = loaded) { Text("Aceptar") }
        },
    ) {
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("Coincidir")
                OutlinedTextField(text, { text = it }, singleLine = true, modifier = Modifier.fillMaxWidth())
                RadioGroup(listOf("Coincide", "No coincide"), if (invert) 1 else 0) { invert = it == 1 }
                CheckRow("Distinguir mayúsculas", matchCase) { matchCase = it }
            }
        }
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("Modo")
                RadioGroup(listOf("Coincidencia exacta", "Contiene", "Expresión regular"), mode) { mode = it }
            }
        }
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("En el campo")
                RadioGroup(listOf("Texto", "Estilo", "Actor", "Efecto"), field) { field = it }
            }
        }
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("Diálogos y comentarios")
                Row {
                    // One of them is always checked, as in the desktop dialog
                    CheckRow("Diálogos", dialogue, Modifier.weight(1f)) {
                        dialogue = it
                        if (!it) comments = true
                    }
                    CheckRow("Comentarios", comments, Modifier.weight(1f)) {
                        comments = it
                        if (!it) dialogue = true
                    }
                }
            }
        }
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("Acción")
                RadioGroup(
                    listOf("Establecer selección", "Añadir a la selección", "Quitar de la selección", "Intersecar con la selección"),
                    action,
                ) { action = it }
            }
        }
    }
}
