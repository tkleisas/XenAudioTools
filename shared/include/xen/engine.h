// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include "xen/gendyn.h"
#include "xen/osc.h"
#include "xen/upic.h"

namespace xen {

class Engine
{
public:
    static constexpr int kArcVoices = 64;
    static constexpr int kLiveVoices = 16;

    void prepare(double sampleRate, int maxBlock);
    void reset();

    void setPage(std::shared_ptr<const Page> page);
    std::shared_ptr<const Page> page() const;

    void setTransport(bool run, bool loop, double speed, bool reverse);
    void setGains(float masterDb, float pageDb, float liveDb);
    void setArcGendynParams(const GendynVoice::Params& p);
    GendynVoice::Params arcGendynParams() const;
    void setLiveGendynParams(const GendynVoice::Params& p);
    GendynVoice::Params liveGendynParams() const;
    void setLiveEnvelope(float attackSeconds, float releaseSeconds);

    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();

    void render(float* left, float* right, int numSamples);

    double playhead() const { return playheadAtomic_.load(std::memory_order_relaxed); }
    bool running() const { return run_.load(std::memory_order_relaxed); }
    void setPlayhead(double t) { playheadAtomic_.store(std::max(0.0, t), std::memory_order_relaxed); }

private:
    struct ArcVoice
    {
        Oscillator osc;
        GendynVoice gen;
        int arcIndex = -1;
        bool active = false;
        bool fading = false;
        bool gendynGate = false;
        float gain = 0.0f;
        double lastHz = 0.0;
        WaveformType waveform = WaveformType::Gendyn;
    };

    struct LiveVoice
    {
        GendynVoice gen;
        int note = -1;
        float velocity = 0.0f;
        float env = 0.0f;
        int state = 0;
        bool active = false;
    };

    void startLiveVoice(int note, int index, float velocity);
    double noteToHz(int note) const;

    double sampleRate_ = 44100.0;
    size_t voiceCursor_ = 0;

    std::shared_ptr<const Page> page_;
    mutable std::mutex paramsMutex_;
    GendynVoice::Params arcParams_ {};
    GendynVoice::Params liveParams_ {};

    std::atomic<bool> run_ { false };
    std::atomic<bool> loop_ { true };
    std::atomic<bool> reverse_ { false };
    std::atomic<double> speed_ { 1.0 };
    std::atomic<double> playheadAtomic_ { 0.0 };
    std::atomic<float> masterGain_ { 1.0f };
    std::atomic<float> pageGain_ { 1.0f };
    std::atomic<float> liveGain_ { 1.0f };

    ArcVoice arcs_[kArcVoices];
    LiveVoice live_[kLiveVoices];

    float liveAttack_ = 0.005f;
    float liveRelease_ = 0.15f;
};

} // namespace xen
