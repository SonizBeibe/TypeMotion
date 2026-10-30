package app.typemotion.platform

import androidx.compose.runtime.Composable

/** The phone's back button and gesture */
@Composable
fun BackHandler(enabled: Boolean = true, onBack: () -> Unit) {
    androidx.activity.compose.BackHandler(enabled, onBack)
}
