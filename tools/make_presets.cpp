// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "xen/clouds.h"
#include "xen/generators.h"
#include "xen/markov.h"
#include "xen/page_model.h"
#include "xen/sieve.h"
#include "xen/strategy.h"

namespace {

xen::Tuning tuning()
{
    xen::Tuning t;
    t.referenceHz = 8.175798915643707;
    t.referenceIndex = 0.0;
    t.divisionsPerOctave = 12.0;
    return t;
}

void writePage(const std::filesystem::path& directory, const std::string& name, const xen::Page& page)
{
    const std::filesystem::path path = directory / name;
    std::ofstream out(path, std::ios::binary);
    out << xen::serializePage(page);
    std::printf("wrote %s (%zu arcs, %.1f s)\n", path.string().c_str(), page.arcs.size(), page.duration);
}

} // namespace

int main(int argc, char** argv)
{
    const std::filesystem::path directory = argc > 1 ? argv[1] : "presets";
    std::filesystem::create_directories(directory);

    {
        xen::Page page;
        page.name = "Sieve: Diatonic Arpeggio";
        xen::SieveGenParams p;
        p.t0 = 0.0;
        p.t1 = 16.0;
        p.mode = xen::SieveMode::Arpeggio;
        p.noteDuration = 0.18;
        p.lowIndex = 24;
        p.highIndex = 96;
        p.weight = 0.5f;
        p.waveform = xen::WaveformType::Gendyn;
        xen::generateSieve(page, xen::Sieve::diatonic(), p, tuning());
        page.envelopes[0] = xen::Envelope::ad(0.005f, 0.04f);
        page.loopEnd = 16.0;
        writePage(directory, "01-sieve-diatonic.xenpage", page);
    }

    {
        xen::Page page;
        page.name = "Sieve: Rhythmic 3-4-5";
        xen::Sieve rhythm;
        rhythm.addClass(3, 0);
        rhythm.addClass(4, 1);
        rhythm.addClass(5, 2);
        xen::SieveGenParams p;
        p.t0 = 0.0;
        p.t1 = 16.0;
        p.mode = xen::SieveMode::Rhythm;
        p.timeUnit = 0.08;
        p.noteDuration = 0.07;
        p.lowIndex = 36;
        p.highIndex = 84;
        p.weight = 0.6f;
        p.waveform = xen::WaveformType::Square;
        xen::generateSieve(page, rhythm, p, tuning());
        page.envelopes[0] = xen::Envelope::ad(0.002f, 0.02f);
        page.loopEnd = 16.0;
        writePage(directory, "02-sieve-rhythm.xenpage", page);
    }

    {
        xen::Page page;
        page.name = "GENDYN Drone";
        page.duration = 24.0;
        for (int i = 0; i < 6; ++i)
        {
            xen::Arc arc = xen::Arc::line(0.0, 24.0, 0.25f + 0.1f * i, 0.25f + 0.1f * i,
                                         xen::WaveformType::Gendyn);
            arc.weight = 0.18f;
            arc.group = i % 4;
            page.arcs.push_back(arc);
        }
        page.updateDuration();
        page.loopEnd = 24.0;
        page.envelopes[0] = xen::Envelope::ad(1.5f, 3.0f);
        page.frequencyTables[0].lowIndex = 12.0;
        page.frequencyTables[0].highIndex = 84.0;
        page.frequencyTables[0].tuning = tuning();
        writePage(directory, "03-gendyn-drone.xenpage", page);
    }

    {
        xen::Page page;
        page.name = "Markov Mobile";
        xen::MarkovGenParams p;
        p.t0 = 0.0;
        p.t1 = 20.0;
        p.noteDuration = 0.22;
        p.lowIndex = 36;
        p.highIndex = 90;
        p.weight = 0.45f;
        p.waveform = xen::WaveformType::Gendyn;
        p.seed = 4242;
        xen::generateMarkov(page, xen::MarkovChain::randomMatrix(7, 0.9, 4242), p, tuning());
        page.loopEnd = 20.0;
        writePage(directory, "04-markov-mobile.xenpage", page);
    }

    {
        xen::Page page;
        page.name = "Stochastic Cloud";
        xen::CloudGenParams p;
        p.t0 = 0.0;
        p.t1 = 20.0;
        p.cloud.density = 35.0;
        p.cloud.pitchCenter = 440.0;
        p.cloud.pitchSpreadSemitones = 24.0;
        p.cloud.durationMean = 0.08;
        p.cloud.ampMean = 0.22;
        p.waveform = xen::WaveformType::Sine;
        p.lowIndex = 24;
        p.highIndex = 108;
        xen::Rng rng(1964);
        xen::generateCloudArcs(page, p, tuning(), rng);
        page.loopEnd = 20.0;
        writePage(directory, "05-cloud-texture.xenpage", page);
    }

    {
        xen::Page page;
        page.name = "Strategy Game";
        xen::StrategyGame game(4, 4);
        xen::Rng rng(1922);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                game.setPayoff(r, c, rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0));
        xen::StrategyGenParams p;
        p.t0 = 0.0;
        p.t1 = 20.0;
        p.segmentDuration = 0.3;
        p.lowIndex = 36;
        p.highIndex = 88;
        p.weight = 0.45f;
        p.waveform = xen::WaveformType::Triangle;
        xen::generateStrategy(page, game, p, tuning());
        page.loopEnd = 20.0;
        writePage(directory, "06-strategy-game.xenpage", page);
    }

    {
        xen::Page page;
        page.name = "UPIC Glissandi";
        page.duration = 20.0;
        const float pitches[] = { 0.15f, 0.35f, 0.55f, 0.75f, 0.9f };
        const xen::WaveformType waves[] = { xen::WaveformType::Sine, xen::WaveformType::Triangle,
                                            xen::WaveformType::Saw, xen::WaveformType::Square,
                                            xen::WaveformType::Gendyn };
        for (int i = 0; i < 12; ++i)
        {
            const double t0 = i * 1.5;
            const double t1 = t0 + 2.2;
            const float p0 = pitches[i % 5];
            const float p1 = pitches[(i + 2) % 5];
            xen::Arc arc = xen::Arc::line(t0, std::min(t1, 20.0), p0, p1, waves[i % 5]);
            arc.weight = 0.35f;
            arc.group = i % 4;
            page.arcs.push_back(arc);
        }
        page.updateDuration();
        page.loopEnd = 20.0;
        page.frequencyTables[0].lowIndex = 24.0;
        page.frequencyTables[0].highIndex = 96.0;
        page.frequencyTables[0].tuning = tuning();
        page.envelopes[0] = xen::Envelope::ad(0.02f, 0.3f);
        writePage(directory, "07-upic-glissandi.xenpage", page);
    }

    {
        xen::Page page;
        page.name = "Sieve Field";
        xen::Sieve sieve;
        sieve.addClass(12, 0);
        sieve.addClass(12, 3);
        sieve.addClass(12, 7);
        sieve.addClass(12, 10);
        xen::SieveGenParams p;
        p.t0 = 0.0;
        p.t1 = 20.0;
        p.mode = xen::SieveMode::Field;
        p.lowIndex = 24;
        p.highIndex = 96;
        p.weight = 0.12f;
        p.waveform = xen::WaveformType::Sine;
        xen::generateSieve(page, sieve, p, tuning());
        page.loopEnd = 20.0;
        page.envelopes[0] = xen::Envelope::ad(1.0f, 1.0f);
        writePage(directory, "08-sieve-field.xenpage", page);
    }

    return 0;
}
