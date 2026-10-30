package app.typemotion.ui.editor

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.Redo
import androidx.compose.material.icons.automirrored.filled.Undo
import androidx.compose.material.icons.filled.AutoFixHigh
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.PrimaryTabRow
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.typemotion.platform.BackHandler
import app.typemotion.state.AppState
import app.typemotion.state.Screen
import app.typemotion.ui.theme.TypeMotionTheme

/**
 * The main window: the video, the edit box and the subtitle grid. Standing
 * up, the video is on top, the text field in the middle and the grid and
 * the line's fields share the bottom in tabs; lying down, the video is on
 * the left as on the desktop, with the rest beside it.
 */
@Composable
fun EditorScreen(app: AppState) {
    BackHandler { app.back() }
    var menuOpen by rememberSaveable { mutableStateOf(false) }
    // Kept here so that turning the phone keeps the tab
    var tab by rememberSaveable { mutableIntStateOf(0) }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        // On a phone the edit box's own undo and redo buttons are enough
        val wide = maxWidth >= 600.dp
        // A phone lying down has little height to spare for the bar
        val short = maxWidth > maxHeight && maxHeight < 480.dp
        Scaffold(
            topBar = { EditorTopBar(app, showUndo = wide, short = short, onMenu = { menuOpen = true }) },
            containerColor = MaterialTheme.colorScheme.background,
        ) { padding ->
            BoxWithConstraints(
                Modifier
                    .padding(padding)
                    .fillMaxSize()
                    .imePadding(),
            ) {
                if (maxWidth >= 600.dp && maxWidth > maxHeight) LandscapeEditor(app, tab, onTab = { tab = it })
                else PortraitEditor(app, maxVideoHeight = maxHeight * 0.38f, tab, onTab = { tab = it })
            }
        }
    }

    if (menuOpen) MainMenuSheet(app, onDismiss = { menuOpen = false })
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun EditorTopBar(app: AppState, showUndo: Boolean, short: Boolean, onMenu: () -> Unit) {
    val info = app.fileInfo
    TopAppBar(
        colors = TopAppBarDefaults.topAppBarColors(containerColor = TypeMotionTheme.colors.panelHeader),
        expandedHeight = if (short) 48.dp else TopAppBarDefaults.TopAppBarExpandedHeight,
        navigationIcon = {
            IconButton(onClick = { app.back() }) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Inicio")
            }
        },
        title = {
            Column {
                Text(
                    (if (info.modified) "* " else "") + info.displayName,
                    style = MaterialTheme.typography.titleMedium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Surface(color = MaterialTheme.colorScheme.primaryContainer, shape = RoundedCornerShape(4.dp)) {
                        Text(
                            "${info.resX}x${info.resY}",
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onPrimaryContainer,
                            maxLines = 1,
                            softWrap = false,
                            modifier = Modifier.padding(horizontal = 5.dp, vertical = 1.dp),
                        )
                    }
                    Spacer(Modifier.width(6.dp))
                    Text(
                        "${info.lines} líneas",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.secondary,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
        },
        actions = {
            if (showUndo) {
                IconButton(onClick = { app.undo() }, enabled = info.canUndo) {
                    Icon(Icons.AutoMirrored.Filled.Undo, contentDescription = "Deshacer ${info.undo}")
                }
                IconButton(onClick = { app.redo() }, enabled = info.canRedo) {
                    Icon(Icons.AutoMirrored.Filled.Redo, contentDescription = "Rehacer ${info.redo}")
                }
            }
            IconButton(onClick = { app.save() }) {
                Icon(Icons.Default.Save, contentDescription = "Guardar")
            }
            IconButton(onClick = { app.navigate(Screen.Automation) }) {
                Icon(Icons.Default.AutoFixHigh, contentDescription = "Automatizaciones")
            }
            IconButton(onClick = onMenu) {
                Icon(Icons.Default.Menu, contentDescription = "Menú")
            }
        },
    )
}

@Composable
private fun PortraitEditor(app: AppState, maxVideoHeight: Dp, tab: Int, onTab: (Int) -> Unit) {
    Column(Modifier.fillMaxSize()) {
        MediaPanel(app, Modifier.fillMaxWidth(), maxVideoHeight = maxVideoHeight)
        EditTextPanel(app, Modifier.fillMaxWidth())
        LineTabs(app, tab, onTab, Modifier.weight(1f).fillMaxWidth(), tabHeight = 48.dp)
    }
}

@Composable
private fun LandscapeEditor(app: AppState, tab: Int, onTab: (Int) -> Unit) {
    if (app.media.video != null && !app.media.hidden) {
        // The video takes the left as on the desktop, and the line and the
        // grid share the right as they do standing up, which is as wide
        Row(
            Modifier
                .fillMaxSize()
                .background(MaterialTheme.colorScheme.background)
                .padding(4.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            MediaPanel(app, Modifier.weight(0.45f).fillMaxHeight(), fillHeight = true)
            Column(Modifier.weight(0.55f).fillMaxHeight()) {
                EditTextPanel(app, Modifier.fillMaxWidth(), maxLines = 3)
                LineTabs(app, tab, onTab, Modifier.weight(1f).fillMaxWidth(), tabHeight = 36.dp)
            }
        }
        return
    }

    Column(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
        // The folded video's buttons or the audio's bar across the top
        MediaPanel(app, Modifier.fillMaxWidth())
        Row(
            Modifier
                .fillMaxWidth()
                .weight(0.55f)
                .padding(4.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            EditTextPanel(app, Modifier.weight(0.5f).fillMaxHeight(), fill = true)
            LineFieldsPanel(app, Modifier.weight(0.5f).fillMaxHeight())
        }
        SubtitleGrid(
            app,
            compact = false,
            modifier = Modifier
                .weight(0.45f)
                .fillMaxWidth()
                .padding(horizontal = 4.dp, vertical = 2.dp),
        )
    }
}

/** The grid and the line's fields, in tabs below the text */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun LineTabs(app: AppState, tab: Int, onTab: (Int) -> Unit, modifier: Modifier, tabHeight: Dp) {
    Column(modifier) {
        PrimaryTabRow(selectedTabIndex = tab, containerColor = TypeMotionTheme.colors.panelHeader) {
            for ((index, label) in listOf("Líneas (${app.lines.size})", "Tiempos y estilo").withIndex()) {
                Tab(selected = tab == index, onClick = { onTab(index) }, modifier = Modifier.height(tabHeight)) {
                    Text(label, style = MaterialTheme.typography.titleSmall, fontSize = 12.sp, maxLines = 1)
                }
            }
        }
        when (tab) {
            0 -> SubtitleGrid(app, compact = true, modifier = Modifier.weight(1f).fillMaxWidth())
            else -> LineFieldsPanel(app, Modifier.weight(1f).fillMaxWidth())
        }
    }
}
