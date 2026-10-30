package app.typemotion.ui.automation

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import app.typemotion.engine.EngineRequest
import app.typemotion.engine.bool
import app.typemotion.engine.double
import app.typemotion.engine.int
import app.typemotion.engine.jsonOf
import app.typemotion.engine.objects
import app.typemotion.engine.str
import app.typemotion.engine.strings
import app.typemotion.model.AssColor
import app.typemotion.state.AppState
import app.typemotion.ui.common.CheckRow
import app.typemotion.ui.common.ColorField
import app.typemotion.ui.common.Dropdown
import app.typemotion.ui.common.NumberField
import org.json.JSONObject

/** A control of a script's dialog, as the engine describes it */
private class LuaControl(val json: JSONObject) {
    val kind = json.str("class")
    val name = json.str("name")
    val hint = json.str("hint")
    val x = json.int("x")
    val y = json.int("y")
    val width = json.int("width", 1).coerceAtLeast(1)
    val height = json.int("height", 1).coerceAtLeast(1)

    /** The value a new dialog starts with */
    fun initialValue(): Any? = when (kind) {
        "edit", "textbox", "dropdown" -> json.str("value")
        "intedit" -> json.int("value")
        "floatedit" -> json.double("value")
        "checkbox" -> json.bool("value")
        "color", "coloralpha" -> AssColor.parse(json.str("value")) ?: AssColor(0, 0, 0)
        else -> null
    }

    /** The value as the engine reads it back */
    fun toAnswer(value: Any?): Any? = when (kind) {
        "intedit" -> (value as Int).coerceIn(json.int("min", Int.MIN_VALUE), json.int("max", Int.MAX_VALUE))
        "floatedit" -> (value as Double).coerceIn(json.double("min", -Double.MAX_VALUE), json.double("max", Double.MAX_VALUE))
        "color", "coloralpha" -> (value as AssColor).toAss()
        else -> value
    }
}

private val defaultButtonLabels = mapOf(
    "ok" to "Aceptar", "yes" to "Sí", "save" to "Guardar", "apply" to "Aplicar", "close" to "Cerrar",
    "no" to "No", "cancel" to "Cancelar", "help" to "Ayuda", "context_help" to "Ayuda",
)

/**
 * A dialog of an automation script. The desktop program lays the controls
 * out in a grid; here each row of the grid is kept, split in pieces when
 * the screen is too narrow for all of it.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun LuaDialog(app: AppState, request: EngineRequest) {
    val data = request.data
    val controls = remember { data.objects("controls").map(::LuaControl) }
    val values = remember {
        mutableStateMapOf<String, Any>().apply {
            for (control in controls) {
                val value = control.initialValue()
                if (control.name.isNotEmpty() && value != null) put(control.name, value)
            }
        }
    }
    val buttons = remember { data.objects("buttons") }
    val rows = remember { controls.groupBy { it.y }.toSortedMap().values.map { row -> cells(row.sortedBy { it.x }) } }

    fun answer(button: Int) {
        val answered = JSONObject()
        for (control in controls) {
            if (control.name.isEmpty() || control.name !in values) continue
            answered.put(control.name, control.toAnswer(values[control.name]))
        }
        app.answer(request, jsonOf("button" to button, "values" to answered))
    }

    // Closing it is the desktop dialog's Escape: the engine picks the button
    Dialog(onDismissRequest = { answer(-1) }, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(
            shape = RoundedCornerShape(16.dp),
            color = MaterialTheme.colorScheme.surface,
            modifier = Modifier
                .padding(12.dp)
                .widthIn(max = 900.dp)
                .imePadding(),
        ) {
            Column {
                Text(
                    data.str("title").ifEmpty { "Automatización" },
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 16.dp, bottom = 8.dp),
                )
                HorizontalDivider()
                BoxWithConstraints(Modifier.weight(1f, fill = false)) {
                    val slots = if (maxWidth >= 600.dp) 4 else 2
                    Column(
                        modifier = Modifier
                            .verticalScroll(rememberScrollState())
                            .padding(horizontal = 16.dp, vertical = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        for (row in rows) {
                            for (piece in pieces(row, slots)) {
                                Row(
                                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                    modifier = Modifier.fillMaxWidth(),
                                ) {
                                    for (cell in piece) {
                                        Row(
                                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                                            verticalAlignment = Alignment.CenterVertically,
                                            modifier = Modifier.weight(cell.sumOf { it.width }.toFloat()),
                                        ) {
                                            for (control in cell) {
                                                LuaControlView(control, values, Modifier.weight(control.width.toFloat()))
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                HorizontalDivider()
                FlowRow(
                    horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.End),
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 8.dp),
                ) {
                    if (buttons.isEmpty()) {
                        Button(onClick = { answer(-1) }) { Text("Aceptar") }
                    }
                    buttons.forEachIndexed { index, button ->
                        val label = button.str("label").replace("&", "")
                            .ifEmpty { defaultButtonLabels[button.str("id")] ?: "Aceptar" }
                        if (button.bool("default")) Button(onClick = { answer(index) }) { Text(label) }
                        else TextButton(onClick = { answer(index) }) { Text(label) }
                    }
                }
            }
        }
    }
}

/**
 * A row's controls in cells which stay together when the row is split: a
 * label and the control after it, which is the label's field
 */
private fun cells(row: List<LuaControl>): List<List<LuaControl>> {
    val cells = mutableListOf<List<LuaControl>>()
    var i = 0
    while (i < row.size) {
        val next = row.getOrNull(i + 1)
        if (row[i].kind == "label" && next != null && next.kind != "label") {
            cells += listOf(row[i], next)
            i += 2
        } else {
            cells += listOf(row[i])
            i++
        }
    }
    return cells
}

/** A row's cells split in pieces of at most [slots] controls, as the screen fits */
private fun pieces(cells: List<List<LuaControl>>, slots: Int): List<List<List<LuaControl>>> {
    val pieces = mutableListOf<MutableList<List<LuaControl>>>()
    var used = 0
    for (cell in cells) {
        if (pieces.isEmpty() || used + cell.size > slots) {
            pieces += mutableListOf<List<LuaControl>>()
            used = 0
        }
        pieces.last() += cell
        used += cell.size
    }
    return pieces
}

/** Labels which only draw lines or boxes, which are kept on one line */
private fun isDecoration(text: String) = text.isNotBlank() && text.none { it.isLetterOrDigit() }

@Composable
private fun LuaControlView(control: LuaControl, values: MutableMap<String, Any>, modifier: Modifier) {
    val name = control.name
    val hint: @Composable (() -> Unit)? =
        if (control.hint.isNotEmpty()) ({ Text(control.hint, maxLines = 2) }) else null
    when (control.kind) {
        "label" -> {
            val text = control.json.str("label")
            if (isDecoration(text)) {
                Text(text, style = MaterialTheme.typography.bodyMedium, maxLines = 1, softWrap = false, overflow = TextOverflow.Clip, modifier = modifier)
            } else {
                Text(text, style = MaterialTheme.typography.bodyMedium, modifier = modifier)
            }
        }
        "edit" -> OutlinedTextField(
            value = values[name] as? String ?: "",
            onValueChange = { values[name] = it },
            singleLine = true,
            supportingText = hint,
            modifier = modifier,
        )
        "textbox" -> OutlinedTextField(
            value = values[name] as? String ?: "",
            onValueChange = { values[name] = it },
            minLines = control.height.coerceIn(2, 8),
            textStyle = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace),
            supportingText = hint,
            modifier = modifier.heightIn(max = 320.dp),
        )
        "intedit" -> {
            val min = control.json.int("min", Int.MIN_VALUE)
            NumberField("", values[name] as? Int ?: 0, modifier, signed = min < 0) {
                values[name] = it.toInt()
            }
        }
        "floatedit" -> {
            val min = control.json.double("min", -Double.MAX_VALUE)
            NumberField("", values[name] as? Double ?: 0.0, modifier, decimals = true, signed = min < 0) {
                values[name] = it
            }
        }
        "dropdown" -> Dropdown(
            label = control.hint,
            options = control.json.strings("items"),
            selected = values[name] as? String ?: "",
            modifier = modifier,
        ) { _, item -> values[name] = item }
        "checkbox" -> CheckRow(control.json.str("label"), values[name] as? Boolean ?: false, modifier) {
            values[name] = it
        }
        "color", "coloralpha" -> ColorField(
            label = control.hint,
            color = values[name] as? AssColor ?: AssColor(0, 0, 0),
            showAlpha = control.kind == "coloralpha",
            modifier = modifier,
        ) { values[name] = it }
    }
}
