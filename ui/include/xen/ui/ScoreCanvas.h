// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "xen/engine.h"
#include "xen/page_model.h"
#include "xen/upic.h"

namespace xen::ui {

class ScoreCanvas : public juce::Component
{
public:
    enum class Tool { Draw, Erase, Select };

    ScoreCanvas(PageModel& model, Engine& engine);

    void setTool(Tool t) { tool_ = t; repaint(); }
    Tool tool() const { return tool_; }

    void setBrushWaveform(WaveformType w) { brushWaveform_ = w; }
    WaveformType brushWaveform() const { return brushWaveform_; }
    void setBrushWeight(float w) { brushWeight_ = juce::jlimit(0.0f, 1.0f, w); }
    float brushWeight() const { return brushWeight_; }
    void setBrushGroup(int g) { brushGroup_ = juce::jmax(0, g); }
    int brushGroup() const { return brushGroup_; }

    void setFreehand(bool v) { freehand_ = v; }
    bool freehand() const { return freehand_; }
    void setSnapPitch(bool v) { snapPitch_ = v; }
    bool snapPitch() const { return snapPitch_; }
    void setSnapTime(bool v) { snapTime_ = v; }
    bool snapTime() const { return snapTime_; }

    void setViewStart(double t);
    double viewStart() const { return viewStart_; }
    void setViewDuration(double d);
    double viewDuration() const { return viewDuration_; }
    void fitToPage();
    void zoomBy(double factor);

    int selectedArc() const { return selectedArc_; }
    void setSelectedArc(int index) { selectedArc_ = index; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    std::function<void()> onEdited;

private:
    float timeToX(double t) const;
    double xToTime(float x) const;
    float pitchToY(float p) const;
    float yToPitch(float y) const;
    float snapPitchValue(float p) const;
    double snapTimeValue(double t) const;
    int arcNear(int x, int y, double tolerancePx) const;
    void drawGrid(juce::Graphics& g);
    void drawArc(juce::Graphics& g, const Arc& arc, bool selected);
    void pruneEmptyArcs();

    PageModel& model_;
    Engine& engine_;
    Tool tool_ = Tool::Draw;
    WaveformType brushWaveform_ = WaveformType::Gendyn;
    float brushWeight_ = 0.5f;
    int brushGroup_ = 0;

    bool freehand_ = false;
    bool snapPitch_ = true;
    bool snapTime_ = false;

    double viewStart_ = 0.0;
    double viewDuration_ = 30.0;
    int selectedArc_ = -1;

    bool drawing_ = false;
    bool erasing_ = false;
    bool draggingSelection_ = false;
    ArcPoint dragStart_ {};
    ArcPoint dragCurrent_ {};
    std::vector<ArcPoint> freehandPoints_;

    juce::Colour groupColours_[8] = {
        juce::Colour(0xff5ec8e5), juce::Colour(0xffe58e5e), juce::Colour(0xff9be55e),
        juce::Colour(0xffe55eb0), juce::Colour(0xffc9b25e), juce::Colour(0xff7e6ee5),
        juce::Colour(0xff5ee5a1), juce::Colour(0xffe55e5e)
    };
};

} // namespace xen::ui
