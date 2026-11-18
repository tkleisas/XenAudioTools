// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include "PluginProcessor.h"

#include <memory>

#include <pluginterfaces/vst/ivsteditcontroller.h>

#include "PluginEditor.h"

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout buildLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using BoolParam = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto addFloat = [&layout](const char* id, const char* name, float lo, float hi,
                                    float def, float step, const char* label)
    {
        layout.add(std::make_unique<FloatParam>(
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float>(lo, hi, step), def,
            juce::AudioParameterFloatAttributes().withLabel(label)));
    };
    const auto addBool = [&layout](const char* id, const char* name, bool def)
    {
        layout.add(std::make_unique<BoolParam>(juce::ParameterID { id, 1 }, name, def));
    };

    addFloat("master", "Master", -60.0f, 12.0f, 0.0f, 0.1f, "dB");
    addFloat("page", "Page", -60.0f, 12.0f, 0.0f, 0.1f, "dB");
    addFloat("live", "MIDI", -60.0f, 12.0f, 0.0f, 0.1f, "dB");
    addFloat("speed", "Speed", 0.05f, 4.0f, 1.0f, 0.01f, "x");
    addBool("run", "Run", false);
    addBool("loop", "Loop", true);
    addBool("reverse", "Reverse", false);
    return layout;
}

} // namespace

XenakisUPICProcessor::XenakisUPICProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "XENAKIS_UPIC", buildLayout())
{
    model_.setPublisher([this](std::shared_ptr<const xen::Page> page)
    {
        engine_.setPage(std::move(page));
    });
    model_.publish();
}

XenakisUPICProcessor::~XenakisUPICProcessor()
{
    if (auto* handler = componentHandler_.exchange(nullptr))
        handler->release();
}

void XenakisUPICProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, samplesPerBlock);
    model_.publish();
}

void XenakisUPICProcessor::releaseResources()
{
    engine_.reset();
}

bool XenakisUPICProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (!layouts.getMainInputChannelSet().isDisabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void XenakisUPICProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    buffer.clear();

    const float master = apvts_.getRawParameterValue("master")->load();
    const float pageGain = apvts_.getRawParameterValue("page")->load();
    const float live = apvts_.getRawParameterValue("live")->load();
    const bool run = apvts_.getRawParameterValue("run")->load() > 0.5f;
    const bool loop = apvts_.getRawParameterValue("loop")->load() > 0.5f;
    const bool reverse = apvts_.getRawParameterValue("reverse")->load() > 0.5f;
    const double speed = (double)apvts_.getRawParameterValue("speed")->load();

    engine_.setTransport(run, loop, speed, reverse);
    engine_.setGains(master, pageGain, live);

    auto* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    int position = 0;
    for (const auto metadata : midi)
    {
        const int sample = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (sample > position)
        {
            engine_.render(left + position, right + position, sample - position);
            position = sample;
        }
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            engine_.noteOn(message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            engine_.noteOff(message.getNoteNumber());
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            engine_.allNotesOff();
    }
    if (position < numSamples)
        engine_.render(left + position, right + position, numSamples - position);

    for (int channel = 2; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);
}

juce::AudioProcessorEditor* XenakisUPICProcessor::createEditor()
{
    return new XenakisUPICEditor(*this);
}

void XenakisUPICProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement root("XENAKIS_UPIC_STATE");
    root.setAttribute("version", 1);

    if (auto params = apvts_.copyState().createXml())
        root.createNewChildElement("PARAMS")->addChildElement(params.release());

    root.createNewChildElement("PAGE")->addTextElement(model_.serialize());

    juce::MemoryBlock block;
    copyXmlToBinary(root, block);
    destData = block;
}

void XenakisUPICProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryBlock copy(data, (size_t)sizeInBytes);
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        applyState(copy);
        return;
    }
    juce::MessageManager::callAsync([this, copy]() mutable { applyState(copy); });
}

void XenakisUPICProcessor::applyState(const juce::MemoryBlock& block)
{
    auto xml = getXmlFromBinary(block.getData(), (int)block.getSize());
    if (xml == nullptr)
        return;

    if (auto* params = xml->getChildByName("PARAMS"))
    {
        if (auto* child = params->getFirstChildElement())
        {
            const auto tree = juce::ValueTree::fromXml(*child);
            if (tree.isValid())
                apvts_.replaceState(tree);
        }
    }

    if (auto* page = xml->getChildByName("PAGE"))
    {
        const juce::String text = page->getAllSubText();
        if (text.isNotEmpty())
            model_.deserialize(text.toStdString());
    }

    model_.publish();
}

void XenakisUPICProcessor::markDirty()
{
    updateHostDisplay();
    if (auto* handler = componentHandler_.load())
        handler->restartComponent(Steinberg::Vst::RestartFlags::kParamValuesChanged);
}

void XenakisUPICProcessor::setIComponentHandler(Steinberg::FUnknown* handler)
{
    if (auto* old = componentHandler_.exchange(nullptr))
        old->release();
    if (handler == nullptr)
        return;
    Steinberg::Vst::IComponentHandler* vstHandler = nullptr;
    if (handler->queryInterface(Steinberg::Vst::IComponentHandler::iid, (void**)&vstHandler) == Steinberg::kResultOk)
        componentHandler_.store(vstHandler);
}

void XenakisUPICProcessor::setTransportParams(bool run, bool loop, double speed, bool reverse)
{
    const auto setBool = [this](const char* id, bool value)
    {
        if (auto* parameter = apvts_.getParameter(id))
            parameter->setValueNotifyingHost(value ? 1.0f : 0.0f);
    };
    setBool("run", run);
    setBool("loop", loop);
    setBool("reverse", reverse);
    if (auto* parameter = apvts_.getParameter("speed"))
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float)speed));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XenakisUPICProcessor();
}
