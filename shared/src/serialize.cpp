// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include "xen/page_model.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

namespace xen {

std::string serializePage(const Page& page)
{
    std::ostringstream out;
    out << std::setprecision(9);
    out << "XENPAGE 1\n";
    out << "name " << std::quoted(page.name) << "\n";
    out << "duration " << page.duration << "\n";
    out << "loop " << page.loopStart << ' ' << page.loopEnd << "\n";

    out << "groups " << page.groups.size() << "\n";
    for (const auto& g : page.groups)
        out << "group " << (g.muted ? 1 : 0) << ' ' << (g.solo ? 1 : 0) << ' '
            << g.transposeSemitones << ' ' << std::quoted(g.name) << "\n";

    out << "envelopes " << page.envelopes.size() << "\n";
    for (const auto& e : page.envelopes)
    {
        out << "env " << e.points.size();
        for (const auto& p : e.points)
            out << ' ' << p.time << ' ' << p.level;
        out << "\n";
    }

    out << "tables " << page.frequencyTables.size() << "\n";
    for (const auto& t : page.frequencyTables)
    {
        out << "table " << t.lowIndex << ' ' << t.highIndex << ' ' << t.tuning.referenceHz << ' '
            << t.tuning.divisionsPerOctave << ' ' << t.tuning.referenceIndex << ' '
            << (t.tuning.discrete ? 1 : 0) << ' ' << (t.useSieve ? 1 : 0) << ' '
            << t.sieve.groups.size();
        for (const auto& group : t.sieve.groups)
        {
            out << ' ' << group.size();
            for (const auto& c : group)
                out << ' ' << c.modulus << ' ' << c.offset;
        }
        out << "\n";
    }

    out << "arcs " << page.arcs.size() << "\n";
    for (const auto& a : page.arcs)
    {
        out << "arc " << (int)a.waveform << ' ' << a.group << ' ' << a.channel << ' '
            << a.envelopeIndex << ' ' << a.freqTableIndex << ' ' << a.weight << ' '
            << (a.muted ? 1 : 0) << ' ' << a.points.size();
        for (const auto& p : a.points)
            out << ' ' << p.time << ' ' << p.pitch;
        out << "\n";
    }
    return out.str();
}

bool deserializePage(const std::string& text, Page& page)
{
    std::istringstream in(text);
    std::string token;
    if (!(in >> token) || token != "XENPAGE")
        return false;
    int version = 0;
    if (!(in >> version) || version != 1)
        return false;

    Page loaded;
    try
    {
        while (in >> token)
        {
            if (token == "name")
            {
                in >> std::quoted(loaded.name);
            }
            else if (token == "duration")
            {
                in >> loaded.duration;
            }
            else if (token == "loop")
            {
                in >> loaded.loopStart >> loaded.loopEnd;
            }
            else if (token == "groups")
            {
                size_t n = 0;
                in >> n;
                loaded.groups.clear();
                for (size_t i = 0; i < n; ++i)
                {
                    if (!(in >> token) || token != "group")
                        return false;
                    ArcGroup g;
                    int muted = 0;
                    int solo = 0;
                    in >> muted >> solo >> g.transposeSemitones >> std::quoted(g.name);
                    g.muted = muted != 0;
                    g.solo = solo != 0;
                    loaded.groups.push_back(g);
                }
            }
            else if (token == "envelopes")
            {
                size_t n = 0;
                in >> n;
                loaded.envelopes.clear();
                for (size_t i = 0; i < n; ++i)
                {
                    if (!(in >> token) || token != "env")
                        return false;
                    size_t count = 0;
                    in >> count;
                    Envelope e;
                    e.points.clear();
                    for (size_t j = 0; j < count; ++j)
                    {
                        EnvelopePoint p;
                        in >> p.time >> p.level;
                        e.points.push_back(p);
                    }
                    loaded.envelopes.push_back(e);
                }
            }
            else if (token == "tables")
            {
                size_t n = 0;
                in >> n;
                loaded.frequencyTables.clear();
                for (size_t i = 0; i < n; ++i)
                {
                    if (!(in >> token) || token != "table")
                        return false;
                    FrequencyTable t;
                    int discrete = 0;
                    int useSieve = 0;
                    size_t groupCount = 0;
                    in >> t.lowIndex >> t.highIndex >> t.tuning.referenceHz
                       >> t.tuning.divisionsPerOctave >> t.tuning.referenceIndex
                       >> discrete >> useSieve >> groupCount;
                    t.tuning.discrete = discrete != 0;
                    t.useSieve = useSieve != 0;
                    t.sieve.groups.clear();
                    for (size_t g = 0; g < groupCount; ++g)
                    {
                        size_t classCount = 0;
                        in >> classCount;
                        SieveGroup group;
                        for (size_t c = 0; c < classCount; ++c)
                        {
                            SieveClass sc;
                            in >> sc.modulus >> sc.offset;
                            group.push_back(sc);
                        }
                        t.sieve.groups.push_back(group);
                    }
                    loaded.frequencyTables.push_back(t);
                }
            }
            else if (token == "arcs")
            {
                size_t n = 0;
                in >> n;
                loaded.arcs.clear();
                for (size_t i = 0; i < n; ++i)
                {
                    if (!(in >> token) || token != "arc")
                        return false;
                    Arc a;
                    int wave = 0;
                    int muted = 0;
                    size_t count = 0;
                    in >> wave >> a.group >> a.channel >> a.envelopeIndex >> a.freqTableIndex
                       >> a.weight >> muted >> count;
                    a.waveform = (WaveformType)clamp(wave, 0, 4);
                    a.muted = muted != 0;
                    a.points.clear();
                    for (size_t j = 0; j < count; ++j)
                    {
                        ArcPoint p;
                        in >> p.time >> p.pitch;
                        a.points.push_back(p);
                    }
                    a.sortPoints();
                    loaded.arcs.push_back(a);
                }
            }
            else
            {
                std::string rest;
                std::getline(in, rest);
            }
            if (!in)
                return false;
        }
    }
    catch (...)
    {
        return false;
    }

    if (loaded.groups.empty())
        loaded.groups.push_back(ArcGroup { "Group A", false, false, 0.0f });
    if (loaded.envelopes.empty())
        loaded.envelopes.push_back(Envelope::ad(0.01f, 0.05f));
    if (loaded.frequencyTables.empty())
        loaded.frequencyTables.push_back(FrequencyTable {});
    loaded.sortArcs();
    loaded.updateDuration();
    if (loaded.loopEnd <= loaded.loopStart)
        loaded.loopEnd = loaded.duration;
    page = std::move(loaded);
    return true;
}

} // namespace xen
