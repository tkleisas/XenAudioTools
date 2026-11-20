// SPDX-License-Identifier: GPL-3.0-or-later
#include <jni.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>

#include "AudioOutput.h"

#include "xen/clouds.h"
#include "xen/generators.h"
#include "xen/markov.h"
#include "xen/page_model.h"
#include "xen/strategy.h"

namespace {

std::mutex gEngineMutex;
std::unique_ptr<upic::AudioOutput> gAudio;

xen::Tuning equalTemperament() {
    xen::Tuning tuning;
    tuning.referenceHz = 8.175798915643707;
    tuning.referenceIndex = 0.0;
    tuning.divisionsPerOctave = 12.0;
    tuning.discrete = false;
    return tuning;
}

std::string toString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string text(chars);
    env->ReleaseStringUTFChars(value, chars);
    return text;
}

jstring toJString(JNIEnv* env, const std::string& text) {
    return env->NewStringUTF(text.c_str());
}

xen::Sieve sievePreset(int preset) {
    switch (((preset % 6) + 6) % 6) {
    case 1:
        return xen::Sieve::chromatic12();
    case 2:
        return xen::Sieve::fromOffsets(12, {0, 2, 4, 6, 8, 10});
    case 3:
        return xen::Sieve::fromOffsets(12, {0, 2, 3, 4, 6, 7, 8, 10});
    case 4:
        return xen::Sieve::fromOffsets(12, {0, 4, 7});
    case 5:
        return xen::Sieve::fromOffsets(12, {0, 3, 4, 5, 7, 8, 9, 11});
    case 0:
    default:
        return xen::Sieve::diatonic();
    }
}

const char* generatorName(int generator) {
    switch (generator) {
    case 1: return "Markov Mobile";
    case 2: return "GENDYN Cloud";
    case 3: return "Strategy Game";
    case 0:
    default: return "Sieve Arpeggio";
    }
}

xen::Page buildPage(int generator, double duration, int lowIndex,
                    int highIndex, uint64_t seed, int waveform,
                    int sieve, double noteDuration, int markovStates,
                    double cloudDensity, int strategyRows,
                    int strategyCols, bool replicator) {
    xen::Page page;
    page.name = generatorName(generator);
    const double length = std::clamp(duration, 1.0, 120.0);
    page.duration = length;
    page.loopStart = 0.0;
    page.loopEnd = length;
    page.envelopes.front() = xen::Envelope::ad(0.004f, 0.05f);
    page.frequencyTables.front().tuning = equalTemperament();

    const int low = std::clamp(lowIndex, 0, 134);
    const int high = std::clamp(highIndex, low + 1, 135);
    const auto wave = static_cast<xen::WaveformType>(
        std::clamp(waveform, 0, 4));
    const xen::Tuning tuning = equalTemperament();

    switch (generator) {
    case 1: {
        xen::MarkovGenParams p;
        p.t0 = 0.0;
        p.t1 = length;
        p.lowIndex = low;
        p.highIndex = high;
        p.noteDuration = std::clamp(noteDuration, 0.02, 2.0);
        p.waveform = wave;
        p.weight = 0.55f;
        p.seed = seed;
        const int states = std::clamp(markovStates, 2, 16);
        const xen::MarkovChain chain =
            xen::MarkovChain::randomMatrix(states, 1.8, seed);
        xen::generateMarkov(page, chain, p, tuning);
        break;
    }
    case 2: {
        xen::CloudGenParams p;
        p.cloud.density = std::clamp(cloudDensity, 0.5, 160.0);
        p.cloud.durationMean = 0.07;
        p.cloud.durationSpread = 0.6;
        p.cloud.pitchCenter = 440.0;
        p.cloud.pitchSpreadSemitones = 24.0;
        p.cloud.ampMean = 0.22;
        p.cloud.group = 0;
        p.t0 = 0.0;
        p.t1 = length;
        p.lowIndex = low;
        p.highIndex = high;
        p.waveform = wave;
        p.group = 0;
        xen::Rng rng(seed);
        xen::generateCloudArcs(page, p, tuning, rng);
        break;
    }
    case 3: {
        const int rows = std::clamp(strategyRows, 2, 12);
        const int cols = std::clamp(strategyCols, 2, 12);
        xen::StrategyGame game(rows, cols);
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const double phase =
                    static_cast<double>((row + 1) * (col + 2)) * 0.37 +
                    static_cast<double>(seed % 997) * 0.013;
                game.setPayoff(row, col, std::sin(phase),
                               std::cos(phase * 1.17));
            }
        }
        xen::StrategyGenParams p;
        p.t0 = 0.0;
        p.t1 = length;
        p.lowIndex = low;
        p.highIndex = high;
        p.segmentDuration = std::clamp(noteDuration, 0.05, 2.0);
        p.waveform = wave;
        p.weight = 0.55f;
        p.seed = seed;
        p.useReplicator = replicator;
        xen::generateStrategy(page, game, p, tuning);
        break;
    }
    case 0:
    default: {
        xen::SieveGenParams p;
        p.t0 = 0.0;
        p.t1 = length;
        p.lowIndex = low;
        p.highIndex = high;
        p.mode = xen::SieveMode::Arpeggio;
        p.waveform = wave;
        p.weight = 0.5f;
        p.noteDuration = std::clamp(noteDuration, 0.02, 2.0);
        p.maxArcs = 320;
        xen::generateSieve(page, sievePreset(sieve), p, tuning);
        break;
    }
    }

    page.updateDuration();
    page.duration = length;
    page.loopStart = 0.0;
    page.loopEnd = length;
    return page;
}

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeCreate(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (!gAudio) gAudio = std::make_unique<upic::AudioOutput>();
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeDestroy(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    gAudio.reset();
}

extern "C" JNIEXPORT jint JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeStart(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (!gAudio) gAudio = std::make_unique<upic::AudioOutput>();
    return gAudio->start() ? 0 : -1;
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeStop(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (gAudio) gAudio->stop();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeIsRunning(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    return gAudio && gAudio->running() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeError(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    return toJString(env, gAudio ? gAudio->error() : "audio engine not created");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeSetPage(JNIEnv* env, jobject,
                                                     jstring pageText) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (!gAudio) return JNI_FALSE;
    const std::string text = toString(env, pageText);
    xen::Page page;
    if (!xen::deserializePage(text, page)) return JNI_FALSE;
    gAudio->engine().setPage(
        std::make_shared<const xen::Page>(std::move(page)));
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeGeneratePage(
    JNIEnv* env, jobject, jint generator, jdouble duration, jint lowIndex,
    jint highIndex, jlong seed, jint waveform, jint sievePresetValue,
    jdouble noteDuration, jint markovStates, jdouble cloudDensity,
    jint strategyRows, jint strategyCols, jboolean replicator) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    xen::Page page = buildPage(
        generator, duration, lowIndex, highIndex,
        static_cast<uint64_t>(seed), waveform, sievePresetValue,
        noteDuration, markovStates, cloudDensity, strategyRows,
        strategyCols, replicator != JNI_FALSE);
    const std::string text = xen::serializePage(page);
    if (gAudio) {
        gAudio->engine().setPage(
            std::make_shared<const xen::Page>(page));
    }
    return toJString(env, text);
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeSetTransport(
    JNIEnv*, jobject, jboolean playing, jboolean loop, jfloat speed) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (!gAudio) return;
    gAudio->engine().setTransport(playing == JNI_TRUE, loop == JNI_TRUE,
                                  std::clamp((double)speed, 0.05, 4.0),
                                  false);
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeSetGains(
    JNIEnv*, jobject, jfloat masterDb, jfloat pageDb, jfloat liveDb) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (!gAudio) return;
    gAudio->engine().setGains(masterDb, pageDb, liveDb);
}

extern "C" JNIEXPORT jdouble JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativePlayhead(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    return gAudio ? gAudio->engine().playhead() : 0.0;
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeSetPlayhead(
    JNIEnv*, jobject, jdouble seconds) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (gAudio) gAudio->engine().setPlayhead(std::max(0.0, seconds));
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeSetGendyn(
    JNIEnv*, jobject, jint segments, jint distribution, jfloat durationStep,
    jfloat durationBarrier, jfloat ampStep, jfloat ampBarrier,
    jboolean mirror, jlong seed) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (!gAudio) return;
    xen::GendynVoice::Params params;
    params.segments = std::clamp((int)segments, 1, 32);
    params.distribution = static_cast<xen::Distribution>(
        std::clamp((int)distribution, 0, 4));
    params.durationStep = std::clamp((double)durationStep, 0.0, 1.5);
    params.durationBarrier = std::clamp((double)durationBarrier, 0.0, 0.95);
    params.ampStep = std::clamp((double)ampStep, 0.0, 1.0);
    params.ampBarrier = std::clamp((double)ampBarrier, 0.0, 1.0);
    params.mirror = mirror == JNI_TRUE;
    params.seed = static_cast<uint64_t>(seed);
    gAudio->engine().setArcGendynParams(params);
    gAudio->engine().setLiveGendynParams(params);
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeSetLiveEnvelope(
    JNIEnv*, jobject, jfloat attackSeconds, jfloat releaseSeconds) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (!gAudio) return;
    gAudio->engine().setLiveEnvelope(attackSeconds, releaseSeconds);
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeNoteOn(
    JNIEnv*, jobject, jint note, jfloat velocity) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (gAudio) gAudio->noteOn(note, velocity);
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeNoteOff(JNIEnv*, jobject,
                                                     jint note) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (gAudio) gAudio->noteOff(note);
}

extern "C" JNIEXPORT void JNICALL
Java_gr_tkleisas_upicstudio_UpicEngine_nativeAllNotesOff(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gEngineMutex);
    if (gAudio) gAudio->allNotesOff();
}
