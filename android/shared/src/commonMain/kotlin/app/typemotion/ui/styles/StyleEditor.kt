package app.typemotion.ui.styles

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import app.typemotion.engine.EngineError
import app.typemotion.engine.bool
import app.typemotion.engine.str
import app.typemotion.engine.strings
import app.typemotion.model.AssColor
import app.typemotion.model.AssStyle
import app.typemotion.state.AppState
import app.typemotion.ui.common.CheckRow
import app.typemotion.ui.common.ColorField
import app.typemotion.ui.common.ComboField
import app.typemotion.ui.common.Dropdown
import app.typemotion.ui.common.NumberField
import app.typemotion.ui.common.Panel
import app.typemotion.ui.common.SectionTitle
import app.typemotion.ui.common.ToolScreen

/** A style to edit: of the script when [catalog] is null, otherwise of that catalog */
class StyleEditTarget(
    val style: AssStyle,
    val catalog: String?,
    /** The name it is saved under now; empty for a new style */
    val oldName: String,
    val isNew: Boolean,
)

private val borderStyles = listOf("Contorno", "Cajas de borde", "Caja de sombra (solo libass)")
private fun borderToControl(style: Int) = when (style) {
    3 -> 1
    4 -> 2
    else -> 0
}
private fun controlToBorder(choice: Int) = when (choice) {
    1 -> 3
    2 -> 4
    else -> 1
}

/** The desktop Style Editor, as a full screen window */
@Composable
fun StyleEditor(app: AppState, target: StyleEditTarget, onClose: (saved: Boolean) -> Unit) {
    var style by remember { mutableStateOf(target.style) }
    var oldName by remember { mutableStateOf(target.oldName) }
    var isNew by remember { mutableStateOf(target.isNew) }
    var savedOnce by remember { mutableStateOf(false) }
    var families by remember { mutableStateOf(emptyList<String>()) }
    var encodings by remember { mutableStateOf(emptyList<String>()) }

    LaunchedEffect(Unit) {
        try {
            families = app.call("fonts").strings("families")
            encodings = app.call("style_encodings").strings("encodings")
        } catch (_: EngineError) {
        }
    }

    /** Save the style; OK then closes, Apply keeps editing the saved style */
    fun save(close: Boolean) = app.run {
        val result = app.call(
            "save_style",
            "catalog" to target.catalog,
            "name" to oldName,
            "new" to isNew,
            "style" to style.toJson(),
        )
        if (!result.bool("saved")) return@run
        savedOnce = true
        oldName = result.str("name", style.name)
        isNew = false
        if (close) onClose(true)
    }

    fun update(change: AssStyle.() -> AssStyle) {
        style = style.change()
    }

    Dialog(onDismissRequest = { onClose(savedOnce) }, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxSize()) {
            ToolScreen(
                title = "Editor de estilo",
                subtitle = (if (target.catalog != null) "Catálogo ${target.catalog}" else "Script actual") +
                    if (isNew) " • nuevo" else "",
                onBack = { onClose(savedOnce) },
                buttons = {
                    TextButton(onClick = { onClose(savedOnce) }) { Text("Cancelar") }
                    OutlinedButton(onClick = { save(close = false) }, enabled = style.name.isNotBlank()) { Text("Aplicar") }
                    Button(onClick = { save(close = true) }, enabled = style.name.isNotBlank()) { Text("Aceptar") }
                },
            ) {
                Panel(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp)) {
                        SectionTitle("Nombre del estilo")
                        OutlinedTextField(style.name, { update { copy(name = it) } }, singleLine = true, modifier = Modifier.fillMaxWidth())
                    }
                }

                Panel(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        SectionTitle("Fuente")
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                            val query = style.font.trim()
                            val options = remember(families, query) {
                                (if (query.isEmpty()) families else families.filter { it.contains(query, ignoreCase = true) }).take(80)
                            }
                            ComboField(
                                "Fuente",
                                style.font,
                                options,
                                Modifier.weight(2f),
                                onType = { update { copy(font = it) } },
                                onPick = { update { copy(font = it) } },
                            )
                            NumberField("Tamaño", style.size, Modifier.weight(1f), decimals = true) { update { copy(size = it) } }
                        }
                        Row {
                            CheckRow("Negrita", style.bold, Modifier.weight(1f)) { update { copy(bold = it) } }
                            CheckRow("Cursiva", style.italic, Modifier.weight(1f)) { update { copy(italic = it) } }
                        }
                        Row {
                            CheckRow("Subrayado", style.underline, Modifier.weight(1f)) { update { copy(underline = it) } }
                            CheckRow("Tachado", style.strikeout, Modifier.weight(1f)) { update { copy(strikeout = it) } }
                        }
                    }
                }

                Panel(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        SectionTitle("Colores")
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            StyleColor("Primario", style.primary, Modifier.weight(1f)) { update { copy(primary = it) } }
                            StyleColor("Secundario", style.secondary, Modifier.weight(1f)) { update { copy(secondary = it) } }
                        }
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            StyleColor("Contorno", style.outline, Modifier.weight(1f)) { update { copy(outline = it) } }
                            StyleColor("Sombra", style.shadow, Modifier.weight(1f)) { update { copy(shadow = it) } }
                        }
                    }
                }

                Panel(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp)) {
                        SectionTitle("Márgenes")
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            NumberField("Izquierdo", style.marginL, Modifier.weight(1f), signed = true) { update { copy(marginL = it.toInt()) } }
                            NumberField("Derecho", style.marginR, Modifier.weight(1f), signed = true) { update { copy(marginR = it.toInt()) } }
                            NumberField("Vertical", style.marginV, Modifier.weight(1f), signed = true) { update { copy(marginV = it.toInt()) } }
                        }
                    }
                }

                Panel(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                        SectionTitle("Alineación", Modifier.fillMaxWidth())
                        AlignmentPad(style.alignment) { update { copy(alignment = it) } }
                    }
                }

                Panel(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        SectionTitle("Contorno")
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            NumberField("Contorno", style.outlineWidth, Modifier.weight(1f), decimals = true) { update { copy(outlineWidth = it) } }
                            NumberField("Sombra", style.shadowWidth, Modifier.weight(1f), decimals = true) { update { copy(shadowWidth = it) } }
                        }
                        Dropdown(
                            "Estilo de borde",
                            borderStyles,
                            borderStyles[borderToControl(style.borderStyle)],
                            Modifier.fillMaxWidth(),
                        ) { i, _ -> update { copy(borderStyle = controlToBorder(i)) } }
                    }
                }

                Panel(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        SectionTitle("Varios")
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            NumberField("Escala X%", style.scaleX, Modifier.weight(1f), decimals = true) { update { copy(scaleX = it) } }
                            NumberField("Escala Y%", style.scaleY, Modifier.weight(1f), decimals = true) { update { copy(scaleY = it) } }
                        }
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            NumberField("Rotación", style.angle, Modifier.weight(1f), decimals = true, signed = true) { update { copy(angle = it) } }
                            NumberField("Espaciado", style.spacing, Modifier.weight(1f), decimals = true, signed = true) { update { copy(spacing = it) } }
                        }
                        if (encodings.isNotEmpty()) {
                            val current = encodings.firstOrNull { it.substringBefore(" - ").trim() == style.encoding.toString() }
                                ?: style.encoding.toString()
                            Dropdown("Codificación", encodings, current, Modifier.fillMaxWidth()) { _, item ->
                                item.substringBefore(" - ").trim().toIntOrNull()?.let { value -> update { copy(encoding = value) } }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun StyleColor(label: String, value: String, modifier: Modifier, onChange: (String) -> Unit) {
    val color = AssColor.parse(value) ?: AssColor(255, 255, 255)
    ColorField(label, color, modifier, showAlpha = true) { onChange(it.toAss()) }
}

/** The alignment in numpad order, as the desktop radio box shows it */
@Composable
private fun AlignmentPad(selected: Int, onSelect: (Int) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        for (row in listOf(listOf(7, 8, 9), listOf(4, 5, 6), listOf(1, 2, 3))) {
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (value in row) {
                    val modifier = Modifier.size(56.dp, 44.dp)
                    if (value == selected) {
                        FilledTonalButton(onClick = { onSelect(value) }, modifier = modifier, shape = RoundedCornerShape(8.dp)) {
                            Text("$value")
                        }
                    } else {
                        OutlinedButton(onClick = { onSelect(value) }, modifier = modifier, shape = RoundedCornerShape(8.dp)) {
                            Text("$value")
                        }
                    }
                }
            }
        }
    }
}
