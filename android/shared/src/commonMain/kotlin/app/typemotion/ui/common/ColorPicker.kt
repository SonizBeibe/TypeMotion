package app.typemotion.ui.common

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.drag
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.typemotion.model.AssColor
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/** Pick a colour, and its transparency if [showAlpha] */
@Composable
fun ColorPickerDialog(initial: AssColor, showAlpha: Boolean, onDismiss: () -> Unit, onPick: (AssColor) -> Unit) {
    val start = remember { toHsv(initial) }
    var hue by remember { mutableFloatStateOf(start[0]) }
    var saturation by remember { mutableFloatStateOf(start[1]) }
    var brightness by remember { mutableFloatStateOf(start[2]) }
    var alpha by remember { mutableIntStateOf(initial.a) }
    val color = fromHsv(hue, saturation, brightness, if (showAlpha) alpha else initial.a)
    var text by remember { mutableStateOf(initial.toAss() + "&") }
    var editingText by remember { mutableStateOf(false) }
    val shownText = if (editingText) text else color.toAss() + "&"

    fun setColor(c: AssColor) {
        val hsv = toHsv(c)
        // Keep the hue of greys, which have none
        if (hsv[1] > 0f && hsv[2] > 0f) hue = hsv[0]
        saturation = hsv[1]
        brightness = hsv[2]
        if (showAlpha) alpha = c.a
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Seleccionar color") },
        text = {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.verticalScroll(rememberScrollState()),
            ) {
                SaturationBrightnessBox(hue, saturation, brightness) { s, v ->
                    editingText = false
                    saturation = s
                    brightness = v
                }
                HueBar(hue) {
                    editingText = false
                    hue = it
                }
                if (showAlpha) {
                    Text("Transparencia (alfa): $alpha", style = MaterialTheme.typography.bodySmall)
                    Slider(
                        value = alpha.toFloat(),
                        onValueChange = {
                            editingText = false
                            alpha = it.roundToInt()
                        },
                        valueRange = 0f..255f,
                    )
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    ColorSwatch(initial, Modifier.size(32.dp))
                    Text("→")
                    ColorSwatch(color, Modifier.size(40.dp))
                    OutlinedTextField(
                        value = shownText,
                        onValueChange = {
                            editingText = true
                            text = it
                            AssColor.parse(it)?.let(::setColor)
                        },
                        singleLine = true,
                        label = { Text("ASS o #HTML", fontSize = 10.sp) },
                        textStyle = MaterialTheme.typography.labelMedium,
                        modifier = Modifier.weight(1f),
                    )
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    for ((label, component) in listOf("R" to 0, "G" to 1, "B" to 2)) {
                        val value = when (component) { 0 -> color.r; 1 -> color.g; else -> color.b }
                        NumberField(label, value, modifier = Modifier.weight(1f)) { number ->
                            val v = number.toInt().coerceIn(0, 255)
                            editingText = false
                            setColor(when (component) {
                                0 -> color.copy(r = v)
                                1 -> color.copy(g = v)
                                else -> color.copy(b = v)
                            })
                        }
                    }
                }
            }
        },
        confirmButton = { TextButton(onClick = { onPick(color) }) { Text("Aceptar") } },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancelar") } },
    )
}

@Composable
private fun SaturationBrightnessBox(hue: Float, saturation: Float, brightness: Float, onChange: (Float, Float) -> Unit) {
    val hueColor = Color(fromHsv(hue, 1f, 1f, 0).toArgb())
    Canvas(
        Modifier
            .fillMaxWidth()
            .height(180.dp)
            .pointerInput(Unit) {
                fun update(position: Offset) {
                    onChange(
                        (position.x / size.width).coerceIn(0f, 1f),
                        1f - (position.y / size.height).coerceIn(0f, 1f),
                    )
                }
                awaitEachGesture {
                    val down = awaitFirstDown()
                    update(down.position)
                    drag(down.id) { change ->
                        update(change.position)
                        change.consume()
                    }
                }
            },
    ) {
        drawRect(Brush.horizontalGradient(listOf(Color.White, hueColor)))
        drawRect(Brush.verticalGradient(listOf(Color.Transparent, Color.Black)))
        val center = Offset(saturation * size.width, (1f - brightness) * size.height)
        drawCircle(Color.White, radius = 9.dp.toPx(), center = center, style = Stroke(width = 3.dp.toPx()))
        drawCircle(Color.Black, radius = 11.dp.toPx(), center = center, style = Stroke(width = 1.dp.toPx()))
    }
}

@Composable
private fun HueBar(hue: Float, onChange: (Float) -> Unit) {
    val colors = (0..6).map { Color(fromHsv(it * 60f, 1f, 1f, 0).toArgb()) }
    Canvas(
        Modifier
            .fillMaxWidth()
            .height(28.dp)
            .pointerInput(Unit) {
                fun update(position: Offset) {
                    onChange((position.x / size.width).coerceIn(0f, 1f) * 360f)
                }
                awaitEachGesture {
                    val down = awaitFirstDown()
                    update(down.position)
                    drag(down.id) { change ->
                        update(change.position)
                        change.consume()
                    }
                }
            },
    ) {
        drawRect(Brush.horizontalGradient(colors))
        val x = hue / 360f * size.width
        drawLine(Color.White, Offset(x, 0f), Offset(x, size.height), strokeWidth = 4.dp.toPx())
        drawLine(Color.Black, Offset(x - 3.dp.toPx(), 0f), Offset(x - 3.dp.toPx(), size.height), strokeWidth = 1.dp.toPx())
        drawLine(Color.Black, Offset(x + 3.dp.toPx(), 0f), Offset(x + 3.dp.toPx(), size.height), strokeWidth = 1.dp.toPx())
    }
}

private fun toHsv(c: AssColor): FloatArray {
    val r = c.r / 255f
    val g = c.g / 255f
    val b = c.b / 255f
    val maxC = max(r, max(g, b))
    val minC = min(r, min(g, b))
    val delta = maxC - minC
    val h = when {
        delta == 0f -> 0f
        maxC == r -> 60f * (((g - b) / delta) % 6f)
        maxC == g -> 60f * ((b - r) / delta + 2f)
        else -> 60f * ((r - g) / delta + 4f)
    }.let { if (it < 0) it + 360f else it }
    val s = if (maxC == 0f) 0f else delta / maxC
    return floatArrayOf(h, s, maxC)
}

private fun fromHsv(h: Float, s: Float, v: Float, alpha: Int): AssColor {
    val c = v * s
    val x = c * (1 - abs((h / 60f) % 2f - 1))
    val m = v - c
    val (r, g, b) = when {
        h < 60 -> Triple(c, x, 0f)
        h < 120 -> Triple(x, c, 0f)
        h < 180 -> Triple(0f, c, x)
        h < 240 -> Triple(0f, x, c)
        h < 300 -> Triple(x, 0f, c)
        else -> Triple(c, 0f, x)
    }
    fun channel(value: Float) = ((value + m) * 255f).roundToInt().coerceIn(0, 255)
    return AssColor(channel(r), channel(g), channel(b), alpha)
}
