// SPDX-License-Identifier: GPL-3.0-or-later
package gr.tkleisas.upicstudio

object UpicEngine {
    init {
        System.loadLibrary("upic")
    }

    external fun nativeCreate(): Boolean
    external fun nativeDestroy()
    external fun nativeStart(): Int
    external fun nativeStop()
    external fun nativeIsRunning(): Boolean
    external fun nativeError(): String

    external fun nativeSetPage(pageText: String): Boolean
    external fun nativeGeneratePage(
        generator: Int,
        duration: Double,
        lowIndex: Int,
        highIndex: Int,
        seed: Long,
        waveform: Int,
        sievePreset: Int,
        noteDuration: Double,
        markovStates: Int,
        cloudDensity: Double,
        strategyRows: Int,
        strategyCols: Int,
        replicator: Boolean,
    ): String

    external fun nativeSetTransport(playing: Boolean, loop: Boolean, speed: Float)
    external fun nativeSetGains(masterDb: Float, pageDb: Float, liveDb: Float)
    external fun nativePlayhead(): Double
    external fun nativeSetPlayhead(seconds: Double)
    external fun nativeSetGendyn(
        segments: Int,
        distribution: Int,
        durationStep: Float,
        durationBarrier: Float,
        ampStep: Float,
        ampBarrier: Float,
        mirror: Boolean,
        seed: Long,
    )
    external fun nativeSetLiveEnvelope(attackSeconds: Float, releaseSeconds: Float)
    external fun nativeNoteOn(note: Int, velocity: Float)
    external fun nativeNoteOff(note: Int)
    external fun nativeAllNotesOff()

    fun start(): String? {
        if (nativeStart() == 0) return null
        return nativeError().ifBlank { "Could not start AAudio output" }
    }
}
