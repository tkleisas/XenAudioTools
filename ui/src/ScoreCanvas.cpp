// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include "xen/ui/ScoreCanvas.h"

#include <algorithm>
#include <cmath>

namespace xen::ui {

ScoreCanvas::ScoreCanvas(PageModel& model, Engine& engine)
    : model_(model), engine_(engine)
{
    setOpaque(true);
    setWantsKeyboardFocus(false);
    fitToPage();
}

void ScoreCanvas::setViewStart(double t)
{
    viewStart_ = std::max(0.0, t);
    repaint();
}

void ScoreCanvas::setViewDuration(double d)
{
    viewDuration_ = juce::jlimit(0.05, 3600.0, d);
    repaint();
}

void ScoreCanvas::fitToPage()
{
    const Page& page = model_.pageRef();
    viewStart_ = 0.0;
    viewDuration_ = std::max(1.0, page.duration);
    if (viewDuration_ < 4.0)
        viewDuration_ = 4.0;
    repaint();
}

void ScoreCanvas::zoomBy(double factor)
{
    const double centre = viewStart_ + viewDuration_ * 0.5;
    viewDuration_ = juce::jlimit(0.1, 3600.0, viewDuration_ * factor);
    viewStart_ = std::max(0.0, centre - viewDuration_ * 0.5);
    repaint();
}

float ScoreCanvas::timeToX(double t) const
{
    return (float)((t - viewStart_) / std::max(1e-9, viewDuration_) * getWidth());
}

double ScoreCanvas::xToTime(float x) const
{
    return viewStart_ + (double)x / std::max(1, getWidth()) * viewDuration_;
}

float ScoreCanvas::pitchToY(float p) const
{
    const float top = 16.0f;
    const float h = (float)getHeight() - top - 4.0f;
    return top + (1.0f - juce::jlimit(0.0f, 1.0f, p)) * std::max(1.0f, h);
}

float ScoreCanvas::yToPitch(float y) const
{
    const float top = 16.0f;
    const float h = std::max(1.0f, (float)getHeight() - top - 4.0f);
    return juce::jlimit(0.0f, 1.0f, 1.0f - (y - top) / h);
}

float ScoreCanvas::snapPitchValue(float p) const
{
    const Page& page = model_.pageRef();
    if (page.frequencyTables.empty())
        return p;
    const FrequencyTable& table = page.frequencyTables.front();
    double idx = table.indexForNorm(p);
    if (snapPitch_)
    {
        if (table.useSieve && !table.sieve.empty())
        {
            const int64_t base = (int64_t)std::llround(idx);
            for (int64_t d = 0; d <= 96; ++d)
            {
                if (table.sieve.matches(base + d))
                {
                    idx = (double)(base + d);
                    break;
                }
                if (d != 0 && table.sieve.matches(base - d))
                {
                    idx = (double)(base - d);
                    break;
                }
            }
        }
        else if (table.tuning.discrete)
        {
            idx = std::round(idx);
        }
    }
    const double span = std::max(1e-9, table.highIndex - table.lowIndex);
    return (float)juce::jlimit(0.0, 1.0, (idx - table.lowIndex) / span);
}

double ScoreCanvas::snapTimeValue(double t) const
{
    if (!snapTime_)
        return t;
    const double quantum = viewDuration_ / 32.0;
    return std::round(t / quantum) * quantum;
}

int ScoreCanvas::arcNear(int x, int y, double tolerancePx) const
{
    const Page& page = model_.pageRef();
    const double t = xToTime((float)x);
    int best = -1;
    double bestDist = tolerancePx;
    for (int i = 0; i < (int)page.arcs.size(); ++i)
    {
        const Arc& a = page.arcs[(size_t)i];
        if (!a.valid())
            continue;
        if (t < a.start() - 0.01 || t > a.end() + 0.01)
            continue;
        const float ap = a.pitchAt(t);
        const double d = std::abs(pitchToY(ap) - y);
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void ScoreCanvas::drawGrid(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    g.setColour(juce::Colour(0xff15171c));
    g.fillRect(0.0f, 0.0f, w, h);

    g.setColour(juce::Colour(0xff232833));
    g.fillRect(0.0f, 0.0f, w, 16.0f);

    double step = 1.0;
    const double targets[] = { 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0, 120.0, 300.0 };
    for (double t : targets)
    {
        if (viewDuration_ / t <= 14.0)
        {
            step = t;
            break;
        }
        step = t;
    }
    const double first = std::floor(viewStart_ / step) * step;
    for (double t = first; t <= viewStart_ + viewDuration_; t += step)
    {
        const float x = timeToX(t);
        g.setColour(juce::Colour(0xff2a303c));
        g.drawVerticalLine((int)std::round(x), 16.0f, h);
        g.setColour(juce::Colour(0xff8892a6));
        g.setFont(11.0f);
        juce::String label;
        if (step >= 1.0)
            label = juce::String((int)std::round(t)) + "s";
        else
            label = juce::String(t, 2) + "s";
        g.drawText(label, (int)x + 3, 2, 60, 14, juce::Justification::centredLeft, false);
    }

    const Page& page = model_.pageRef();
    if (!page.frequencyTables.empty())
    {
        const FrequencyTable& table = page.frequencyTables.front();
        const double low = table.lowIndex;
        const double high = table.highIndex;
        const double span = std::max(1e-9, high - low);
        const double divisions = std::max(1.0, table.tuning.divisionsPerOctave);
        const double firstOct = std::ceil(low / divisions) * divisions;
        g.setFont(10.0f);
        for (double idx = firstOct; idx <= high; idx += divisions)
        {
            const float p = (float)((idx - low) / span);
            const float y = pitchToY(p);
            g.setColour(juce::Colour(0xff333c4a));
            g.drawHorizontalLine((int)std::round(y), 0.0f, w);
            const double hz = table.tuning.indexToHz(idx);
            g.setColour(juce::Colour(0xff77839a));
            juce::String label;
            if (hz >= 1000.0)
                label = juce::String(hz / 1000.0, 1) + "k";
            else
                label = juce::String((int)std::round(hz));
            g.drawText(label, 2, (int)y - 12, 44, 12, juce::Justification::centredLeft, false);
        }
    }

    g.setColour(juce::Colour(0xff3b4658));
    g.drawRect(0, 0, getWidth(), getHeight(), 1);
}

void ScoreCanvas::drawArc(juce::Graphics& g, const Arc& arc, bool selected)
{
    if (arc.points.empty())
        return;
    const int group = juce::jlimit(0, 7, arc.group);
    juce::Colour colour = groupColours_[group];
    if (arc.muted)
        colour = colour.withAlpha(0.18f);
    else
        colour = colour.withAlpha(selected ? 1.0f : 0.85f);

    juce::Path path;
    bool started = false;
    for (const auto& p : arc.points)
    {
        const float x = timeToX(p.time);
        const float y = pitchToY(p.pitch);
        if (!started)
        {
            path.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }
    if (arc.points.size() == 1)
        path.addEllipse(timeToX(arc.points[0].time) - 2.0f, pitchToY(arc.points[0].pitch) - 2.0f, 4.0f, 4.0f);

    g.setColour(colour);
    const float thickness = 1.0f + juce::jlimit(0.0f, 1.0f, arc.weight) * 4.0f;
    g.strokePath(path, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (selected)
    {
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.strokePath(path, juce::PathStrokeType(thickness + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (arc.waveform == WaveformType::Gendyn)
    {
        g.setColour(colour.withAlpha(0.25f));
        for (const auto& p : arc.points)
            g.fillEllipse(timeToX(p.time) - 2.5f, pitchToY(p.pitch) - 2.5f, 5.0f, 5.0f);
    }
}

void ScoreCanvas::paint(juce::Graphics& g)
{
    drawGrid(g);
    const Page& page = model_.pageRef();
    for (int i = 0; i < (int)page.arcs.size(); ++i)
        drawArc(g, page.arcs[(size_t)i], i == selectedArc_);

    if (drawing_)
    {
        if (freehand_ && freehandPoints_.size() >= 2)
        {
            juce::Path path;
            path.startNewSubPath(timeToX(freehandPoints_[0].time), pitchToY(freehandPoints_[0].pitch));
            for (size_t i = 1; i < freehandPoints_.size(); ++i)
                path.lineTo(timeToX(freehandPoints_[i].time), pitchToY(freehandPoints_[i].pitch));
            g.setColour(groupColours_[juce::jlimit(0, 7, brushGroup_)].withAlpha(0.7f));
            g.strokePath(path, juce::PathStrokeType(2.0f));
        }
        else
        {
            g.setColour(groupColours_[juce::jlimit(0, 7, brushGroup_)].withAlpha(0.8f));
            g.drawLine(timeToX(dragStart_.time), pitchToY(dragStart_.pitch),
                       timeToX(dragCurrent_.time), pitchToY(dragCurrent_.pitch), 2.0f);
        }
    }

    const double ph = engine_.playhead();
    const float px = timeToX(ph);
    if (px >= 0.0f && px <= (float)getWidth())
    {
        g.setColour(juce::Colour(0xffff5f6d));
        g.drawVerticalLine((int)std::round(px), 0.0f, (float)getHeight());
        g.fillEllipse(px - 4.0f, 0.0f, 8.0f, 8.0f);
    }

    g.setColour(juce::Colour(0xff8892a6));
    g.setFont(11.0f);
    juce::String hint;
    if (tool_ == Tool::Erase)
        hint = "Erase: drag over arcs";
    else if (tool_ == Tool::Select)
        hint = "Select: click an arc";
    else
        hint = freehand_ ? "Draw freehand" : "Draw: drag from start to end";
    g.drawText(hint, 6, getHeight() - 16, 300, 14, juce::Justification::centredLeft, false);
}

void ScoreCanvas::mouseDown(const juce::MouseEvent& e)
{
    const float x = (float)e.x;
    const float y = (float)e.y;
    const int hit = arcNear(e.x, e.y, 7.0);

    if (tool_ == Tool::Erase)
    {
        erasing_ = true;
        if (hit >= 0)
        {
            model_.pageRef().arcs.erase(model_.pageRef().arcs.begin() + hit);
            selectedArc_ = -1;
            model_.publish();
            if (onEdited)
                onEdited();
        }
        repaint();
        return;
    }

    if (tool_ == Tool::Select)
    {
        selectedArc_ = hit;
        repaint();
        return;
    }

    if (hit >= 0 && !e.mods.isShiftDown())
    {
        selectedArc_ = hit;
        repaint();
        return;
    }

    selectedArc_ = -1;
    drawing_ = true;
    dragStart_.time = (float)snapTimeValue(xToTime(x));
    dragStart_.pitch = snapPitchValue(yToPitch(y));
    dragCurrent_ = dragStart_;
    freehandPoints_.clear();
    freehandPoints_.push_back(dragStart_);
    repaint();
}

void ScoreCanvas::mouseDrag(const juce::MouseEvent& e)
{
    if (!drawing_ && !erasing_)
        return;

    const float x = (float)e.x;
    const float y = (float)e.y;

    if (erasing_)
    {
        const int hit = arcNear(e.x, e.y, 7.0);
        if (hit >= 0)
        {
            model_.pageRef().arcs.erase(model_.pageRef().arcs.begin() + hit);
            selectedArc_ = -1;
            repaint();
        }
        return;
    }

    dragCurrent_.time = (float)snapTimeValue(xToTime(x));
    dragCurrent_.pitch = snapPitchValue(yToPitch(y));

    if (freehand_)
    {
        const ArcPoint& last = freehandPoints_.back();
        const float dx = timeToX(dragCurrent_.time) - timeToX(last.time);
        const float dy = pitchToY(dragCurrent_.pitch) - pitchToY(last.pitch);
        if (dx * dx + dy * dy > 9.0f)
            freehandPoints_.push_back(dragCurrent_);
    }
    repaint();
}

void ScoreCanvas::mouseUp(const juce::MouseEvent&)
{
    if (erasing_)
    {
        erasing_ = false;
        pruneEmptyArcs();
        model_.publish();
        if (onEdited)
            onEdited();
        return;
    }

    if (!drawing_)
        return;
    drawing_ = false;

    Arc arc;
    arc.waveform = brushWaveform_;
    arc.weight = brushWeight_;
    arc.group = brushGroup_;
    if (freehand_ && freehandPoints_.size() >= 2)
    {
        arc.points = freehandPoints_;
    }
    else
    {
        const float threshold = 2.0f / std::max(1, getWidth());
        if (std::fabs(dragCurrent_.time - dragStart_.time) < threshold)
            dragCurrent_.time = dragStart_.time + (float)(viewDuration_ / 16.0);
        arc.points = { dragStart_, dragCurrent_ };
    }
    freehandPoints_.clear();

    if (arc.points.size() >= 2)
    {
        arc.sortPoints();
        model_.pageRef().arcs.push_back(arc);
        model_.pageRef().sortArcs();
        model_.pageRef().updateDuration();
        selectedArc_ = (int)model_.pageRef().arcs.size() - 1;
        model_.publish();
        if (onEdited)
            onEdited();
    }
    repaint();
}

void ScoreCanvas::mouseDoubleClick(const juce::MouseEvent&)
{
    selectedArc_ = -1;
    repaint();
}

void ScoreCanvas::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomBy(wheel.deltaY > 0 ? 0.85 : 1.18);
        return;
    }
    const double delta = -wheel.deltaY * viewDuration_ * 0.15;
    viewStart_ = std::max(0.0, viewStart_ + delta);
    repaint();
}

void ScoreCanvas::pruneEmptyArcs()
{
    auto& arcs = model_.pageRef().arcs;
    arcs.erase(std::remove_if(arcs.begin(), arcs.end(), [](const Arc& a) { return !a.valid(); }), arcs.end());
}

} // namespace xen::ui
