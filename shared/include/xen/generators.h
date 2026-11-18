// SPDX-License-Identifier: MIT OR GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "xen/clouds.h"
#include "xen/common.h"
#include "xen/markov.h"
#include "xen/rng.h"
#include "xen/sieve.h"
#include "xen/strategy.h"
#include "xen/upic.h"

namespace xen {

enum class SieveMode
{
    Field,
    Arpeggio,
    Rhythm
};

struct SieveGenParams
{
    double t0 = 0.0;
    double t1 = 30.0;
    int lowIndex = 36;
    int highIndex = 96;
    SieveMode mode = SieveMode::Arpeggio;
    WaveformType waveform = WaveformType::Gendyn;
    float weight = 0.45f;
    double noteDuration = 0.25;
    double timeUnit = 0.125;
    int group = 0;
    int maxArcs = 96;
};

inline void applyTuning(Page& page, int lowIndex, int highIndex, const Tuning& tuning, const Sieve* sieve = nullptr)
{
    if (page.frequencyTables.empty())
        page.frequencyTables.push_back(FrequencyTable {});
    FrequencyTable& table = page.frequencyTables.front();
    table.tuning = tuning;
    table.lowIndex = (double)lowIndex;
    table.highIndex = (double)highIndex;
    table.useSieve = sieve != nullptr && !sieve->empty();
    if (sieve)
        table.sieve = *sieve;
}

inline float indexToNorm(double index, double lowIndex, double highIndex)
{
    return (float)clamp((index - lowIndex) / std::max(1e-9, highIndex - lowIndex), 0.0, 1.0);
}

inline std::vector<int> sieveIndices(const Sieve& sieve, int lowIndex, int highIndex, int maxCount)
{
    std::vector<int> out;
    const std::vector<int64_t> pts = sieve.points(lowIndex, highIndex);
    for (int64_t p : pts)
    {
        out.push_back((int)p);
        if ((int)out.size() >= maxCount)
            break;
    }
    return out;
}

inline void generateSieve(Page& page, const Sieve& sieve, const SieveGenParams& p, const Tuning& tuning)
{
    applyTuning(page, p.lowIndex, p.highIndex, tuning, &sieve);
    const std::vector<int> indices = sieveIndices(sieve, p.lowIndex, p.highIndex, std::max(1, p.maxArcs));
    if (indices.empty())
        return;

    const int group = clamp(p.group, 0, (int)page.groups.size() - 1);
    const double span = std::max(1e-3, p.t1 - p.t0);

    if (p.mode == SieveMode::Field)
    {
        for (int idx : indices)
        {
            Arc a = Arc::line(p.t0, p.t1, indexToNorm(idx, p.lowIndex, p.highIndex),
                              indexToNorm(idx, p.lowIndex, p.highIndex), p.waveform);
            a.weight = p.weight;
            a.group = group;
            page.arcs.push_back(a);
        }
    }
    else if (p.mode == SieveMode::Arpeggio)
    {
        int k = 0;
        double t = p.t0;
        while (t < p.t1)
        {
            const int idx = indices[(size_t)(k % (int)indices.size())];
            const double dur = std::min(p.noteDuration, p.t1 - t);
            Arc a = Arc::point(t, dur, indexToNorm(idx, p.lowIndex, p.highIndex), p.waveform);
            a.weight = p.weight;
            a.group = group;
            page.arcs.push_back(a);
            t += dur;
            ++k;
        }
        (void)span;
    }
    else
    {
        const double unit = std::max(0.001, p.timeUnit);
        const int steps = (int)std::ceil(span / unit);
        int k = 0;
        for (int step = 0; step < steps; ++step)
        {
            if (!sieve.matches(step))
                continue;
            const int idx = indices[(size_t)(k % (int)indices.size())];
            const double t = p.t0 + step * unit;
            const double dur = std::min(std::max(p.noteDuration, unit), p.t1 - t);
            Arc a = Arc::point(t, dur, indexToNorm(idx, p.lowIndex, p.highIndex), p.waveform);
            a.weight = p.weight;
            a.group = group;
            page.arcs.push_back(a);
            ++k;
        }
    }
    page.sortArcs();
    page.updateDuration();
}

struct MarkovGenParams
{
    double t0 = 0.0;
    double t1 = 30.0;
    int lowIndex = 48;
    int highIndex = 84;
    double noteDuration = 0.25;
    WaveformType waveform = WaveformType::Gendyn;
    float weight = 0.45f;
    int group = 0;
    bool stateControlsDuration = false;
    uint64_t seed = 12345;
};

inline void generateMarkov(Page& page, const MarkovChain& chainIn, const MarkovGenParams& p, const Tuning& tuning)
{
    applyTuning(page, p.lowIndex, p.highIndex, tuning, nullptr);
    MarkovChain chain = chainIn;
    chain.seedRng(p.seed);
    const int states = std::max(1, chain.size());
    const int group = clamp(p.group, 0, (int)page.groups.size() - 1);

    double t = p.t0;
    while (t < p.t1)
    {
        const int state = chain.step();
        const double norm = states > 1 ? (double)state / (states - 1) : 0.5;
        double dur = p.noteDuration;
        if (p.stateControlsDuration)
            dur *= 0.5 + norm;
        dur = std::min(dur, p.t1 - t);
        Arc a = Arc::point(t, std::max(0.005, dur), (float)norm, p.waveform);
        a.weight = p.weight;
        a.group = group;
        page.arcs.push_back(a);
        t += std::max(0.005, dur);
    }
    page.sortArcs();
    page.updateDuration();
}

struct CloudGenParams
{
    CloudParams cloud;
    double t0 = 0.0;
    double t1 = 20.0;
    int lowIndex = 24;
    int highIndex = 108;
    WaveformType waveform = WaveformType::Sine;
    int group = 0;
};

inline void generateCloudArcs(Page& page, const CloudGenParams& p, const Tuning& tuning, Rng& rng)
{
    applyTuning(page, p.lowIndex, p.highIndex, tuning, nullptr);
    const int group = clamp(p.group, 0, (int)page.groups.size() - 1);
    const auto grains = generateCloud(p.cloud, p.t0, p.t1, rng);
    for (const auto& g : grains)
    {
        const double idx = tuning.hzToIndex(g.freq);
        const float norm = indexToNorm(idx, p.lowIndex, p.highIndex);
        Arc a = Arc::point(g.time, g.duration, norm, p.waveform);
        a.weight = clamp((float)g.amp, 0.0f, 1.0f);
        a.group = group;
        page.arcs.push_back(a);
    }
    page.sortArcs();
    page.updateDuration();
}

struct StrategyGenParams
{
    double t0 = 0.0;
    double t1 = 30.0;
    int lowIndex = 48;
    int highIndex = 84;
    double segmentDuration = 0.35;
    WaveformType waveform = WaveformType::Gendyn;
    float weight = 0.45f;
    int group = 0;
    uint64_t seed = 777;
    bool useReplicator = false;
};

inline void generateStrategy(Page& page, StrategyGame game, const StrategyGenParams& p, const Tuning& tuning)
{
    applyTuning(page, p.lowIndex, p.highIndex, tuning, nullptr);
    Rng rng(p.seed);
    game.reset();
    const int group = clamp(p.group, 0, (int)page.groups.size() - 1);
    const int rows = std::max(1, game.rows());
    const int cols = std::max(1, game.cols());

    double t = p.t0;
    int step = 0;
    while (t < p.t1)
    {
        if (p.useReplicator)
            game.replicatorStep(0.12);
        else
            game.fictitiousPlayStep();

        const int row = game.sampleRow(rng);
        const int col = game.sampleCol(rng);
        const double norm = rows > 1 ? (double)row / (rows - 1) : 0.5;
        const double dur = std::max(0.01, p.segmentDuration * (0.6 + 0.8 * (double)col / std::max(1, cols - 1)));
        const double payoff = clamp(game.payoffA(row, col) * 0.1, -1.0, 1.0);
        Arc a = Arc::point(t, std::min(dur, p.t1 - t), (float)clamp(norm + 0.1 * payoff, 0.0, 1.0), p.waveform);
        a.weight = p.weight;
        a.group = group;
        page.arcs.push_back(a);
        t += std::max(0.005, dur);
        ++step;
    }
    page.sortArcs();
    page.updateDuration();
}

} // namespace xen
