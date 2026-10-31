package app.typemotion.ui.tools

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import app.typemotion.engine.bool
import app.typemotion.engine.int
import app.typemotion.engine.objects
import app.typemotion.engine.str
import app.typemotion.model.AssTime
import app.typemotion.state.AppState
import app.typemotion.ui.common.NumberField
import app.typemotion.ui.common.Panel
import app.typemotion.ui.common.RadioGroup
import app.typemotion.ui.common.SectionTitle
import app.typemotion.ui.common.TimeField
import app.typemotion.ui.common.ToolScreen
import app.typemotion.json.JSONObject

private data class ShiftSettings(
    val timeMs: Int = 0,
    val frames: Int = 0,
    val byTime: Boolean = true,
    val backward: Boolean = true,
    /** 0 start and end, 1 start only, 2 end only */
    val fields: Int = 0,
    /** 0 all rows, 1 selected rows, 2 selection onward */
    val mode: Int = 0,
) {
    fun toArgs(apply: Boolean): Array<Pair<String, Any?>> = arrayOf(
        "time" to AssTime.format(timeMs),
        "frames" to frames,
        "by_time" to byTime,
        "backward" to backward,
        "fields" to fields,
        "mode" to mode,
        "apply" to apply,
    )

    companion object {
        fun from(o: JSONObject, timeKey: String) = ShiftSettings(
            timeMs = AssTime.parse(o.str(timeKey)) ?: 0,
            frames = if (o.bool("by_time", true)) o.int("frames") else o.str(timeKey).toIntOrNull() ?: o.int("frames"),
            byTime = o.bool("by_time", true),
            backward = o.bool("backward"),
            fields = o.int("fields"),
            mode = o.int("mode"),
        )
    }
}

/** Shift Times, like the desktop dialog, which also keeps its settings when closed */
@Composable
fun ShiftTimesScreen(app: AppState) {
    var loaded by remember { mutableStateOf(false) }
    var settings by remember { mutableStateOf(ShiftSettings()) }
    var canUseFrames by remember { mutableStateOf(false) }
    var hasSelection by remember { mutableStateOf(false) }
    var history by remember { mutableStateOf(emptyList<JSONObject>()) }
    var reload by remember { mutableIntStateOf(0) }
    var applied by remember { mutableStateOf(false) }

    LaunchedEffect(reload) {
        try {
            val state = app.call("shift_times_state")
            if (!loaded) settings = ShiftSettings.from(state, "time")
            canUseFrames = state.bool("can_use_frames")
            hasSelection = state.bool("has_selection")
            history = state.objects("history")
            loaded = true
        } catch (e: Exception) {
            app.errorMessage = e.message
        }
    }

    // Closing the window keeps the settings, as closing the desktop dialog does
    val current by rememberUpdatedState(settings)
    DisposableEffect(Unit) {
        onDispose {
            if (loaded && !applied) app.run { app.call("shift_times", *current.toArgs(apply = false)) }
        }
    }

    fun update(change: ShiftSettings.() -> ShiftSettings) {
        settings = settings.change()
    }

    ToolScreen(
        title = "Desplazar tiempos",
        onBack = { app.back() },
        buttons = {
            TextButton(onClick = { app.back() }) { Text("Cancelar") }
            Button(
                onClick = {
                    applied = true
                    app.run {
                        app.call("shift_times", *settings.toArgs(apply = true))
                        app.back()
                    }
                },
                enabled = loaded,
            ) { Text("Aceptar") }
        },
    ) {
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionTitle("Desplazar por")
                Row(verticalAlignment = Alignment.CenterVertically) {
                    RadioButton(selected = settings.byTime, onClick = { update { copy(byTime = true) } })
                    TimeField("Tiempo", settings.timeMs, Modifier.weight(1f), enabled = settings.byTime) { ms ->
                        update { copy(timeMs = ms.coerceAtLeast(0)) }
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    RadioButton(
                        selected = !settings.byTime,
                        onClick = { update { copy(byTime = false) } },
                        enabled = canUseFrames,
                    )
                    NumberField(
                        "Fotogramas",
                        settings.frames,
                        Modifier.weight(1f),
                        enabled = canUseFrames && !settings.byTime,
                    ) { update { copy(frames = it.toInt()) } }
                }
                if (!canUseFrames) {
                    Text(
                        "Desplazar por fotogramas necesita un video abierto.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                RadioGroup(
                    listOf("Hacia adelante (aparecen más tarde)", "Hacia atrás (aparecen antes)"),
                    if (settings.backward) 1 else 0,
                ) { update { copy(backward = it == 1) } }
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("Afectar")
                RadioGroup(
                    listOf("Todas las filas", "Filas seleccionadas", "Desde la selección en adelante"),
                    if (hasSelection) settings.mode else 0,
                    enabled = hasSelection,
                ) { update { copy(mode = it) } }
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("Tiempos")
                RadioGroup(
                    listOf("Inicio y final", "Solo inicio", "Solo final"),
                    settings.fields,
                ) { update { copy(fields = it) } }
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    SectionTitle("Cargar del historial", Modifier.weight(1f))
                    OutlinedButton(
                        onClick = {
                            app.run {
                                app.call("clear_shift_history")
                                reload++
                            }
                        },
                        enabled = history.isNotEmpty(),
                    ) { Text("Borrar") }
                }
                if (history.isEmpty()) {
                    Text("Vacío", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                history.forEachIndexed { i, entry ->
                    if (i > 0) HorizontalDivider()
                    Text(
                        entry.str("display"),
                        style = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace),
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable {
                                // Like the desktop list, picking an entry loads its settings
                                val loadedEntry = ShiftSettings.from(entry, "amount")
                                update {
                                    copy(
                                        timeMs = if (loadedEntry.byTime) loadedEntry.timeMs else timeMs,
                                        frames = if (loadedEntry.byTime) frames else loadedEntry.frames,
                                        byTime = loadedEntry.byTime || !canUseFrames,
                                        backward = loadedEntry.backward,
                                        fields = loadedEntry.fields,
                                        mode = loadedEntry.mode,
                                    )
                                }
                            }
                            .padding(vertical = 10.dp),
                    )
                }
            }
        }
    }
}
