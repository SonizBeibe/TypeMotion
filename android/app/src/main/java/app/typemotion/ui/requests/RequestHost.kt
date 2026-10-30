package app.typemotion.ui.requests

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Error
import androidx.compose.material.icons.filled.Help
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import app.typemotion.engine.EngineError
import app.typemotion.engine.EngineRequest
import app.typemotion.engine.bool
import app.typemotion.engine.int
import app.typemotion.engine.jsonOf
import app.typemotion.engine.objects
import app.typemotion.engine.str
import app.typemotion.engine.strings
import app.typemotion.model.AssColor
import app.typemotion.state.AppState
import app.typemotion.ui.automation.LuaDialog
import app.typemotion.ui.common.CheckRow
import app.typemotion.ui.common.ChoiceDialog
import app.typemotion.ui.common.ColorPickerDialog
import app.typemotion.ui.common.Dropdown
import app.typemotion.ui.common.NumberField
import app.typemotion.ui.common.RadioGroup
import app.typemotion.ui.common.SectionTitle
import java.text.DateFormat
import java.util.Date

/** Shows the first question the engine is waiting for an answer to */
@Composable
fun RequestHost(app: AppState) {
    val request = app.requests.firstOrNull() ?: return
    key(request.id) {
        when (request.kind) {
            "message" -> MessageDialog(app, request)
            "confirm_scripts" -> ConfirmScriptsDialog(app, request)
            "choice" -> {
                val data = request.data
                ChoiceDialog(
                    title = data.str("caption"),
                    message = data.str("message"),
                    choices = data.strings("choices"),
                    onDismiss = { app.answer(request, jsonOf("index" to -1)) },
                    onPick = { app.answer(request, jsonOf("index" to it)) },
                )
            }
            "color" -> {
                val data = request.data
                ColorPickerDialog(
                    initial = AssColor(data.int("r"), data.int("g"), data.int("b"), data.int("a")),
                    showAlpha = data.bool("show_alpha"),
                    onDismiss = { app.answer(request, jsonOf("ok" to false)) },
                    onPick = { c ->
                        app.answer(request, jsonOf("ok" to true, "r" to c.r, "g" to c.g, "b" to c.b, "a" to c.a))
                    },
                )
            }
            "font" -> FontDialog(app, request)
            "paste_over" -> PasteOverDialog(app, request)
            "pick_autosave" -> AutosaveDialog(app, request)
            "plain_text_import" -> PlainTextImportDialog(app, request)
            "resample" -> ResampleDialog(app, request)
            "dialog" -> LuaDialog(app, request)
            // Something this version of the app doesn't know: answer nothing,
            // which the engine takes as cancelling
            else -> app.answer(request)
        }
    }
}

@Composable
private fun MessageDialog(app: AppState, request: EngineRequest) {
    val data = request.data
    fun answer(button: String) = app.answer(request, jsonOf("button" to button))
    val icon = when (data.str("icon")) {
        "error" -> Icons.Default.Error
        "warning" -> Icons.Default.Warning
        "question" -> Icons.Default.Help
        else -> Icons.Default.Info
    }
    AlertDialog(
        onDismissRequest = { app.answer(request) },
        icon = { Icon(icon, contentDescription = null) },
        title = { if (data.str("caption").isNotEmpty()) Text(data.str("caption")) },
        text = {
            Text(data.str("message"), modifier = Modifier.verticalScroll(rememberScrollState()))
        },
        confirmButton = {
            Row {
                if (data.bool("yes")) TextButton(onClick = { answer("yes") }) { Text("Sí") }
                if (data.bool("ok")) TextButton(onClick = { answer("ok") }) { Text("Aceptar") }
            }
        },
        dismissButton = {
            Row {
                if (data.bool("no")) TextButton(onClick = { answer("no") }) { Text("No") }
                if (data.bool("cancel")) TextButton(onClick = { answer("cancel") }) { Text("Cancelar") }
            }
        },
    )
}

@Composable
private fun ConfirmScriptsDialog(app: AppState, request: EngineRequest) {
    val data = request.data
    AlertDialog(
        onDismissRequest = { app.answer(request, jsonOf("yes" to false)) },
        icon = { Icon(Icons.Default.Warning, contentDescription = null) },
        title = { Text(data.str("caption")) },
        text = { Text(data.str("message"), modifier = Modifier.verticalScroll(rememberScrollState())) },
        confirmButton = { TextButton(onClick = { app.answer(request, jsonOf("yes" to true)) }) { Text("Sí") } },
        dismissButton = { TextButton(onClick = { app.answer(request, jsonOf("yes" to false)) }) { Text("No") } },
    )
}

/** Paste Lines Over: which fields to paste, with the desktop dialog's shortcuts */
@Composable
private fun PasteOverDialog(app: AppState, request: EngineRequest) {
    val fields = request.data.objects("fields")
    val checked = remember { mutableStateListOf(*fields.map { it.bool("checked") }.toTypedArray()) }
    AlertDialog(
        onDismissRequest = { app.answer(request, jsonOf("ok" to false)) },
        title = { Text("Pegar sobre: campos") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                Text("Elige qué campos pegar sobre las líneas seleccionadas", style = MaterialTheme.typography.bodySmall)
                fields.forEachIndexed { i, field ->
                    CheckRow(field.str("name"), checked[i]) { checked[i] = it }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    // All, None, Times and Text, as the desktop buttons
                    TextButton(onClick = { for (i in checked.indices) checked[i] = true }) { Text("Todos") }
                    TextButton(onClick = { for (i in checked.indices) checked[i] = false }) { Text("Ninguno") }
                    TextButton(onClick = { for (i in checked.indices) checked[i] = i in 1..2 }) { Text("Tiempos") }
                    TextButton(onClick = { for (i in checked.indices) checked[i] = i == 9 }) { Text("Texto") }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = { app.answer(request, jsonOf("ok" to true, "checked" to checked.toList())) }) { Text("Aceptar") }
        },
        dismissButton = { TextButton(onClick = { app.answer(request, jsonOf("ok" to false)) }) { Text("Cancelar") } },
    )
}

/** Open Autosaved Subtitles: a file, then one of its versions */
@Composable
private fun AutosaveDialog(app: AppState, request: EngineRequest) {
    val files = request.data.objects("files")
    var fileIndex by remember { mutableStateOf(if (files.isEmpty()) -1 else 0) }
    var path by remember { mutableStateOf(files.firstOrNull()?.objects("versions")?.firstOrNull()?.str("path") ?: "") }
    val format = remember { DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.MEDIUM) }
    AlertDialog(
        onDismissRequest = { app.answer(request) },
        title = { Text("Abrir subtítulos autoguardados") },
        text = {
            if (files.isEmpty()) Text("No hay archivos autoguardados.")
            else Column {
                Text("Archivo", style = MaterialTheme.typography.titleSmall)
                LazyColumn(Modifier.heightIn(max = 180.dp)) {
                    items(files.size) { i ->
                        Text(
                            files[i].str("name"),
                            color = if (i == fileIndex) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable {
                                    fileIndex = i
                                    path = files[i].objects("versions").firstOrNull()?.str("path") ?: ""
                                }
                                .padding(vertical = 10.dp),
                        )
                    }
                }
                HorizontalDivider()
                Text("Versión", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(top = 8.dp))
                val versions = if (fileIndex >= 0) files[fileIndex].objects("versions") else emptyList()
                LazyColumn(Modifier.heightIn(max = 180.dp)) {
                    items(versions) { version ->
                        Text(
                            version.str("display").ifEmpty { format.format(Date(version.optLong("time") * 1000)) },
                            color = if (version.str("path") == path) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { path = version.str("path") }
                                .padding(vertical = 10.dp),
                        )
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = { app.answer(request, jsonOf("path" to path)) }, enabled = path.isNotEmpty()) { Text("Abrir") }
        },
        dismissButton = { TextButton(onClick = { app.answer(request) }) { Text("Cancelar") } },
    )
}

/** The options for importing a plain text file */
@Composable
private fun PlainTextImportDialog(app: AppState, request: EngineRequest) {
    val data = request.data
    var actor by remember { mutableStateOf(data.str("actor_separator")) }
    var comment by remember { mutableStateOf(data.str("comment_starter")) }
    var blank by remember { mutableStateOf(data.bool("include_blank")) }
    AlertDialog(
        onDismissRequest = { app.answer(request, jsonOf("ok" to false)) },
        title = { Text("Importar texto plano") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(actor, { actor = it }, label = { Text("Separador de actor") }, singleLine = true)
                OutlinedTextField(comment, { comment = it }, label = { Text("Inicio de comentario") }, singleLine = true)
                CheckRow("Incluir líneas en blanco", blank) { blank = it }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                app.answer(request, jsonOf("ok" to true, "actor_separator" to actor, "comment_starter" to comment, "include_blank" to blank))
            }) { Text("Aceptar") }
        },
        dismissButton = { TextButton(onClick = { app.answer(request, jsonOf("ok" to false)) }) { Text("Cancelar") } },
    )
}

/** The font dialog of \fn and the style editor: a family from the fonts the engine knows */
@Composable
private fun FontDialog(app: AppState, request: EngineRequest) {
    val data = request.data
    var face by remember { mutableStateOf(data.str("face")) }
    var size by remember { mutableStateOf(data.int("size", 20)) }
    var bold by remember { mutableStateOf(data.bool("bold")) }
    var italic by remember { mutableStateOf(data.bool("italic")) }
    var underline by remember { mutableStateOf(data.bool("underline")) }
    var families by remember { mutableStateOf(emptyList<String>()) }
    LaunchedEffect(Unit) {
        families = try {
            app.call("fonts").strings("families")
        } catch (_: EngineError) {
            emptyList()
        }
    }
    val matching = remember(families, face) {
        val query = face.trim()
        if (query.isEmpty()) families else families.filter { it.contains(query, ignoreCase = true) }
    }
    AlertDialog(
        onDismissRequest = { app.answer(request, jsonOf("ok" to false)) },
        title = { Text("Fuente") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                OutlinedTextField(face, { face = it }, label = { Text("Familia") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                LazyColumn(Modifier.fillMaxWidth().heightIn(max = 220.dp)) {
                    items(matching) { family ->
                        Text(
                            family,
                            color = if (family.equals(face, ignoreCase = true)) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { face = family }
                                .padding(vertical = 10.dp, horizontal = 4.dp),
                        )
                    }
                }
                NumberField("Tamaño", size, Modifier.fillMaxWidth()) { size = it.toInt() }
                Row {
                    CheckRow("Negrita", bold, Modifier.weight(1f)) { bold = it }
                    CheckRow("Cursiva", italic, Modifier.weight(1f)) { italic = it }
                }
                CheckRow("Subrayado", underline) { underline = it }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    app.answer(request, jsonOf(
                        "ok" to true, "face" to face.trim(), "size" to size,
                        "bold" to bold, "italic" to italic, "underline" to underline,
                    ))
                },
                enabled = face.isNotBlank() && size > 0,
            ) { Text("Aceptar") }
        },
        dismissButton = { TextButton(onClick = { app.answer(request, jsonOf("ok" to false)) }) { Text("Cancelar") } },
    )
}

/** Resample Resolution, with the desktop dialog's rules for what can be changed */
@Composable
private fun ResampleDialog(app: AppState, request: EngineRequest) {
    val data = request.data
    val scriptX = data.int("script_x")
    val scriptY = data.int("script_y")
    val hasVideo = data.bool("has_video")
    val videoX = data.int("video_x")
    val videoY = data.int("video_y")
    val matrices = data.strings("matrices")
    val arModes = data.strings("ar_modes")

    var sourceX by remember { mutableStateOf(data.int("source_x", 1)) }
    var sourceY by remember { mutableStateOf(data.int("source_y", 1)) }
    var destX by remember { mutableStateOf(data.int("dest_x", 1)) }
    var destY by remember { mutableStateOf(data.int("dest_y", 1)) }
    var sourceMatrix by remember { mutableStateOf(data.str("source_matrix")) }
    var destMatrix by remember { mutableStateOf(data.str("dest_matrix")) }
    var arMode by remember { mutableStateOf(data.int("ar_mode")) }
    var symmetrical by remember { mutableStateOf(true) }
    // Left, right, top and bottom
    val margins = remember { mutableStateListOf(0, 0, 0, 0) }

    val sourceAr = sourceX.toDouble() / sourceY.coerceAtLeast(1)
    val destAr = destX.toDouble() / destY.coerceAtLeast(1)
    val arChanged = Math.abs(sourceAr - destAr) / destAr > .01
    val manualMargins = arChanged && arMode == 3

    fun resolution(value: Double) = value.toInt().coerceIn(1, 999999)
    fun margin(value: Double) = value.toInt().coerceIn(-9999, 9999)
    fun matrixLabel(m: String) = m.ifEmpty { "(no convertir)" }

    AlertDialog(
        onDismissRequest = { app.answer(request, jsonOf("ok" to false)) },
        title = { Text("Remuestrear resolución") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionTitle("Resolución de origen")
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    NumberField("Ancho", sourceX, Modifier.weight(1f)) { sourceX = resolution(it) }
                    Text("×")
                    NumberField("Alto", sourceY, Modifier.weight(1f)) { sourceY = resolution(it) }
                }
                TextButton(
                    onClick = {
                        sourceX = scriptX
                        sourceY = scriptY
                        sourceMatrix = data.str("script_matrix")
                    },
                    enabled = sourceX != scriptX || sourceY != scriptY,
                ) { Text("Del script") }
                Dropdown("Matriz YCbCr", matrices.map(::matrixLabel), matrixLabel(sourceMatrix), Modifier.fillMaxWidth()) { i, _ ->
                    sourceMatrix = matrices[i]
                }

                SectionTitle("Resolución de destino")
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    NumberField("Ancho", destX, Modifier.weight(1f)) { destX = resolution(it) }
                    Text("×")
                    NumberField("Alto", destY, Modifier.weight(1f)) { destY = resolution(it) }
                }
                TextButton(
                    onClick = {
                        destX = videoX
                        destY = videoY
                        destMatrix = data.str("video_matrix")
                    },
                    enabled = hasVideo && (destX != videoX || destY != videoY),
                ) { Text("Del video") }
                Dropdown("Matriz YCbCr", matrices.map(::matrixLabel), matrixLabel(destMatrix), Modifier.fillMaxWidth()) { i, _ ->
                    destMatrix = matrices[i]
                }

                SectionTitle("Relación de aspecto")
                RadioGroup(arModes, arMode, enabled = arChanged) { arMode = it }

                SectionTitle("Desplazamiento de márgenes")
                CheckRow("Simétrico", symmetrical, enabled = manualMargins) { checked ->
                    symmetrical = checked
                    if (checked) {
                        margins[1] = margins[0]
                        margins[3] = margins[2]
                    }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    NumberField("Izquierda", margins[0], Modifier.weight(1f), signed = true, enabled = manualMargins) {
                        margins[0] = margin(it)
                        if (symmetrical) margins[1] = margins[0]
                    }
                    NumberField("Derecha", margins[1], Modifier.weight(1f), signed = true, enabled = manualMargins && !symmetrical) {
                        margins[1] = margin(it)
                    }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    NumberField("Arriba", margins[2], Modifier.weight(1f), signed = true, enabled = manualMargins) {
                        margins[2] = margin(it)
                        if (symmetrical) margins[3] = margins[2]
                    }
                    NumberField("Abajo", margins[3], Modifier.weight(1f), signed = true, enabled = manualMargins && !symmetrical) {
                        margins[3] = margin(it)
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                app.answer(request, jsonOf(
                    "ok" to true,
                    "source_x" to sourceX, "source_y" to sourceY,
                    "dest_x" to destX, "dest_y" to destY,
                    "source_matrix" to sourceMatrix, "dest_matrix" to destMatrix,
                    "ar_mode" to arMode,
                    "margin_left" to margins[0], "margin_right" to margins[1],
                    "margin_top" to margins[2], "margin_bottom" to margins[3],
                ))
            }) { Text("Aceptar") }
        },
        dismissButton = { TextButton(onClick = { app.answer(request, jsonOf("ok" to false)) }) { Text("Cancelar") } },
    )
}
