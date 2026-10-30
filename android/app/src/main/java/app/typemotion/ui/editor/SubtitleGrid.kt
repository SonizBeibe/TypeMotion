package app.typemotion.ui.editor

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.MoreHoriz
import androidx.compose.material3.AssistChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.typemotion.engine.EngineError
import app.typemotion.model.AssTime
import app.typemotion.model.Line
import app.typemotion.state.AppState
import app.typemotion.ui.theme.TypeMotionTheme
import org.json.JSONObject

/** Commands of the selection bar, from the desktop grid's context menu */
private val selectionActions = listOf(
    "Antes" to "subtitle/insert/before",
    "Después" to "subtitle/insert/after",
    "Duplicar" to "edit/line/duplicate",
    "Unir" to "edit/line/join/concatenate",
    "Unir (karaoke)" to "edit/line/join/as_karaoke",
    "Copiar" to "edit/line/copy",
    "Cortar" to "edit/line/cut",
    "Pegar" to "edit/line/paste",
    "Pegar sobre" to "edit/line/paste/over",
    "Crear pliegue" to "grid/fold/create",
    "Borrar" to "edit/line/delete",
)

/**
 * The subtitle grid. A tap selects a line like a click; a long press starts
 * selecting several, where taps add or take out lines like Ctrl+click and a
 * long press selects up to the line like Shift+click.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun SubtitleGrid(app: AppState, compact: Boolean, modifier: Modifier = Modifier) {
    var selecting by remember { mutableStateOf(false) }
    var showContextMenu by remember { mutableStateOf(false) }
    val visible by remember { derivedStateOf { app.lines.filter { it.visible } } }
    val hasFolds by remember { derivedStateOf { app.lines.any { it.fold.isNotBlank() } } }
    val listState = rememberLazyListState()

    LaunchedEffect(app) {
        app.scrollRequests.collect { row ->
            val index = visible.indexOfFirst { it.row >= row }
            if (index < 0) return@collect
            val shown = listState.layoutInfo.visibleItemsInfo
            if (shown.none { it.index == index && it.offset >= 0 })
                listState.animateScrollToItem((index - 2).coerceAtLeast(0))
        }
    }

    Surface(modifier = modifier, color = TypeMotionTheme.colors.panelBackground, shape = RoundedCornerShape(8.dp)) {
        Column(Modifier.fillMaxSize()) {
            if (selecting || app.selection.size > 1) {
                SelectionBar(
                    app,
                    onMore = { showContextMenu = true },
                    onClose = {
                        selecting = false
                        if (app.activeRow >= 0) app.selectOnly(app.activeRow)
                    },
                )
            }

            GridHeader(compact, hasFolds)

            LazyColumn(state = listState, modifier = Modifier.fillMaxSize()) {
                items(visible, key = { it.id }) { line ->
                    GridRow(
                        app = app,
                        line = line,
                        compact = compact,
                        showFolds = hasFolds,
                        selected = line.row in app.selection,
                        active = line.row == app.activeRow,
                        onClick = {
                            if (selecting) app.toggleSelected(line.row)
                            else app.selectOnly(line.row)
                        },
                        onLongClick = {
                            if (selecting) app.selectRange(line.row)
                            else {
                                selecting = true
                                if (line.row !in app.selection) app.toggleSelected(line.row)
                            }
                        },
                    )
                    HorizontalDivider(color = MaterialTheme.colorScheme.outline.copy(alpha = 0.25f))
                }
            }
        }
    }

    if (showContextMenu) {
        MenuSheet(app, rootMenu = "grid_context", title = "Líneas seleccionadas", onDismiss = { showContextMenu = false })
    }
}

@Composable
private fun SelectionBar(app: AppState, onMore: () -> Unit, onClose: () -> Unit) {
    var valid by remember { mutableStateOf(JSONObject()) }
    LaunchedEffect(app.selection, app.activeRow, app.fileInfo) {
        valid = try {
            app.commandState(selectionActions.map { it.second }).optJSONObject("valid") ?: JSONObject()
        } catch (e: EngineError) {
            JSONObject()
        }
    }
    Surface(color = MaterialTheme.colorScheme.primaryContainer, modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState())
                .padding(horizontal = 8.dp, vertical = 2.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            IconButton(onClick = onClose) {
                Icon(Icons.Default.Close, contentDescription = "Terminar selección")
            }
            Text(
                "${app.selection.size} sel.",
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onPrimaryContainer,
            )
            for ((label, command) in selectionActions) {
                AssistChip(
                    onClick = { app.runCommand(command) },
                    enabled = valid.optBoolean(command, true),
                    label = { Text(label, fontSize = 11.sp) },
                )
            }
            IconButton(onClick = onMore) {
                Icon(Icons.Default.MoreHoriz, contentDescription = "Más acciones")
            }
        }
    }
}

private val numberWidth = 38.dp
private val timeWidth = 72.dp
private val styleWidth = 74.dp
private val cpsWidth = 32.dp

@Composable
private fun GridHeader(compact: Boolean, showFolds: Boolean) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(TypeMotionTheme.colors.panelHeader)
            .padding(horizontal = 8.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val style = MaterialTheme.typography.labelSmall
        Text("#", Modifier.width(numberWidth), style = style)
        if (showFolds) Text(">", Modifier.width(18.dp), style = style)
        Text("Inicio", Modifier.width(timeWidth), style = style)
        Text("Fin", Modifier.width(timeWidth), style = style)
        if (!compact) Text("CPS", Modifier.width(cpsWidth), style = style)
        Text("Estilo", Modifier.width(styleWidth), style = style)
        if (!compact) {
            Text("Actor", Modifier.width(70.dp), style = style)
            Text("Efecto", Modifier.width(60.dp), style = style)
        }
        Text("Texto", Modifier.weight(1f), style = style)
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun GridRow(
    app: AppState,
    line: Line,
    compact: Boolean,
    showFolds: Boolean,
    selected: Boolean,
    active: Boolean,
    onClick: () -> Unit,
    onLongClick: () -> Unit,
) {
    val colors = TypeMotionTheme.colors
    val background = when {
        selected && line.comment -> colors.gridRowSelected.copy(alpha = 0.7f)
        selected -> colors.gridRowSelected
        line.comment -> colors.gridRowComment
        line.row % 2 == 0 -> colors.gridRowEven
        else -> colors.gridRowOdd
    }
    val textColor = if (line.comment) MaterialTheme.colorScheme.onSurfaceVariant else MaterialTheme.colorScheme.onSurface
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = TypeMotionTheme.dimens.gridRowHeight)
            .background(background)
            .combinedClickable(onClick = onClick, onLongClick = onLongClick),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        // The active line is marked like the desktop grid's border
        Box(
            Modifier
                .width(3.dp)
                .fillMaxHeight()
                .heightIn(min = TypeMotionTheme.dimens.gridRowHeight)
                .background(if (active) MaterialTheme.colorScheme.primary else Color.Transparent),
        )
        Row(Modifier.padding(start = 5.dp, end = 8.dp, top = 4.dp, bottom = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(
                (line.row + 1).toString(),
                modifier = Modifier.width(numberWidth),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            if (showFolds) {
                Text(
                    line.fold,
                    modifier = Modifier
                        .width(18.dp)
                        .let { if (line.foldOpener) it.clickable { app.toggleFold(line.row) } else it },
                    style = MaterialTheme.typography.labelSmall,
                    color = Color(0xFFA855F7),
                    maxLines = 1,
                )
            }
            Text(AssTime.format(line.start), Modifier.width(timeWidth), style = MaterialTheme.typography.labelSmall, color = textColor)
            Text(AssTime.format(line.end), Modifier.width(timeWidth), style = MaterialTheme.typography.labelSmall, color = textColor)
            if (!compact) {
                Text(
                    if (line.cps in 0..100) line.cps.toString() else "",
                    Modifier.width(cpsWidth),
                    style = MaterialTheme.typography.labelSmall,
                    color = textColor,
                )
            }
            Text(
                line.style,
                modifier = Modifier.width(styleWidth),
                style = MaterialTheme.typography.bodySmall,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                color = MaterialTheme.colorScheme.secondary,
            )
            if (!compact) {
                Text(line.actor, Modifier.width(70.dp), style = MaterialTheme.typography.bodySmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(line.effect, Modifier.width(60.dp), style = MaterialTheme.typography.bodySmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            Text(
                gridText(line.text, app.tagMode, app.tagChar, colors.assTagHighlight, textColor),
                modifier = Modifier.weight(1f),
                style = MaterialTheme.typography.bodyMedium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

/**
 * The text as the desktop grid's text column shows it: override blocks
 * shown, replaced by a character or hidden, and long text cut
 */
fun gridText(text: String, mode: Int, replaceChar: String, tagColor: Color, textColor: Color): AnnotatedString =
    buildAnnotatedString {
        var start = 0
        while (start < text.length) {
            val open = text.indexOf('{', start)
            if (open < 0) break
            withStyle(SpanStyle(color = textColor)) { append(text, start, open) }
            val close = text.indexOf('}', open)
            val end = if (close < 0) text.length else close + 1
            when (mode) {
                0 -> withStyle(SpanStyle(color = tagColor)) { append(text, open, end) }
                1 -> withStyle(SpanStyle(color = tagColor)) { append(replaceChar) }
            }
            start = end
        }
        if (start < text.length) withStyle(SpanStyle(color = textColor)) { append(text, start, text.length) }
    }.let { if (it.length > 512) it.subSequence(0, 512) + AnnotatedString("...") else it }
