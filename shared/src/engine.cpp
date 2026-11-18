// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include "xen/engine.h"

#include <algorithm>
#include <cmath>

namespace xen {

void Engine::prepare(double sampleRate, int)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (auto& v : arcs_)
    {
        v.osc.prepare(sampleRate_);
        v.gen.prepare(sampleRate_);
    }
    for (auto& v : live_)
    {
        v.gen.prepare(sampleRate_);
        v.active = false;
        v.env = 0.0f;
        v.state = 0;
        v.note = -1;
    }
    reset();
}

void Engine::reset()
{
    for (auto& v : arcs_)
    {
        v.arcIndex = -1;
        v.active = false;
        v.fading = false;
        v.gendynGate = false;
        v.gain = 0.0f;
        v.osc.reset();
        v.gen.reset(false);
    }
    for (auto& v : live_)
    {
        v.active = false;
        v.env = 0.0f;
        v.state = 0;
        v.note = -1;
        v.gen.noteOff();
    }
    playheadAtomic_.store(0.0);
    voiceCursor_ = 0;
}

void Engine::setPage(std::shared_ptr<const Page> page)
{
    std::atomic_store(&page_, std::move(page));
}

std::shared_ptr<const Page> Engine::page() const
{
    return std::atomic_load(&page_);
}

void Engine::setTransport(bool run, bool loop, double speed, bool reverse)
{
    run_.store(run);
    loop_.store(loop);
    speed_.store(clamp(speed, 0.02, 16.0));
    reverse_.store(reverse);
}

void Engine::setGains(float masterDb, float pageDb, float liveDb)
{
    masterGain_.store((float)dbToGain(masterDb));
    pageGain_.store((float)dbToGain(pageDb));
    liveGain_.store((float)dbToGain(liveDb));
}

void Engine::setArcGendynParams(const GendynVoice::Params& p)
{
    std::lock_guard<std::mutex> lock(paramsMutex_);
    arcParams_ = p;
}

GendynVoice::Params Engine::arcGendynParams() const
{
    std::lock_guard<std::mutex> lock(paramsMutex_);
    return arcParams_;
}

void Engine::setLiveGendynParams(const GendynVoice::Params& p)
{
    std::lock_guard<std::mutex> lock(paramsMutex_);
    liveParams_ = p;
}

GendynVoice::Params Engine::liveGendynParams() const
{
    std::lock_guard<std::mutex> lock(paramsMutex_);
    return liveParams_;
}

void Engine::setLiveEnvelope(float attackSeconds, float releaseSeconds)
{
    liveAttack_ = clamp(attackSeconds, 0.001f, 10.0f);
    liveRelease_ = clamp(releaseSeconds, 0.005f, 20.0f);
}

double Engine::noteToHz(int note) const
{
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

void Engine::startLiveVoice(int note, int index, float velocity)
{
    LiveVoice& v = live_[(size_t)index];
    GendynVoice::Params p;
    {
        std::lock_guard<std::mutex> lock(paramsMutex_);
        p = liveParams_;
    }
    v.gen.setParams(p);
    v.note = note;
    v.velocity = clamp(velocity, 0.0f, 1.0f);
    v.env = 0.0f;
    v.state = 1;
    v.active = true;
    v.gen.noteOn(noteToHz(note), 1.0);
}

void Engine::noteOn(int note, float velocity)
{
    int freeIndex = -1;
    for (int i = 0; i < kLiveVoices; ++i)
    {
        if (!live_[(size_t)i].active)
        {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex < 0)
    {
        freeIndex = (int)(voiceCursor_ % kLiveVoices);
        voiceCursor_ = (voiceCursor_ + 1) % kLiveVoices;
    }
    startLiveVoice(note, freeIndex, velocity);
}

void Engine::noteOff(int note)
{
    for (auto& v : live_)
    {
        if (v.active && v.note == note)
        {
            v.state = 3;
            v.note = -1;
        }
    }
}

void Engine::allNotesOff()
{
    for (auto& v : live_)
    {
        v.state = 3;
        v.note = -1;
    }
}

void Engine::render(float* left, float* right, int numSamples)
{
    if (numSamples <= 0)
        return;

    const std::shared_ptr<const Page> page = std::atomic_load(&page_);
    GendynVoice::Params arcParams;
    {
        std::lock_guard<std::mutex> lock(paramsMutex_);
        arcParams = arcParams_;
    }

    const bool run = run_.load();
    const bool loop = loop_.load();
    const bool reverse = reverse_.load();
    const double speed = speed_.load();
    const float master = masterGain_.load();
    const float pageGain = pageGain_.load();
    const float liveGain = liveGain_.load();

    const int totalArcs = page ? std::min<int>(4096, (int)page->arcs.size()) : 0;
    const double loopStart = page ? page->loopStart : 0.0;
    const double loopEnd = page ? std::max(page->loopEnd, page->loopStart + 1e-3) : 0.0;
    const double pageEnd = page ? std::max(1e-3, page->duration) : 0.0;
    const double step = (speed / sampleRate_) * (reverse ? -1.0 : 1.0);

    double ph = playheadAtomic_.load();

    const float gainCoeff = (float)(1.0 - std::exp(-1.0 / (0.004 * sampleRate_)));
    const float releaseCoeff = (float)std::exp(-1.0 / (std::max(0.005f, liveRelease_) * sampleRate_));
    const float attackInc = (float)(1.0 / (std::max(0.001f, liveAttack_) * sampleRate_));

    constexpr int kControl = 16;
    int activeIndices[kArcVoices] {};
    float target[kArcVoices] {};
    bool audibleNow[kArcVoices] {};

    int written = 0;
    while (written < numSamples)
    {
        const int len = std::min(kControl, numSamples - written);

        int activeCount = 0;
        if (run && page)
        {
            for (int i = 0; i < totalArcs && activeCount < kArcVoices; ++i)
            {
                const Arc& a = page->arcs[(size_t)i];
                if (ph >= a.start() && ph <= a.end() && !a.muted && page->groupAudible(a.group))
                    activeIndices[activeCount++] = i;
            }
        }

        for (int v = 0; v < kArcVoices; ++v)
        {
            target[v] = 0.0f;
            audibleNow[v] = false;
        }

        for (int v = 0; v < kArcVoices; ++v)
        {
            ArcVoice& av = arcs_[v];
            if (!av.active)
                continue;
            bool stillActive = false;
            for (int k = 0; k < activeCount; ++k)
            {
                if (activeIndices[k] == av.arcIndex)
                {
                    stillActive = true;
                    break;
                }
            }
            if (!stillActive)
                av.fading = true;
        }

        for (int k = 0; k < activeCount; ++k)
        {
            const int arcIndex = activeIndices[k];
            const Arc& a = page->arcs[(size_t)arcIndex];

            int chosen = -1;
            for (int v = 0; v < kArcVoices; ++v)
            {
                ArcVoice& av = arcs_[v];
                if (av.active && !av.fading && av.arcIndex == arcIndex)
                {
                    chosen = v;
                    break;
                }
            }

            if (chosen < 0)
            {
                for (int v = 0; v < kArcVoices; ++v)
                {
                    ArcVoice& av = arcs_[v];
                    if (!av.active || (av.fading && av.gain < 1.0e-4f))
                    {
                        chosen = v;
                        break;
                    }
                }
                if (chosen < 0)
                {
                    float quietest = 1.0e9f;
                    for (int v = 0; v < kArcVoices; ++v)
                    {
                        ArcVoice& av = arcs_[v];
                        if (av.fading && av.gain < quietest)
                        {
                            quietest = av.gain;
                            chosen = v;
                        }
                    }
                }
                if (chosen >= 0)
                {
                    ArcVoice& av = arcs_[chosen];
                    if (av.gendynGate)
                    {
                        av.gen.noteOff();
                        av.gendynGate = false;
                    }
                    av.arcIndex = arcIndex;
                    av.active = true;
                    av.fading = false;
                    av.gain = 0.0f;
                    av.lastHz = 0.0;
                    av.osc.reset();
                }
            }

            if (chosen < 0)
                continue;

            ArcVoice& v = arcs_[chosen];
            const double span = std::max(1e-9, a.end() - a.start());
            const double u = clamp((ph - a.start()) / span, 0.0, 1.0);
            const Envelope& env = page->envelopeFor(a);
            const float amp = a.weight * env.at((float)u);
            const float pitch = a.pitchAt(ph);
            double hz = page->tableFor(a).hzForNorm(pitch);
            const float transpose = page->groupTranspose(a.group);
            if (transpose != 0.0f)
                hz *= semitonesToRatio(transpose);
            hz = clamp(hz, 0.01, sampleRate_ * 0.49);

            v.waveform = a.waveform;
            v.lastHz = hz;
            target[chosen] = clamp(amp, 0.0f, 2.0f) * pageGain;
            audibleNow[chosen] = true;

            if (a.waveform == WaveformType::Gendyn)
            {
                if (!v.gendynGate)
                {
                    v.gen.setParams(arcParams);
                    v.gen.noteOn(hz, 1.0);
                    v.gendynGate = true;
                }
                else
                {
                    v.gen.setFrequency(hz);
                }
            }
            else
            {
                v.osc.setWaveform(a.waveform);
                v.osc.setFrequency(hz);
            }
        }

        for (int s = 0; s < len; ++s)
        {
            float mix = 0.0f;

            for (int v = 0; v < kArcVoices; ++v)
            {
                ArcVoice& av = arcs_[v];
                if (!av.active && av.gain < 1.0e-5f)
                    continue;

                float sample = 0.0f;
                if (av.waveform == WaveformType::Gendyn)
                    sample = av.gen.process();
                else
                    sample = av.osc.process();

                av.gain += (target[v] - av.gain) * gainCoeff;
                mix += sample * av.gain;

                if (!audibleNow[v] && av.gain < 1.0e-4f)
                {
                    av.active = false;
                    av.fading = false;
                    av.arcIndex = -1;
                    if (av.gendynGate)
                    {
                        av.gen.noteOff();
                        av.gendynGate = false;
                    }
                }
            }

            for (auto& v : live_)
            {
                if (!v.active)
                    continue;
                const float y = v.gen.process() * v.velocity;
                if (v.state == 1)
                {
                    v.env += attackInc;
                    if (v.env >= 1.0f)
                    {
                        v.env = 1.0f;
                        v.state = 2;
                    }
                }
                else if (v.state == 2)
                {
                    v.env = 1.0f;
                }
                else if (v.state == 3)
                {
                    v.env *= releaseCoeff;
                    if (v.env < 1.0e-5f)
                    {
                        v.env = 0.0f;
                        v.active = false;
                        v.gen.noteOff();
                        v.state = 0;
                    }
                }
                mix += y * v.env * liveGain;
            }

            mix *= master;
            const float out = std::tanh(mix * 1.25f) * 0.8f;
            left[written + s] = out;
            right[written + s] = out;

            if (run && page)
            {
                ph += step;
                if (loop)
                {
                    const double span = loopEnd - loopStart;
                    if (span > 1e-6)
                    {
                        if (ph > loopEnd)
                            ph = loopStart + std::fmod(ph - loopEnd, span);
                        else if (ph < loopStart)
                            ph = loopEnd - std::fmod(loopStart - ph, span);
                    }
                }
                else
                {
                    if (ph > pageEnd)
                    {
                        ph = pageEnd;
                        run_.store(false);
                    }
                    else if (ph < 0.0)
                    {
                        ph = 0.0;
                        run_.store(false);
                    }
                }
            }
        }

        written += len;
    }

    playheadAtomic_.store(ph);
}

} // namespace xen
