package app.typemotion.platform

import androidx.compose.runtime.Composable

/** The phone's back button and gesture (Android), or nothing (iPhone) */
@Composable
expect fun BackHandler(enabled: Boolean = true, onBack: () -> Unit)
