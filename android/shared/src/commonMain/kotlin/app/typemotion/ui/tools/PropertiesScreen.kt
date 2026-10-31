package app.typemotion.ui.tools

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import app.typemotion.engine.bool
import app.typemotion.engine.int
import app.typemotion.engine.str
import app.typemotion.engine.strings
import app.typemotion.state.AppState
import app.typemotion.ui.common.CheckRow
import app.typemotion.ui.common.Dropdown
import app.typemotion.ui.common.Panel
import app.typemotion.ui.common.SectionTitle
import app.typemotion.ui.common.ToolScreen
import app.typemotion.json.JSONObject

/** The [Script Info] fields of the desktop dialog, with its labels */
private val textProperties = listOf(
    "Title" to "Título",
    "Original Script" to "Guion original",
    "Original Translation" to "Traducción",
    "Original Editing" to "Edición",
    "Original Timing" to "Tiempos",
    "Synch Point" to "Punto de sincronía",
    "Script Updated By" to "Actualizado por",
    "Update Details" to "Detalles de la actualización",
)

private val wrapStyles = listOf(
    "0: Ajuste inteligente, la línea de arriba más ancha",
    "1: Ajuste al final de línea, solo \\N corta",
    "2: Sin ajuste, \\n y \\N cortan",
    "3: Ajuste inteligente, la línea de abajo más ancha",
)

/** Script Properties */
@Composable
fun PropertiesScreen(app: AppState) {
    var loaded by remember { mutableStateOf(false) }
    val text = remember { mutableStateMapOf<String, String>() }
    var resX by remember { mutableStateOf("") }
    var resY by remember { mutableStateOf("") }
    var layoutX by remember { mutableStateOf("") }
    var layoutY by remember { mutableStateOf("") }
    var wrapStyle by remember { mutableStateOf(0) }
    var scaledBorder by remember { mutableStateOf(false) }
    var matrix by remember { mutableStateOf("") }
    var matrices by remember { mutableStateOf(emptyList<String>()) }
    var video by remember { mutableStateOf<JSONObject?>(null) }

    LaunchedEffect(Unit) {
        try {
            val state = app.call("properties")
            val info = state.optJSONObject("text") ?: JSONObject()
            for ((key, _) in textProperties) text[key] = info.str(key)
            resX = state.str("res_x")
            resY = state.str("res_y")
            layoutX = state.str("layout_res_x")
            layoutY = state.str("layout_res_y")
            wrapStyle = state.int("wrap_style")
            scaledBorder = state.bool("scaled_border")
            matrix = state.str("matrix")
            matrices = state.strings("matrices")
            video = state.optJSONObject("video")?.takeIf { it.length() > 0 }
            loaded = true
        } catch (e: Exception) {
            app.errorMessage = e.message
        }
    }

    fun digits(value: String) = value.filter { it in '0'..'9' }

    ToolScreen(
        title = "Propiedades del script",
        onBack = { app.back() },
        buttons = {
            TextButton(onClick = { app.back() }) { Text("Cancelar") }
            Button(
                onClick = {
                    app.run {
                        app.call(
                            "set_properties",
                            "text" to JSONObject(text.toMap()),
                            "res_x" to resX, "res_y" to resY,
                            "layout_res_x" to layoutX, "layout_res_y" to layoutY,
                            "wrap_style" to wrapStyle, "scaled_border" to scaledBorder,
                            "matrix" to matrix,
                        )
                        app.back()
                    }
                },
                enabled = loaded,
            ) { Text("Aceptar") }
        },
    ) {
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionTitle("Script")
                for ((key, label) in textProperties) {
                    OutlinedTextField(
                        text[key] ?: "",
                        { text[key] = it },
                        label = { Text(label) },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionTitle("Resolución")
                ResolutionRow("Script", resX, resY, video != null, { resX = digits(it) }, { resY = digits(it) }) {
                    resX = video!!.int("width").toString()
                    resY = video!!.int("height").toString()
                }
                ResolutionRow("Diseño", layoutX, layoutY, video != null, { layoutX = digits(it) }, { layoutY = digits(it) }) {
                    layoutX = video!!.int("width").toString()
                    layoutY = video!!.int("height").toString()
                }
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    // The script's own value stays in the list even when it isn't a valid one
                    val options = if (matrix in matrices) matrices else listOf(matrix) + matrices
                    Dropdown("Matriz YCbCr", options, matrix, Modifier.weight(1f)) { _, m -> matrix = m }
                    TextButton(
                        onClick = { matrix = video!!.str("matrix") },
                        enabled = video != null && video!!.str("matrix") in matrices,
                    ) { Text("Del video") }
                }
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionTitle("Opciones")
                Dropdown("Estilo de ajuste", wrapStyles, wrapStyles.getOrElse(wrapStyle) { "" }, Modifier.fillMaxWidth()) { i, _ ->
                    wrapStyle = i
                }
                CheckRow("Escalar borde y sombra", scaledBorder) { scaledBorder = it }
            }
        }
    }
}

@Composable
private fun ResolutionRow(
    label: String,
    x: String,
    y: String,
    hasVideo: Boolean,
    onX: (String) -> Unit,
    onY: (String) -> Unit,
    fromVideo: () -> Unit,
) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        OutlinedTextField(
            x, onX,
            label = { Text("$label: ancho") },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.weight(1f),
        )
        Text("×")
        OutlinedTextField(
            y, onY,
            label = { Text("alto") },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.weight(1f),
        )
        TextButton(onClick = fromVideo, enabled = hasVideo) { Text("Del video") }
    }
}
