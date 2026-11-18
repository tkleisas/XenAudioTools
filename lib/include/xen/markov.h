// SPDX-License-Identifier: MIT OR GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// Markovian stochastic music, Formalized Music chapters II and III.

#pragma once

#include <cstddef>
#include <vector>

#include "common.h"
#include "rng.h"

namespace xen {

class MarkovChain
{
public:
    MarkovChain() = default;
    explicit MarkovChain(int states, uint64_t seed = 0x123456789ABCDEF0ULL)
    {
        resize(states);
        rng_.seed(seed);
        reset();
    }

    void resize(int states)
    {
        const int n = std::max(1, states);
        matrix_.assign((size_t)n, std::vector<double>((size_t)n, 1.0 / (double)n));
        state_ = 0;
    }

    int size() const { return (int)matrix_.size(); }

    void setMatrix(const std::vector<std::vector<double>>& m)
    {
        const size_t n = m.size();
        matrix_.assign(n, std::vector<double>(n, 0.0));
        for (size_t i = 0; i < n; ++i)
        {
            double total = 0.0;
            for (size_t j = 0; j < n && j < m[i].size(); ++j)
                total += std::max(0.0, m[i][j]);
            if (total <= 0.0)
                total = 1.0;
            for (size_t j = 0; j < n; ++j)
                matrix_[i][j] = (j < m[i].size() ? std::max(0.0, m[i][j]) : 0.0) / total;
        }
        state_ = 0;
    }

    const std::vector<std::vector<double>>& matrix() const { return matrix_; }

    static MarkovChain fromCounts(const std::vector<std::vector<double>>& counts, double smoothing = 0.5)
    {
        const size_t n = counts.size();
        std::vector<std::vector<double>> m(n, std::vector<double>(n, 0.0));
        for (size_t i = 0; i < n; ++i)
        {
            for (size_t j = 0; j < n; ++j)
                m[i][j] = smoothing + (j < counts[i].size() ? std::max(0.0, counts[i][j]) : 0.0);
        }
        MarkovChain chain;
        chain.setMatrix(m);
        return chain;
    }

    static MarkovChain randomMatrix(int states, double selfBias, uint64_t seed)
    {
        Rng rng(seed);
        std::vector<std::vector<double>> m((size_t)states, std::vector<double>((size_t)states));
        for (int i = 0; i < states; ++i)
        {
            double total = 0.0;
            for (int j = 0; j < states; ++j)
            {
                m[(size_t)i][(size_t)j] = rng.uniform(0.0, 1.0) + (i == j ? selfBias : 0.0);
                total += m[(size_t)i][(size_t)j];
            }
            for (int j = 0; j < states; ++j)
                m[(size_t)i][(size_t)j] /= total;
        }
        MarkovChain chain;
        chain.setMatrix(m);
        return chain;
    }

    static MarkovChain periodicScale(int states, uint64_t seed = 1)
    {
        MarkovChain chain(states, seed);
        std::vector<std::vector<double>> m((size_t)states, std::vector<double>((size_t)states, 0.0));
        for (int i = 0; i < states; ++i)
            m[(size_t)i][(size_t)((i + 1) % states)] = 1.0;
        chain.setMatrix(m);
        return chain;
    }

    void reset(int startState = 0)
    {
        state_ = std::max(0, std::min(startState, size() - 1));
    }

    int current() const { return state_; }

    void setCurrent(int s) { state_ = std::max(0, std::min(s, size() - 1)); }

    int step()
    {
        const auto& row = matrix_[(size_t)state_];
        state_ = rng_.weightedIndex(row.begin(), row.end());
        return state_;
    }

    int stepFrom(int from)
    {
        setCurrent(from);
        return step();
    }

    int jump(int steps)
    {
        for (int i = 0; i < steps; ++i)
            step();
        return state_;
    }

    std::vector<double> distribution() const { return matrix_[(size_t)state_]; }

    std::vector<double> stationary() const
    {
        const int n = size();
        std::vector<double> v((size_t)n, 1.0 / (double)n);
        if (n == 1)
            return v;
        for (int iter = 0; iter < 2000; ++iter)
        {
            std::vector<double> next((size_t)n, 0.0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    next[(size_t)j] += v[(size_t)i] * matrix_[(size_t)i][(size_t)j];
            double total = 0.0;
            for (double x : next)
                total += x;
            if (total <= 0.0)
                break;
            for (double& x : next)
                x /= total;
            double delta = 0.0;
            for (int i = 0; i < n; ++i)
                delta += std::abs(next[(size_t)i] - v[(size_t)i]);
            v.swap(next);
            if (delta < 1e-12)
                break;
        }
        return v;
    }

    void seedRng(uint64_t seed) { rng_.seed(seed); }

private:
    std::vector<std::vector<double>> matrix_;
    int state_ = 0;
    Rng rng_ { 0x123456789ABCDEF0ULL };
};

} // namespace xen
