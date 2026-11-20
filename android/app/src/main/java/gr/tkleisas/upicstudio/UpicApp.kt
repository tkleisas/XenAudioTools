// SPDX-License-Identifier: GPL-3.0-or-later
package gr.tkleisas.upicstudio

import android.graphics.Paint
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import kotlinx.coroutines.delay
import java.util.Locale
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

private val Background = Color(0xFF071116)
private val Panel = Color(0xFF0E1D24)
private val PanelRaised = Color(0xFF17313A)
private val Grid = Color(0xFF24444E)
private val Accent = Color(0xFF4EC9B0)
private val Amber = Color(0xFFFFB454)
private val TextDim = Color(0xFF8CA6AD)
private val TextFaint = Color(0xFF59747C)
private val GroupColors = listOf(
    Color(0xFF4EC9B0),
    Color(0xFFFFB454),
    Color(0xFF70CE8E),
    Color(0xFFBE8EE8),
)

private val UpicScheme = darkColorScheme(
    primary = Accent,
    onPrimary = Color(0xFF00201A),
    secondary = Amber,
    onSecondary = Color(0xFF2A1700),
    background = Background,
    onBackground = Color(0xFFD8E4E7),
    surface = Panel,
    onSurface = Color(0xFFD8E4E7),
    surfaceVariant = PanelRaised,
    onSurfaceVariant = TextDim,
    outline = Color(0xFF2D525E),
)

@Composable
fun UpicTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = UpicScheme,
        typography = Typography(),
        content = content,
    )
}

enum class UpicTool(val label: String) {
    Draw("Draw"),
    Select("Select"),
    Erase("Erase"),
}

enum class GeneratorType(val label: String) {
    Sieve("Sieve"),
    Markov("Markov"),
    Cloud("Cloud"),
    Strategy("Strategy"),
}

private val WaveformNames = listOf("Sine", "Saw", "Square", "Triangle", "GENDYN")
private val SievePresetNames =
    listOf("Diatonic", "Chromatic", "Whole", "Messiaen", "Triad", "Octatonic")
private val DistributionNames =
    listOf("Uniform", "Gaussian", "Cauchy", "Logistic", "Exponential")

class UpicState {
    var page by mutableStateOf(UpicPage())
    var revision by mutableIntStateOf(0)
    var selectedArc by mutableIntStateOf(-1)
    var tool by mutableStateOf(UpicTool.Draw)
    var draftPoints by mutableStateOf<List<ArcPoint>>(emptyList())

    var generator by mutableStateOf(GeneratorType.Sieve)
    var duration by mutableFloatStateOf(16f)
    var lowIndex by mutableIntStateOf(36)
    var highIndex by mutableIntStateOf(96)
    var seed by mutableLongStateOf(1964L)
    var waveform by mutableIntStateOf(4)
    var sievePreset by mutableIntStateOf(0)
    var noteDuration by mutableFloatStateOf(0.25f)
    var markovStates by mutableIntStateOf(6)
    var cloudDensity by mutableFloatStateOf(24f)
    var strategyRows by mutableIntStateOf(3)
    var strategyCols by mutableIntStateOf(3)
    var replicator by mutableStateOf(false)

    var arcGroup by mutableIntStateOf(0)
    var arcWaveform by mutableIntStateOf(4)

    var playing by mutableStateOf(false)
    var loop by mutableStateOf(true)
    var speed by mutableFloatStateOf(1f)
    var masterDb by mutableFloatStateOf(-6f)
    var pageDb by mutableFloatStateOf(0f)
    var liveDb by mutableFloatStateOf(-10f)
    var playhead by mutableDoubleStateOf(0.0)
    var audioReady by mutableStateOf(false)
    var status by mutableStateOf("Starting audio…")

    var gendynSegments by mutableIntStateOf(12)
    var gendynDistribution by mutableIntStateOf(2)
    var gendynDurationStep by mutableFloatStateOf(0.45f)
    var gendynDurationBarrier by mutableFloatStateOf(0.45f)
    var gendynAmpStep by mutableFloatStateOf(0.18f)
    var gendynAmpBarrier by mutableFloatStateOf(0.20f)
    var gendynMirror by mutableStateOf(false)
    var gendynSeed by mutableLongStateOf(1964L)

    val undo = mutableStateListOf<String>()
    val canUndo: Boolean get() = undo.isNotEmpty()

    fun snapshot() {
        undo.add(page.toText())
        if (undo.size > 40) undo.removeAt(0)
    }

    fun syncPage() {
        if (!UpicEngine.nativeSetPage(page.toText())) {
            status = "The native engine rejected the page"
        }
    }

    fun generate() {
        if (highIndex <= lowIndex) {
            highIndex = min(127, lowIndex + 1)
        }
        val text = UpicEngine.nativeGeneratePage(
            generator.ordinal,
            duration.toDouble(),
            lowIndex,
            highIndex,
            seed,
            waveform,
            sievePreset,
            noteDuration.toDouble(),
            markovStates,
            cloudDensity.toDouble(),
            strategyRows,
            strategyCols,
            replicator,
        )
        try {
            snapshot()
            page = UpicPage.parse(text)
            revision++
            selectedArc = -1
            playing = false
            UpicEngine.nativeSetTransport(false, loop, speed)
            UpicEngine.nativeSetPlayhead(0.0)
            playhead = 0.0
            status = "${page.arcs.size} arcs · ${"%.1f".format(page.duration)} s"
        } catch (error: Exception) {
            status = "Generate failed: ${error.message}"
        }
    }

    fun loadText(text: String) {
        try {
            val loaded = UpicPage.parse(text)
            snapshot()
            page = loaded
            revision++
            selectedArc = -1
            playing = false
            UpicEngine.nativeSetTransport(false, loop, speed)
            UpicEngine.nativeSetPlayhead(0.0)
            playhead = 0.0
            syncPage()
            status = "Loaded ${page.name}"
        } catch (error: Exception) {
            status = "Open failed: ${error.message}"
        }
    }

    fun undoPage() {
        if (undo.isEmpty()) return
        try {
            val text = undo.removeAt(undo.lastIndex)
            page = UpicPage.parse(text)
            revision++
            selectedArc = -1
            syncPage()
            status = "Undo"
        } catch (error: Exception) {
            status = "Undo failed: ${error.message}"
        }
    }

    fun startDraft(time: Double, pitch: Double) {
        draftPoints = listOf(ArcPoint(time, pitch))
    }

    fun appendDraft(time: Double, pitch: Double) {
        val points = draftPoints
        if (points.isEmpty()) return
        val last = points.last()
        val dt = abs(time - last.time)
        val dp = abs(pitch - last.pitch)
        if (dt < 0.0008 && dp < 0.002) return
        draftPoints = points + ArcPoint(time, pitch)
    }

    fun finishDraft() {
        val points = draftPoints
        draftPoints = emptyList()
        if (points.size < 2) return
        if (abs(points.last().time - points.first().time) < 0.01) return
        snapshot()
        page.arcs.add(
            UpicArc(
                points = points.sortedBy { it.time }.toMutableList(),
                waveform = arcWaveform,
                group = arcGroup,
                weight = 0.55,
            ),
        )
        page.refreshDuration()
        revision++
        selectedArc = page.arcs.lastIndex
        syncPage()
        status = "Arc ${selectedArc + 1} added"
    }

    fun cancelDraft() {
        draftPoints = emptyList()
    }

    fun tapAt(time: Double, pitch: Double, tolerance: Double) {
        val candidates = page.arcs.indices.filter { index ->
            val arc = page.arcs[index]
            time >= arc.start - 0.01 && time <= arc.end + 0.01
        }
        val nearest = candidates.minByOrNull { index ->
            abs(page.arcs[index].pitchAt(time) - pitch)
        }
        if (nearest == null ||
            abs(page.arcs[nearest].pitchAt(time) - pitch) > tolerance) {
            if (tool == UpicTool.Select) selectedArc = -1
            return
        }
        if (tool == UpicTool.Erase) {
            snapshot()
            page.arcs.removeAt(nearest)
            selectedArc = -1
            revision++
            page.refreshDuration()
            syncPage()
            status = "Arc erased"
        } else {
            selectedArc = nearest
            status = "Arc ${nearest + 1} selected"
        }
    }

    fun deleteSelected() {
        if (selectedArc !in page.arcs.indices) return
        snapshot()
        page.arcs.removeAt(selectedArc)
        selectedArc = -1
        revision++
        page.refreshDuration()
        syncPage()
        status = "Arc deleted"
    }

    fun updateSelected(block: (UpicArc) -> Unit) {
        if (selectedArc !in page.arcs.indices) return
        block(page.arcs[selectedArc])
        revision++
        syncPage()
    }

    fun togglePlay() {
        if (playing) {
            playing = false
            UpicEngine.nativeSetTransport(false, loop, speed)
            UpicEngine.nativeAllNotesOff()
            status = "Stopped"
            return
        }
        if (playhead >= page.duration - 0.01) {
            UpicEngine.nativeSetPlayhead(0.0)
            playhead = 0.0
        }
        UpicEngine.nativeSetTransport(true, loop, speed)
        playing = true
        status = "Playing ${page.name}"
    }

    fun stop() {
        playing = false
        UpicEngine.nativeSetTransport(false, loop, speed)
        UpicEngine.nativeAllNotesOff()
    }

    fun applyTransport() {
        UpicEngine.nativeSetTransport(playing, loop, speed)
    }

    fun applyGains() {
        UpicEngine.nativeSetGains(masterDb, pageDb, liveDb)
    }

    fun applyGendyn() {
        UpicEngine.nativeSetGendyn(
            gendynSegments,
            gendynDistribution,
            gendynDurationStep,
            gendynDurationBarrier,
            gendynAmpStep,
            gendynAmpBarrier,
            gendynMirror,
            gendynSeed,
        )
    }

    fun liveNoteOn(note: Int) {
        if (!audioReady) return
        UpicEngine.nativeNoteOn(note, 0.8f)
    }

    fun liveNoteOff(note: Int) {
        UpicEngine.nativeNoteOff(note)
    }

    fun pauseForBackground() {
        stop()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun UpicApp() {
    val state = remember { UpicState() }
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current

    DisposableEffect(Unit) {
        UpicEngine.nativeCreate()
        val error = UpicEngine.start()
        state.audioReady = error == null
        if (error == null) {
            state.applyGains()
            state.applyGendyn()
        }
        state.generate()
        state.status = if (error == null) {
            "Audio ready · ${state.page.arcs.size} arcs"
        } else {
            error
        }
        onDispose {
            UpicEngine.nativeAllNotesOff()
            UpicEngine.nativeStop()
            UpicEngine.nativeDestroy()
        }
    }

    LaunchedEffect(Unit) {
        while (true) {
            state.playhead = UpicEngine.nativePlayhead()
            delay(33)
        }
    }

    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_STOP -> {
                    state.pauseForBackground()
                    UpicEngine.nativeStop()
                    state.audioReady = false
                    state.status = "Audio paused"
                }
                Lifecycle.Event.ON_START -> {
                    if (!UpicEngine.nativeIsRunning()) {
                        val error = UpicEngine.start()
                        state.audioReady = error == null
                        if (error != null) {
                            state.status = error
                        } else {
                            state.applyGains()
                            state.applyGendyn()
                            state.status = "Audio ready"
                        }
                    }
                }
                else -> Unit
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }

    val openLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        runCatching {
            context.contentResolver.openInputStream(uri)?.bufferedReader()
                ?.use { it.readText() }
        }.onSuccess { text ->
            if (text != null) state.loadText(text)
        }.onFailure { state.status = "Open failed: ${it.message}" }
    }

    val saveLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("application/x-xenpage"),
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        runCatching {
            context.contentResolver.openOutputStream(uri)?.bufferedWriter()
                ?.use { it.write(state.page.toText()) }
        }.onSuccess {
            state.status = "Saved ${state.page.name}.xenpage"
        }.onFailure { state.status = "Save failed: ${it.message}" }
    }

    Scaffold(
        containerColor = Background,
        contentWindowInsets = WindowInsets.safeDrawing,
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text("UPIC Studio", fontWeight = FontWeight.Bold)
                        Text(
                            state.page.name,
                            fontSize = 11.sp,
                            color = TextDim,
                        )
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = Background,
                    titleContentColor = Color(0xFFD8E4E7),
                ),
                actions = {
                    TextButton(onClick = {
                        openLauncher.launch(
                            arrayOf("application/x-xenpage", "text/plain",
                                "application/octet-stream"),
                        )
                    }) { Text("Open") }
                    TextButton(onClick = {
                        saveLauncher.launch(
                            "${state.page.name.ifBlank { "upic" }}.xenpage",
                        )
                    }) { Text("Save") }
                },
            )
        },
    ) { padding ->
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding),
        ) {
            val wide = maxWidth >= 700.dp
            if (wide) {
                Row(modifier = Modifier.fillMaxSize()) {
                    ScoreCanvas(
                        state = state,
                        modifier = Modifier
                            .weight(1.25f)
                            .fillMaxHeight(),
                    )
                    ControlsPanel(
                        state = state,
                        modifier = Modifier
                            .weight(0.85f)
                            .fillMaxHeight(),
                    )
                }
            } else {
                Column(modifier = Modifier.fillMaxSize()) {
                    ScoreCanvas(
                        state = state,
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1.05f),
                    )
                    ControlsPanel(
                        state = state,
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1.15f),
                    )
                }
            }
        }
    }
}

@Composable
private fun ScoreCanvas(state: UpicState, modifier: Modifier = Modifier) {
    val density = LocalDensity.current
    val labelPaint = remember {
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = android.graphics.Color.argb(180, 140, 166, 173)
            textSize = 26f
        }
    }
    val labelSize = with(density) { 10.sp.toPx() }
    val duration = max(0.1, state.page.duration)

    Column(
        modifier = modifier
            .background(Background)
            .padding(horizontal = 10.dp, vertical = 4.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            UpicTool.entries.forEach { tool ->
                FilterChip(
                    selected = state.tool == tool,
                    onClick = { state.tool = tool },
                    label = { Text(tool.label, fontSize = 11.sp) },
                )
            }
            Spacer(Modifier.weight(1f))
            Text(
                "${state.page.arcs.size} arcs · " +
                    String.format(Locale.US, "%.1f s", duration),
                fontSize = 10.sp,
                color = TextDim,
            )
        }

        Canvas(
            modifier = Modifier
                .fillMaxSize()
                .background(Panel)
                .pointerInput(state.tool, state.revision) {
                    val width = size.width.toFloat().coerceAtLeast(1f)
                    val height = size.height.toFloat().coerceAtLeast(1f)
                    fun timeAt(x: Float) =
                        (x / width).coerceIn(0f, 1f) * duration
                    fun pitchAt(y: Float) =
                        (1f - (y / height).coerceIn(0f, 1f)).toDouble()

                    if (state.tool == UpicTool.Draw) {
                        detectDragGestures(
                            onDragStart = { offset ->
                                state.startDraft(
                                    timeAt(offset.x),
                                    pitchAt(offset.y),
                                )
                            },
                            onDrag = { change, _ ->
                                state.appendDraft(
                                    timeAt(change.position.x),
                                    pitchAt(change.position.y),
                                )
                                change.consume()
                            },
                            onDragEnd = { state.finishDraft() },
                            onDragCancel = { state.cancelDraft() },
                        )
                    } else {
                        detectTapGestures { offset ->
                            state.tapAt(
                                timeAt(offset.x),
                                pitchAt(offset.y),
                                20.0 / height,
                            )
                        }
                    }
                },
        ) {
            drawRect(Panel)
            val width = size.width
            val height = size.height

            labelPaint.textSize = labelSize

            for (i in 0..4) {
                val x = width * i / 4f
                drawLine(Grid, Offset(x, 0f), Offset(x, height), 1f)
                val seconds = duration * i / 4.0
                drawContext.canvas.nativeCanvas.drawText(
                    String.format(Locale.US, "%.1fs", seconds),
                    if (i == 0) x + 4f else x - 24f,
                    height - 4f,
                    labelPaint,
                )
            }

            val table = state.page.table
            for (i in 0..4) {
                val y = height * i / 4f
                drawLine(Grid, Offset(0f, y), Offset(width, y), 1f)
                if (i == 4) continue
                val norm = 1.0 - i / 4.0
                drawContext.canvas.nativeCanvas.drawText(
                    formattedHz(table.normToHz(norm)),
                    4f,
                    if (i == 0) y + labelSize else y - 4f,
                    labelPaint,
                )
            }

            state.page.arcs.forEachIndexed { index, arc ->
                if (arc.points.size < 2) return@forEachIndexed
                val selected = index == state.selectedArc
                val color = GroupColors[arc.group.mod(GroupColors.size)]
                val path = Path()
                arc.points.forEachIndexed { pointIndex, point ->
                    val x = (point.time / duration).toFloat() * width
                    val y = (1.0 - point.pitch).toFloat() * height
                    if (pointIndex == 0) path.moveTo(x, y)
                    else path.lineTo(x, y)
                }
                drawPath(
                    path,
                    color = if (selected) Color.White else color,
                    style = Stroke(
                        width = (1.5f + arc.weight.toFloat() * 3.5f),
                    ),
                    alpha = if (arc.muted) 0.25f else 1f,
                )
            }

            if (state.draftPoints.size >= 2) {
                val path = Path()
                state.draftPoints.forEachIndexed { index, point ->
                    val x = (point.time / duration).toFloat() * width
                    val y = (1.0 - point.pitch).toFloat() * height
                    if (index == 0) path.moveTo(x, y)
                    else path.lineTo(x, y)
                }
                drawPath(
                    path,
                    color = Accent,
                    style = Stroke(width = 4f),
                )
            }

            val playX = (state.playhead / duration).toFloat() * width
            if (playX in 0f..width) {
                drawLine(Amber, Offset(playX, 0f), Offset(playX, height), 2f)
                drawCircle(Amber, 5f, Offset(playX, 0f))
            }

            drawRect(
                Color(0xFF2D525E),
                style = Stroke(width = 1f),
            )
        }
    }
}

@Composable
private fun ControlsPanel(state: UpicState, modifier: Modifier = Modifier) {
    state.revision // observe page mutations
    LazyColumn(
        modifier = modifier
            .background(Background)
            .padding(horizontal = 12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        item { TransportCard(state) }
        if (state.selectedArc in state.page.arcs.indices) {
            item { ArcCard(state) }
        }
        item { GeneratorCard(state) }
        item { GendynCard(state) }
        item {
            Text(
                state.status,
                color = TextDim,
                fontSize = 11.sp,
                modifier = Modifier.padding(bottom = 16.dp),
            )
        }
    }
}

@Composable
private fun TransportCard(state: UpicState) {
    UpicCard("TRANSPORT") {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Button(
                onClick = { state.togglePlay() },
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (state.playing) Amber else Accent,
                    contentColor = Background,
                ),
            ) {
                Text(if (state.playing) "Stop" else "Play")
            }
            Column {
                Text(
                    String.format(Locale.US, "%.2f s", state.playhead),
                    fontWeight = FontWeight.Bold,
                )
                Text("of ${"%.1f".format(state.page.duration)} s",
                    fontSize = 10.sp, color = TextDim)
            }
            Spacer(Modifier.weight(1f))
            Text("Loop", fontSize = 11.sp, color = TextDim)
            Switch(
                checked = state.loop,
                onCheckedChange = {
                    state.loop = it
                    state.applyTransport()
                },
            )
        }
        ControlSlider(
            "Speed",
            String.format(Locale.US, "%.2fx", state.speed),
            state.speed,
            0.25f..2f,
        ) {
            state.speed = it
            state.applyTransport()
        }
        ControlSlider(
            "Master",
            String.format(Locale.US, "%.1f dB", state.masterDb),
            state.masterDb,
            -30f..0f,
        ) {
            state.masterDb = it
            state.applyGains()
        }
    }
}

@Composable
private fun ArcCard(state: UpicState) {
    val index = state.selectedArc
    if (index !in state.page.arcs.indices) return
    val arc = state.page.arcs[index]
    UpicCard("ARC ${index + 1}") {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                "${String.format(Locale.US, "%.2f", arc.start)} – " +
                    "${String.format(Locale.US, "%.2f", arc.end)} s",
                fontSize = 11.sp,
                color = TextDim,
            )
            Spacer(Modifier.weight(1f))
            TextButton(onClick = { state.deleteSelected() }) {
                Text("Delete", color = Amber)
            }
        }
        ControlSlider(
            "Weight",
            String.format(Locale.US, "%.2f", arc.weight),
            arc.weight.toFloat(),
            0f..1f,
        ) { value ->
            state.updateSelected { selected -> selected.weight = value.toDouble() }
        }
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            WaveformNames.forEachIndexed { wave, name ->
                FilterChip(
                    selected = arc.waveform == wave,
                    onClick = {
                        state.updateSelected { it.waveform = wave }
                    },
                    label = { Text(name, fontSize = 10.sp) },
                )
            }
        }
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("Group", fontSize = 10.sp, color = TextDim)
            state.page.groups.indices.forEach { group ->
                FilterChip(
                    selected = arc.group == group,
                    onClick = { state.updateSelected { it.group = group } },
                    label = { Text("${group + 1}", fontSize = 10.sp) },
                )
            }
            Text("Mute", fontSize = 10.sp, color = TextDim)
            Switch(
                checked = arc.muted,
                onCheckedChange = { checked ->
                    state.updateSelected { selected -> selected.muted = checked }
                },
            )
        }
    }
}

@Composable
private fun GeneratorCard(state: UpicState) {
    UpicCard("GENERATOR") {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            GeneratorType.entries.forEach { generator ->
                FilterChip(
                    selected = state.generator == generator,
                    onClick = { state.generator = generator },
                    label = { Text(generator.label, fontSize = 10.sp) },
                )
            }
        }
        ControlSlider(
            "Length",
            String.format(Locale.US, "%.0f s", state.duration),
            state.duration,
            4f..60f,
        ) { state.duration = it }
        ControlSlider(
            "Low index",
            state.lowIndex.toString(),
            state.lowIndex.toFloat(),
            0f..127f,
        ) {
            state.lowIndex = it.roundToInt().coerceAtMost(state.highIndex - 1)
        }
        ControlSlider(
            "High index",
            state.highIndex.toString(),
            state.highIndex.toFloat(),
            1f..127f,
        ) {
            state.highIndex = it.roundToInt().coerceAtLeast(state.lowIndex + 1)
        }
        ControlSlider(
            "Note length",
            String.format(Locale.US, "%.2f s", state.noteDuration),
            state.noteDuration,
            0.03f..1.5f,
        ) { state.noteDuration = it }
        ControlSlider(
            "Seed",
            state.seed.toString(),
            state.seed.toFloat(),
            0f..9999f,
        ) { state.seed = it.roundToInt().toLong() }
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            WaveformNames.forEachIndexed { wave, name ->
                FilterChip(
                    selected = state.waveform == wave,
                    onClick = { state.waveform = wave },
                    label = { Text(name, fontSize = 10.sp) },
                )
            }
        }
        when (state.generator) {
            GeneratorType.Sieve -> Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                SievePresetNames.forEachIndexed { preset, name ->
                    FilterChip(
                        selected = state.sievePreset == preset,
                        onClick = { state.sievePreset = preset },
                        label = { Text(name, fontSize = 10.sp) },
                    )
                }
            }
            GeneratorType.Markov -> ControlSlider(
                "States",
                state.markovStates.toString(),
                state.markovStates.toFloat(),
                2f..12f,
            ) { state.markovStates = it.roundToInt() }
            GeneratorType.Cloud -> ControlSlider(
                "Density",
                String.format(Locale.US, "%.0f/s", state.cloudDensity),
                state.cloudDensity,
                1f..80f,
            ) { state.cloudDensity = it }
            GeneratorType.Strategy -> {
                ControlSlider(
                    "Rows",
                    state.strategyRows.toString(),
                    state.strategyRows.toFloat(),
                    2f..8f,
                ) { state.strategyRows = it.roundToInt() }
                ControlSlider(
                    "Columns",
                    state.strategyCols.toString(),
                    state.strategyCols.toFloat(),
                    2f..8f,
                ) { state.strategyCols = it.roundToInt() }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Replicator dynamics", fontSize = 11.sp,
                        color = TextDim)
                    Spacer(Modifier.weight(1f))
                    Switch(
                        checked = state.replicator,
                        onCheckedChange = { state.replicator = it },
                    )
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = { state.generate() },
                modifier = Modifier.weight(1f),
            ) {
                Text("Generate page")
            }
            OutlinedButton(
                onClick = { state.undoPage() },
                enabled = state.canUndo,
            ) { Text("Undo") }
        }
    }
}

@Composable
private fun GendynCard(state: UpicState) {
    UpicCard("GENDYN / LIVE") {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            DistributionNames.forEachIndexed { index, name ->
                FilterChip(
                    selected = state.gendynDistribution == index,
                    onClick = {
                        state.gendynDistribution = index
                        state.applyGendyn()
                    },
                    label = { Text(name, fontSize = 10.sp) },
                )
            }
        }
        ControlSlider(
            "Segments",
            state.gendynSegments.toString(),
            state.gendynSegments.toFloat(),
            2f..32f,
        ) {
            state.gendynSegments = it.roundToInt()
            state.applyGendyn()
        }
        ControlSlider(
            "Amp step",
            String.format(Locale.US, "%.2f", state.gendynAmpStep),
            state.gendynAmpStep,
            0f..1f,
        ) {
            state.gendynAmpStep = it
            state.applyGendyn()
        }
        ControlSlider(
            "Amp barrier",
            String.format(Locale.US, "%.2f", state.gendynAmpBarrier),
            state.gendynAmpBarrier,
            0f..1f,
        ) {
            state.gendynAmpBarrier = it
            state.applyGendyn()
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Mirror", fontSize = 11.sp, color = TextDim)
            Spacer(Modifier.weight(1f))
            Switch(
                checked = state.gendynMirror,
                onCheckedChange = {
                    state.gendynMirror = it
                    state.applyGendyn()
                },
            )
        }
        Text("Live notes (touch and hold)", fontSize = 10.sp,
            color = TextFaint)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            for (note in 48..60) {
                val name = noteName(note)
                Surface(
                    color = PanelRaised,
                    shape = MaterialTheme.shapes.small,
                    modifier = Modifier
                        .size(width = 42.dp, height = 42.dp)
                        .pointerInput(note) {
                            detectTapGestures(
                                onPress = {
                                    state.liveNoteOn(note)
                                    tryAwaitRelease()
                                    state.liveNoteOff(note)
                                },
                            )
                        },
                ) {
                    Box(contentAlignment = Alignment.Center) {
                        Text(name, fontSize = 9.sp, color = Accent)
                    }
                }
            }
        }
    }
}

@Composable
private fun UpicCard(title: String, content: @Composable () -> Unit) {
    Card(
        colors = CardDefaults.cardColors(containerColor = Panel),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(title, color = Accent, fontSize = 10.sp,
                fontWeight = FontWeight.Bold)
            HorizontalDivider(color = PanelRaised)
            content()
        }
    }
}

@Composable
private fun ControlSlider(
    label: String,
    valueText: String,
    value: Float,
    range: ClosedFloatingPointRange<Float>,
    onValueChange: (Float) -> Unit,
) {
    Column {
        Row(modifier = Modifier.fillMaxWidth()) {
            Text(label, fontSize = 11.sp, color = TextDim)
            Spacer(Modifier.weight(1f))
            Text(valueText, fontSize = 11.sp, color = Color(0xFFD8E4E7))
        }
        Slider(
            value = value.coerceIn(range.start, range.endInclusive),
            onValueChange = onValueChange,
            valueRange = range,
        )
    }
}

private fun noteName(note: Int): String {
    val names = listOf("C", "C#", "D", "D#", "E", "F", "F#", "G",
        "G#", "A", "A#", "B")
    return names[note.mod(12)] + (note / 12 - 1)
}
