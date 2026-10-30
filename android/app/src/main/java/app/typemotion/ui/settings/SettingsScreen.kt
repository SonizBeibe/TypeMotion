package app.typemotion.ui.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import app.typemotion.engine.EngineError
import app.typemotion.state.AppState
import app.typemotion.ui.common.CheckRow
import app.typemotion.ui.common.NumberField
import app.typemotion.ui.common.Panel
import app.typemotion.ui.common.RadioGroup
import app.typemotion.ui.common.SectionTitle
import app.typemotion.ui.common.ToolScreen
import app.typemotion.ui.theme.TypeMotionTheme
import java.io.File

private val themes = listOf("dark" to "Oscuro", "light" to "Claro", "system" to "Como el teléfono")

/**
 * The app's settings, and the desktop preferences which matter on the
 * phone; those are the engine's options, kept in its config.json
 */
@Composable
fun SettingsScreen(app: AppState) {
    ToolScreen(title = "Ajustes", onBack = { app.back() }) {
        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("Tema")
                RadioGroup(themes.map { it.second }, themes.indexOfFirst { it.first == app.theme }.coerceAtLeast(0)) {
                    app.setThemeSetting(themes[it].first)
                }
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionTitle("Archivos y carpeta TypeMotion")
                if (app.storageAccess) {
                    Text("TypeMotion tiene acceso a los archivos del teléfono.", style = MaterialTheme.typography.bodySmall)
                } else {
                    Text(
                        "TypeMotion no tiene acceso a los archivos del teléfono.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.error,
                    )
                    Button(onClick = { app.platform.requestStorageAccess() }) { Text("Dar acceso") }
                }
                FolderLine("Tus automatizaciones", app.userAutomationFolder)
                FolderLine("Módulos (include)", app.userIncludeFolder)
                FolderLine("Tus fuentes", File(app.platform.folders.storage, "fonts"))
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("Guardado automático")
                BoolOption(app, "App/Auto/Save", "Autoguardar")
                IntOption(app, "App/Auto/Save Every Seconds", "Cada cuántos segundos")
                BoolOption(app, "App/Auto/Save on Every Change", "Autoguardar con cada cambio")
                BoolOption(app, "App/Auto/Backup", "Copia del original al abrir")
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("Caja de edición y cuadrícula")
                IntOption(app, "Subtitle/Character Limit", "Límite de caracteres por línea")
                BoolOption(app, "Subtitle/Character Counter/Ignore Whitespace", "No contar espacios")
                BoolOption(app, "Subtitle/Character Counter/Ignore Punctuation", "No contar puntuación")
                IntOption(app, "Subtitle/Character Counter/CPS Warning Threshold", "CPS para advertencia")
                IntOption(app, "Subtitle/Character Counter/CPS Error Threshold", "CPS para error")
                StringOption(app, "Subtitle/Grid/Hide Overrides Char", "Carácter que reemplaza las etiquetas")
            }
        }

        Panel(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                SectionTitle("Acerca de")
                Text("TypeMotion para Android", style = MaterialTheme.typography.bodyMedium)
                Text(
                    "Basado en tu versión modificada de Aegisub, con el mismo motor de subtítulos y de automatización.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun FolderLine(label: String, folder: File) {
    Text(label, style = MaterialTheme.typography.labelMedium)
    Surface(color = MaterialTheme.colorScheme.surfaceVariant, shape = RoundedCornerShape(6.dp), modifier = Modifier.fillMaxWidth()) {
        Text(
            folder.path,
            style = MaterialTheme.typography.labelSmall.copy(fontFamily = FontFamily.Monospace),
            color = TypeMotionTheme.colors.assTagHighlight,
            modifier = Modifier.padding(8.dp),
        )
    }
}

@Composable
private fun BoolOption(app: AppState, name: String, label: String) {
    var value by remember { mutableStateOf<Boolean?>(null) }
    LaunchedEffect(name) {
        value = try {
            app.call("get_option", "name" to name).optBoolean("value")
        } catch (_: EngineError) {
            null
        }
    }
    CheckRow(label, value == true, enabled = value != null) { checked ->
        value = checked
        app.run { app.call("set_option", "name" to name, "value" to checked) }
    }
}

@Composable
private fun IntOption(app: AppState, name: String, label: String) {
    var value by remember { mutableStateOf<Int?>(null) }
    LaunchedEffect(name) {
        value = try {
            app.call("get_option", "name" to name).optInt("value")
        } catch (_: EngineError) {
            null
        }
    }
    NumberField(label, value ?: 0, Modifier.fillMaxWidth(), enabled = value != null) { number ->
        value = number.toInt()
        app.run { app.call("set_option", "name" to name, "value" to number.toInt()) }
    }
}

@Composable
private fun StringOption(app: AppState, name: String, label: String) {
    var value by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(name) {
        value = try {
            app.call("get_option", "name" to name).optString("value")
        } catch (_: EngineError) {
            null
        }
    }
    OutlinedTextField(
        value ?: "",
        { text ->
            value = text
            app.run { app.call("set_option", "name" to name, "value" to text) }
        },
        label = { Text(label) },
        singleLine = true,
        enabled = value != null,
        modifier = Modifier.fillMaxWidth(),
    )
}
