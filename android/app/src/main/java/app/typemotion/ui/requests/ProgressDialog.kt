package app.typemotion.ui.requests

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.DialogProperties
import app.typemotion.state.AppState
import app.typemotion.state.ProgressState

/** The progress of an automation script, with what it writes to its log */
@Composable
fun ProgressDialog(app: AppState, progress: ProgressState) {
    // The script writes pieces of text, as to the desktop dialog's text box
    val log = progress.log.joinToString("")
    val scroll = rememberScrollState()
    LaunchedEffect(log.length) { scroll.animateScrollTo(scroll.maxValue) }
    AlertDialog(
        // A running script can only be cancelled
        onDismissRequest = { if (progress.finished) app.progress = null },
        properties = DialogProperties(dismissOnBackPress = progress.finished, dismissOnClickOutside = false),
        title = { Text(progress.title) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                if (progress.message.isNotEmpty()) Text(progress.message, style = MaterialTheme.typography.bodyMedium)
                if (progress.indeterminate && !progress.finished)
                    LinearProgressIndicator(Modifier.fillMaxWidth())
                else
                    LinearProgressIndicator(progress = { progress.percent / 100f }, modifier = Modifier.fillMaxWidth())
                if (log.isNotEmpty()) {
                    Text(
                        log.trimEnd('\n'),
                        style = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace),
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = 280.dp)
                            .background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(8.dp))
                            .verticalScroll(scroll)
                            .padding(8.dp),
                    )
                }
            }
        },
        confirmButton = {
            if (progress.finished) {
                TextButton(onClick = { app.progress = null }) { Text("Cerrar") }
            } else {
                TextButton(
                    onClick = {
                        progress.cancelling = true
                        app.engine.cancel()
                    },
                    enabled = !progress.cancelling,
                ) { Text(if (progress.cancelling) "Cancelando…" else "Cancelar") }
            }
        },
    )
}
