// SPDX-License-Identifier: MIT OR GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// UPIC-style graphical score model, Formalized Music appendix III.
// An arc is a pitch-versus-time curve; a page is a collection of arcs.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "common.h"
#include "sieve.h"
#include "tuning.h"

namespace xen {

enum class WaveformType
{
    Sine = 0,
    Saw,
    Square,
    Triangle,
    Gendyn
};

inline const char* waveformName(WaveformType w)
{
    switch (w)
    {
        case WaveformType::Saw:      return "Saw";
        case WaveformType::Square:   return "Square";
        case WaveformType::Triangle: return "Triangle";
        case WaveformType::Gendyn:   return "Gendyn";
        case WaveformType::Sine:
        default:                     return "Sine";
    }
}

struct ArcPoint
{
    float time = 0.0f;
    float pitch = 0.5f;
};

struct EnvelopePoint
{
    float time = 0.0f;
    float level = 1.0f;
};

struct Envelope
{
    std::vector<EnvelopePoint> points { { 0.0f, 0.0f }, { 0.01f, 1.0f }, { 0.1f, 0.0f } };

    float at(float u) const
    {
        if (points.empty())
            return 1.0f;
        u = clamp(u, 0.0f, 1.0f);
        if (u <= points.front().time)
            return points.front().level;
        for (size_t i = 1; i < points.size(); ++i)
        {
            if (u <= points[i].time)
            {
                const float span = std::max(1e-6f, points[i].time - points[i - 1].time);
                const float t = (u - points[i - 1].time) / span;
                return points[i - 1].level + (points[i].level - points[i - 1].level) * t;
            }
        }
        return points.back().level;
    }

    static Envelope ad(float attack, float release)
    {
        Envelope e;
        e.points = { { 0.0f, 0.0f }, { clamp(attack, 0.0f, 1.0f), 1.0f },
                     { clamp(1.0f - release, 0.0f, 1.0f), 1.0f }, { 1.0f, 0.0f } };
        if (e.points[1].time > e.points[2].time)
            e.points[2].time = e.points[1].time;
        return e;
    }
};

struct FrequencyTable
{
    Tuning tuning;
    uint64_t sieveId = 0;
    Sieve sieve;
    bool useSieve = false;
    double lowIndex = 0.0;
    double highIndex = 120.0;

    double hzForNorm(double norm) const
    {
        if (useSieve && !sieve.empty())
            return tuning.snapToSieve(norm, lowIndex, highIndex, sieve);
        return tuning.normToHz(norm, lowIndex, highIndex);
    }

    double indexForNorm(double norm) const { return tuning.normToIndex(norm, lowIndex, highIndex); }
};

struct Arc
{
    std::vector<ArcPoint> points;
    WaveformType waveform = WaveformType::Gendyn;
    int group = 0;
    int channel = 0;
    int envelopeIndex = 0;
    int freqTableIndex = 0;
    float weight = 0.7f;
    bool muted = false;

    bool valid() const { return points.size() >= 2; }
    double start() const { return points.empty() ? 0.0 : (double)points.front().time; }
    double end() const { return points.empty() ? 0.0 : (double)points.back().time; }
    double duration() const { return std::max(0.0, end() - start()); }

    void sortPoints() { std::sort(points.begin(), points.end(), [](const ArcPoint& a, const ArcPoint& b) { return a.time < b.time; }); }

    float pitchAt(double t) const
    {
        if (points.empty())
            return 0.5f;
        if (t <= points.front().time)
            return points.front().pitch;
        if (t >= points.back().time)
            return points.back().pitch;
        for (size_t i = 1; i < points.size(); ++i)
        {
            if (t <= points[i].time)
            {
                const double span = std::max(1e-9, (double)points[i].time - points[i - 1].time);
                const double u = (t - points[i - 1].time) / span;
                return (float)(points[i - 1].pitch + (points[i].pitch - points[i - 1].pitch) * u);
            }
        }
        return points.back().pitch;
    }

    static Arc line(double t0, double t1, float pitch0, float pitch1, WaveformType w = WaveformType::Gendyn)
    {
        Arc a;
        a.waveform = w;
        a.points = { { (float)t0, pitch0 }, { (float)t1, pitch1 } };
        return a;
    }

    static Arc point(double t, double dur, float pitch, WaveformType w = WaveformType::Gendyn)
    {
        Arc a;
        a.waveform = w;
        a.points = { { (float)t, pitch }, { (float)(t + dur), pitch } };
        return a;
    }

    void quantizePitch(const FrequencyTable& table, double quantum)
    {
        if (quantum <= 0.0)
            return;
        for (auto& p : points)
        {
            const double idx = table.indexForNorm(p.pitch);
            const double snapped = std::round(idx / quantum) * quantum;
            p.pitch = (float)clamp((snapped - table.lowIndex) / std::max(1e-9, table.highIndex - table.lowIndex), 0.0, 1.0);
        }
    }
};

struct ArcGroup
{
    std::string name;
    bool muted = false;
    bool solo = false;
    float transposeSemitones = 0.0f;
};

struct Page
{
    std::string name = "Untitled";
    std::vector<Arc> arcs;
    std::vector<Envelope> envelopes { Envelope::ad(0.01f, 0.05f) };
    std::vector<FrequencyTable> frequencyTables { FrequencyTable {} };
    std::vector<ArcGroup> groups { ArcGroup { "Group A", false, false, 0.0f },
                                   ArcGroup { "Group B", false, false, 0.0f },
                                   ArcGroup { "Group C", false, false, 0.0f },
                                   ArcGroup { "Group D", false, false, 0.0f } };
    double duration = 60.0;
    double loopStart = 0.0;
    double loopEnd = 60.0;

    void sortArcs()
    {
        std::sort(arcs.begin(), arcs.end(), [](const Arc& a, const Arc& b) { return a.start() < b.start(); });
    }

    void updateDuration()
    {
        double end = 0.0;
        for (const auto& a : arcs)
            end = std::max(end, a.end());
        duration = std::max(1.0, end);
        loopStart = clamp(loopStart, 0.0, duration);
        loopEnd = clamp(loopEnd <= loopStart ? duration : loopEnd, loopStart + 0.001, duration);
    }

    const Envelope& envelopeFor(const Arc& arc) const
    {
        if (arc.envelopeIndex >= 0 && arc.envelopeIndex < (int)envelopes.size())
            return envelopes[(size_t)arc.envelopeIndex];
        return envelopes.front();
    }

    const FrequencyTable& tableFor(const Arc& arc) const
    {
        if (arc.freqTableIndex >= 0 && arc.freqTableIndex < (int)frequencyTables.size())
            return frequencyTables[(size_t)arc.freqTableIndex];
        return frequencyTables.front();
    }

    float groupTranspose(int groupIndex) const
    {
        if (groupIndex >= 0 && groupIndex < (int)groups.size())
            return groups[(size_t)groupIndex].transposeSemitones;
        return 0.0f;
    }

    bool groupAudible(int groupIndex) const
    {
        if (groupIndex < 0 || groupIndex >= (int)groups.size())
            return true;
        bool anySolo = false;
        for (const auto& g : groups)
            anySolo = anySolo || g.solo;
        if (anySolo && !groups[(size_t)groupIndex].solo)
            return false;
        return !groups[(size_t)groupIndex].muted;
    }
};

} // namespace xen
