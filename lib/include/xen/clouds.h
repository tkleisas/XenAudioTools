// SPDX-License-Identifier: MIT OR GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// Stochastic clouds of sound-points, Formalized Music chapters I, V and IX.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "common.h"
#include "rng.h"

namespace xen {

struct Grain
{
    double time = 0.0;
    double duration = 0.05;
    double freq = 440.0;
    double amp = 0.2;
    double pan = 0.0;
    int group = 0;
};

struct CloudParams
{
    double density = 20.0;
    double densitySpread = 0.0;
    double durationMean = 0.06;
    double durationSpread = 0.6;
    double durationMin = 0.002;
    double durationMax = 1.5;
    double pitchCenter = 440.0;
    double pitchSpreadSemitones = 12.0;
    double ampMean = 0.25;
    double ampSpread = 0.7;
    double panSpread = 0.0;
    Distribution pitchDistribution = Distribution::Logistic;
    Distribution durationDistribution = Distribution::Exponential;
    Distribution ampDistribution = Distribution::Gaussian;
    int group = 0;
};

inline std::vector<Grain> generateCloud(const CloudParams& p, double t0, double t1, Rng& rng)
{
    std::vector<Grain> out;
    if (t1 <= t0 || p.density <= 0.0)
        return out;

    const double density = std::max(0.0001, p.density);
    const auto interval = [&]()
    {
        double base = rng.exponential(1.0 / density);
        if (p.densitySpread > 0.0)
            base *= std::exp(rng.uniform(-p.densitySpread, p.densitySpread));
        return std::max(0.0001, base);
    };

    double t = t0 + interval();
    while (t < t1)
    {
        Grain g;
        g.time = t;

        double dur = p.durationMean;
        if (p.durationDistribution == Distribution::Uniform)
            dur *= 1.0 + rng.uniform(-p.durationSpread, p.durationSpread);
        else if (p.durationDistribution == Distribution::Exponential)
            dur *= std::exp(rng.exponential(p.durationSpread * 0.8));
        else
            dur *= std::exp(sample(p.durationDistribution, rng, p.durationSpread));
        g.duration = clamp(dur, p.durationMin, p.durationMax);

        double semis = 0.0;
        if (p.pitchDistribution == Distribution::Uniform)
            semis = rng.uniform(-p.pitchSpreadSemitones, p.pitchSpreadSemitones);
        else
            semis = sample(p.pitchDistribution, rng, p.pitchSpreadSemitones * 0.35);
        g.freq = clamp(p.pitchCenter * semitonesToRatio(semis), 8.0, 24000.0);

        double amp = p.ampMean;
        if (p.ampDistribution == Distribution::Uniform)
            amp *= 1.0 + rng.uniform(-p.ampSpread, p.ampSpread);
        else
            amp *= std::exp(sample(p.ampDistribution, rng, p.ampSpread * 0.5));
        g.amp = clamp(amp, 0.0, 2.0);

        g.pan = clamp(rng.uniform(-1.0, 1.0) * p.panSpread, -1.0, 1.0);
        g.group = p.group;
        out.push_back(g);
        t += interval();
    }
    return out;
}

inline void addCloud(std::vector<Grain>& destination, const CloudParams& p, double t0, double t1, Rng& rng)
{
    const std::vector<Grain> generated = generateCloud(p, t0, t1, rng);
    destination.insert(destination.end(), generated.begin(), generated.end());
    std::sort(destination.begin(), destination.end(), [](const Grain& a, const Grain& b) { return a.time < b.time; });
}

} // namespace xen
