package app.typemotion.ui.files

import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.InsertDriveFile
import androidx.compose.material.icons.filled.ArrowUpward
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.CreateNewFolder
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import app.typemotion.io.File
import app.typemotion.platform.IoDispatcher
import app.typemotion.platform.fixed
import app.typemotion.platform.formatDateTime
import app.typemotion.platform.textComparator
import app.typemotion.state.AppState
import app.typemotion.state.FilePickerRequest
import app.typemotion.ui.common.Dropdown
import app.typemotion.ui.theme.TypeMotionTheme
import kotlinx.coroutines.withContext

/**
 * The app's own file dialog, which works on the phone's files directly,
 * as the engine opens and saves them by path
 */
@Composable
fun FilePickerDialog(app: AppState, request: FilePickerRequest) {
    val roots = remember { app.platform.storageRoots() }
    val start = remember {
        request.startDir?.takeIf { it.isDirectory }
            ?: roots.firstOrNull()?.second
            ?: File("/")
    }
    var dir by remember { mutableStateOf(start) }
    var filterIndex by remember { mutableIntStateOf(0) }
    var fileName by remember { mutableStateOf(request.fileName) }
    var checked by remember { mutableStateOf(emptySet<String>()) }
    var entries by remember { mutableStateOf<List<File>?>(null) }
    var reload by remember { mutableIntStateOf(0) }
    var confirmOverwrite by remember { mutableStateOf<File?>(null) }
    var newFolder by remember { mutableStateOf(false) }
    val filter = request.filters.getOrNull(filterIndex)

    LaunchedEffect(dir, filterIndex, reload) {
        entries = withContext(IoDispatcher) {
            val collator = textComparator()
            dir.listFiles()
                ?.filter { !it.name.startsWith(".") && (it.isDirectory || filter == null || filter.matches(it.name)) }
                ?.sortedWith(compareBy<File> { !it.isDirectory }.thenComparator { a, b -> collator.compare(a.name, b.name) })
        }
    }

    fun cancel() = request.onResult(null)

    fun save() {
        var name = fileName.trim()
        if (name.isEmpty()) return
        // Like the Windows file dialog, add the extension of the chosen filter
        val ext = filter?.patterns?.singleOrNull()?.takeIf { it.startsWith("*.") && it.indexOf('*', 1) < 0 }?.substring(1)
        if (ext != null && !name.contains('.')) name += ext
        val file = File(dir, name)
        if (file.exists() && request.promptOverwrite) confirmOverwrite = file
        else request.onResult(listOf(file.path))
    }

    Dialog(onDismissRequest = ::cancel, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(color = MaterialTheme.colorScheme.background, modifier = Modifier.fillMaxSize()) {
            Column(Modifier.safeDrawingPadding().imePadding()) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 4.dp),
                ) {
                    IconButton(onClick = ::cancel) { Icon(Icons.Default.Close, contentDescription = "Cancelar") }
                    Text(
                        request.title.ifEmpty { if (request.save) "Guardar" else "Abrir" },
                        style = MaterialTheme.typography.titleMedium,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    if (request.save) {
                        IconButton(onClick = { newFolder = true }) {
                            Icon(Icons.Default.CreateNewFolder, contentDescription = "Carpeta nueva")
                        }
                    }
                }

                if (roots.size > 1) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        modifier = Modifier.horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp),
                    ) {
                        for ((name, root) in roots) {
                            FilterChip(
                                selected = dir.path.startsWith(root.path),
                                onClick = { dir = root },
                                label = { Text(name) },
                            )
                        }
                    }
                }

                Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(horizontal = 4.dp)) {
                    IconButton(onClick = { dir.parentFile?.let { dir = it } }, enabled = dir.parentFile?.canRead() == true) {
                        Icon(Icons.Default.ArrowUpward, contentDescription = "Subir")
                    }
                    Text(
                        dir.path,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.secondary,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                }
                if (request.filters.size > 1) {
                    Dropdown(
                        label = "Tipo",
                        options = request.filters.map { it.name },
                        selected = filter?.name ?: "",
                        modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 4.dp),
                    ) { i, _ -> filterIndex = i }
                }
                HorizontalDivider()

                val list = entries
                Column(Modifier.weight(1f)) {
                    when {
                        !app.storageAccess -> Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            Text("TypeMotion no tiene acceso a los archivos del teléfono.")
                            Button(onClick = { app.platform.requestStorageAccess() }) { Text("Dar acceso") }
                        }
                        list == null -> Unit
                        list.isEmpty() -> Text(
                            "No hay archivos aquí.",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(16.dp),
                        )
                        else -> LazyColumn(Modifier.fillMaxSize()) {
                            items(list, key = { it.path }) { file ->
                                FileRow(
                                    file = file,
                                    checked = if (request.multiple && !file.isDirectory) file.path in checked else null,
                                    selected = request.save && file.name == fileName,
                                ) {
                                    when {
                                        file.isDirectory -> dir = file
                                        request.save -> fileName = file.name
                                        request.multiple -> checked = if (file.path in checked) checked - file.path else checked + file.path
                                        else -> request.onResult(listOf(file.path))
                                    }
                                }
                            }
                        }
                    }
                }

                HorizontalDivider()
                if (request.save) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        modifier = Modifier.fillMaxWidth().padding(12.dp),
                    ) {
                        OutlinedTextField(
                            fileName,
                            { fileName = it },
                            label = { Text("Nombre") },
                            singleLine = true,
                            modifier = Modifier.weight(1f),
                        )
                        Button(onClick = ::save, enabled = fileName.isNotBlank() && app.storageAccess) { Text("Guardar") }
                    }
                } else if (request.multiple) {
                    Row(Modifier.fillMaxWidth().padding(12.dp), horizontalArrangement = Arrangement.End) {
                        Button(onClick = { request.onResult(checked.toList()) }, enabled = checked.isNotEmpty()) {
                            Text("Abrir (${checked.size})")
                        }
                    }
                }
            }
        }
    }

    confirmOverwrite?.let { file ->
        AlertDialog(
            onDismissRequest = { confirmOverwrite = null },
            title = { Text("Reemplazar archivo") },
            text = { Text("${file.name} ya existe. ¿Quieres reemplazarlo?") },
            confirmButton = { TextButton(onClick = { request.onResult(listOf(file.path)) }) { Text("Reemplazar") } },
            dismissButton = { TextButton(onClick = { confirmOverwrite = null }) { Text("Cancelar") } },
        )
    }

    if (newFolder) {
        var name by remember { mutableStateOf("") }
        AlertDialog(
            onDismissRequest = { newFolder = false },
            title = { Text("Carpeta nueva") },
            text = { OutlinedTextField(name, { name = it }, singleLine = true, label = { Text("Nombre") }) },
            confirmButton = {
                TextButton(
                    onClick = {
                        val folder = File(dir, name.trim())
                        if (folder.mkdirs()) dir = folder
                        else reload++
                        newFolder = false
                    },
                    enabled = name.isNotBlank() && '/' !in name,
                ) { Text("Crear") }
            },
            dismissButton = { TextButton(onClick = { newFolder = false }) { Text("Cancelar") } },
        )
    }
}

@Composable
private fun FileRow(file: File, checked: Boolean?, selected: Boolean, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .heightIn(min = 52.dp)
            .padding(horizontal = 16.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            if (file.isDirectory) Icons.Default.Folder else Icons.AutoMirrored.Filled.InsertDriveFile,
            contentDescription = null,
            tint = if (file.isDirectory) TypeMotionTheme.colors.assTagHighlight else MaterialTheme.colorScheme.secondary,
        )
        Spacer(Modifier.width(14.dp))
        Column(Modifier.weight(1f)) {
            Text(
                file.name,
                style = MaterialTheme.typography.bodyMedium,
                color = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            if (!file.isDirectory) {
                Text(
                    "${formatSize(file.length())} • ${formatDateTime(file.lastModified())}",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        if (checked != null) Checkbox(checked = checked, onCheckedChange = { onClick() })
    }
}

private fun formatSize(bytes: Long): String = when {
    bytes < 1024 -> "$bytes B"
    bytes < 1024 * 1024 -> "${fixed(bytes / 1024.0, 1)} KB"
    else -> "${fixed(bytes / (1024.0 * 1024.0), 1)} MB"
}
