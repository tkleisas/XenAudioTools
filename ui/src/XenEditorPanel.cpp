// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include "xen/ui/XenEditorPanel.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "xen/generators.h"
#include "xen/strategy.h"

namespace xen::ui {
namespace {

class StackPanel : public juce::Component
{
public:
    StackPanel() { setOpaque(true); }

    void addLabeledSlider(const juce::String& text, juce::Slider& slider, int sliderHeight = 24)
    {
        auto label = std::make_unique<juce::Label>();
        label->setText(text, juce::dontSendNotification);
        label->setFont(juce::Font(12.0f));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffaeb8c7));
        addAndMakeVisible(*label);
        labels_.push_back(std::move(label));
        addAndMakeVisible(slider);
        items_.push_back({ labels_.back().get(), 15 });
        items_.push_back({ &slider, sliderHeight });
    }

    void addLabeledEditor(const juce::String& text, juce::TextEditor& editor, int height = 48)
    {
        auto label = std::make_unique<juce::Label>();
        label->setText(text, juce::dontSendNotification);
        label->setFont(juce::Font(12.0f));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffaeb8c7));
        addAndMakeVisible(*label);
        labels_.push_back(std::move(label));
        addAndMakeVisible(editor);
        items_.push_back({ labels_.back().get(), 15 });
        items_.push_back({ &editor, height });
    }

    void addLabeledCombo(const juce::String& text, juce::ComboBox& combo, int height = 24)
    {
        auto label = std::make_unique<juce::Label>();
        label->setText(text, juce::dontSendNotification);
        label->setFont(juce::Font(12.0f));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffaeb8c7));
        addAndMakeVisible(*label);
        labels_.push_back(std::move(label));
        addAndMakeVisible(combo);
        items_.push_back({ labels_.back().get(), 15 });
        items_.push_back({ &combo, height });
    }

    void addControl(juce::Component& c, int height = 26)
    {
        addAndMakeVisible(c);
        items_.push_back({ &c, height });
    }

    void addSpacer(int height) { items_.push_back({ nullptr, height }); }

    void resized() override
    {
        int y = 6;
        const int w = std::max(20, getWidth() - 12);
        for (auto& item : items_)
        {
            if (item.component != nullptr)
                item.component->setBounds(6, y, w, item.height);
            y += item.height + 4;
        }
    }

private:
    struct Item
    {
        juce::Component* component = nullptr;
        int height = 20;
    };
    std::vector<Item> items_;
    std::vector<std::unique_ptr<juce::Label>> labels_;
};

Tuning defaultTuning()
{
    Tuning t;
    t.referenceHz = 8.175798915643707;
    t.referenceIndex = 0.0;
    t.divisionsPerOctave = 12.0;
    t.discrete = false;
    return t;
}

void styleSlider(juce::Slider& s, double lo, double hi, double step, double value, const juce::String& suffix = {})
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 20);
    s.setRange(lo, hi, step);
    s.setValue(value, juce::dontSendNotification);
    s.setTextValueSuffix(suffix);
    s.setColour(juce::Slider::trackColourId, juce::Colour(0xff5ec8e5).withAlpha(0.5f));
    s.setColour(juce::Slider::thumbColourId, juce::Colour(0xff5ec8e5));
}

void styleCombo(juce::ComboBox& c)
{
    c.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff222834));
    c.setColour(juce::ComboBox::textColourId, juce::Colour(0xffd5dbe5));
    c.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a4352));
}

Sieve parseSieveText(const juce::String& text)
{
    juce::String cleaned = text;
    cleaned = cleaned.replaceCharacters(":,;|", "    ");
    juce::StringArray tokens;
    tokens.addTokens(cleaned, " \t\r\n", "\"");
    tokens.removeEmptyStrings();
    std::vector<int> numbers;
    for (const auto& token : tokens)
    {
        const juce::String trimmed = token.trim();
        if (trimmed.isNotEmpty() && trimmed.containsOnly("-0123456789"))
            numbers.push_back(trimmed.getIntValue());
    }
    if (numbers.size() < 2)
        return Sieve::chromatic12();
    Sieve s;
    const int modulus = std::max(1, numbers[0]);
    for (size_t i = 1; i < numbers.size(); ++i)
        s.addClass(modulus, numbers[i]);
    return s;
}

juce::String sieveToText(const Sieve& sieve)
{
    const Sieve r = sieve.reduced();
    juce::String out;
    for (size_t i = 0; i < r.groups.size(); ++i)
    {
        if (i != 0)
            out << " + ";
        out << r.groups[i].front().modulus << ":" << r.groups[i].front().offset;
    }
    return out;
}

} // namespace

XenEditorPanel::XenEditorPanel(PageModel& model, Engine& engine, bool pluginMode)
    : model_(model), engine_(engine), pluginMode_(pluginMode), canvas_(model, engine)
{
    setOpaque(true);
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff101216));

    addAndMakeVisible(canvas_);
    canvas_.onEdited = [this]() { publish(); };

    titleLabel_.setText("XENAKIS UPIC", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(15.0f, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff5ec8e5));
    addAndMakeVisible(titleLabel_);

    for (auto* b : { &newButton_, &openButton_, &saveButton_, &undoButton_, &redoButton_,
                     &fitButton_, &zoomInButton_, &zoomOutButton_ })
    {
        addAndMakeVisible(*b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222834));
        b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd5dbe5));
    }

    newButton_.onClick = [this]() { newPage(); };
    openButton_.onClick = [this]() { openFile(); };
    saveButton_.onClick = [this]() { saveFile(); };
    undoButton_.onClick = [this]() { restoreFromHistory(-1); };
    redoButton_.onClick = [this]() { restoreFromHistory(1); };
    fitButton_.onClick = [this]() { canvas_.fitToPage(); };
    zoomInButton_.onClick = [this]() { canvas_.zoomBy(0.8); };
    zoomOutButton_.onClick = [this]() { canvas_.zoomBy(1.25); };

    toolBox_.addItem("Draw", 1);
    toolBox_.addItem("Erase", 2);
    toolBox_.addItem("Select", 3);
    toolBox_.setSelectedId(1, juce::dontSendNotification);
    styleCombo(toolBox_);
    toolBox_.onChange = [this]()
    {
        const int id = toolBox_.getSelectedId();
        canvas_.setTool(id == 2 ? ScoreCanvas::Tool::Erase : (id == 3 ? ScoreCanvas::Tool::Select : ScoreCanvas::Tool::Draw));
    };
    addAndMakeVisible(toolBox_);

    waveBox_.addItem("Sine", 1);
    waveBox_.addItem("Saw", 2);
    waveBox_.addItem("Square", 3);
    waveBox_.addItem("Triangle", 4);
    waveBox_.addItem("Gendyn", 5);
    waveBox_.setSelectedId(5, juce::dontSendNotification);
    styleCombo(waveBox_);
    waveBox_.onChange = [this]()
    {
        const int id = waveBox_.getSelectedId();
        canvas_.setBrushWaveform((WaveformType)juce::jlimit(0, 4, id - 1));
    };
    addAndMakeVisible(waveBox_);

    styleCombo(groupBox_);
    groupBox_.onChange = [this]()
    {
        const int index = juce::jmax(0, groupBox_.getSelectedId() - 1);
        canvas_.setBrushGroup(index);
        updateSelectedArcControls();
    };
    addAndMakeVisible(groupBox_);

    styleSlider(weightSlider_, 0.0, 1.0, 0.01, 0.5);
    weightSlider_.onValueChange = [this]()
    {
        canvas_.setBrushWeight((float)weightSlider_.getValue());
        if (!updating_)
            applySelectedArc();
    };
    addAndMakeVisible(weightSlider_);
    weightLabel_.setText("Weight", juce::dontSendNotification);
    weightLabel_.setFont(juce::Font(11.0f));
    weightLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8d98aa));
    addAndMakeVisible(weightLabel_);

    freehandToggle_.onClick = [this]() { canvas_.setFreehand(freehandToggle_.getToggleState()); };
    snapPitchToggle_.setToggleState(true, juce::dontSendNotification);
    snapPitchToggle_.onClick = [this]() { canvas_.setSnapPitch(snapPitchToggle_.getToggleState()); };
    snapTimeToggle_.onClick = [this]() { canvas_.setSnapTime(snapTimeToggle_.getToggleState()); };
    for (auto* t : { &freehandToggle_, &snapPitchToggle_, &snapTimeToggle_ })
    {
        t->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaeb8c7));
        t->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff5ec8e5));
    }

    playButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c7a5a));
    playButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    playButton_.onClick = [this]() { setPlaying(!playing_); };
    addAndMakeVisible(playButton_);

    loopToggle_.setToggleState(true, juce::dontSendNotification);
    loopToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaeb8c7));
    loopToggle_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff5ec8e5));
    loopToggle_.onClick = [this]() { syncTransport(); };
    addAndMakeVisible(loopToggle_);

    reverseToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaeb8c7));
    reverseToggle_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff5ec8e5));
    reverseToggle_.onClick = [this]() { syncTransport(); };
    addAndMakeVisible(reverseToggle_);

    styleSlider(speedSlider_, 0.05, 4.0, 0.01, 1.0, "x");
    speedSlider_.onValueChange = [this]() { syncTransport(); };
    addAndMakeVisible(speedSlider_);
    speedLabel_.setText("Speed", juce::dontSendNotification);
    speedLabel_.setFont(juce::Font(11.0f));
    speedLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8d98aa));
    addAndMakeVisible(speedLabel_);

    styleSlider(positionSlider_, 0.0, 60.0, 0.001, 0.0, "s");
    positionSlider_.onValueChange = [this]()
    {
        if (!updating_)
            engine_.setPlayhead(positionSlider_.getValue());
    };
    addAndMakeVisible(positionSlider_);
    positionLabel_.setText("0:00.0", juce::dontSendNotification);
    positionLabel_.setFont(juce::Font(12.0f));
    positionLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffd5dbe5));
    addAndMakeVisible(positionLabel_);

    scorePanel_.reset(makeScorePanel());
    sievePanel_.reset(makeSievePanel());
    markovPanel_.reset(makeMarkovPanel());
    cloudPanel_.reset(makeCloudPanel());
    strategyPanel_.reset(makeStrategyPanel());
    gendynPanel_.reset(makeGendynPanel());

    tabs_.setTabBarDepth(62);
    tabs_.setOutline(0);
    tabs_.addTab("Score", juce::Colour(0xff1b2029), scorePanel_.get(), false);
    tabs_.addTab("Sieve", juce::Colour(0xff1b2029), sievePanel_.get(), false);
    tabs_.addTab("Markov", juce::Colour(0xff1b2029), markovPanel_.get(), false);
    tabs_.addTab("Cloud", juce::Colour(0xff1b2029), cloudPanel_.get(), false);
    tabs_.addTab("Game", juce::Colour(0xff1b2029), strategyPanel_.get(), false);
    tabs_.addTab("GENDYN", juce::Colour(0xff1b2029), gendynPanel_.get(), false);
    addAndMakeVisible(tabs_);

    statusLabel_.setFont(juce::Font(11.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8d98aa));
    statusLabel_.setText("Draw arcs on the page. June 2026.", juce::dontSendNotification);
    addAndMakeVisible(statusLabel_);

    syncTransport();
    refreshFromModel();
    startTimerHz(30);
}

XenEditorPanel::~XenEditorPanel()
{
    stopTimer();
    tabs_.clearTabs();
}

void XenEditorPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101216));
    g.setColour(juce::Colour(0xff1c2230));
    g.fillRect(0, 0, getWidth(), 34);
    g.fillRect(0, 34, getWidth(), 32);
    g.setColour(juce::Colour(0xff2c3543));
    g.drawHorizontalLine(33, 0.0f, (float)getWidth());
    g.drawHorizontalLine(65, 0.0f, (float)getWidth());
}

void XenEditorPanel::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop(34);
    titleLabel_.setBounds(top.removeFromLeft(140).reduced(6, 4));

    const int bw = 48;
    const int gap = 2;
    for (auto* b : { &newButton_, &openButton_, &saveButton_, &undoButton_, &redoButton_ })
    {
        b->setBounds(top.removeFromLeft(bw).reduced(gap, 4));
        top.removeFromLeft(1);
    }
    top.removeFromLeft(6);
    for (auto* b : { &fitButton_, &zoomOutButton_, &zoomInButton_ })
    {
        b->setBounds(top.removeFromLeft(60).reduced(gap, 4));
        top.removeFromLeft(1);
    }
    top.removeFromLeft(6);
    toolBox_.setBounds(top.removeFromLeft(84).reduced(2, 5));
    waveBox_.setBounds(top.removeFromLeft(84).reduced(2, 5));
    groupBox_.setBounds(top.removeFromLeft(84).reduced(2, 5));
    weightLabel_.setBounds(top.removeFromLeft(44).reduced(2, 6));
    weightSlider_.setBounds(top.removeFromLeft(100).reduced(2, 6));

    auto transport = area.removeFromTop(32);
    playButton_.setBounds(transport.removeFromLeft(64).reduced(4, 4));
    loopToggle_.setBounds(transport.removeFromLeft(60).reduced(2, 4));
    reverseToggle_.setBounds(transport.removeFromLeft(74).reduced(2, 4));
    speedLabel_.setBounds(transport.removeFromLeft(40).reduced(2, 6));
    speedSlider_.setBounds(transport.removeFromLeft(130).reduced(2, 6));
    positionLabel_.setBounds(transport.removeFromLeft(62).reduced(2, 6));
    positionSlider_.setBounds(transport.reduced(2, 6));

    auto bottom = area.removeFromBottom(20);
    statusLabel_.setBounds(bottom.reduced(6, 2));

    int sideWidth = juce::jlimit(240, 360, area.getWidth() / 3);
    if (area.getWidth() < 720)
        sideWidth = 240;
    auto side = area.removeFromRight(sideWidth);
    tabs_.setBounds(side);
    canvas_.setBounds(area.reduced(2));
}

juce::Component* XenEditorPanel::makeScorePanel()
{
    auto* p = new StackPanel();
    p->setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff161b23));

    arcInfoLabel_.setFont(juce::Font(11.0f));
    arcInfoLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffaeb8c7));
    arcInfoLabel_.setText("No arc selected", juce::dontSendNotification);
    p->addControl(arcInfoLabel_, 34);
    p->addControl(freehandToggle_, 22);
    p->addControl(snapPitchToggle_, 22);
    p->addControl(snapTimeToggle_, 22);

    styleSlider(attackSlider_, 0.0, 2.0, 0.001, 0.01, "s");
    attackSlider_.onValueChange = [this]()
    {
        if (updating_)
            return;
        Page& page = model_.pageRef();
        if (!page.envelopes.empty())
        {
            page.envelopes[0] = Envelope::ad((float)attackSlider_.getValue(), (float)releaseSlider_.getValue());
            publish();
        }
    };
    p->addLabeledSlider("Page attack", attackSlider_);

    styleSlider(releaseSlider_, 0.0, 4.0, 0.001, 0.05, "s");
    releaseSlider_.onValueChange = [this]()
    {
        if (updating_)
            return;
        Page& page = model_.pageRef();
        if (!page.envelopes.empty())
        {
            page.envelopes[0] = Envelope::ad((float)attackSlider_.getValue(), (float)releaseSlider_.getValue());
            publish();
        }
    };
    p->addLabeledSlider("Page release", releaseSlider_);

    styleSlider(arcWeightSlider_, 0.0, 1.0, 0.01, 0.5);
    arcWeightSlider_.onValueChange = [this]()
    {
        if (!updating_)
            applySelectedArc();
    };
    p->addLabeledSlider("Selected arc weight", arcWeightSlider_);

    arcWaveBox_.addItem("Sine", 1);
    arcWaveBox_.addItem("Saw", 2);
    arcWaveBox_.addItem("Square", 3);
    arcWaveBox_.addItem("Triangle", 4);
    arcWaveBox_.addItem("Gendyn", 5);
    styleCombo(arcWaveBox_);
    arcWaveBox_.onChange = [this]()
    {
        if (!updating_)
            applySelectedArc();
    };
    p->addLabeledCombo("Selected arc waveform", arcWaveBox_);

    arcMuteToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaeb8c7));
    arcMuteToggle_.onClick = [this]()
    {
        if (!updating_)
            applySelectedArc();
    };
    p->addControl(arcMuteToggle_, 22);

    styleSlider(groupTransposeSlider_, -24.0, 24.0, 1.0, 0.0, " st");
    groupTransposeSlider_.onValueChange = [this]()
    {
        if (updating_)
            return;
        const int g = juce::jlimit(0, (int)model_.pageRef().groups.size() - 1, groupBox_.getSelectedId() - 1);
        model_.pageRef().groups[(size_t)g].transposeSemitones = (float)groupTransposeSlider_.getValue();
        publish();
    };
    p->addLabeledSlider("Group transpose", groupTransposeSlider_);

    groupMuteToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaeb8c7));
    groupMuteToggle_.onClick = [this]()
    {
        if (updating_)
            return;
        const int g = juce::jlimit(0, (int)model_.pageRef().groups.size() - 1, groupBox_.getSelectedId() - 1);
        model_.pageRef().groups[(size_t)g].muted = groupMuteToggle_.getToggleState();
        publish();
    };
    p->addControl(groupMuteToggle_, 22);

    groupSoloToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaeb8c7));
    groupSoloToggle_.onClick = [this]()
    {
        if (updating_)
            return;
        const int g = juce::jlimit(0, (int)model_.pageRef().groups.size() - 1, groupBox_.getSelectedId() - 1);
        model_.pageRef().groups[(size_t)g].solo = groupSoloToggle_.getToggleState();
        publish();
    };
    p->addControl(groupSoloToggle_, 22);

    p->addSpacer(8);
    p->addControl(clearButton_, 28);
    clearButton_.onClick = [this]() { clearPage(); };

    return p;
}

juce::Component* XenEditorPanel::makeSievePanel()
{
    auto* p = new StackPanel();
    sievePresetBox_.addItem("Diatonic", 1);
    sievePresetBox_.addItem("Chromatic", 2);
    sievePresetBox_.addItem("Whole tone", 3);
    sievePresetBox_.addItem("Messiaen 3", 4);
    sievePresetBox_.addItem("Harmonic 7", 5);
    sievePresetBox_.addItem("Custom", 6);
    styleCombo(sievePresetBox_);
    sievePresetBox_.onChange = [this]()
    {
        switch (sievePresetBox_.getSelectedId())
        {
            case 1: sieveEditor_.setText("12: 0 2 4 5 7 9 11"); break;
            case 2: sieveEditor_.setText("1: 0"); break;
            case 3: sieveEditor_.setText("12: 0 2 4 6 8 10"); break;
            case 4: sieveEditor_.setText("12: 0 2 3 4 6 7 8 10 11"); break;
            case 5: sieveEditor_.setText("7: 0 2 4 5 7 9 11"); break;
            default: break;
        }
    };
    p->addLabeledCombo("Preset", sievePresetBox_);

    sieveEditor_.setMultiLine(true);
    sieveEditor_.setReturnKeyStartsNewLine(false);
    sieveEditor_.setText("12: 0 2 4 5 7 9 11");
    sieveEditor_.setFont(juce::Font(12.0f));
    sieveEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff11151c));
    sieveEditor_.setColour(juce::TextEditor::textColourId, juce::Colour(0xffd5dbe5));
    sieveEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a4352));
    p->addLabeledEditor("Sieve (modulus then offsets)", sieveEditor_, 46);

    sieveModeBox_.addItem("Field (sustained)", 1);
    sieveModeBox_.addItem("Arpeggio", 2);
    sieveModeBox_.addItem("Rhythm", 3);
    sieveModeBox_.setSelectedId(2, juce::dontSendNotification);
    styleCombo(sieveModeBox_);
    p->addLabeledCombo("Mode", sieveModeBox_);

    styleSlider(sieveLowSlider_, 0.0, 127.0, 1.0, 24.0);
    p->addLabeledSlider("Low index", sieveLowSlider_);
    styleSlider(sieveHighSlider_, 0.0, 127.0, 1.0, 96.0);
    p->addLabeledSlider("High index", sieveHighSlider_);
    styleSlider(sieveDurationSlider_, 0.02, 2.0, 0.01, 0.25, "s");
    p->addLabeledSlider("Note duration", sieveDurationSlider_);
    p->addControl(sieveGenerateButton_, 30);
    sieveGenerateButton_.onClick = [this]() { generateSieve(); };

    return p;
}

juce::Component* XenEditorPanel::makeMarkovPanel()
{
    auto* p = new StackPanel();
    styleSlider(markovStatesSlider_, 2.0, 16.0, 1.0, 6.0);
    p->addLabeledSlider("States", markovStatesSlider_);
    styleSlider(markovBiasSlider_, 0.0, 8.0, 0.1, 1.0);
    p->addLabeledSlider("Self bias", markovBiasSlider_);
    styleSlider(markovDurationSlider_, 0.02, 2.0, 0.01, 0.25, "s");
    p->addLabeledSlider("Note duration", markovDurationSlider_);
    styleSlider(markovSeedSlider_, 1.0, 99999.0, 1.0, 1234.0);
    p->addLabeledSlider("Seed", markovSeedSlider_);
    p->addControl(markovGenerateButton_, 30);
    markovGenerateButton_.onClick = [this]() { generateMarkov(); };
    return p;
}

juce::Component* XenEditorPanel::makeCloudPanel()
{
    auto* p = new StackPanel();
    styleSlider(cloudDensitySlider_, 0.2, 400.0, 0.1, 28.0, "/s");
    p->addLabeledSlider("Density", cloudDensitySlider_);
    styleSlider(cloudPitchSlider_, 20.0, 8000.0, 1.0, 440.0, " Hz");
    p->addLabeledSlider("Pitch centre", cloudPitchSlider_);
    styleSlider(cloudSpreadSlider_, 0.0, 48.0, 0.1, 12.0, " st");
    p->addLabeledSlider("Pitch spread", cloudSpreadSlider_);
    styleSlider(cloudDurationSlider_, 0.002, 1.0, 0.001, 0.06, "s");
    p->addLabeledSlider("Grain duration", cloudDurationSlider_);
    styleSlider(cloudAmpSlider_, 0.0, 1.0, 0.01, 0.25);
    p->addLabeledSlider("Grain amplitude", cloudAmpSlider_);
    p->addControl(cloudGenerateButton_, 30);
    cloudGenerateButton_.onClick = [this]() { generateCloud(); };
    return p;
}

juce::Component* XenEditorPanel::makeStrategyPanel()
{
    auto* p = new StackPanel();
    styleSlider(strategyRowsSlider_, 2.0, 8.0, 1.0, 3.0);
    p->addLabeledSlider("Player A strategies", strategyRowsSlider_);
    styleSlider(strategyColsSlider_, 2.0, 8.0, 1.0, 3.0);
    p->addLabeledSlider("Player B strategies", strategyColsSlider_);
    styleSlider(strategySegmentSlider_, 0.02, 2.0, 0.01, 0.3, "s");
    p->addLabeledSlider("Segment duration", strategySegmentSlider_);
    strategyReplicatorToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaeb8c7));
    p->addControl(strategyReplicatorToggle_, 22);
    p->addControl(strategyGenerateButton_, 30);
    strategyGenerateButton_.onClick = [this]() { generateStrategy(); };
    return p;
}

juce::Component* XenEditorPanel::makeGendynPanel()
{
    auto* p = new StackPanel();
    styleSlider(gendynSegmentsSlider_, 2.0, 32.0, 1.0, 12.0);
    p->addLabeledSlider("Segments", gendynSegmentsSlider_);

    gendynDistributionBox_.addItem("Uniform", 1);
    gendynDistributionBox_.addItem("Gaussian", 2);
    gendynDistributionBox_.addItem("Cauchy", 3);
    gendynDistributionBox_.addItem("Logistic", 4);
    gendynDistributionBox_.addItem("Exponential", 5);
    gendynDistributionBox_.setSelectedId(3, juce::dontSendNotification);
    styleCombo(gendynDistributionBox_);
    p->addLabeledCombo("Distribution", gendynDistributionBox_);

    styleSlider(gendynDurationStepSlider_, 0.0, 1.5, 0.01, 0.45);
    p->addLabeledSlider("Duration step", gendynDurationStepSlider_);
    styleSlider(gendynDurationBarrierSlider_, 0.0, 0.95, 0.01, 0.45);
    p->addLabeledSlider("Duration barrier", gendynDurationBarrierSlider_);
    styleSlider(gendynAmpStepSlider_, 0.0, 1.0, 0.01, 0.18);
    p->addLabeledSlider("Amplitude step", gendynAmpStepSlider_);
    styleSlider(gendynAmpBarrierSlider_, 0.0, 1.0, 0.01, 0.2);
    p->addLabeledSlider("Amplitude barrier", gendynAmpBarrierSlider_);
    gendynMirrorToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaeb8c7));
    p->addControl(gendynMirrorToggle_, 22);

    styleSlider(liveAttackSlider_, 0.001, 2.0, 0.001, 0.005, "s");
    p->addLabeledSlider("MIDI attack", liveAttackSlider_);
    styleSlider(liveReleaseSlider_, 0.005, 5.0, 0.001, 0.15, "s");
    p->addLabeledSlider("MIDI release", liveReleaseSlider_);

    p->addControl(gendynApplyButton_, 30);
    gendynApplyButton_.onClick = [this]() { applyGendyn(); };
    return p;
}

void XenEditorPanel::setStatus(const juce::String& text)
{
    statusLabel_.setText(text, juce::dontSendNotification);
    if (onStatus)
        onStatus(text);
}

void XenEditorPanel::setPlaying(bool playing)
{
    playing_ = playing;
    playButton_.setButtonText(playing_ ? "Stop" : "Play");
    playButton_.setColour(juce::TextButton::buttonColourId,
                          playing_ ? juce::Colour(0xff8a3340) : juce::Colour(0xff2c7a5a));
    syncTransport();
    canvas_.repaint();
}

void XenEditorPanel::syncTransport()
{
    if (onTransportChanged)
        onTransportChanged(playing_, loopToggle_.getToggleState(), speedSlider_.getValue(), reverseToggle_.getToggleState());
    else
        engine_.setTransport(playing_, loopToggle_.getToggleState(),
                             speedSlider_.getValue(), reverseToggle_.getToggleState());
}

void XenEditorPanel::publish()
{
    model_.publish();
    lastRevision_ = model_.revision();
    canvas_.repaint();
    if (onStateChanged)
        onStateChanged();
}

void XenEditorPanel::pushUndo()
{
    undoStack_.push_back(model_.snapshot());
    if (undoStack_.size() > 40)
        undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

void XenEditorPanel::restoreFromHistory(int direction)
{
    if (direction < 0)
    {
        if (undoStack_.empty())
            return;
        redoStack_.push_back(model_.snapshot());
        Page previous = undoStack_.back();
        undoStack_.pop_back();
        model_.replace(std::move(previous));
        model_.publish();
        refreshFromModel();
        setStatus("Undo");
    }
    else
    {
        if (redoStack_.empty())
            return;
        undoStack_.push_back(model_.snapshot());
        Page next = redoStack_.back();
        redoStack_.pop_back();
        model_.replace(std::move(next));
        model_.publish();
        refreshFromModel();
        setStatus("Redo");
    }
    canvas_.repaint();
    if (onStateChanged)
        onStateChanged();
}

void XenEditorPanel::refreshTools()
{
    groupBox_.clear(juce::dontSendNotification);
    const Page& page = model_.pageRef();
    for (int i = 0; i < (int)page.groups.size(); ++i)
    {
        const juce::String groupName(page.groups[(size_t)i].name);
        groupBox_.addItem(groupName.isEmpty() ? "Group " + juce::String(i + 1) : groupName, i + 1);
    }
    if (groupBox_.getNumItems() == 0)
        groupBox_.addItem("Group A", 1);
    groupBox_.setSelectedId(juce::jlimit(1, groupBox_.getNumItems(), groupBox_.getSelectedId()), juce::dontSendNotification);
}

void XenEditorPanel::refreshFromModel()
{
    updating_ = true;
    refreshTools();
    const int selectedGroup = juce::jmax(0, groupBox_.getSelectedId() - 1);
    if (selectedGroup < (int)model_.pageRef().groups.size())
    {
        groupTransposeSlider_.setValue(model_.pageRef().groups[(size_t)selectedGroup].transposeSemitones, juce::dontSendNotification);
        groupMuteToggle_.setToggleState(model_.pageRef().groups[(size_t)selectedGroup].muted, juce::dontSendNotification);
        groupSoloToggle_.setToggleState(model_.pageRef().groups[(size_t)selectedGroup].solo, juce::dontSendNotification);
    }

    const Page& page = model_.pageRef();
    if (!page.envelopes.empty())
    {
        const Envelope& env = page.envelopes.front();
        if (env.points.size() >= 2)
            attackSlider_.setValue(env.points[1].time, juce::dontSendNotification);
        if (env.points.size() >= 4)
            releaseSlider_.setValue(1.0f - env.points[2].time, juce::dontSendNotification);
    }

    positionSlider_.setRange(0.0, std::max(1.0, page.duration), 0.001);
    positionSlider_.setValue(engine_.playhead(), juce::dontSendNotification);
    canvas_.fitToPage();
    canvas_.setBrushWeight((float)weightSlider_.getValue());
    canvas_.setBrushGroup(selectedGroup);
    updateSelectedArcControls();
    updating_ = false;
    lastRevision_ = model_.revision();
    canvas_.repaint();
}

void XenEditorPanel::updateSelectedArcControls()
{
    const int index = canvas_.selectedArc();
    const Page& page = model_.pageRef();
    if (index >= 0 && index < (int)page.arcs.size())
    {
        const Arc& arc = page.arcs[(size_t)index];
        updating_ = true;
        arcWeightSlider_.setValue(arc.weight, juce::dontSendNotification);
        arcWaveBox_.setSelectedId((int)arc.waveform + 1, juce::dontSendNotification);
        arcMuteToggle_.setToggleState(arc.muted, juce::dontSendNotification);
        arcInfoLabel_.setText("Arc " + juce::String(index + 1) + ": " + waveformName(arc.waveform)
                                  + "  w=" + juce::String(arc.weight, 2)
                                  + "  " + juce::String(arc.points.size()) + " pts  "
                                  + juce::String(arc.start(), 2) + "-" + juce::String(arc.end(), 2) + "s",
                              juce::dontSendNotification);
        updating_ = false;
    }
    else
    {
        updating_ = true;
        arcInfoLabel_.setText("No arc selected", juce::dontSendNotification);
        updating_ = false;
    }
}

void XenEditorPanel::applySelectedArc()
{
    const int index = canvas_.selectedArc();
    Page& page = model_.pageRef();
    if (index < 0 || index >= (int)page.arcs.size())
        return;
    Arc& arc = page.arcs[(size_t)index];
    arc.weight = (float)arcWeightSlider_.getValue();
    arc.waveform = (WaveformType)juce::jlimit(0, 4, arcWaveBox_.getSelectedId() - 1);
    arc.muted = arcMuteToggle_.getToggleState();
    publish();
}

void XenEditorPanel::applyGendyn()
{
    GendynVoice::Params p;
    p.segments = (int)gendynSegmentsSlider_.getValue();
    p.durationStep = gendynDurationStepSlider_.getValue();
    p.durationBarrier = gendynDurationBarrierSlider_.getValue();
    p.ampStep = gendynAmpStepSlider_.getValue();
    p.ampBarrier = gendynAmpBarrierSlider_.getValue();
    p.mirror = gendynMirrorToggle_.getToggleState();
    const int dist = gendynDistributionBox_.getSelectedId();
    p.distribution = dist == 1 ? Distribution::Uniform : dist == 2 ? Distribution::Gaussian
                   : dist == 3 ? Distribution::Cauchy : dist == 4 ? Distribution::Logistic
                   : Distribution::Exponential;
    engine_.setArcGendynParams(p);
    engine_.setLiveGendynParams(p);
    engine_.setLiveEnvelope((float)liveAttackSlider_.getValue(), (float)liveReleaseSlider_.getValue());
    setStatus("GENDYN parameters applied.");
    if (onStateChanged)
        onStateChanged();
}

void XenEditorPanel::timerCallback()
{
    if (model_.revision() != lastRevision_)
        refreshFromModel();

    const double t = engine_.playhead();
    updating_ = true;
    positionSlider_.setValue(t, juce::dontSendNotification);
    const int minutes = (int)(t / 60.0);
    const double seconds = t - minutes * 60.0;
    positionLabel_.setText(juce::String(minutes) + ":" + (seconds < 10.0 ? "0" : "")
                               + juce::String(seconds, 1),
                           juce::dontSendNotification);
    updating_ = false;

    if (playing_)
        canvas_.repaint();
    if (canvas_.selectedArc() != lastSelectedArc_)
    {
        lastSelectedArc_ = canvas_.selectedArc();
        updateSelectedArcControls();
    }
}

void XenEditorPanel::newPage()
{
    pushUndo();
    Page page;
    page.arcs.push_back(Arc::point(0.0, 4.0, 0.5f, WaveformType::Gendyn));
    page.updateDuration();
    page.loopEnd = page.duration;
    model_.replace(std::move(page));
    model_.publish();
    refreshFromModel();
    canvas_.fitToPage();
    setStatus("New page.");
    if (onStateChanged)
        onStateChanged();
}

void XenEditorPanel::clearPage()
{
    pushUndo();
    Page& page = model_.pageRef();
    page.arcs.clear();
    page.updateDuration();
    model_.publish();
    canvas_.setSelectedArc(-1);
    refreshFromModel();
    setStatus("Page cleared.");
    if (onStateChanged)
        onStateChanged();
}

void XenEditorPanel::openFile()
{
    chooseFile(false);
}

void XenEditorPanel::saveFile()
{
    chooseFile(true);
}

void XenEditorPanel::chooseFile(bool save)
{
    const juce::File initial = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                   .getChildFile(save ? "xenakis-page.xenpage" : "");
    const juce::String title = save ? "Save Xenakis page" : "Open Xenakis page";
    chooser_ = std::make_unique<juce::FileChooser>(title, initial, "*.xenpage", true);
    const int flags = save
        ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
           | juce::FileBrowserComponent::warnAboutOverwriting)
        : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles);
    chooser_->launchAsync(flags, [this, save](const juce::FileChooser& fc)
    {
        juce::File file = fc.getResult();
        if (file == juce::File())
            return;
        if (save)
        {
            if (!file.hasFileExtension("xenpage"))
                file = file.withFileExtension("xenpage");
            if (!file.replaceWithText(juce::String(model_.serialize())))
                setStatus("Could not save " + file.getFileName());
            else
                setStatus("Saved " + file.getFileName());
        }
        else
        {
            const juce::String text = file.loadFileAsString();
            if (text.isEmpty() || !model_.deserialize(text.toStdString()))
            {
                setStatus("Could not open " + file.getFileName());
                return;
            }
            undoStack_.push_back(model_.snapshot());
            model_.publish();
            refreshFromModel();
            canvas_.fitToPage();
            setStatus("Opened " + file.getFileName());
            if (onStateChanged)
                onStateChanged();
        }
    });
}

void XenEditorPanel::generateSieve()
{
    pushUndo();
    Page& page = model_.pageRef();
    page.arcs.clear();
    Sieve sieve = parseSieveText(sieveEditor_.getText());
    SieveGenParams p;
    p.t0 = 0.0;
    p.t1 = std::max(8.0, page.duration);
    p.lowIndex = (int)sieveLowSlider_.getValue();
    p.highIndex = (int)sieveHighSlider_.getValue();
    p.mode = sieveModeBox_.getSelectedId() == 1 ? SieveMode::Field
           : sieveModeBox_.getSelectedId() == 3 ? SieveMode::Rhythm
           : SieveMode::Arpeggio;
    const int waveId = waveBox_.getSelectedId();
    p.waveform = (WaveformType)juce::jlimit(0, 4, waveId - 1);
    p.weight = (float)weightSlider_.getValue();
    p.noteDuration = sieveDurationSlider_.getValue();
    p.group = juce::jmax(0, groupBox_.getSelectedId() - 1);
    xen::generateSieve(page, sieve, p, defaultTuning());
    model_.publish();
    refreshFromModel();
    canvas_.fitToPage();
    setStatus("Sieve generated: " + sieveToText(sieve));
    if (onStateChanged)
        onStateChanged();
}

void XenEditorPanel::generateMarkov()
{
    pushUndo();
    Page& page = model_.pageRef();
    page.arcs.clear();
    const int states = (int)markovStatesSlider_.getValue();
    MarkovChain chain = MarkovChain::randomMatrix(states, markovBiasSlider_.getValue(),
                                                  (uint64_t)markovSeedSlider_.getValue());
    MarkovGenParams p;
    p.t0 = 0.0;
    p.t1 = std::max(8.0, page.duration);
    p.noteDuration = markovDurationSlider_.getValue();
    p.seed = (uint64_t)markovSeedSlider_.getValue();
    const int waveId = waveBox_.getSelectedId();
    p.waveform = (WaveformType)juce::jlimit(0, 4, waveId - 1);
    p.weight = (float)weightSlider_.getValue();
    p.group = juce::jmax(0, groupBox_.getSelectedId() - 1);
    p.lowIndex = (int)sieveLowSlider_.getValue();
    p.highIndex = (int)sieveHighSlider_.getValue();
    xen::generateMarkov(page, chain, p, defaultTuning());
    model_.publish();
    refreshFromModel();
    canvas_.fitToPage();
    setStatus("Markov chain generated: " + juce::String(states) + " states.");
    if (onStateChanged)
        onStateChanged();
}

void XenEditorPanel::generateCloud()
{
    pushUndo();
    Page& page = model_.pageRef();
    page.arcs.clear();
    CloudGenParams p;
    p.t0 = 0.0;
    p.t1 = std::max(8.0, page.duration);
    p.cloud.density = cloudDensitySlider_.getValue();
    p.cloud.pitchCenter = cloudPitchSlider_.getValue();
    p.cloud.pitchSpreadSemitones = cloudSpreadSlider_.getValue();
    p.cloud.durationMean = cloudDurationSlider_.getValue();
    p.cloud.ampMean = cloudAmpSlider_.getValue();
    p.group = juce::jmax(0, groupBox_.getSelectedId() - 1);
    Rng rng((uint64_t)(juce::Time::currentTimeMillis() & 0x7fffffff));
    xen::generateCloudArcs(page, p, defaultTuning(), rng);
    model_.publish();
    refreshFromModel();
    canvas_.fitToPage();
    setStatus("Stochastic cloud generated.");
    if (onStateChanged)
        onStateChanged();
}

void XenEditorPanel::generateStrategy()
{
    pushUndo();
    Page& page = model_.pageRef();
    page.arcs.clear();
    const int rows = (int)strategyRowsSlider_.getValue();
    const int cols = (int)strategyColsSlider_.getValue();
    Rng rng((uint64_t)(juce::Time::currentTimeMillis() & 0x7fffffff));
    StrategyGame game(rows, cols);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            game.setPayoff(r, c, rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0));

    StrategyGenParams p;
    p.t0 = 0.0;
    p.t1 = std::max(8.0, page.duration);
    p.segmentDuration = strategySegmentSlider_.getValue();
    p.useReplicator = strategyReplicatorToggle_.getToggleState();
    p.lowIndex = (int)sieveLowSlider_.getValue();
    p.highIndex = (int)sieveHighSlider_.getValue();
    const int waveId = waveBox_.getSelectedId();
    p.waveform = (WaveformType)juce::jlimit(0, 4, waveId - 1);
    p.weight = (float)weightSlider_.getValue();
    p.group = juce::jmax(0, groupBox_.getSelectedId() - 1);
    xen::generateStrategy(page, game, p, defaultTuning());
    model_.publish();
    refreshFromModel();
    canvas_.fitToPage();
    setStatus("Game-theory strategy generated: " + juce::String(rows) + "x" + juce::String(cols) + ".");
    if (onStateChanged)
        onStateChanged();
}

} // namespace xen::ui
