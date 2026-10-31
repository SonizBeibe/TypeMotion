package app.typemotion.platform

import androidx.compose.runtime.Composable

/** The iPhone has no back button; the screens have their own back arrows */
@Composable
actual fun BackHandler(enabled: Boolean, onBack: () -> Unit) {
}
