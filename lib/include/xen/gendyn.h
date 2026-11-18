// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// Dynamic stochastic synthesis, Formalized Music chapter XIII.

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "common.h"
#include "rng.h"

namespace xen {

class GendynVoice
{
public:
    static constexpr int kMaxSegments = 64;

    struct Params
    {
        int segments = 12;
        double freq = 110.0;
        double durationStep = 0.45;
        double durationBarrier = 0.45;
        double durationMin = 0.00002;
        double durationMax = 0.25;
        double ampStep = 0.18;
        double ampBarrier = 0.2;
        double ampMin = -1.0;
        double ampMax = 1.0;
        Distribution distribution = Distribution::Cauchy;
        double distributionScale = 1.0;
        bool mirror = false;
        uint64_t seed = 0x5EED1234ULL;
    };

    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        dur_.assign(kMaxSegments, 0.0);
        amp_.assign(kMaxSegments, 0.0);
        params_.segments = clamp(params_.segments, 1, kMaxSegments);
        reset(true);
    }

    void setParams(const Params& p)
    {
        params_ = p;
        params_.segments = clamp(params_.segments, 1, kMaxSegments);
        setFrequency(params_.freq);
    }

    const Params& params() const { return params_; }

    void seed(uint64_t s)
    {
        params_.seed = s;
        rng_.seed(s);
    }

    void reset(bool randomize = true)
    {
        if (dur_.size() != (size_t)kMaxSegments)
        {
            dur_.assign(kMaxSegments, 0.0);
            amp_.assign(kMaxSegments, 0.0);
        }
        rng_.seed(params_.seed);
        cur_ = 0;
        pos_ = 0.0;
        active_ = false;
        level_ = 0.0;
        meanSamples_ = sampleRate_ / (std::max(0.01, params_.freq) * params_.segments);
        for (int i = 0; i < kMaxSegments; ++i)
        {
            dur_[(size_t)i] = meanSamples_;
            amp_[(size_t)i] = 0.0;
        }
        if (randomize)
        {
            for (int i = 0; i < params_.segments; ++i)
            {
                const double phase = kTwoPi * (double)i / (double)params_.segments;
                amp_[(size_t)i] = std::sin(phase) * 0.8 + rng_.gaussian() * 0.03;
            }
            applySymmetry();
        }
    }

    void noteOn(double hz, double level)
    {
        if (!active_)
            reset(true);
        active_ = true;
        level_ = clamp(level, 0.0, 1.0);
        setFrequency(hz);
        pos_ = 0.0;
        cur_ = 0;
    }

    void noteOff() { active_ = false; }

    bool isActive() const { return active_; }

    void setFrequency(double hz)
    {
        hz = clamp(hz, 0.01, 24000.0);
        const double oldMean = meanSamples_;
        params_.freq = hz;
        const double newMean = sampleRate_ / (hz * std::max(1, params_.segments));
        meanSamples_ = newMean;
        const int n = params_.segments;
        const double maxSamples = sampleRate_ * params_.durationMax;
        if (oldMean > 0.0)
        {
            const double ratio = newMean / oldMean;
            for (int i = 0; i < n; ++i)
            {
                double& d = dur_[(size_t)i];
                d = clamp(d * ratio, 1.0, maxSamples);
            }
        }
        else
        {
            for (int i = 0; i < n; ++i)
                dur_[(size_t)i] = clamp(newMean, 1.0, maxSamples);
        }
    }

    float process()
    {
        if (!active_)
            return 0.0f;

        const int n = params_.segments;
        const int nxt = (cur_ + 1) % n;
        const double seg = std::max(1.0, dur_[(size_t)cur_]);
        const double frac = clamp(pos_ / seg, 0.0, 1.0);
        const double y = amp_[(size_t)cur_] + (amp_[(size_t)nxt] - amp_[(size_t)cur_]) * frac;

        pos_ += 1.0;
        if (pos_ >= seg)
        {
            pos_ -= seg;
            cur_ = nxt;
            if (cur_ == 0)
                advancePeriod();
        }
        return (float)(y * level_);
    }

private:
    void applySymmetry()
    {
        if (!params_.mirror)
            return;
        const int n = params_.segments;
        for (int i = 0; i < n / 2; ++i)
            amp_[(size_t)(n - 1 - i)] = -amp_[(size_t)i];
    }

    void advancePeriod()
    {
        const int n = params_.segments;
        const double dmin = std::max(1.0, params_.durationMin * sampleRate_);
        const double dmax = std::max(dmin, params_.durationMax * sampleRate_);
        const double mean = std::max(1.0, meanSamples_);

        for (int i = 0; i < n; ++i)
        {
            double& d = dur_[(size_t)i];
            const double delta = sample(params_.distribution, rng_, params_.distributionScale) * mean * params_.durationStep;
            double next = d + delta;
            const double band = std::max(1.0, mean * params_.durationBarrier);
            next = clamp(next, d - band, d + band);
            d = clamp(next, dmin, dmax);
        }

        for (int i = 0; i < n; ++i)
        {
            double& a = amp_[(size_t)i];
            const double delta = sample(params_.distribution, rng_, params_.distributionScale) * params_.ampStep;
            double next = a + delta;
            const double band = std::max(1e-6, params_.ampBarrier);
            next = clamp(next, a - band, a + band);
            a = clamp(next, params_.ampMin, params_.ampMax);
        }
        applySymmetry();
    }

    double sampleRate_ = 44100.0;
    Params params_ {};
    std::vector<double> dur_;
    std::vector<double> amp_;
    Rng rng_ { 0x5EED1234ULL };
    double meanSamples_ = 1.0;
    double pos_ = 0.0;
    int cur_ = 0;
    double level_ = 0.0;
    bool active_ = false;
};

} // namespace xen
