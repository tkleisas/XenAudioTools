// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "xen/engine.h"
#include "xen/page_model.h"
#include "xen/ui/ScoreCanvas.h"

namespace xen::ui {

class XenEditorPanel : public juce::Component,
                       private juce::Timer
{
public:
    XenEditorPanel(PageModel& model, Engine& engine, bool pluginMode);
    ~XenEditorPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshFromModel();
    ScoreCanvas& canvas() { return canvas_; }

    std::function<void()> onStateChanged;
    std::function<void(bool run, bool loop, double speed, bool reverse)> onTransportChanged;
    std::function<void(const juce::String&)> onStatus;

    void setStatus(const juce::String& text);
    void setPlaying(bool playing);

private:
    void timerCallback() override;

    juce::Component* makeScorePanel();
    juce::Component* makeSievePanel();
    juce::Component* makeMarkovPanel();
    juce::Component* makeCloudPanel();
    juce::Component* makeStrategyPanel();
    juce::Component* makeGendynPanel();

    void syncTransport();
    void publish();
    void pushUndo();
    void restoreFromHistory(int direction);
    void refreshTools();
    void applySelectedArc();
    void applyGendyn();

    void newPage();
    void clearPage();
    void openFile();
    void saveFile();
    void chooseFile(bool save);

    void generateSieve();
    void generateMarkov();
    void generateCloud();
    void generateStrategy();

    void updateSelectedArcControls();

    PageModel& model_;
    Engine& engine_;
    bool pluginMode_ = false;

    ScoreCanvas canvas_;

    juce::Label titleLabel_;
    juce::Label statusLabel_;

    juce::TextButton newButton_ { "New" };
    juce::TextButton openButton_ { "Open" };
    juce::TextButton saveButton_ { "Save" };
    juce::TextButton undoButton_ { "Undo" };
    juce::TextButton redoButton_ { "Redo" };
    juce::TextButton fitButton_ { "Fit" };
    juce::TextButton zoomInButton_ { "Zoom+" };
    juce::TextButton zoomOutButton_ { "Zoom-" };

    juce::ComboBox toolBox_;
    juce::ComboBox waveBox_;
    juce::ComboBox groupBox_;
    juce::Slider weightSlider_;
    juce::Label weightLabel_;
    juce::ToggleButton freehandToggle_ { "Freehand" };
    juce::ToggleButton snapPitchToggle_ { "Snap pitch" };
    juce::ToggleButton snapTimeToggle_ { "Snap time" };

    juce::TextButton playButton_ { "Play" };
    juce::ToggleButton loopToggle_ { "Loop" };
    juce::ToggleButton reverseToggle_ { "Reverse" };
    juce::Slider speedSlider_;
    juce::Slider positionSlider_;
    juce::Label positionLabel_;
    juce::Label speedLabel_;

    juce::TabbedComponent tabs_ { juce::TabbedButtonBar::TabsAtRight };

    std::unique_ptr<juce::Component> scorePanel_;
    std::unique_ptr<juce::Component> sievePanel_;
    std::unique_ptr<juce::Component> markovPanel_;
    std::unique_ptr<juce::Component> cloudPanel_;
    std::unique_ptr<juce::Component> strategyPanel_;
    std::unique_ptr<juce::Component> gendynPanel_;

    juce::TextButton clearButton_ { "Clear page" };
    juce::Slider attackSlider_;
    juce::Slider releaseSlider_;
    juce::Slider arcWeightSlider_;
    juce::ComboBox arcWaveBox_;
    juce::ToggleButton arcMuteToggle_ { "Mute arc" };
    juce::Label arcInfoLabel_;
    juce::Slider groupTransposeSlider_;
    juce::ToggleButton groupMuteToggle_ { "Mute group" };
    juce::ToggleButton groupSoloToggle_ { "Solo group" };

    juce::ComboBox sievePresetBox_;
    juce::TextEditor sieveEditor_;
    juce::ComboBox sieveModeBox_;
    juce::Slider sieveLowSlider_;
    juce::Slider sieveHighSlider_;
    juce::Slider sieveDurationSlider_;
    juce::TextButton sieveGenerateButton_ { "Generate sieve" };

    juce::Slider markovStatesSlider_;
    juce::Slider markovBiasSlider_;
    juce::Slider markovDurationSlider_;
    juce::Slider markovSeedSlider_;
    juce::TextButton markovGenerateButton_ { "Generate Markov" };

    juce::Slider cloudDensitySlider_;
    juce::Slider cloudPitchSlider_;
    juce::Slider cloudSpreadSlider_;
    juce::Slider cloudDurationSlider_;
    juce::Slider cloudAmpSlider_;
    juce::TextButton cloudGenerateButton_ { "Generate cloud" };

    juce::Slider strategyRowsSlider_;
    juce::Slider strategyColsSlider_;
    juce::Slider strategySegmentSlider_;
    juce::ToggleButton strategyReplicatorToggle_ { "Replicator" };
    juce::TextButton strategyGenerateButton_ { "Generate strategy" };

    juce::Slider gendynSegmentsSlider_;
    juce::ComboBox gendynDistributionBox_;
    juce::Slider gendynDurationStepSlider_;
    juce::Slider gendynDurationBarrierSlider_;
    juce::Slider gendynAmpStepSlider_;
    juce::Slider gendynAmpBarrierSlider_;
    juce::ToggleButton gendynMirrorToggle_ { "Mirror" };
    juce::Slider liveAttackSlider_;
    juce::Slider liveReleaseSlider_;
    juce::TextButton gendynApplyButton_ { "Apply GENDYN" };

    std::vector<Page> undoStack_;
    std::vector<Page> redoStack_;
    std::unique_ptr<juce::FileChooser> chooser_;
    bool updating_ = false;
    bool playing_ = false;
    uint64_t lastRevision_ = 0;
    int lastSelectedArc_ = -1;
};

} // namespace xen::ui
