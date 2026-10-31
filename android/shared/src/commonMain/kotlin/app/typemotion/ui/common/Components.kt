package app.typemotion.ui.common

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.typemotion.model.AssColor
import app.typemotion.model.AssTime
import app.typemotion.platform.fixed
import app.typemotion.ui.theme.TypeMotionTheme

/** The top bar of the screens other than the editor */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ScreenTopBar(
    title: String,
    subtitle: String? = null,
    onBack: (() -> Unit)?,
    actions: @Composable () -> Unit = {},
) {
    TopAppBar(
        colors = TopAppBarDefaults.topAppBarColors(containerColor = TypeMotionTheme.colors.panelHeader),
        navigationIcon = {
            if (onBack != null) {
                IconButton(onClick = onBack) {
                    Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Atrás")
                }
            }
        },
        title = {
            Column {
                Text(title, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                if (subtitle != null) {
                    Text(
                        subtitle,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.secondary,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
        },
        actions = { actions() },
    )
}

@Composable
fun SectionTitle(text: String, modifier: Modifier = Modifier) {
    Text(
        text,
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.primary,
        modifier = modifier.padding(top = 8.dp, bottom = 2.dp),
    )
}

/** A panel in the colours of the Gemini design */
@Composable
fun Panel(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Surface(
        modifier = modifier,
        color = TypeMotionTheme.colors.panelBackground,
        shape = RoundedCornerShape(10.dp),
        content = content,
    )
}

/** A check box with its label, which can be tapped anywhere */
@Composable
fun CheckRow(label: String, checked: Boolean, modifier: Modifier = Modifier, enabled: Boolean = true, onChange: (Boolean) -> Unit) {
    Row(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .clickable(enabled = enabled) { onChange(!checked) }
            .heightIn(min = 44.dp)
            .padding(end = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Checkbox(checked = checked, onCheckedChange = onChange, enabled = enabled)
        Text(label, style = MaterialTheme.typography.bodyMedium)
    }
}

/** A choice between a few options, as the desktop radio boxes show them */
@Composable
fun RadioGroup(options: List<String>, selected: Int, modifier: Modifier = Modifier, enabled: Boolean = true, onSelect: (Int) -> Unit) {
    Column(modifier) {
        options.forEachIndexed { index, option ->
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .clickable(enabled = enabled) { onSelect(index) }
                    .heightIn(min = 40.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                RadioButton(selected = index == selected, onClick = { onSelect(index) }, enabled = enabled)
                Text(option, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

/** A drop-down list, like a read-only desktop combo box */
@Composable
fun Dropdown(
    label: String,
    options: List<String>,
    selected: String,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    onSelect: (Int, String) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    Box(modifier) {
        OutlinedButton(
            onClick = { expanded = true },
            enabled = enabled,
            modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp),
            shape = RoundedCornerShape(8.dp),
            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 4.dp),
        ) {
            Column(Modifier.weight(1f)) {
                if (label.isNotEmpty()) {
                    Text(label, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1)
                }
                Text(selected, maxLines = 1, overflow = TextOverflow.Ellipsis, fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurface)
            }
            Icon(Icons.Default.ArrowDropDown, contentDescription = null)
        }
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            options.forEachIndexed { index, option ->
                DropdownMenuItem(
                    text = { Text(option) },
                    onClick = {
                        expanded = false
                        onSelect(index, option)
                    },
                )
            }
        }
    }
}

/** A text field with a list to pick from, like an editable desktop combo box */
@Composable
fun ComboField(
    label: String,
    value: String,
    options: List<String>,
    modifier: Modifier = Modifier,
    onType: (String) -> Unit,
    onPick: (String) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    Box(modifier) {
        OutlinedTextField(
            value = value,
            onValueChange = onType,
            label = { Text(label, fontSize = 11.sp) },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
            trailingIcon = {
                if (options.isNotEmpty()) {
                    IconButton(onClick = { expanded = true }) {
                        Icon(Icons.Default.ArrowDropDown, contentDescription = "Lista")
                    }
                }
            },
        )
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            options.forEach { option ->
                DropdownMenuItem(text = { Text(option) }, onClick = {
                    expanded = false
                    onPick(option)
                })
            }
        }
    }
}

/**
 * A number field which reports a value only when the text is a valid
 * number, keeping what the user is typing otherwise
 */
@Composable
fun NumberField(
    label: String,
    value: Number,
    modifier: Modifier = Modifier,
    decimals: Boolean = false,
    signed: Boolean = false,
    enabled: Boolean = true,
    onChange: (Double) -> Unit,
) {
    val formatted = formatNumber(value)
    var text by remember { mutableStateOf(formatted) }
    LaunchedEffect(formatted) {
        if (text.replace(',', '.').toDoubleOrNull() != value.toDouble()) text = formatted
    }
    OutlinedTextField(
        value = text,
        onValueChange = { new ->
            text = new
            val number = new.replace(',', '.').toDoubleOrNull()
            if (number != null && (decimals || number == kotlin.math.floor(number)) && (signed || number >= 0))
                onChange(number)
        },
        label = { Text(label, fontSize = 11.sp, maxLines = 1) },
        singleLine = true,
        enabled = enabled,
        textStyle = MaterialTheme.typography.labelMedium,
        keyboardOptions = KeyboardOptions(keyboardType = if (decimals) KeyboardType.Decimal else KeyboardType.Number),
        modifier = modifier,
    )
}

fun formatNumber(value: Number): String {
    val d = value.toDouble()
    return if (d == kotlin.math.floor(d) && !d.isInfinite()) d.toLong().toString() else fixed(d, 3).trimEnd('0').trimEnd('.', ',')
}

/**
 * A time field of the edit box: the time, with buttons to move it by
 * centiseconds, and a dialog to type it when tapped
 */
@Composable
fun TimeField(label: String, ms: Int, modifier: Modifier = Modifier, enabled: Boolean = true, onChange: (Int) -> Unit) {
    var editing by remember { mutableStateOf(false) }
    Surface(
        modifier = modifier,
        color = MaterialTheme.colorScheme.surfaceVariant,
        shape = RoundedCornerShape(8.dp),
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.padding(vertical = 4.dp)) {
            Text(label, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(
                AssTime.format(ms),
                style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.Bold, fontSize = 13.sp),
                modifier = Modifier
                    .clip(RoundedCornerShape(4.dp))
                    .clickable(enabled = enabled) { editing = true }
                    .padding(horizontal = 6.dp, vertical = 4.dp),
            )
            Row(horizontalArrangement = Arrangement.SpaceEvenly, modifier = Modifier.fillMaxWidth()) {
                TextButton(onClick = { onChange((ms - 50).coerceAtLeast(0)) }, enabled = enabled, contentPadding = PaddingValues(0.dp), modifier = Modifier.height(30.dp)) {
                    Text("-5cs", fontSize = 10.sp)
                }
                TextButton(onClick = { onChange(ms + 50) }, enabled = enabled, contentPadding = PaddingValues(0.dp), modifier = Modifier.height(30.dp)) {
                    Text("+5cs", fontSize = 10.sp)
                }
            }
        }
    }
    if (editing) {
        TimeDialog(label, ms, onDismiss = { editing = false }) {
            editing = false
            onChange(it)
        }
    }
}

/** Type a time, or move it in steps */
@Composable
fun TimeDialog(title: String, ms: Int, onDismiss: () -> Unit, onDone: (Int) -> Unit) {
    var text by remember { mutableStateOf(AssTime.format(ms)) }
    val parsed = AssTime.parse(text)
    fun step(delta: Int) {
        text = AssTime.format(((parsed ?: ms) + delta).coerceAtLeast(0))
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = text,
                    onValueChange = { text = it },
                    singleLine = true,
                    isError = parsed == null,
                    textStyle = MaterialTheme.typography.labelMedium.copy(fontSize = 18.sp),
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    modifier = Modifier.fillMaxWidth(),
                )
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    for ((label, delta) in listOf("-1s" to -1000, "-10cs" to -100, "-1cs" to -10)) {
                        OutlinedButton(onClick = { step(delta) }, contentPadding = PaddingValues(horizontal = 6.dp), modifier = Modifier.weight(1f)) {
                            Text(label, fontSize = 11.sp)
                        }
                    }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    for ((label, delta) in listOf("+1cs" to 10, "+10cs" to 100, "+1s" to 1000)) {
                        OutlinedButton(onClick = { step(delta) }, contentPadding = PaddingValues(horizontal = 6.dp), modifier = Modifier.weight(1f)) {
                            Text(label, fontSize = 11.sp)
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = { parsed?.let(onDone) }, enabled = parsed != null) { Text("Aceptar") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancelar") } },
    )
}

/** A colour, drawn over a checkerboard so that its transparency shows */
@Composable
fun ColorSwatch(color: AssColor, modifier: Modifier = Modifier.size(36.dp)) {
    val outline = MaterialTheme.colorScheme.outline
    Canvas(modifier.clip(CircleShape)) {
        val cell = size.minDimension / 4
        for (x in 0 until 4) for (y in 0 until 4) {
            drawRect(
                if ((x + y) % 2 == 0) Color(0xFFCCCCCC) else Color(0xFF888888),
                topLeft = Offset(x * cell, y * cell),
                size = Size(cell, cell),
            )
        }
        drawRect(Color(color.toArgb()))
        drawCircle(outline, style = Stroke(width = 2.dp.toPx()))
    }
}

/** A colour field: the swatch and the colour as ASS writes it */
@Composable
fun ColorField(label: String, color: AssColor, modifier: Modifier = Modifier, showAlpha: Boolean = true, onChange: (AssColor) -> Unit) {
    var picking by remember { mutableStateOf(false) }
    Surface(
        onClick = { picking = true },
        color = MaterialTheme.colorScheme.surfaceVariant,
        shape = RoundedCornerShape(8.dp),
        modifier = modifier,
    ) {
        Row(
            modifier = Modifier.padding(8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            ColorSwatch(color)
            Column(Modifier.weight(1f)) {
                Text(label, style = MaterialTheme.typography.bodySmall, maxLines = 1)
                Text(color.toAss() + "&", style = MaterialTheme.typography.labelMedium, color = TypeMotionTheme.colors.assTagHighlight)
            }
        }
    }
    if (picking) {
        ColorPickerDialog(
            initial = color,
            showAlpha = showAlpha,
            onDismiss = { picking = false },
            onPick = {
                picking = false
                onChange(it)
            },
        )
    }
}

/** A list to pick one item from */
@Composable
fun ChoiceDialog(title: String, message: String, choices: List<String>, onDismiss: () -> Unit, onPick: (Int) -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title) },
        text = {
            Column {
                if (message.isNotEmpty()) Text(message, modifier = Modifier.padding(bottom = 8.dp))
                LazyColumn(Modifier.heightIn(max = 360.dp)) {
                    items(choices.size) { index ->
                        Text(
                            choices[index],
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { onPick(index) }
                                .padding(vertical = 12.dp, horizontal = 4.dp),
                        )
                        HorizontalDivider()
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancelar") } },
    )
}

@Composable
fun SmallSpacer() = Spacer(Modifier.width(6.dp))
