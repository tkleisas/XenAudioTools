// SPDX-License-Identifier: GPL-3.0-or-later
package gr.tkleisas.upicstudio

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import java.io.File
import java.util.Locale

data class ProgramEntry(
    val id: String,
    val name: String,
    val builtIn: Boolean,
    val file: File? = null,
)

private const val kPresetAssetDir = "presets"
private const val kProgramExtension = ".xenpage"

fun builtInPrograms(context: Context): List<ProgramEntry> {
    val files = runCatching {
        context.assets.list(kPresetAssetDir)?.toList().orEmpty()
    }.getOrDefault(emptyList())
    return files
        .filter { it.endsWith(kProgramExtension) }
        .sorted()
        .map { fileName ->
            val text = runCatching {
                context.assets.open("$kPresetAssetDir/$fileName")
                    .bufferedReader().use { it.readText() }
            }.getOrNull()
            val name = text?.let {
                runCatching { UpicPage.parse(it).name }.getOrNull()
            } ?: fileName.removeSuffix(kProgramExtension)
            ProgramEntry(fileName, name, builtIn = true)
        }
}

fun savedPrograms(context: Context): List<ProgramEntry> {
    val dir = File(context.filesDir, "programs")
    if (!dir.isDirectory) return emptyList()
    return dir.listFiles()
        ?.filter { it.isFile && it.name.endsWith(kProgramExtension) }
        ?.sortedByDescending { it.lastModified() }
        ?.map {
            ProgramEntry(
                id = "saved:${it.name}",
                name = it.name.removeSuffix(kProgramExtension),
                builtIn = false,
                file = it,
            )
        }
        .orEmpty()
}

fun readProgram(context: Context, entry: ProgramEntry): String? = runCatching {
    if (entry.builtIn) {
        context.assets.open("$kPresetAssetDir/${entry.id}")
            .bufferedReader().use { it.readText() }
    } else {
        entry.file?.readText()
    }
}.getOrNull()

fun saveProgram(context: Context, name: String, text: String): ProgramEntry? {
    val safe = name.trim()
        .replace(Regex("[^A-Za-z0-9._ -]"), "_")
        .take(64)
        .ifBlank { "Untitled" }
    val dir = File(context.filesDir, "programs")
    if (!dir.isDirectory && !dir.mkdirs()) return null
    val file = File(dir, safe + kProgramExtension)
    return runCatching {
        file.writeText(text)
        ProgramEntry("saved:${file.name}", safe, builtIn = false, file = file)
    }.getOrNull()
}

fun deleteProgram(entry: ProgramEntry): Boolean {
    if (entry.builtIn) return false
    return runCatching { entry.file?.delete() == true }.getOrDefault(false)
}

fun currentPageFile(context: Context): File =
    File(context.filesDir, "current" + kProgramExtension)

fun readCurrentPage(context: Context): String? =
    runCatching { currentPageFile(context).takeIf { it.isFile }?.readText() }
        .getOrNull()

fun writeCurrentPage(context: Context, text: String) {
    runCatching { currentPageFile(context).writeText(text) }
}

@Composable
fun ProgramsDialog(
    context: Context,
    currentName: String,
    currentText: String,
    onLoad: (String) -> Unit,
    onClose: () -> Unit,
    onStatus: (String) -> Unit,
) {
    val builtIns = remember { builtInPrograms(context) }
    var saved by remember { mutableStateOf(savedPrograms(context)) }
    var saveName by remember {
        mutableStateOf(currentName.ifBlank { "My Program" })
    }

    Dialog(
        onDismissRequest = onClose,
        properties = DialogProperties(usePlatformDefaultWidth = false),
    ) {
        Surface(
            color = Color(0xFF0E1D24),
            shape = RoundedCornerShape(18.dp),
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(0.88f)
                .padding(16.dp),
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(16.dp),
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        "Programs",
                        fontWeight = FontWeight.Bold,
                        fontSize = 20.sp,
                    )
                    Spacer(Modifier.weight(1f))
                    TextButton(onClick = onClose) { Text("Close") }
                }
                HorizontalDivider(color = Color(0xFF17313A))
                LazyColumn(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                    verticalArrangement = Arrangement.spacedBy(2.dp),
                ) {
                    item {
                        SectionLabel("MY PROGRAMS")
                    }
                    if (saved.isEmpty()) {
                        item {
                            Text(
                                "No saved programs yet.",
                                color = Color(0xFF59747C),
                                fontSize = 12.sp,
                                modifier = Modifier.padding(vertical = 8.dp),
                            )
                        }
                    }
                    items(saved, key = { it.id }) { entry ->
                        ProgramRow(
                            entry = entry,
                            onLoad = {
                                readProgram(context, entry)?.let(onLoad)
                            },
                            onDelete = {
                                if (deleteProgram(entry)) {
                                    saved = savedPrograms(context)
                                    onStatus("Deleted ${entry.name}")
                                }
                            },
                        )
                    }
                    item {
                        SectionLabel("BUILT-IN")
                    }
                    items(builtIns, key = { it.id }) { entry ->
                        ProgramRow(
                            entry = entry,
                            onLoad = {
                                readProgram(context, entry)?.let(onLoad)
                            },
                        )
                    }
                }
                HorizontalDivider(color = Color(0xFF17313A))
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    OutlinedTextField(
                        value = saveName,
                        onValueChange = { saveName = it },
                        label = { Text("Save current as program") },
                        singleLine = true,
                        modifier = Modifier.weight(1f),
                    )
                    Button(onClick = {
                        val entry = saveProgram(
                            context,
                            saveName,
                            currentText,
                        )
                        if (entry != null) {
                            saved = savedPrograms(context)
                            onStatus("Saved program ${entry.name}")
                        } else {
                            onStatus("Could not save program")
                        }
                    }) {
                        Text("Save")
                    }
                }
            }
        }
    }
}

@Composable
private fun SectionLabel(text: String) {
    Text(
        text,
        color = Color(0xFF4EC9B0),
        fontSize = 10.sp,
        fontWeight = FontWeight.Bold,
        modifier = Modifier.padding(top = 10.dp, bottom = 4.dp),
    )
}

@Composable
private fun ProgramRow(
    entry: ProgramEntry,
    onLoad: () -> Unit,
    onDelete: (() -> Unit)? = null,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onLoad)
            .padding(vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(entry.name, color = Color(0xFFD8E4E7), fontSize = 14.sp)
            Text(
                if (entry.builtIn) "Built-in page" else "Saved page",
                color = Color(0xFF59747C),
                fontSize = 10.sp,
            )
        }
        Text("Load", color = Color(0xFF4EC9B0), fontSize = 11.sp)
        if (onDelete != null) {
            TextButton(onClick = onDelete) {
                Text("Delete", color = Color(0xFFFFB454), fontSize = 11.sp)
            }
        }
    }
}

@Composable
fun ArcBrowserDialog(state: UpicState, onClose: () -> Unit) {
    Dialog(
        onDismissRequest = onClose,
        properties = DialogProperties(usePlatformDefaultWidth = false),
    ) {
        Surface(
            color = Color(0xFF0E1D24),
            shape = RoundedCornerShape(18.dp),
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(0.88f)
                .padding(16.dp),
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(16.dp),
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        "Arcs (${state.page.arcs.size})",
                        fontWeight = FontWeight.Bold,
                        fontSize = 20.sp,
                    )
                    Spacer(Modifier.weight(1f))
                    TextButton(onClick = onClose) { Text("Close") }
                }
                Text(
                    "Tap an arc to select it, or delete it directly.",
                    color = Color(0xFF59747C),
                    fontSize = 11.sp,
                )
                HorizontalDivider(
                    color = Color(0xFF17313A),
                    modifier = Modifier.padding(vertical = 8.dp),
                )
                LazyColumn(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                ) {
                    itemsIndexed(state.page.arcs) { index, arc ->
                        val selected = index == state.selectedArc
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .background(
                                    if (selected) Color(0xFF17313A)
                                    else Color.Transparent,
                                )
                                .clickable {
                                    state.selectedArc = index
                                    state.status =
                                        "Arc ${index + 1} selected"
                                    onClose()
                                }
                                .padding(vertical = 7.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    "Arc ${index + 1}  ·  " +
                                        waveformName(arc.waveform),
                                    color = if (selected) Color.White
                                    else Color(0xFFD8E4E7),
                                    fontSize = 13.sp,
                                )
                                Text(
                                    String.format(
                                        Locale.US,
                                        "%.2f – %.2f s  ·  group %d",
                                        arc.start,
                                        arc.end,
                                        arc.group + 1,
                                    ),
                                    color = Color(0xFF59747C),
                                    fontSize = 10.sp,
                                )
                            }
                            TextButton(onClick = {
                                state.deleteArc(index)
                            }) {
                                Text(
                                    "Delete",
                                    color = Color(0xFFFFB454),
                                    fontSize = 11.sp,
                                )
                            }
                        }
                    }
                }
                HorizontalDivider(color = Color(0xFF17313A))
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    TextButton(onClick = {
                        state.selectedArc = -1
                        state.status = "Selection cleared"
                        onClose()
                    }) { Text("Deselect") }
                    Spacer(Modifier.weight(1f))
                    if (state.selectedArc in state.page.arcs.indices) {
                        Button(onClick = {
                            state.deleteSelected()
                            onClose()
                        }) { Text("Delete selected") }
                    }
                }
            }
        }
    }
}
