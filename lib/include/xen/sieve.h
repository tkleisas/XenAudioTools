// SPDX-License-Identifier: MIT OR GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// Xenakis sieve theory, Formalized Music chapters XI and XII.
// A sieve is a union of intersections of congruence classes (modulus, offset).

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <vector>

#include "common.h"

namespace xen {

struct SieveClass
{
    int modulus = 1;
    int offset = 0;

    bool matches(int64_t x) const { return imod(x - offset, modulus) == 0; }
};

using SieveGroup = std::vector<SieveClass>;

struct Sieve
{
    std::vector<SieveGroup> groups;

    void addClass(int modulus, int offset)
    {
        groups.push_back(SieveGroup { SieveClass { std::max(1, modulus), mod(offset, std::max(1, modulus)) } });
    }

    void addIntersection(std::initializer_list<SieveClass> classes)
    {
        SieveGroup g(classes);
        if (!g.empty())
            groups.push_back(std::move(g));
    }

    bool empty() const { return groups.empty(); }

    bool matches(int64_t x) const
    {
        for (const auto& group : groups)
        {
            bool all = true;
            for (const auto& c : group)
            {
                if (!c.matches(x))
                {
                    all = false;
                    break;
                }
            }
            if (all)
                return true;
        }
        return false;
    }

    std::vector<int64_t> points(int64_t lo, int64_t hi) const
    {
        std::vector<int64_t> out;
        if (hi < lo)
            return out;
        const Sieve r = reduced();
        if (r.empty())
            return out;

        for (const auto& group : r.groups)
        {
            const SieveClass& c = group.front();
            int64_t first = lo + imod(c.offset - lo, c.modulus);
            for (int64_t x = first; x <= hi; x += c.modulus)
                out.push_back(x);
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    static bool intersectClass(const SieveClass& a, const SieveClass& b, SieveClass& out)
    {
        const int m1 = std::max(1, a.modulus);
        const int m2 = std::max(1, b.modulus);
        const int i1 = mod(a.offset, m1);
        const int i2 = mod(b.offset, m2);
        const int g = gcd(m1, m2);
        if (imod((int64_t)i1 - i2, g) != 0)
            return false;

        const int c1 = m1 / g;
        const int c2 = m2 / g;
        int64_t offset = i1;
        if (c2 != 1)
        {
            const int64_t diff = imod((int64_t)i2 - i1, m2);
            const int64_t step = imod((int64_t)diff / g, c2);
            const int inv = modInverse(mod(c1, c2), c2);
            const int64_t k = imod(step * inv, c2);
            offset = (int64_t)i1 + (int64_t)m1 * k;
        }
        const int64_t period = lcm(m1, m2);
        out.modulus = (int)std::min<int64_t>(period, std::numeric_limits<int>::max());
        out.offset = (int)imod(offset, period);
        return true;
    }

    Sieve reduced() const
    {
        Sieve out;
        for (const auto& group : groups)
        {
            if (group.empty())
                continue;
            SieveClass acc = group.front();
            acc.modulus = std::max(1, acc.modulus);
            acc.offset = mod(acc.offset, acc.modulus);
            bool ok = true;
            for (size_t i = 1; i < group.size(); ++i)
            {
                SieveClass next;
                if (!intersectClass(acc, group[i], next))
                {
                    ok = false;
                    break;
                }
                acc = next;
            }
            if (ok)
                out.groups.push_back(SieveGroup { acc });
        }

        out.groups.erase(std::unique(out.groups.begin(), out.groups.end(), [](const SieveGroup& a, const SieveGroup& b)
        {
            return a.front().modulus == b.front().modulus && a.front().offset == b.front().offset;
        }), out.groups.end());

        for (size_t i = 0; i < out.groups.size();)
        {
            bool subsumed = false;
            for (size_t j = 0; j < out.groups.size(); ++j)
            {
                if (i == j)
                    continue;
                const SieveClass& a = out.groups[i].front();
                const SieveClass& b = out.groups[j].front();
                if (a.modulus % b.modulus == 0 && imod((int64_t)a.offset - b.offset, b.modulus) == 0)
                {
                    subsumed = true;
                    break;
                }
            }
            if (subsumed)
                out.groups.erase(out.groups.begin() + (std::ptrdiff_t)i);
            else
                ++i;
        }
        return out;
    }

    int64_t period() const
    {
        const Sieve r = reduced();
        if (r.empty())
            return 0;
        int64_t p = 1;
        for (const auto& group : r.groups)
            p = lcm((int)std::min<int64_t>(p, std::numeric_limits<int>::max()), group.front().modulus);
        return p;
    }

    void transpose(int n)
    {
        for (auto& group : groups)
            for (auto& c : group)
            {
                c.offset = mod(c.offset + n, std::max(1, c.modulus));
            }
    }

    void invert()
    {
        for (auto& group : groups)
            for (auto& c : group)
            {
                c.modulus = std::max(1, c.modulus);
                c.offset = mod(-c.offset, c.modulus);
            }
    }

    static Sieve fromOffsets(int modulus, std::initializer_list<int> offsets)
    {
        Sieve s;
        modulus = std::max(1, modulus);
        for (int o : offsets)
            s.addClass(modulus, o);
        return s;
    }

    static Sieve diatonic(int divisions = 12)
    {
        return fromOffsets(divisions, { 0, 2, 4, 5, 7, 9, 11 });
    }

    static Sieve chromatic12()
    {
        Sieve s;
        s.addClass(1, 0);
        return s;
    }

    static Sieve fromPoints(const std::vector<int64_t>& rawPoints)
    {
        std::vector<int64_t> pts = rawPoints;
        std::sort(pts.begin(), pts.end());
        pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
        Sieve out;
        if (pts.empty())
            return out;
        if (pts.size() == 1)
        {
            const int64_t period = 4096;
            out.addClass((int)period, (int)imod(pts.front(), period));
            return out;
        }

        std::vector<bool> covered(pts.size(), false);
        const int64_t span = std::max<int64_t>(1, pts.back() - pts.front());
        const int maxModulus = (int)std::min<int64_t>(span + 1, 4096);

        bool progress = true;
        while (progress)
        {
            progress = false;
            for (size_t i = 0; i < pts.size(); ++i)
            {
                if (covered[i])
                    continue;

                bool found = false;
                for (int q = 1; q <= maxModulus && !found; ++q)
                {
                    bool allIn = true;
                    int hits = 0;
                    std::vector<size_t> hitIndices;
                    for (int64_t x = pts[i]; x <= pts.back(); x += q)
                    {
                        const auto it = std::lower_bound(pts.begin(), pts.end(), x);
                        if (it == pts.end() || *it != x)
                        {
                            allIn = false;
                            break;
                        }
                        ++hits;
                        hitIndices.push_back((size_t)(it - pts.begin()));
                    }
                    if (allIn && hits >= 2)
                    {
                        out.addClass(q, (int)imod(pts[i], q));
                        for (size_t idx : hitIndices)
                            covered[idx] = true;
                        progress = true;
                        found = true;
                    }
                }
                if (!found && !covered[i])
                {
                    out.addClass(maxModulus, (int)imod(pts[i], maxModulus));
                    covered[i] = true;
                    progress = true;
                }
            }
        }
        return out.reduced();
    }
};

inline Sieve sieveUnion(const Sieve& a, const Sieve& b)
{
    Sieve out = a;
    out.groups.insert(out.groups.end(), b.groups.begin(), b.groups.end());
    return out.reduced();
}

inline Sieve sieveIntersect(const Sieve& a, const Sieve& b)
{
    Sieve out;
    const Sieve ra = a.reduced();
    const Sieve rb = b.reduced();
    for (const auto& ga : ra.groups)
        for (const auto& gb : rb.groups)
        {
            SieveClass c;
            if (Sieve::intersectClass(ga.front(), gb.front(), c))
                out.groups.push_back(SieveGroup { c });
        }
    return out.reduced();
}

inline Sieve sieveComplement(const Sieve& a, const Sieve& universe)
{
    const int64_t period = lcm((int)std::min<int64_t>(a.period(), 65536), (int)std::min<int64_t>(universe.period(), 65536));
    Sieve out;
    if (period <= 0 || period > 65536)
        return out;
    for (int64_t x = 0; x < period; ++x)
        if (universe.matches(x) && !a.matches(x))
            out.addClass((int)period, (int)x);
    return out.reduced();
}

} // namespace xen
