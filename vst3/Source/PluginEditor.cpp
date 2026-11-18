// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include "PluginEditor.h"

XenakisUPICEditor::XenakisUPICEditor(XenakisUPICProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
      processor_(processor),
      panel_(processor.pageModel(), processor.engine(), true)
{
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(780, 520, 2600, 1500);
    addAndMakeVisible(panel_);

    panel_.onStateChanged = [this]()
    {
        processor_.markDirty();
    };
    panel_.onTransportChanged = [this](bool run, bool loop, double speed, bool reverse)
    {
        processor_.setTransportParams(run, loop, speed, reverse);
    };

    setSize(1120, 740);
}

void XenakisUPICEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101216));
}

void XenakisUPICEditor::resized()
{
    panel_.setBounds(getLocalBounds());
}
