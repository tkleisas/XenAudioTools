// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

#include "xen/engine.h"
#include "xen/page_model.h"

namespace Steinberg { namespace Vst { struct IComponentHandler; } }

class XenakisUPICProcessor : public juce::AudioProcessor,
                             public juce::VST3ClientExtensions
{
public:
    XenakisUPICProcessor();
    ~XenakisUPICProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Xenakis UPIC"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    xen::Engine& engine() { return engine_; }
    xen::PageModel& pageModel() { return model_; }
    juce::AudioProcessorValueTreeState& apvts() { return apvts_; }

    void markDirty();
    void setTransportParams(bool run, bool loop, double speed, bool reverse);
    void setIComponentHandler(Steinberg::FUnknown* handler) override;

private:
    void applyState(const juce::MemoryBlock& block);

    juce::AudioProcessorValueTreeState apvts_;
    xen::Engine engine_;
    xen::PageModel model_;
    std::atomic<Steinberg::Vst::IComponentHandler*> componentHandler_ { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XenakisUPICProcessor)
};
