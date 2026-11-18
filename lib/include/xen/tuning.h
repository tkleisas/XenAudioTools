// SPDX-License-Identifier: MIT OR GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <cmath>

#include "common.h"
#include "sieve.h"

namespace xen {

struct Tuning
{
    double referenceHz = 8.25;
    double divisionsPerOctave = 12.0;
    double referenceIndex = 0.0;
    bool discrete = false;
    double lowIndex = 0.0;
    double highIndex = 120.0;

    double indexToHz(double index) const
    {
        const double idx = discrete ? std::round(index) : index;
        return referenceHz * std::pow(2.0, (idx - referenceIndex) / divisionsPerOctave);
    }

    double hzToIndex(double hz) const
    {
        if (hz <= 0.0)
            return referenceIndex;
        return referenceIndex + divisionsPerOctave * std::log2(hz / referenceHz);
    }

    double normToIndex(double norm, double lo, double hi) const
    {
        return lo + clamp(norm, 0.0, 1.0) * (hi - lo);
    }

    double normToHz(double norm, double lo, double hi) const
    {
        return indexToHz(normToIndex(norm, lo, hi));
    }

    double snapToSieve(double norm, double lo, double hi, const Sieve& sieve) const
    {
        const double index = normToIndex(norm, lo, hi);
        int64_t base = (int64_t)std::llround(index);
        for (int64_t d = 0; d <= 256; ++d)
        {
            if (sieve.matches(base + d))
                return indexToHz((double)(base + d));
            if (d != 0 && sieve.matches(base - d))
                return indexToHz((double)(base - d));
        }
        return indexToHz(index);
    }
};

} // namespace xen
