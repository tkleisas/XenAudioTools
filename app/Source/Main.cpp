// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

#include "xen/engine.h"
#include "xen/page_model.h"
#include "xen/ui/XenEditorPanel.h"

class XenakisMainComponent : public juce::AudioAppComponent
{
public:
    explicit XenakisMainComponent(const juce::File& preset)
        : panel_(model_, engine_, false)
    {
        setOpaque(true);
        addAndMakeVisible(panel_);
        setSize(1120, 740);

        model_.setPublisher([this](std::shared_ptr<const xen::Page> page)
        {
            engine_.setPage(std::move(page));
        });
        if (preset.existsAsFile())
        {
            const juce::String text = preset.loadFileAsString();
            if (text.isNotEmpty())
                model_.deserialize(text.toStdString());
        }
        model_.publish();

        setAudioChannels(0, 2);
    }

    ~XenakisMainComponent() override
    {
        shutdownAudio();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        engine_.prepare(sampleRate, samplesPerBlockExpected);
        model_.publish();
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        if (info.buffer == nullptr || info.numSamples <= 0)
            return;
        info.clearActiveBufferRegion();
        auto* left = info.buffer->getWritePointer(0, info.startSample);
        auto* right = info.buffer->getNumChannels() > 1
            ? info.buffer->getWritePointer(1, info.startSample)
            : left;
        engine_.render(left, right, info.numSamples);
    }

    void releaseResources() override
    {
        engine_.reset();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff101216));
    }

    void resized() override
    {
        panel_.setBounds(getLocalBounds());
    }

private:
    xen::Engine engine_;
    xen::PageModel model_;
    xen::ui::XenEditorPanel panel_;
};

class XenakisMainWindow : public juce::DocumentWindow
{
public:
    XenakisMainWindow(const juce::String& name, const juce::File& preset)
        : juce::DocumentWindow(name, juce::Colour(0xff101216), juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new XenakisMainComponent(preset), true);
        setResizable(true, true);
        setResizeLimits(780, 520, 2600, 1500);
        centreWithSize(1120, 740);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class XenakisUPICApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Xenakis UPIC Studio"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        juce::File preset;
        juce::StringArray arguments = juce::StringArray::fromTokens(commandLine, " ", "\"");
        arguments.removeEmptyStrings();
        if (!arguments.isEmpty())
        {
            preset = juce::File::getCurrentWorkingDirectory().getChildFile(arguments[0].unquoted());
            if (!preset.existsAsFile())
                preset = juce::File(arguments[0].unquoted());
        }
        window_ = std::make_unique<XenakisMainWindow>(getApplicationName(), preset);
    }

    void shutdown() override
    {
        window_ = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    std::unique_ptr<XenakisMainWindow> window_;
};

START_JUCE_APPLICATION(XenakisUPICApplication)
