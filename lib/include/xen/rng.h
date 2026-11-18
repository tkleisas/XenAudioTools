// SPDX-License-Identifier: MIT OR GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "common.h"

namespace xen {

class Rng
{
public:
    explicit Rng(uint64_t seedValue = 0x9E3779B97F4A7C15ULL) { seed(seedValue); }

    void seed(uint64_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            value += 0x9E3779B97F4A7C15ULL;
            uint64_t z = value;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            z ^= z >> 31;
            state_[i] = z;
        }
        haveGaussian_ = false;
    }

    uint64_t next()
    {
        const uint64_t result = rotl(state_[1] * 5ULL, 7) * 9ULL;
        const uint64_t t = state_[1] << 17;
        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = rotl(state_[3], 45);
        return result;
    }

    double uniform01()
    {
        return (double)(next() >> 11) * (1.0 / 9007199254740992.0);
    }

    double uniform(double a = 0.0, double b = 1.0) { return a + (b - a) * uniform01(); }

    double gaussian()
    {
        if (haveGaussian_)
        {
            haveGaussian_ = false;
            return cachedGaussian_;
        }
        double u1, u2;
        do
        {
            u1 = uniform01();
            u2 = uniform01();
        } while (u1 <= 1e-300);
        const double r = std::sqrt(-2.0 * std::log(u1));
        cachedGaussian_ = r * std::sin(kTwoPi * u2);
        haveGaussian_ = true;
        return r * std::cos(kTwoPi * u2);
    }

    double cauchy(double scale = 1.0)
    {
        const double u = uniform01();
        return scale * std::tan(kPi * (u - 0.5));
    }

    double logistic(double scale = 1.0)
    {
        const double u = clamp(uniform01(), 1e-12, 1.0 - 1e-12);
        return scale * std::log(u / (1.0 - u));
    }

    double exponential(double mean = 1.0)
    {
        const double u = clamp(uniform01(), 1e-12, 1.0);
        return -mean * std::log(u);
    }

    double triangular(double lo, double peak, double hi)
    {
        const double u = uniform01();
        const double split = (peak - lo) / (hi - lo);
        if (u < split)
            return lo + std::sqrt(u * (hi - lo) * (peak - lo));
        return hi - std::sqrt((1.0 - u) * (hi - lo) * (hi - peak));
    }

    int intRange(int lo, int hi)
    {
        if (hi <= lo)
            return lo;
        return lo + (int)(next() % (uint64_t)(hi - lo + 1));
    }

    int poisson(double lambda)
    {
        if (lambda <= 0.0)
            return 0;
        if (lambda < 30.0)
        {
            const double limit = std::exp(-lambda);
            double product = 1.0;
            int k = 0;
            do
            {
                ++k;
                product *= uniform01();
            } while (product > limit);
            return k - 1;
        }
        const double x = std::round(lambda + std::sqrt(lambda) * gaussian());
        return (int)std::max(0.0, x);
    }

    template <class It>
    int weightedIndex(It begin, It end)
    {
        double total = 0.0;
        for (It it = begin; it != end; ++it)
            total += *it;
        if (total <= 0.0)
            return 0;
        double pick = uniform01() * total;
        int index = 0;
        for (It it = begin; it != end; ++it, ++index)
        {
            pick -= *it;
            if (pick <= 0.0)
                return index;
        }
        return index - 1;
    }

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

    uint64_t state_[4] {};
    bool haveGaussian_ = false;
    double cachedGaussian_ = 0.0;
};

enum class Distribution
{
    Uniform,
    Gaussian,
    Cauchy,
    Logistic,
    Exponential
};

inline double sample(Distribution type, Rng& rng, double scale = 1.0)
{
    switch (type)
    {
        case Distribution::Gaussian:    return rng.gaussian() * scale;
        case Distribution::Cauchy:      return rng.cauchy(scale);
        case Distribution::Logistic:    return rng.logistic(scale);
        case Distribution::Exponential: return rng.exponential(scale);
        case Distribution::Uniform:
        default:                        return rng.uniform(-scale, scale);
    }
}

} // namespace xen
