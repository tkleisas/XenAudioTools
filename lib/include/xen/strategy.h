// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// Musical strategy, Formalized Music chapter IV.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "common.h"
#include "rng.h"

namespace xen {

class StrategyGame
{
public:
    StrategyGame() = default;

    StrategyGame(int rows, int cols)
    {
        resize(rows, cols);
    }

    void resize(int rows, int cols)
    {
        rows_ = std::max(1, rows);
        cols_ = std::max(1, cols);
        payoffA_.assign((size_t)rows_, std::vector<double>((size_t)cols_, 0.0));
        payoffB_.assign((size_t)rows_, std::vector<double>((size_t)cols_, 0.0));
        reset();
    }

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    void setPayoff(int row, int col, double a, double b)
    {
        if (row < 0 || row >= rows_ || col < 0 || col >= cols_)
            return;
        payoffA_[(size_t)row][(size_t)col] = a;
        payoffB_[(size_t)row][(size_t)col] = b;
    }

    double payoffA(int row, int col) const { return payoffA_[(size_t)row][(size_t)col]; }
    double payoffB(int row, int col) const { return payoffB_[(size_t)row][(size_t)col]; }

    void reset()
    {
        mixedRow_.assign((size_t)rows_, 1.0 / rows_);
        mixedCol_.assign((size_t)cols_, 1.0 / cols_);
        countsRow_.assign((size_t)rows_, 0.0);
        countsCol_.assign((size_t)cols_, 0.0);
        empiricalRow_ = mixedRow_;
        empiricalCol_ = mixedCol_;
        steps_ = 0;
    }

    void setMixedRow(const std::vector<double>& v)
    {
        mixedRow_ = normalize(v, rows_);
    }

    void setMixedCol(const std::vector<double>& v)
    {
        mixedCol_ = normalize(v, cols_);
    }

    const std::vector<double>& mixedRow() const { return mixedRow_; }
    const std::vector<double>& mixedCol() const { return mixedCol_; }
    const std::vector<double>& empiricalRow() const { return empiricalRow_; }
    const std::vector<double>& empiricalCol() const { return empiricalCol_; }

    int bestResponseRow(int col) const
    {
        int best = 0;
        double bestValue = -1e300;
        for (int r = 0; r < rows_; ++r)
        {
            const double v = payoffA_[(size_t)r][(size_t)col];
            if (v > bestValue)
            {
                bestValue = v;
                best = r;
            }
        }
        return best;
    }

    int bestResponseCol(int row) const
    {
        int best = 0;
        double bestValue = -1e300;
        for (int c = 0; c < cols_; ++c)
        {
            const double v = payoffB_[(size_t)row][(size_t)c];
            if (v > bestValue)
            {
                bestValue = v;
                best = c;
            }
        }
        return best;
    }

    int sampleRow(Rng& rng) const { return rng.weightedIndex(mixedRow_.begin(), mixedRow_.end()); }
    int sampleCol(Rng& rng) const { return rng.weightedIndex(mixedCol_.begin(), mixedCol_.end()); }

    void fictitiousPlayStep()
    {
        int row = 0;
        double bestValue = -1e300;
        for (int r = 0; r < rows_; ++r)
        {
            double v = 0.0;
            for (int c = 0; c < cols_; ++c)
                v += payoffA_[(size_t)r][(size_t)c] * mixedCol_[(size_t)c];
            if (v > bestValue)
            {
                bestValue = v;
                row = r;
            }
        }
        int col = 0;
        bestValue = -1e300;
        for (int c = 0; c < cols_; ++c)
        {
            double v = 0.0;
            for (int r = 0; r < rows_; ++r)
                v += payoffB_[(size_t)r][(size_t)c] * mixedRow_[(size_t)r];
            if (v > bestValue)
            {
                bestValue = v;
                col = c;
            }
        }
        countsRow_[(size_t)row] += 1.0;
        countsCol_[(size_t)col] += 1.0;
        ++steps_;
        mixedRow_ = normalize(countsRow_, rows_);
        mixedCol_ = normalize(countsCol_, cols_);
        empiricalRow_ = mixedRow_;
        empiricalCol_ = mixedCol_;
    }

    void replicatorStep(double dt)
    {
        std::vector<double> fitnessRow((size_t)rows_, 0.0);
        std::vector<double> fitnessCol((size_t)cols_, 0.0);
        double avgRow = 0.0;
        double avgCol = 0.0;
        for (int r = 0; r < rows_; ++r)
        {
            for (int c = 0; c < cols_; ++c)
            {
                fitnessRow[(size_t)r] += payoffA_[(size_t)r][(size_t)c] * mixedCol_[(size_t)c];
                fitnessCol[(size_t)c] += payoffB_[(size_t)r][(size_t)c] * mixedRow_[(size_t)r];
            }
            avgRow += mixedRow_[(size_t)r] * fitnessRow[(size_t)r];
        }
        for (int c = 0; c < cols_; ++c)
            avgCol += mixedCol_[(size_t)c] * fitnessCol[(size_t)c];

        const double alpha = clamp(dt, 0.0, 1.0);
        for (int r = 0; r < rows_; ++r)
        {
            double v = mixedRow_[(size_t)r] * (1.0 + alpha * (fitnessRow[(size_t)r] - avgRow));
            mixedRow_[(size_t)r] = std::max(0.0, v);
        }
        for (int c = 0; c < cols_; ++c)
        {
            double v = mixedCol_[(size_t)c] * (1.0 + alpha * (fitnessCol[(size_t)c] - avgCol));
            mixedCol_[(size_t)c] = std::max(0.0, v);
        }
        mixedRow_ = normalize(mixedRow_, rows_);
        mixedCol_ = normalize(mixedCol_, cols_);
    }

    double valueA() const
    {
        double v = 0.0;
        for (int r = 0; r < rows_; ++r)
            for (int c = 0; c < cols_; ++c)
                v += mixedRow_[(size_t)r] * mixedCol_[(size_t)c] * payoffA_[(size_t)r][(size_t)c];
        return v;
    }

    double valueB() const
    {
        double v = 0.0;
        for (int r = 0; r < rows_; ++r)
            for (int c = 0; c < cols_; ++c)
                v += mixedRow_[(size_t)r] * mixedCol_[(size_t)c] * payoffB_[(size_t)r][(size_t)c];
        return v;
    }

    int steps() const { return steps_; }

    static StrategyGame zeroSumPrisoner(uint64_t)
    {
        StrategyGame g(2, 2);
        g.setPayoff(0, 0, 3.0, 3.0);
        g.setPayoff(0, 1, 0.0, 5.0);
        g.setPayoff(1, 0, 5.0, 0.0);
        g.setPayoff(1, 1, 1.0, 1.0);
        return g;
    }

private:
    static std::vector<double> normalize(const std::vector<double>& v, int n)
    {
        std::vector<double> out((size_t)n, 0.0);
        double total = 0.0;
        for (int i = 0; i < n; ++i)
        {
            out[(size_t)i] = std::max(0.0, i < (int)v.size() ? v[(size_t)i] : 0.0);
            total += out[(size_t)i];
        }
        if (total <= 0.0)
            return std::vector<double>((size_t)n, 1.0 / n);
        for (double& x : out)
            x /= total;
        return out;
    }

    int rows_ = 2;
    int cols_ = 2;
    std::vector<std::vector<double>> payoffA_ { { 1.0, 0.0 }, { 0.0, 1.0 } };
    std::vector<std::vector<double>> payoffB_ { { 1.0, 0.0 }, { 0.0, 1.0 } };
    std::vector<double> mixedRow_ { 0.5, 0.5 };
    std::vector<double> mixedCol_ { 0.5, 0.5 };
    std::vector<double> countsRow_ { 0.0, 0.0 };
    std::vector<double> countsCol_ { 0.0, 0.0 };
    std::vector<double> empiricalRow_ { 0.5, 0.5 };
    std::vector<double> empiricalCol_ { 0.5, 0.5 };
    int steps_ = 0;
};

} // namespace xen
