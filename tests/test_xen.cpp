// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "xen/clouds.h"
#include "xen/engine.h"
#include "xen/generators.h"
#include "xen/page_model.h"
#include "xen/gendyn.h"
#include "xen/markov.h"
#include "xen/sieve.h"
#include "xen/strategy.h"
#include "xen/tuning.h"
#include "xen/upic.h"

static int failures = 0;

#define CHECK(cond)                                                                     \
    do                                                                                  \
    {                                                                                   \
        if (!(cond))                                                                    \
        {                                                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                           \
    do                                                                                  \
    {                                                                                   \
        const double va = (a);                                                          \
        const double vb = (b);                                                          \
        if (std::fabs(va - vb) > (eps))                                                 \
        {                                                                               \
            std::printf("FAIL %s:%d: %s=%f vs %s=%f\n", __FILE__, __LINE__, #a, va, #b, vb); \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

static void testSieve()
{
    using namespace xen;
    const Sieve dia = Sieve::diatonic();
    CHECK(dia.matches(0));
    CHECK(dia.matches(2));
    CHECK(dia.matches(4));
    CHECK(dia.matches(5));
    CHECK(dia.matches(7));
    CHECK(dia.matches(9));
    CHECK(dia.matches(11));
    CHECK(!dia.matches(1));
    CHECK(!dia.matches(3));
    CHECK(dia.period() == 12);

    SieveClass out;
    CHECK(Sieve::intersectClass({ 3, 2 }, { 4, 7 }, out));
    CHECK(out.modulus == 12);
    CHECK(out.offset == 11);

    CHECK(!Sieve::intersectClass({ 6, 3 }, { 8, 4 }, out));

    Sieve a;
    a.addClass(3, 0);
    Sieve b;
    b.addClass(4, 0);
    const Sieve u = sieveUnion(a, b);
    CHECK(u.matches(0));
    CHECK(u.matches(3));
    CHECK(u.matches(4));
    CHECK(!u.matches(1));
    const Sieve in = sieveIntersect(a, b);
    CHECK(in.matches(0));
    CHECK(in.matches(12));
    CHECK(!in.matches(3));

    const Sieve comp = sieveComplement(a, Sieve::chromatic12());
    CHECK(comp.matches(1));
    CHECK(!comp.matches(3));

    const std::vector<int64_t> pts = { 3, 23, 33, 47, 63, 70, 71, 93, 95, 119, 123, 143, 153, 167 };
    const Sieve from = Sieve::fromPoints(pts);
    for (int64_t p : pts)
        CHECK(from.matches(p));

    const Sieve single = Sieve::fromPoints({ 5 });
    CHECK(single.matches(5));
    CHECK(!single.matches(6));
    CHECK(!single.matches(7));

    Sieve t = Sieve::diatonic();
    t.transpose(2);
    CHECK(t.matches(1));
    CHECK(t.matches(4));
    CHECK(!t.matches(3));
    CHECK(!t.matches(0));
}

static void testTuning()
{
    using namespace xen;
    Tuning t;
    t.referenceHz = 440.0;
    t.referenceIndex = 69.0;
    t.divisionsPerOctave = 12.0;
    t.discrete = false;
    CHECK_NEAR(t.indexToHz(69.0), 440.0, 1e-9);
    CHECK_NEAR(t.indexToHz(81.0), 880.0, 1e-9);
    CHECK_NEAR(t.hzToIndex(880.0), 81.0, 1e-9);

    const Sieve dia = Sieve::diatonic();
    t.discrete = false;
    const double hz = t.snapToSieve(60.3 / 120.0, 0.0, 120.0, dia);
    CHECK(hz > 0.0);
}

static void testMarkov()
{
    using namespace xen;
    MarkovChain chain = MarkovChain::periodicScale(4, 42);
    CHECK(chain.current() == 0);
    CHECK(chain.step() == 1);
    CHECK(chain.step() == 2);
    CHECK(chain.step() == 3);
    CHECK(chain.step() == 0);

    MarkovChain random = MarkovChain::randomMatrix(6, 1.0, 7);
    const auto dist = random.stationary();
    double total = 0.0;
    for (double p : dist)
    {
        CHECK(p >= 0.0);
        total += p;
    }
    CHECK_NEAR(total, 1.0, 1e-6);

    std::vector<std::vector<double>> counts = { { 0, 3, 1 }, { 2, 0, 1 }, { 1, 1, 0 } };
    MarkovChain fromCounts = MarkovChain::fromCounts(counts, 0.1);
    CHECK(fromCounts.size() == 3);
    for (int i = 0; i < 100; ++i)
    {
        const int s = fromCounts.step();
        CHECK(s >= 0 && s < 3);
    }
}

static void testGendyn()
{
    using namespace xen;
    GendynVoice voice;
    voice.prepare(48000.0);
    GendynVoice::Params p;
    p.freq = 220.0;
    p.segments = 12;
    p.distribution = Distribution::Cauchy;
    voice.setParams(p);
    voice.noteOn(220.0, 1.0);

    double energy = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = voice.process();
        CHECK(std::isfinite(y));
        energy += (double)y * y;
        peak = std::max(peak, std::fabs(y));
    }
    CHECK(energy > 0.01);
    CHECK(peak <= 1.0001f);

    voice.noteOff();
    CHECK_NEAR(voice.process(), 0.0f, 1e-6f);

    voice.setFrequency(880.0);
    voice.noteOn(880.0, 0.5);
    for (int i = 0; i < 1000; ++i)
        CHECK(std::isfinite(voice.process()));
}

static void testClouds()
{
    using namespace xen;
    Rng rng(99);
    CloudParams p;
    p.density = 40.0;
    p.pitchCenter = 220.0;
    p.pitchSpreadSemitones = 7.0;
    p.durationMean = 0.04;
    p.ampMean = 0.2;
    const auto grains = generateCloud(p, 0.0, 5.0, rng);
    CHECK(grains.size() > 50);
    double last = -1.0;
    for (const auto& g : grains)
    {
        CHECK(g.time >= 0.0 && g.time <= 5.0);
        CHECK(g.duration >= p.durationMin);
        CHECK(g.freq > 0.0);
        CHECK(g.time >= last);
        last = g.time;
    }
}

static void testStrategy()
{
    using namespace xen;
    StrategyGame matchingPennies(2, 2);
    matchingPennies.setPayoff(0, 0, 1.0, -1.0);
    matchingPennies.setPayoff(0, 1, -1.0, 1.0);
    matchingPennies.setPayoff(1, 0, -1.0, 1.0);
    matchingPennies.setPayoff(1, 1, 1.0, -1.0);
    for (int i = 0; i < 5000; ++i)
        matchingPennies.replicatorStep(0.05);
    CHECK_NEAR(matchingPennies.mixedRow()[0], 0.5, 0.05);
    CHECK_NEAR(matchingPennies.mixedCol()[0], 0.5, 0.05);
    CHECK_NEAR(matchingPennies.valueA(), 0.0, 0.05);

    StrategyGame greedy(3, 3);
    for (int i = 0; i < 100; ++i)
        greedy.fictitiousPlayStep();
    double total = 0.0;
    for (double p : greedy.mixedRow())
        total += p;
    CHECK_NEAR(total, 1.0, 1e-9);
}

static void testUpic()
{
    using namespace xen;
    Arc arc = Arc::line(1.0, 3.0, 0.25f, 0.75f);
    CHECK_NEAR(arc.start(), 1.0, 1e-9);
    CHECK_NEAR(arc.end(), 3.0, 1e-9);
    CHECK_NEAR(arc.pitchAt(2.0), 0.5, 1e-6);
    CHECK_NEAR(arc.pitchAt(1.0), 0.25, 1e-6);
    CHECK_NEAR(arc.pitchAt(3.0), 0.75, 1e-6);

    Envelope env = Envelope::ad(0.1f, 0.2f);
    CHECK_NEAR(env.at(0.0f), 0.0, 1e-6);
    CHECK_NEAR(env.at(0.1f), 1.0, 1e-6);
    CHECK_NEAR(env.at(1.0f), 0.0, 1e-6);

    Page page;
    page.arcs.push_back(Arc::point(0.0, 10.0, 0.5f, WaveformType::Sine));
    page.updateDuration();
    CHECK(page.duration >= 10.0);
    CHECK(page.groupAudible(0));
}

static void testEngine()
{
    using namespace xen;
    Engine engine;
    engine.prepare(48000.0, 512);

    auto page = std::make_shared<Page>();
    page->duration = 5.0;
    page->loopStart = 0.0;
    page->loopEnd = 5.0;
    Arc arc = Arc::point(0.0, 5.0, 0.5f, WaveformType::Sine);
    arc.weight = 0.5f;
    page->arcs.push_back(arc);
    page->frequencyTables[0].tuning.referenceHz = 440.0;
    page->frequencyTables[0].tuning.referenceIndex = 69.0;
    page->frequencyTables[0].lowIndex = 60.0;
    page->frequencyTables[0].highIndex = 72.0;
    engine.setPage(page);
    engine.setTransport(true, true, 1.0, false);
    engine.setGains(-6.0f, 0.0f, 0.0f);

    std::vector<float> l(512), r(512);
    double energy = 0.0;
    for (int block = 0; block < 100; ++block)
    {
        engine.render(l.data(), r.data(), 512);
        for (int i = 0; i < 512; ++i)
        {
            CHECK(std::isfinite(l[(size_t)i]));
            energy += (double)l[(size_t)i] * l[(size_t)i];
        }
    }
    CHECK(energy > 0.01);

    engine.noteOn(60, 0.8f);
    for (int block = 0; block < 20; ++block)
        engine.render(l.data(), r.data(), 512);
    engine.noteOff(60);
    for (int block = 0; block < 20; ++block)
        engine.render(l.data(), r.data(), 512);
    engine.allNotesOff();
    engine.reset();
}


static void testGenerators()
{
    using namespace xen;
    Tuning tuning;
    tuning.referenceHz = 8.1758;
    tuning.referenceIndex = 0.0;
    tuning.divisionsPerOctave = 12.0;

    Page page;
    SieveGenParams sp;
    sp.t0 = 0.0;
    sp.t1 = 8.0;
    sp.mode = SieveMode::Arpeggio;
    sp.noteDuration = 0.25;
    generateSieve(page, Sieve::diatonic(), sp, tuning);
    CHECK(page.arcs.size() >= 30);
    CHECK(page.duration >= 7.9);

    Page markovPage;
    MarkovGenParams mp;
    mp.t1 = 4.0;
    mp.noteDuration = 0.2;
    generateMarkov(markovPage, MarkovChain::randomMatrix(5, 0.8, 3), mp, tuning);
    CHECK(markovPage.arcs.size() > 10);

    Page cloudPage;
    Rng rng(5);
    CloudGenParams cp;
    cp.t1 = 3.0;
    cp.cloud.density = 30.0;
    generateCloudArcs(cloudPage, cp, tuning, rng);
    CHECK(cloudPage.arcs.size() > 20);

    Page strategyPage;
    StrategyGenParams st;
    st.t1 = 4.0;
    st.segmentDuration = 0.2;
    StrategyGame g(3, 3);
    generateStrategy(strategyPage, g, st, tuning);
    CHECK(strategyPage.arcs.size() > 10);

    Page roundtrip = strategyPage;
    roundtrip.arcs[0].waveform = WaveformType::Square;
    roundtrip.groups[0].name = "A group with spaces";
    roundtrip.frequencyTables[0].useSieve = true;
    roundtrip.frequencyTables[0].sieve = Sieve::diatonic();
    const std::string text = serializePage(roundtrip);
    Page restored;
    CHECK(deserializePage(text, restored));
    CHECK(restored.arcs.size() == roundtrip.arcs.size());
    CHECK(restored.name == roundtrip.name);
    CHECK(restored.groups.size() == roundtrip.groups.size());
    if (!restored.arcs.empty())
    {
        CHECK(restored.arcs[0].waveform == WaveformType::Square);
        CHECK(restored.arcs[0].points.size() == roundtrip.arcs[0].points.size());
    }
    CHECK(restored.frequencyTables[0].sieve.matches(0));
    CHECK(restored.frequencyTables[0].sieve.matches(2));
    CHECK(!restored.frequencyTables[0].sieve.matches(1));

    PageModel model;
    bool published = false;
    model.setPublisher([&](std::shared_ptr<const Page> p) { published = p != nullptr; });
    model.publish();
    CHECK(published);
    const std::string state = model.serialize();
    PageModel model2;
    CHECK(model2.deserialize(state));
    CHECK(model2.snapshot().arcs.size() == model.snapshot().arcs.size());
}


static double renderEnergy(xen::Page page, int blocks, xen::Engine& engine)
{
    using namespace xen;
    engine.prepare(48000.0, 512);
    engine.setPage(std::make_shared<const Page>(page));
    engine.setTransport(true, true, 1.0, false);
    engine.setGains(0.0f, 0.0f, 0.0f);
    std::vector<float> l(512), r(512);
    double energy = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        engine.render(l.data(), r.data(), 512);
        for (float v : l)
        {
            CHECK(std::isfinite(v));
            energy += (double)v * v;
        }
    }
    return energy;
}

static void testEngineGenerators()
{
    using namespace xen;
    Tuning tuning;
    tuning.referenceHz = 8.175798915643707;
    tuning.referenceIndex = 0.0;
    tuning.divisionsPerOctave = 12.0;

    Engine engine;
    Page sievePage;
    SieveGenParams sp;
    sp.t0 = 0.0;
    sp.t1 = 4.0;
    sp.mode = SieveMode::Arpeggio;
    sp.noteDuration = 0.2;
    sp.waveform = WaveformType::Gendyn;
    generateSieve(sievePage, Sieve::diatonic(), sp, tuning);
    CHECK(renderEnergy(sievePage, 60, engine) > 0.01);

    Page markovPage;
    MarkovGenParams mp;
    mp.t1 = 4.0;
    mp.noteDuration = 0.2;
    mp.waveform = WaveformType::Gendyn;
    generateMarkov(markovPage, MarkovChain::randomMatrix(5, 0.8, 3), mp, tuning);
    CHECK(renderEnergy(markovPage, 60, engine) > 0.01);

    Page cloudPage;
    Rng rng(5);
    CloudGenParams cp;
    cp.t1 = 4.0;
    cp.cloud.density = 30.0;
    cp.waveform = WaveformType::Sine;
    generateCloudArcs(cloudPage, cp, tuning, rng);
    CHECK(renderEnergy(cloudPage, 60, engine) > 0.01);

    Page strategyPage;
    StrategyGenParams st;
    st.t1 = 4.0;
    st.segmentDuration = 0.2;
    st.waveform = WaveformType::Gendyn;
    generateStrategy(strategyPage, StrategyGame(3, 3), st, tuning);
    CHECK(renderEnergy(strategyPage, 60, engine) > 0.01);

    for (WaveformType w : { WaveformType::Sine, WaveformType::Saw, WaveformType::Square,
                            WaveformType::Triangle, WaveformType::Gendyn })
    {
        Page p;
        Arc a = Arc::point(0.0, 4.0, 0.5f, w);
        a.weight = 0.8f;
        p.arcs.push_back(a);
        p.frequencyTables[0].tuning.referenceHz = 8.175798915643707;
        p.frequencyTables[0].lowIndex = 48.0;
        p.frequencyTables[0].highIndex = 72.0;
        p.updateDuration();
        p.loopEnd = p.duration;
        CHECK(renderEnergy(p, 40, engine) > 0.001);
    }
}


static void testPresetFiles()
{
    using namespace xen;
    const std::filesystem::path directory = XEN_PRESETS_DIR;
    if (!std::filesystem::exists(directory))
        return;
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.path().extension() != ".xenpage")
            continue;
        std::ifstream in(entry.path(), std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        Page page;
        CHECK(deserializePage(buffer.str(), page));
        CHECK(!page.arcs.empty());
        Engine engine;
        CHECK(renderEnergy(page, 24, engine) > 0.0);
        ++count;
    }
    CHECK(count >= 8);
}

int main()
{
    testSieve();
    testTuning();
    testMarkov();
    testGendyn();
    testClouds();
    testStrategy();
    testUpic();
    testEngine();
    testGenerators();
    testEngineGenerators();
    testPresetFiles();

    if (failures == 0)
    {
        std::printf("All Xenakis library tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
