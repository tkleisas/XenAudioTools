// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <cmath>

#include "common.h"
#include "upic.h"

namespace xen {

class Oscillator
{
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void setWaveform(WaveformType w) { waveform_ = w; }
    WaveformType waveform() const { return waveform_; }

    void setFrequency(double hz)
    {
        freq_ = clamp(hz, 0.01, sampleRate_ * 0.49);
        inc_ = freq_ / sampleRate_;
    }

    void reset(double phase = 0.0)
    {
        phase_ = phase - std::floor(phase);
        triangleState_ = 0.0;
    }

    float process()
    {
        const double t = phase_;
        double y = 0.0;
        switch (waveform_)
        {
            case WaveformType::Sine:
                y = std::sin(kTwoPi * t);
                break;
            case WaveformType::Saw:
                y = 2.0 * t - 1.0 - polyBlep(t, inc_);
                break;
            case WaveformType::Square:
            {
                y = t < 0.5 ? 1.0 : -1.0;
                y += polyBlep(t, inc_);
                y -= polyBlep(std::fmod(t + 0.5, 1.0), inc_);
                break;
            }
            case WaveformType::Triangle:
            {
                const double square = t < 0.5 ? 1.0 : -1.0;
                triangleState_ = clamp(triangleState_ + inc_ * 4.0 * square, -1.0, 1.0);
                y = triangleState_;
                break;
            }
            case WaveformType::Gendyn:
                y = std::sin(kTwoPi * t);
                break;
        }
        phase_ += inc_;
        if (phase_ >= 1.0)
            phase_ -= 1.0;
        return (float)y;
    }

private:
    static double polyBlep(double t, double dt)
    {
        if (dt <= 0.0)
            return 0.0;
        if (t < dt)
        {
            const double x = t / dt;
            return x + x - x * x - 1.0;
        }
        if (t > 1.0 - dt)
        {
            const double x = (t - 1.0) / dt;
            return x * x + x + x + 1.0;
        }
        return 0.0;
    }

    double sampleRate_ = 44100.0;
    double freq_ = 440.0;
    double inc_ = 440.0 / 44100.0;
    double phase_ = 0.0;
    double triangleState_ = 0.0;
    WaveformType waveform_ = WaveformType::Sine;
};

} // namespace xen
