package app.typemotion.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Immutable
data class TypeMotionExtendedColors(
    val panelBackground: Color,
    val panelHeader: Color,
    val gridRowEven: Color,
    val gridRowOdd: Color,
    val gridRowSelected: Color,
    val gridRowComment: Color,
    val gridRowFold: Color,
    val assTagHighlight: Color,
    val audioWaveformBg: Color,
    val audioWaveFill: Color,
    val audioStartMarker: Color,
    val audioEndMarker: Color,
    val audioSyllableMarker: Color,
    val visualToolVector: Color,
    val statusSuccess: Color,
    val statusError: Color
)

@Immutable
data class TypeMotionDimens(
    val minTouchTarget: Dp = 48.dp,
    val gridRowHeight: Dp = 44.dp,
    val panelCornerRadius: Dp = 10.dp,
    val panelSpacing: Dp = 6.dp,
    val compactPadding: Dp = 8.dp
)

val LocalTypeMotionColors = staticCompositionLocalOf {
    TypeMotionExtendedColors(
        panelBackground = Color.Unspecified,
        panelHeader = Color.Unspecified,
        gridRowEven = Color.Unspecified,
        gridRowOdd = Color.Unspecified,
        gridRowSelected = Color.Unspecified,
        gridRowComment = Color.Unspecified,
        gridRowFold = Color.Unspecified,
        assTagHighlight = Color.Unspecified,
        audioWaveformBg = Color.Unspecified,
        audioWaveFill = Color.Unspecified,
        audioStartMarker = Color.Unspecified,
        audioEndMarker = Color.Unspecified,
        audioSyllableMarker = Color.Unspecified,
        visualToolVector = Color.Unspecified,
        statusSuccess = Color.Unspecified,
        statusError = Color.Unspecified
    )
}

val LocalTypeMotionDimens = staticCompositionLocalOf { TypeMotionDimens() }

private val DarkColorScheme = darkColorScheme(
    primary = Color(0xFF818CF8),
    onPrimary = Color(0xFF0F111A),
    primaryContainer = Color(0xFF312E81),
    onPrimaryContainer = Color(0xFFE0E7FF),
    secondary = Color(0xFF22D3EE),
    onSecondary = Color(0xFF083344),
    secondaryContainer = Color(0xFF164E63),
    onSecondaryContainer = Color(0xFFCFFAFE),
    tertiary = Color(0xFFF472B6),
    background = Color(0xFF0D0F14),
    onBackground = Color(0xFFE2E8F0),
    surface = Color(0xFF151922),
    onSurface = Color(0xFFE2E8F0),
    surfaceVariant = Color(0xFF1E2430),
    onSurfaceVariant = Color(0xFF94A3B8),
    outline = Color(0xFF334155),
    error = Color(0xFFF87171)
)

private val LightColorScheme = lightColorScheme(
    primary = Color(0xFF4F46E5),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFE0E7FF),
    onPrimaryContainer = Color(0xFF1E1B4B),
    secondary = Color(0xFF0891B2),
    onSecondary = Color.White,
    secondaryContainer = Color(0xFFCFFAFE),
    onSecondaryContainer = Color(0xFF083344),
    tertiary = Color(0xFFDB2777),
    background = Color(0xFFF1F5F9),
    onBackground = Color(0xFF0F172A),
    surface = Color(0xFFFFFFFF),
    onSurface = Color(0xFF0F172A),
    surfaceVariant = Color(0xFFE2E8F0),
    onSurfaceVariant = Color(0xFF475569),
    outline = Color(0xFFCBD5E1),
    error = Color(0xFFDC2626)
)

private val DarkExtendedColors = TypeMotionExtendedColors(
    panelBackground = Color(0xFF141821),
    panelHeader = Color(0xFF1C2230),
    gridRowEven = Color(0xFF12161F),
    gridRowOdd = Color(0xFF171C26),
    gridRowSelected = Color(0xFF283152),
    gridRowComment = Color(0xFF1A1E26),
    gridRowFold = Color(0xFF2A1B3D),
    assTagHighlight = Color(0xFFFBBF24),
    audioWaveformBg = Color(0xFF090C10),
    audioWaveFill = Color(0xFF38BDF8),
    audioStartMarker = Color(0xFF22C55E),
    audioEndMarker = Color(0xFFEF4444),
    audioSyllableMarker = Color(0xFFFACC15),
    visualToolVector = Color(0xFFEC4899),
    statusSuccess = Color(0xFF34D399),
    statusError = Color(0xFFF87171)
)

private val LightExtendedColors = TypeMotionExtendedColors(
    panelBackground = Color(0xFFFFFFFF),
    panelHeader = Color(0xFFE2E8F0),
    gridRowEven = Color(0xFFFFFFFF),
    gridRowOdd = Color(0xFFF8FAFC),
    gridRowSelected = Color(0xFFDDE7FF),
    gridRowComment = Color(0xFFE2E8F0),
    gridRowFold = Color(0xFFF3E8FF),
    assTagHighlight = Color(0xFFD97706),
    audioWaveformBg = Color(0xFF0F172A),
    audioWaveFill = Color(0xFF38BDF8),
    audioStartMarker = Color(0xFF16A34A),
    audioEndMarker = Color(0xFFDC2626),
    audioSyllableMarker = Color(0xFFEAB308),
    visualToolVector = Color(0xFFDB2777),
    statusSuccess = Color(0xFF059669),
    statusError = Color(0xFFDC2626)
)

val TypeMotionTypography = Typography(
    titleLarge = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.Bold, fontSize = 18.sp),
    titleMedium = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.SemiBold, fontSize = 15.sp),
    titleSmall = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.SemiBold, fontSize = 13.sp),
    bodyMedium = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.Normal, fontSize = 13.sp),
    bodySmall = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.Normal, fontSize = 11.sp),
    labelMedium = TextStyle(fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Medium, fontSize = 12.sp),
    labelSmall = TextStyle(fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Normal, fontSize = 11.sp)
)

val TypeMotionShapes = Shapes(
    extraSmall = RoundedCornerShape(4.dp),
    small = RoundedCornerShape(6.dp),
    medium = RoundedCornerShape(10.dp),
    large = RoundedCornerShape(14.dp)
)

object TypeMotionTheme {
    val colors: TypeMotionExtendedColors
        @Composable get() = LocalTypeMotionColors.current
    val dimens: TypeMotionDimens
        @Composable get() = LocalTypeMotionDimens.current
}

@Composable
fun TypeMotionAppTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    val colorScheme = if (darkTheme) DarkColorScheme else LightColorScheme
    val extendedColors = if (darkTheme) DarkExtendedColors else LightExtendedColors

    CompositionLocalProvider(
        LocalTypeMotionColors provides extendedColors,
        LocalTypeMotionDimens provides TypeMotionDimens()
    ) {
        MaterialTheme(
            colorScheme = colorScheme,
            typography = TypeMotionTypography,
            shapes = TypeMotionShapes,
            content = content
        )
    }
}
