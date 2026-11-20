// SPDX-License-Identifier: GPL-3.0-or-later
package gr.tkleisas.upicstudio

import java.util.Locale
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow

data class ArcPoint(val time: Double, val pitch: Double)

data class UpicArc(
    val points: MutableList<ArcPoint> = mutableListOf(),
    var waveform: Int = 4,
    var group: Int = 0,
    var channel: Int = 0,
    var envelopeIndex: Int = 0,
    var freqTableIndex: Int = 0,
    var weight: Double = 0.55,
    var muted: Boolean = false,
) {
    val start: Double get() = points.firstOrNull()?.time ?: 0.0
    val end: Double get() = points.lastOrNull()?.time ?: 0.0

    fun pitchAt(time: Double): Double {
        if (points.isEmpty()) return 0.5
        if (time <= points.first().time) return points.first().pitch
        if (time >= points.last().time) return points.last().pitch
        for (i in 1 until points.size) {
            val a = points[i - 1]
            val b = points[i]
            if (time <= b.time) {
                val span = max(1e-9, b.time - a.time)
                val u = (time - a.time) / span
                return a.pitch + (b.pitch - a.pitch) * u
            }
        }
        return points.last().pitch
    }
}

data class UpicGroup(
    var muted: Boolean = false,
    var solo: Boolean = false,
    var transposeSemitones: Double = 0.0,
    var name: String = "Group A",
)

data class EnvelopePoint(val time: Double, val level: Double)

data class UpicEnvelope(
    val points: MutableList<EnvelopePoint> = mutableListOf(
        EnvelopePoint(0.0, 0.0),
        EnvelopePoint(0.01, 1.0),
        EnvelopePoint(0.05, 1.0),
        EnvelopePoint(0.10, 0.0),
    ),
)

data class SieveClass(val modulus: Int, val offset: Int)

data class UpicFrequencyTable(
    var lowIndex: Double = 24.0,
    var highIndex: Double = 135.0,
    var referenceHz: Double = 8.175798915643707,
    var divisionsPerOctave: Double = 12.0,
    var referenceIndex: Double = 0.0,
    var discrete: Boolean = false,
    var useSieve: Boolean = false,
    val sieve: MutableList<MutableList<SieveClass>> = mutableListOf(),
) {
    fun normToHz(norm: Double): Double {
        val index = lowIndex + norm.coerceIn(0.0, 1.0) * (highIndex - lowIndex)
        return referenceHz *
            2.0.pow((index - referenceIndex) / max(1e-9, divisionsPerOctave))
    }

    fun normToIndex(norm: Double): Double =
        lowIndex + norm.coerceIn(0.0, 1.0) * (highIndex - lowIndex)

    fun hzToNorm(hz: Double): Double {
        if (hz <= 0.0) return 0.0
        val index = referenceIndex +
            divisionsPerOctave * (kotlin.math.ln(hz / referenceHz) /
                kotlin.math.ln(2.0))
        return ((index - lowIndex) / max(1e-9, highIndex - lowIndex))
            .coerceIn(0.0, 1.0)
    }
}

data class UpicPage(
    var name: String = "UPIC Page",
    var duration: Double = 16.0,
    var loopStart: Double = 0.0,
    var loopEnd: Double = 16.0,
    val groups: MutableList<UpicGroup> = mutableListOf(
        UpicGroup(name = "Group A"),
        UpicGroup(name = "Group B"),
        UpicGroup(name = "Group C"),
        UpicGroup(name = "Group D"),
    ),
    val envelopes: MutableList<UpicEnvelope> = mutableListOf(UpicEnvelope()),
    val frequencyTables: MutableList<UpicFrequencyTable> =
        mutableListOf(UpicFrequencyTable()),
    val arcs: MutableList<UpicArc> = mutableListOf(),
) {
    val table: UpicFrequencyTable
        get() = frequencyTables.firstOrNull() ?: UpicFrequencyTable()

    fun refreshDuration() {
        val end = arcs.maxOfOrNull { it.end } ?: 0.0
        duration = max(max(1.0, duration), end)
        loopStart = loopStart.coerceIn(0.0, duration)
        loopEnd = duration
    }

    fun toText(): String {
        val out = StringBuilder()
        out.append("XENPAGE 1\n")
        out.append("name ").append(quote(name)).append('\n')
        out.append("duration ").append(number(duration)).append('\n')
        out.append("loop ").append(number(loopStart)).append(' ')
            .append(number(loopEnd)).append('\n')

        out.append("groups ").append(groups.size).append('\n')
        groups.forEach {
            out.append("group ").append(if (it.muted) 1 else 0).append(' ')
                .append(if (it.solo) 1 else 0).append(' ')
                .append(number(it.transposeSemitones)).append(' ')
                .append(quote(it.name)).append('\n')
        }

        out.append("envelopes ").append(envelopes.size).append('\n')
        envelopes.forEach { envelope ->
            out.append("env ").append(envelope.points.size)
            envelope.points.forEach {
                out.append(' ').append(number(it.time))
                    .append(' ').append(number(it.level))
            }
            out.append('\n')
        }

        out.append("tables ").append(frequencyTables.size).append('\n')
        frequencyTables.forEach { table ->
            out.append("table ").append(number(table.lowIndex)).append(' ')
                .append(number(table.highIndex)).append(' ')
                .append(number(table.referenceHz)).append(' ')
                .append(number(table.divisionsPerOctave)).append(' ')
                .append(number(table.referenceIndex)).append(' ')
                .append(if (table.discrete) 1 else 0).append(' ')
                .append(if (table.useSieve) 1 else 0).append(' ')
                .append(table.sieve.size)
            table.sieve.forEach { group ->
                out.append(' ').append(group.size)
                group.forEach { item ->
                    out.append(' ').append(item.modulus)
                        .append(' ').append(item.offset)
                }
            }
            out.append('\n')
        }

        out.append("arcs ").append(arcs.size).append('\n')
        arcs.forEach { arc ->
            out.append("arc ").append(arc.waveform).append(' ')
                .append(arc.group).append(' ').append(arc.channel).append(' ')
                .append(arc.envelopeIndex).append(' ')
                .append(arc.freqTableIndex).append(' ')
                .append(number(arc.weight)).append(' ')
                .append(if (arc.muted) 1 else 0).append(' ')
                .append(arc.points.size)
            arc.points.forEach {
                out.append(' ').append(number(it.time))
                    .append(' ').append(number(it.pitch))
            }
            out.append('\n')
        }
        return out.toString()
    }

    companion object {
        fun parse(text: String): UpicPage {
            val lexer = Lexer(text)
            require(lexer.next() == "XENPAGE") { "Not a XENPAGE file" }
            require(lexer.nextInt() == 1) { "Unsupported XENPAGE version" }

            val page = UpicPage(name = "Untitled", groups = mutableListOf(),
                envelopes = mutableListOf(), frequencyTables = mutableListOf(),
                arcs = mutableListOf())

            while (true) {
                when (val key = lexer.next() ?: break) {
                    "name" -> page.name = lexer.next() ?: "Untitled"
                    "duration" -> page.duration = lexer.nextDouble()
                    "loop" -> {
                        page.loopStart = lexer.nextDouble()
                        page.loopEnd = lexer.nextDouble()
                    }
                    "groups" -> {
                        repeat(lexer.nextInt()) {
                            require(lexer.next() == "group")
                            page.groups.add(
                                UpicGroup(
                                    muted = lexer.nextInt() != 0,
                                    solo = lexer.nextInt() != 0,
                                    transposeSemitones = lexer.nextDouble(),
                                    name = lexer.next() ?: "Group",
                                ),
                            )
                        }
                    }
                    "envelopes" -> {
                        repeat(lexer.nextInt()) {
                            require(lexer.next() == "env")
                            val count = lexer.nextInt()
                            val points = mutableListOf<EnvelopePoint>()
                            repeat(count) {
                                points.add(
                                    EnvelopePoint(lexer.nextDouble(),
                                        lexer.nextDouble()),
                                )
                            }
                            page.envelopes.add(UpicEnvelope(points))
                        }
                    }
                    "tables" -> {
                        repeat(lexer.nextInt()) {
                            require(lexer.next() == "table")
                            val table = UpicFrequencyTable(
                                lowIndex = lexer.nextDouble(),
                                highIndex = lexer.nextDouble(),
                                referenceHz = lexer.nextDouble(),
                                divisionsPerOctave = lexer.nextDouble(),
                                referenceIndex = lexer.nextDouble(),
                                discrete = lexer.nextInt() != 0,
                                useSieve = lexer.nextInt() != 0,
                                sieve = mutableListOf(),
                            )
                            repeat(lexer.nextInt()) {
                                val classes = mutableListOf<SieveClass>()
                                repeat(lexer.nextInt()) {
                                    classes.add(SieveClass(lexer.nextInt(),
                                        lexer.nextInt()))
                                }
                                table.sieve.add(classes)
                            }
                            page.frequencyTables.add(table)
                        }
                    }
                    "arcs" -> {
                        repeat(lexer.nextInt()) {
                            require(lexer.next() == "arc")
                            val arc = UpicArc(
                                waveform = lexer.nextInt(),
                                group = lexer.nextInt(),
                                channel = lexer.nextInt(),
                                envelopeIndex = lexer.nextInt(),
                                freqTableIndex = lexer.nextInt(),
                                weight = lexer.nextDouble(),
                                muted = lexer.nextInt() != 0,
                            )
                            repeat(lexer.nextInt()) {
                                arc.points.add(
                                    ArcPoint(lexer.nextDouble(),
                                        lexer.nextDouble()),
                                )
                            }
                            page.arcs.add(arc)
                        }
                    }
                    else -> error("Unknown XENPAGE field: $key")
                }
            }

            if (page.groups.isEmpty()) {
                page.groups.addAll(
                    listOf(
                        UpicGroup(name = "Group A"),
                        UpicGroup(name = "Group B"),
                        UpicGroup(name = "Group C"),
                        UpicGroup(name = "Group D"),
                    ),
                )
            }
            if (page.envelopes.isEmpty()) page.envelopes.add(UpicEnvelope())
            if (page.frequencyTables.isEmpty())
                page.frequencyTables.add(UpicFrequencyTable())
            return page
        }

        private fun quote(value: String): String =
            "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\""

        private fun number(value: Double): String =
            String.format(Locale.US, "%.9f", value)
    }
}

private class Lexer(private val text: String) {
    private var index = 0

    fun next(): String? {
        while (index < text.length && text[index].isWhitespace()) index++
        if (index >= text.length) return null
        if (text[index] == '"') {
            index++
            val value = StringBuilder()
            while (index < text.length) {
                val c = text[index++]
                if (c == '\\' && index < text.length) {
                    value.append(text[index++])
                } else if (c == '"') {
                    return value.toString()
                } else {
                    value.append(c)
                }
            }
            error("Unterminated quoted string")
        }
        val start = index
        while (index < text.length && !text[index].isWhitespace()) index++
        return text.substring(start, index)
    }

    fun nextInt(): Int = next()?.toIntOrNull() ?: error("Expected integer")
    fun nextDouble(): Double =
        next()?.toDoubleOrNull() ?: error("Expected number")

    private fun error(message: String): Nothing = throw IllegalArgumentException(message)
}

fun formattedHz(hz: Double): String = when {
    hz >= 1000.0 -> String.format(Locale.US, "%.1f kHz", hz / 1000.0)
    else -> String.format(Locale.US, "%.0f Hz", hz)
}

fun waveformName(waveform: Int): String = when (waveform) {
    0 -> "Sine"
    1 -> "Saw"
    2 -> "Square"
    3 -> "Triangle"
    else -> "GENDYN"
}

fun arcTimeExtent(arc: UpicArc): Double = abs(arc.end - arc.start)

fun pitchAtDistance(arc: UpicArc, time: Double): Double = min(1.0,
    max(0.0, arc.pitchAt(time)))
