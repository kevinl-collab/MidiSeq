#pragma once
// Built-in starting points. Filled in place: a Project is ~0.5 MB, keep it off small thread stacks.
#include "Generator.h"
#include "Pattern.h"

namespace ms
{
inline constexpr const char* kFactoryNames[] = { "Default", "Empty", "Bassline", "Arpeggio", "Rolling Groove" };
constexpr int kNumFactoryPresets = 5;

namespace detail
{
inline void setupTrack (Track& t, int outNote, int channel, int steps = 16)
{
    t = Track {};
    t.outNote = outNote;
    t.inNote = outNote;
    t.outChannel = channel;
    t.stepCount = steps;
    t.euclid.steps = steps;
}

inline void setAllScenes (Project& pr, const Scene& base, const Global& g)
{
    pr.global = g;
    pr.numScenes = 4;
    for (auto& s : pr.scenes) s = base;
}
} // namespace detail

inline void fillFactoryProject (int index, Project& pr)
{
    using namespace detail;
    switch (index)
    {
        case 0:
            fillDemoProject (pr);
            return;

        case 1:   // Empty: GM-style drum map on channel 10, nothing programmed
        {
            Scene base;
            base.numTracks = 8;
            const int notes[] = { 36, 38, 42, 46, 45, 48, 49, 51 };
            for (int i = 0; i < 8; ++i) setupTrack (base.tracks[size_t (i)], notes[i], 10);
            setAllScenes (pr, base, Global {});
            return;
        }

        case 2:   // Bassline: C minor, bass + kick + hat; each scene re-rolls the bass
        {
            Global g;
            g.scaleEnabled = true; g.root = 0; g.scaleMask = maskOf ({ 0, 2, 3, 5, 7, 8, 10 });
            g.rangeLow = 24; g.rangeHigh = 60;
            Scene base;
            base.numTracks = 3;
            setupTrack (base.tracks[0], 36, 1);
            setupTrack (base.tracks[1], 36, 10);
            setupTrack (base.tracks[2], 42, 10);
            applyEuclidTo (base.tracks[1], 4, 16, 0);
            applyEuclidTo (base.tracks[2], 8, 16, 0);
            base.tracks[2].followTranspose = false;
            base.tracks[1].followTranspose = false;
            setAllScenes (pr, base, g);
            for (int s = 0; s < pr.numScenes; ++s)
                generateTrack (pr.scenes[size_t (s)].tracks[0], g, presetSettings (GenPreset::Bassline, uint32_t (s + 1)));
            return;
        }

        case 3:   // Arpeggio: D dorian; up-down / up / down / random over the chord
        {
            Global g;
            g.scaleEnabled = true; g.root = 2; g.scaleMask = maskOf ({ 0, 2, 3, 5, 7, 9, 10 });
            Scene base;
            base.numTracks = 2;
            setupTrack (base.tracks[0], 62, 1);
            setupTrack (base.tracks[1], 38, 1);
            applyEuclidTo (base.tracks[1], 3, 8, 0);
            setAllScenes (pr, base, g);
            const ArpPattern pats[] = { ArpPattern::UpDown, ArpPattern::Up, ArpPattern::Down, ArpPattern::Random };
            for (int s = 0; s < pr.numScenes; ++s)
            {
                auto cfg = presetSettings (GenPreset::Arpeggio, uint32_t (s + 1));
                cfg.arp = pats[s];
                generateTrack (pr.scenes[size_t (s)].tracks[0], g, cfg);
            }
            return;
        }

        case 4:   // Rolling Groove: A minor, rolling bass over a four-on-the-floor kit
        default:
        {
            Global g;
            g.scaleEnabled = true; g.root = 9; g.scaleMask = maskOf ({ 0, 2, 3, 5, 7, 8, 10 });
            g.shuffleType = ShuffleType::MPC; g.shuffleAmount = 20.0f;
            Scene base;
            base.numTracks = 4;
            setupTrack (base.tracks[0], 33, 1);
            setupTrack (base.tracks[1], 36, 10);
            setupTrack (base.tracks[2], 38, 10);
            setupTrack (base.tracks[3], 42, 10);
            applyEuclidTo (base.tracks[1], 4, 16, 0);
            applyEuclidTo (base.tracks[2], 2, 16, 4);
            applyEuclidTo (base.tracks[3], 11, 16, 0);
            for (int t = 1; t < 4; ++t) base.tracks[size_t (t)].followTranspose = false;
            setAllScenes (pr, base, g);
            for (int s = 0; s < pr.numScenes; ++s)
                generateTrack (pr.scenes[size_t (s)].tracks[0], g, presetSettings (GenPreset::Rolling, uint32_t (s + 10)));
            return;
        }
    }
}
} // namespace ms
