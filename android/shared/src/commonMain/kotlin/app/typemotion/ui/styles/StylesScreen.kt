package app.typemotion.ui.styles

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.DriveFileMove
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.SwapVert
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.PrimaryTabRow
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import app.typemotion.engine.EngineError
import app.typemotion.engine.int
import app.typemotion.engine.str
import app.typemotion.engine.strings
import app.typemotion.model.AssColor
import app.typemotion.model.AssStyle
import app.typemotion.state.AppState
import app.typemotion.ui.common.ColorSwatch
import app.typemotion.ui.common.Dropdown
import app.typemotion.ui.common.ScreenTopBar
import app.typemotion.ui.common.formatNumber
import app.typemotion.ui.theme.TypeMotionTheme

/**
 * The Styles Manager: the script's styles and a catalog's, with the
 * desktop buttons to edit, copy, delete and move them and to copy them
 * between the two
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun StylesScreen(app: AppState, editStyle: String?) {
    var tab by remember { mutableIntStateOf(0) }
    var catalogs by remember { mutableStateOf(emptyList<String>()) }
    var catalog by remember { mutableStateOf("") }
    var catalogVersion by remember { mutableIntStateOf(0) }
    var scriptStyles by remember { mutableStateOf(emptyList<AssStyle>()) }
    var catalogStyles by remember { mutableStateOf(emptyList<AssStyle>()) }
    var scriptSelected by remember { mutableStateOf(emptySet<String>()) }
    var catalogSelected by remember { mutableStateOf(emptySet<String>()) }
    var editing by remember { mutableStateOf<StyleEditTarget?>(null) }
    /** The style the edit box asked to edit, whose editor opens once the styles are read */
    var pendingEdit by remember { mutableStateOf(editStyle) }
    /** Closing that editor goes back to the edit box, as only the desktop editor was shown */
    var openedFor by remember { mutableStateOf<String?>(null) }
    var newCatalog by remember { mutableStateOf(false) }

    LaunchedEffect(app.stylesVersion) {
        try {
            scriptStyles = AssStyle.list(app.call("styles"))
            scriptSelected = scriptSelected.filter { name -> scriptStyles.any { it.name == name } }.toSet()
            // From the edit box's button: straight to that style's editor
            pendingEdit?.let { name ->
                pendingEdit = null
                scriptStyles.firstOrNull { it.name == name }?.let { style ->
                    openedFor = name
                    editing = StyleEditTarget(style, null, style.name, isNew = false)
                }
            }
        } catch (e: EngineError) {
            app.errorMessage = e.message
        }
    }
    LaunchedEffect(catalogVersion) {
        try {
            val result = app.call("catalogs")
            catalogs = result.strings("catalogs")
            catalog = result.str("current")
        } catch (e: EngineError) {
            app.errorMessage = e.message
        }
    }
    LaunchedEffect(catalog, catalogVersion) {
        if (catalog.isEmpty()) return@LaunchedEffect
        try {
            catalogStyles = AssStyle.list(app.call("catalog_styles", "catalog" to catalog))
            catalogSelected = catalogSelected.filter { name -> catalogStyles.any { it.name == name } }.toSet()
        } catch (e: EngineError) {
            app.errorMessage = e.message
        }
    }

    val inCatalog = tab == 1
    val styles = if (inCatalog) catalogStyles else scriptStyles
    val selected = if (inCatalog) catalogSelected else scriptSelected
    val listCatalog = if (inCatalog) catalog else null

    fun setSelected(names: Set<String>) {
        if (inCatalog) catalogSelected = names else scriptSelected = names
    }

    fun changed() {
        if (inCatalog) catalogVersion++
    }

    fun edit(style: AssStyle) {
        editing = StyleEditTarget(style, listCatalog, style.name, isNew = false)
    }

    fun move(type: Int) = app.run {
        val names = styles.map { it.name }.filter { it in selected }
        val result = app.call("move_styles", "catalog" to listCatalog, "names" to names, "type" to type)
        val first = result.int("first", -1)
        val last = result.int("last", -1)
        changed()
        // The moved styles stay selected where they went, as in the desktop lists
        if (first >= 0 && type != 4) {
            val all = AssStyle.list(
                if (listCatalog != null) app.call("catalog_styles", "catalog" to listCatalog) else app.call("styles"),
            )
            setSelected(all.subList(first, (last + 1).coerceAtMost(all.size)).map { it.name }.toSet())
        }
    }

    Scaffold(
        topBar = {
            Column {
                ScreenTopBar(title = "Administrador de estilos", subtitle = app.fileInfo.displayName, onBack = { app.back() })
                PrimaryTabRow(selectedTabIndex = tab, containerColor = TypeMotionTheme.colors.panelHeader) {
                    Tab(selected = tab == 0, onClick = { tab = 0 }, text = { Text("Script actual (${scriptStyles.size})") })
                    Tab(selected = tab == 1, onClick = { tab = 1 }, text = { Text("Catálogo (${catalogStyles.size})") })
                }
            }
        },
        bottomBar = {
            StyleActions(
                selectedCount = selected.size,
                copyLabel = if (inCatalog) "Al script" else "Al catálogo",
                onNew = { editing = StyleEditTarget(AssStyle(), listCatalog, "", isNew = true) },
                onEdit = { styles.firstOrNull { it.name in selected }?.let(::edit) },
                onCopy = {
                    val source = styles.firstOrNull { it.name in selected } ?: return@StyleActions
                    app.run {
                        val name = app.call("copy_style_name", "catalog" to listCatalog, "name" to source.name).str("name")
                        editing = StyleEditTarget(source.copy(name = name), listCatalog, "", isNew = true)
                    }
                },
                onDelete = {
                    app.run {
                        app.call("delete_styles", "catalog" to listCatalog, "names" to selected.toList())
                        setSelected(emptySet())
                        changed()
                    }
                },
                onMove = ::move,
                onCopyOther = {
                    val names = styles.map { it.name }.filter { it in selected }
                    app.run {
                        if (inCatalog) {
                            app.call("copy_styles_to_script", "catalog" to catalog, "names" to names)
                            scriptSelected = names.toSet()
                        } else {
                            app.call("copy_styles_to_catalog", "catalog" to catalog, "names" to names)
                            catalogSelected = names.toSet()
                            catalogVersion++
                        }
                    }
                },
            )
        },
        containerColor = MaterialTheme.colorScheme.background,
    ) { padding ->
        Column(Modifier.fillMaxSize().padding(padding)) {
            if (inCatalog) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 6.dp),
                ) {
                    Dropdown("Catálogo", catalogs, catalog, Modifier.weight(1f)) { _, name ->
                        app.run {
                            app.call("set_catalog", "catalog" to name)
                            catalog = name
                            catalogSelected = emptySet()
                        }
                    }
                    OutlinedButton(onClick = { newCatalog = true }) { Text("Nuevo") }
                    OutlinedButton(
                        onClick = {
                            app.run {
                                app.call("delete_catalog", "catalog" to catalog)
                                catalogVersion++
                            }
                        },
                        enabled = catalogs.size > 1,
                    ) { Text("Borrar") }
                }
            }
            if (styles.isEmpty()) {
                Text("No hay estilos.", color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(16.dp))
            }
            LazyColumn(Modifier.fillMaxSize(), contentPadding = androidx.compose.foundation.layout.PaddingValues(vertical = 4.dp)) {
                items(styles, key = { it.name }) { style ->
                    StyleRow(
                        style = style,
                        selected = style.name in selected,
                        onToggle = { setSelected(if (style.name in selected) selected - style.name else selected + style.name) },
                        onEdit = { edit(style) },
                    )
                }
            }
        }
    }

    editing?.let { target ->
        StyleEditor(app, target) { saved ->
            editing = null
            if (saved && target.catalog != null) catalogVersion++
            if (openedFor != null) {
                openedFor = null
                app.back()
            }
        }
    }

    if (newCatalog) {
        var name by remember { mutableStateOf("") }
        AlertDialog(
            onDismissRequest = { newCatalog = false },
            title = { Text("Nuevo nombre de almacenamiento") },
            text = { OutlinedTextField(name, { name = it }, singleLine = true, label = { Text("Nombre del catálogo") }) },
            confirmButton = {
                TextButton(
                    onClick = {
                        newCatalog = false
                        app.run {
                            val result = app.call("new_catalog", "name" to name.trim())
                            if (result.optBoolean("created")) {
                                catalog = result.str("name")
                                catalogSelected = emptySet()
                                catalogVersion++
                            }
                        }
                    },
                    enabled = name.isNotBlank(),
                ) { Text("Crear") }
            },
            dismissButton = { TextButton(onClick = { newCatalog = false }) { Text("Cancelar") } },
        )
    }
}

@Composable
private fun StyleRow(style: AssStyle, selected: Boolean, onToggle: () -> Unit, onEdit: () -> Unit) {
    Surface(
        color = if (selected) TypeMotionTheme.colors.gridRowSelected else MaterialTheme.colorScheme.background,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Row(
            modifier = Modifier
                .clickable(onClick = onToggle)
                .heightIn(min = 56.dp)
                .padding(start = 4.dp, end = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Checkbox(checked = selected, onCheckedChange = { onToggle() })
            Column(Modifier.weight(1f)) {
                Text(style.name, style = MaterialTheme.typography.bodyMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                val flags = listOfNotNull(
                    "N".takeIf { style.bold }, "C".takeIf { style.italic },
                    "S".takeIf { style.underline }, "T".takeIf { style.strikeout },
                ).joinToString("")
                Text(
                    "${style.font} ${formatNumber(style.size)}" + (if (flags.isNotEmpty()) " • $flags" else "") + " • \\an${style.alignment}",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.secondary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            for (color in listOf(style.primary, style.secondary, style.outline, style.shadow)) {
                ColorSwatch(AssColor.parse(color) ?: AssColor(0, 0, 0), Modifier.padding(horizontal = 2.dp).size(20.dp))
            }
            IconButton(onClick = onEdit) { Icon(Icons.Default.Edit, contentDescription = "Editar") }
        }
    }
}

@Composable
private fun StyleActions(
    selectedCount: Int,
    copyLabel: String,
    onNew: () -> Unit,
    onEdit: () -> Unit,
    onCopy: () -> Unit,
    onDelete: () -> Unit,
    onMove: (Int) -> Unit,
    onCopyOther: () -> Unit,
) {
    var moveMenu by remember { mutableStateOf(false) }
    Surface(color = TypeMotionTheme.colors.panelHeader) {
        Row(Modifier.fillMaxWidth().navigationBarsPadding().padding(vertical = 4.dp)) {
            ActionButton(Icons.Default.Add, "Nuevo", true, Modifier.weight(1f), onNew)
            ActionButton(Icons.Default.Edit, "Editar", selectedCount == 1, Modifier.weight(1f), onEdit)
            ActionButton(Icons.Default.ContentCopy, "Copiar", selectedCount == 1, Modifier.weight(1f), onCopy)
            ActionButton(Icons.Default.Delete, "Borrar", selectedCount > 0, Modifier.weight(1f), onDelete)
            Box(Modifier.weight(1f)) {
                ActionButton(Icons.Default.SwapVert, "Mover", true, Modifier.fillMaxWidth()) { moveMenu = true }
                DropdownMenu(expanded = moveMenu, onDismissRequest = { moveMenu = false }) {
                    val moves = listOf("Subir", "Al principio", "Bajar", "Al final")
                    moves.forEachIndexed { type, label ->
                        DropdownMenuItem(
                            text = { Text(label) },
                            enabled = selectedCount > 0,
                            onClick = {
                                moveMenu = false
                                onMove(type)
                            },
                        )
                    }
                    DropdownMenuItem(text = { Text("Ordenar por nombre") }, onClick = {
                        moveMenu = false
                        onMove(4)
                    })
                }
            }
            ActionButton(Icons.AutoMirrored.Filled.DriveFileMove, copyLabel, selectedCount > 0, Modifier.weight(1f), onCopyOther)
        }
    }
}

@Composable
private fun ActionButton(icon: ImageVector, label: String, enabled: Boolean, modifier: Modifier, onClick: () -> Unit) {
    val alpha = if (enabled) 1f else 0.38f
    Column(
        modifier = modifier
            .clickable(enabled = enabled, onClick = onClick)
            .padding(vertical = 6.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Icon(icon, contentDescription = null, tint = MaterialTheme.colorScheme.onSurface.copy(alpha = alpha))
        Text(
            label,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurface.copy(alpha = alpha),
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}
