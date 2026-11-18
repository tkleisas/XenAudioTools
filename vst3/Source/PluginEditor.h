// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "xen/ui/XenEditorPanel.h"

class XenakisUPICEditor : public juce::AudioProcessorEditor
{
public:
    explicit XenakisUPICEditor(XenakisUPICProcessor& processor);
    ~XenakisUPICEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    XenakisUPICProcessor& processor_;
    xen::ui::XenEditorPanel panel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XenakisUPICEditor)
};
