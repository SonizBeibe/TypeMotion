package app.typemotion.ui.editor

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Redo
import androidx.compose.material.icons.automirrored.filled.Undo
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.input.OffsetMapping
import androidx.compose.ui.text.input.TransformedText
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.typemotion.state.AppState
import app.typemotion.state.Screen
import app.typemotion.ui.common.ComboField
import app.typemotion.ui.common.CheckRow
import app.typemotion.ui.common.Dropdown
import app.typemotion.ui.common.NumberField
import app.typemotion.ui.common.TimeField
import app.typemotion.ui.theme.TypeMotionTheme

/** The desktop edit box's buttons, which run these commands */
private data class EditButton(val label: String, val command: String, val style: SpanStyle? = null)

private val editButtons = listOf(
    EditButton("B", "edit/style/bold", SpanStyle(fontWeight = FontWeight.Bold)),
    EditButton("I", "edit/style/italic", SpanStyle(fontStyle = FontStyle.Italic)),
    EditButton("U", "edit/style/underline", SpanStyle(textDecoration = TextDecoration.Underline)),
    EditButton("S", "edit/style/strikeout", SpanStyle(textDecoration = TextDecoration.LineThrough)),
    EditButton("\\fn", "edit/font"),
    EditButton("\\c", "edit/color/primary"),
    EditButton("\\2c", "edit/color/secondary"),
    EditButton("\\3c", "edit/color/outline"),
    EditButton("\\4c", "edit/color/shadow"),
)

/** The text field of the edit box, with the buttons above it */
@Composable
fun EditTextPanel(app: AppState, modifier: Modifier = Modifier, fill: Boolean = false, maxLines: Int = 4) {
    val box = app.editBox
    val state = box.state
    Surface(modifier = modifier, color = TypeMotionTheme.colors.panelBackground) {
        Column(Modifier.padding(horizontal = 6.dp, vertical = 4.dp)) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                IconButton(onClick = { app.undo() }, enabled = app.fileInfo.canUndo, modifier = Modifier.size(36.dp)) {
                    Icon(Icons.AutoMirrored.Filled.Undo, contentDescription = "Deshacer", modifier = Modifier.size(18.dp))
                }
                IconButton(onClick = { app.redo() }, enabled = app.fileInfo.canRedo, modifier = Modifier.size(36.dp)) {
                    Icon(Icons.AutoMirrored.Filled.Redo, contentDescription = "Rehacer", modifier = Modifier.size(18.dp))
                }
                for (button in editButtons) {
                    Surface(
                        onClick = { app.runCommand(button.command) },
                        enabled = state.active,
                        color = MaterialTheme.colorScheme.surfaceVariant,
                        shape = RoundedCornerShape(6.dp),
                        modifier = Modifier.height(32.dp),
                    ) {
                        Box(contentAlignment = Alignment.Center, modifier = Modifier.padding(horizontal = 10.dp)) {
                            Text(
                                buildAnnotatedString {
                                    withStyle(button.style ?: SpanStyle()) { append(button.label) }
                                },
                                style = MaterialTheme.typography.labelMedium,
                                color = TypeMotionTheme.colors.assTagHighlight,
                            )
                        }
                    }
                }
                Text(
                    state.charCount.toString(),
                    style = MaterialTheme.typography.labelSmall,
                    color = if (state.charLimitExceeded) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(horizontal = 6.dp),
                )
            }

            if (state.showOriginal) {
                OriginalText(app)
            }

            Spacer(Modifier.height(4.dp))
            val tagColor = TypeMotionTheme.colors.assTagHighlight
            val breakColor = MaterialTheme.colorScheme.secondary
            val highlight = remember(tagColor, breakColor) { AssHighlight(tagColor, breakColor) }
            OutlinedTextField(
                value = box.value,
                onValueChange = box::onValueChange,
                enabled = state.active,
                modifier = Modifier.fillMaxWidth().let { if (fill) it.weight(1f) else it },
                textStyle = MaterialTheme.typography.labelMedium.copy(fontSize = 14.sp),
                minLines = 2,
                maxLines = if (fill) Int.MAX_VALUE else maxLines,
                visualTransformation = highlight,
                keyboardOptions = KeyboardOptions(capitalization = KeyboardCapitalization.Sentences, autoCorrectEnabled = false),
                placeholder = { Text("Escribe el subtítulo o inserta tags {\\...}") },
                trailingIcon = {
                    // Like the desktop's commit button: go to the next line,
                    // making one at the end
                    IconButton(onClick = { app.runCommand("grid/line/next/create") }, enabled = state.active) {
                        Icon(Icons.Default.CheckCircle, contentDescription = "Siguiente línea", tint = TypeMotionTheme.colors.statusSuccess)
                    }
                },
            )
        }
    }
}

/** The line's text when it was selected, with the desktop's buttons for it */
@Composable
private fun OriginalText(app: AppState) {
    Column(Modifier.padding(top = 4.dp)) {
        Surface(
            color = MaterialTheme.colorScheme.surfaceVariant,
            shape = RoundedCornerShape(6.dp),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(
                app.editBox.state.initialText,
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(8.dp),
            )
        }
        Row(
            Modifier
                .horizontalScroll(rememberScrollState())
                .padding(top = 2.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            for ((label, command) in listOf(
                "Revertir" to "edit/revert",
                "Limpiar" to "edit/clear",
                "Limpiar texto" to "edit/clear/text",
                "Insertar original" to "edit/insert_original",
            )) {
                OutlinedButton(onClick = { app.runCommand(command) }, contentPadding = PaddingValues(horizontal = 10.dp), modifier = Modifier.height(34.dp)) {
                    Text(label, fontSize = 12.sp)
                }
            }
        }
    }
}

/** The active line's other fields: comment, style, layer, times, actor, effect and margins */
@Composable
fun LineFieldsPanel(app: AppState, modifier: Modifier = Modifier) {
    val box = app.editBox
    val state = box.state
    val enabled = state.active
    Surface(modifier = modifier, color = TypeMotionTheme.colors.panelBackground, shape = RoundedCornerShape(8.dp)) {
        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            if (state.selected > 1) {
                Text(
                    "Los cambios se aplican a las ${state.selected} líneas seleccionadas",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.secondary,
                )
            }
            val comment = @Composable {
                FilterChip(
                    selected = state.comment,
                    onClick = { box.set("comment", !state.comment) },
                    enabled = enabled,
                    label = { Text("Comentario", fontSize = 11.sp) },
                )
            }
            val style: @Composable RowScope.() -> Unit = {
                Dropdown(
                    label = "Estilo",
                    options = state.styles,
                    selected = state.style,
                    enabled = enabled,
                    modifier = Modifier.weight(1f),
                ) { _, style -> box.set("style", style) }
                IconButton(
                    onClick = { app.navigate(Screen.Styles(editStyle = state.style)) },
                    enabled = enabled && state.styleExists,
                ) {
                    Icon(Icons.Default.Edit, contentDescription = "Editar estilo")
                }
                IconButton(onClick = { app.navigate(Screen.Styles()) }) {
                    Icon(Icons.Default.Palette, contentDescription = "Gestor de estilos")
                }
            }
            BoxWithConstraints {
                // In a narrow panel the comment chip goes above, so that the
                // style's name fits
                if (maxWidth < 320.dp) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        comment()
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically, content = style)
                    }
                } else {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                        comment()
                        style()
                    }
                }
            }

            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                TimeField("Inicio", state.start, Modifier.weight(1f), enabled) { box.set("start", it) }
                TimeField("Fin", state.end, Modifier.weight(1f), enabled) { box.set("end", it) }
                TimeField("Duración", state.duration, Modifier.weight(1f), enabled) { box.set("duration", it) }
            }

            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                ComboField(
                    label = "Actor",
                    value = state.actor,
                    options = state.actors,
                    modifier = Modifier.weight(1f),
                    onType = { box.set("actor", it, typed = true) },
                    onPick = { box.set("actor", it) },
                )
                ComboField(
                    label = "Efecto",
                    value = state.effect,
                    options = state.effects,
                    modifier = Modifier.weight(1f),
                    onType = { box.set("effect", it, typed = true) },
                    onPick = { box.set("effect", it) },
                )
            }

            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                NumberField("Capa", state.layer, Modifier.width(72.dp), enabled = enabled) { box.set("layer", it.toInt()) }
                NumberField("Margen I", state.marginL, Modifier.weight(1f), signed = true, enabled = enabled) { box.set("margin_l", it.toInt()) }
                NumberField("Margen D", state.marginR, Modifier.weight(1f), signed = true, enabled = enabled) { box.set("margin_r", it.toInt()) }
                NumberField("Margen V", state.marginV, Modifier.weight(1f), signed = true, enabled = enabled) { box.set("margin_v", it.toInt()) }
            }

            CheckRow("Mostrar original", state.showOriginal) { show ->
                app.run {
                    app.call("set_option", "name" to "Subtitle/Show Original", "value" to show)
                    app.editBox.load()
                }
            }
        }
    }
}

/** Colours the override blocks and line breaks of ASS text */
class AssHighlight(private val tagColor: Color, private val breakColor: Color) : VisualTransformation {
    override fun filter(text: AnnotatedString): TransformedText =
        TransformedText(highlight(text.text), OffsetMapping.Identity)

    private fun highlight(text: String): AnnotatedString = buildAnnotatedString {
        var i = 0
        while (i < text.length) {
            val c = text[i]
            when {
                c == '{' -> {
                    val end = text.indexOf('}', i).let { if (it < 0) text.length else it + 1 }
                    withStyle(SpanStyle(color = tagColor)) { append(text, i, end) }
                    i = end
                }
                c == '\\' && i + 1 < text.length && text[i + 1] in "Nnh" -> {
                    withStyle(SpanStyle(color = breakColor, fontWeight = FontWeight.Bold)) { append(text, i, i + 2) }
                    i += 2
                }
                else -> {
                    append(c)
                    i++
                }
            }
        }
    }
}
