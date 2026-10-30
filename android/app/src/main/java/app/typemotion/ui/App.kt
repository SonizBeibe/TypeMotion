package app.typemotion.ui

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import app.typemotion.platform.BackHandler
import app.typemotion.platform.LocalPlatform
import app.typemotion.state.AppState
import app.typemotion.state.EngineStatus
import app.typemotion.state.Screen
import app.typemotion.ui.automation.AutomationScreen
import app.typemotion.ui.automation.MyAutomationsScreen
import app.typemotion.ui.editor.EditorScreen
import app.typemotion.ui.files.FilePickerDialog
import app.typemotion.ui.home.HomeScreen
import app.typemotion.ui.requests.ProgressDialog
import app.typemotion.ui.requests.RequestHost
import app.typemotion.ui.settings.SettingsScreen
import app.typemotion.ui.styles.StylesScreen
import app.typemotion.ui.theme.TypeMotionAppTheme
import app.typemotion.ui.tools.PropertiesScreen
import app.typemotion.ui.tools.SearchScreen
import app.typemotion.ui.tools.SelectLinesScreen
import app.typemotion.ui.tools.ShiftTimesScreen

/** The whole app: the current screen, and what the engine shows over it */
@Composable
fun TypeMotionApp(app: AppState) {
    val dark = when (app.theme) {
        "light" -> false
        "system" -> isSystemInDarkTheme()
        else -> true
    }
    CompositionLocalProvider(LocalPlatform provides app.platform) {
        TypeMotionAppTheme(darkTheme = dark) {
            Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                val crashReport = app.crashReport
                if (crashReport != null) CrashReportScreen(app, crashReport)
                else when (app.status) {
                    EngineStatus.Starting -> StartingScreen()
                    EngineStatus.Failed -> FailedScreen(app)
                    EngineStatus.Running -> RunningApp(app)
                }
            }
        }
    }
}

@Composable
private fun RunningApp(app: AppState) {
    BackHandler(enabled = app.backStack.size > 1) { app.back() }
    Box(Modifier.fillMaxSize()) {
        when (val screen = app.screen) {
            Screen.Home -> HomeScreen(app)
            Screen.Editor -> EditorScreen(app)
            Screen.Automation -> AutomationScreen(app)
            Screen.MyAutomations -> MyAutomationsScreen(app)
            is Screen.Styles -> StylesScreen(app, screen.editStyle)
            Screen.Properties -> PropertiesScreen(app)
            Screen.ShiftTimes -> ShiftTimesScreen(app)
            Screen.SelectLines -> SelectLinesScreen(app)
            is Screen.Search -> SearchScreen(app, screen.replace)
            Screen.Settings -> SettingsScreen(app)
        }

        if (app.busy) {
            LinearProgressIndicator(Modifier.fillMaxWidth().align(Alignment.TopCenter).safeDrawingPadding())
        }
        SnackbarHost(app.snackbar, Modifier.align(Alignment.BottomCenter).safeDrawingPadding())
    }

    // A script's questions go over its progress, and its file dialogs over both
    app.progress?.let { ProgressDialog(app, it) }
    RequestHost(app)
    app.filePicker?.let { FilePickerDialog(app, it) }

    app.errorMessage?.let { message ->
        AlertDialog(
            onDismissRequest = { app.errorMessage = null },
            title = { Text("Error") },
            text = { Text(message, modifier = Modifier.verticalScroll(rememberScrollState())) },
            confirmButton = { TextButton(onClick = { app.errorMessage = null }) { Text("Aceptar") } },
        )
    }
}

@Composable
private fun StartingScreen() {
    Column(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(16.dp, Alignment.CenterVertically),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text("TypeMotion", style = MaterialTheme.typography.titleLarge)
        CircularProgressIndicator()
    }
}

/**
 * What made the app close the last time, to copy and send to whoever can fix
 * it, before the engine starts again
 */
@Composable
private fun CrashReportScreen(app: AppState, report: String) {
    Column(
        modifier = Modifier.fillMaxSize().safeDrawingPadding().padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("TypeMotion se cerró la última vez", style = MaterialTheme.typography.titleMedium)
        Text(
            "Si vuelve a pasar, copia este informe y mándaselo a quien te pasó la app: dice qué falló.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.secondary,
        )
        Surface(
            color = MaterialTheme.colorScheme.surfaceVariant,
            shape = MaterialTheme.shapes.small,
            modifier = Modifier.weight(1f).fillMaxWidth(),
        ) {
            SelectionContainer {
                Text(
                    report,
                    style = MaterialTheme.typography.bodySmall,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier
                        .verticalScroll(rememberScrollState())
                        .horizontalScroll(rememberScrollState())
                        .padding(10.dp),
                )
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            OutlinedButton(onClick = { app.platform.setClipboardText(report) }, modifier = Modifier.weight(1f)) {
                Text("Copiar informe")
            }
            Button(onClick = { app.dismissCrashReport() }, modifier = Modifier.weight(1f)) {
                Text("Continuar")
            }
        }
    }
}

@Composable
private fun FailedScreen(app: AppState) {
    Column(
        modifier = Modifier.fillMaxSize().safeDrawingPadding().padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp, Alignment.CenterVertically),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text("TypeMotion no pudo iniciar", style = MaterialTheme.typography.titleMedium)
        Text(app.startError, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
        Button(onClick = { app.start() }) { Text("Reintentar") }
    }
}
