// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace xen {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

template <class T>
inline T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline int mod(int a, int m)
{
    const int r = a % m;
    return r < 0 ? r + m : r;
}

inline int64_t imod(int64_t a, int64_t m)
{
    const int64_t r = a % m;
    return r < 0 ? r + m : r;
}

inline int gcd(int a, int b)
{
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0)
    {
        const int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

inline int64_t lcm(int a, int b)
{
    if (a == 0 || b == 0)
        return 0;
    return (int64_t)(a / gcd(a, b)) * (int64_t)b;
}

inline int modInverse(int a, int m)
{
    if (m == 1)
        return 0;
    a = mod(a, m);
    int m0 = m;
    int y = 0;
    int x = 1;
    while (a > 1)
    {
        const int q = a / m;
        int t = m;
        m = a % m;
        a = t;
        t = y;
        y = x - q * y;
        x = t;
    }
    if (x < 0)
        x += m0;
    return x;
}

inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
inline double gainToDb(double g) { return 20.0 * std::log10(std::max(g, 1e-12)); }

inline double semitonesToRatio(double st) { return std::pow(2.0, st / 12.0); }

inline bool isPowerOfTwo(int v) { return v > 0 && (v & (v - 1)) == 0; }

} // namespace xen
