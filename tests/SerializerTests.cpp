// JUCE-side tests: session save/restore, versioning and MIDI export. Console app, exit code != 0 on failure.
#include "Engine/Factory.h"
#include "Engine/Pattern.h"
#include "Source/MidiExport.h"
#include "Source/ProjectSerializer.h"
#include <juce_core/juce_core.h>

namespace
{
juce::String xmlOf (const ms::Project& p)
{
    return ser::projectToTree (p, "X").createXml()->toString();
}

struct SerializerTests : public juce::UnitTest
{
    SerializerTests() : juce::UnitTest ("Project serialization", "MelodicSequencer") {}

    void runTest() override
    {
        beginTest ("round trip keeps every field");
        {
            auto original = std::make_unique<ms::Project>();
            ms::fillDemoProject (*original);
            auto& step = original->scenes[1].tracks[2].steps[5];
            step.active = true; step.velocity = 77; step.length = 1.25f; step.shift = -0.2f; step.probability = 0.5f;
            step.reps = 3; step.interval = 0.125f; step.shape = ms::RepeatShape::UpDown; step.curve = -40.0f;
            step.gain = 1.5f; step.condition = ms::Condition::EveryN; step.conditionN = 4; step.pitch = -7;
            auto& tr = original->scenes[2].tracks[1];
            tr.mute = true; tr.solo = true; tr.direction = ms::Direction::Pendulum; tr.chokeMode = 3;
            tr.stepLength = { ms::StepDivision::Eighth, ms::StepModifier::Triplet }; tr.linkStepLength = false;
            tr.transposeOffset = -5; tr.velScale = 0.5f; tr.euclid = { 5, 13, -2, true };
            original->global.scaleMask = ms::maskOf ({ 0, 1, 4 });
            original->global.launchQuantize = ms::LaunchQuantize::Immediate;

            const auto xml = ser::projectToTree (*original, "My Preset").createXml();
            expect (xml != nullptr);
            auto parsed = juce::XmlDocument::parse (xml->toString());
            expect (parsed != nullptr);

            auto loaded = std::make_unique<ms::Project>();
            juce::String name;
            expect (ser::treeToProject (juce::ValueTree::fromXml (*parsed), *loaded, &name));
            expectEquals (name, juce::String ("My Preset"));
            expectEquals (xmlOf (*loaded), xmlOf (*original));
            expectEquals (loaded->scenes[1].tracks[2].steps[5].pitch, -7);
            expectEquals (loaded->scenes[2].tracks[1].chokeMode, 3);
            expect (loaded->scenes[2].tracks[1].euclid.live);
        }

        beginTest ("every factory preset survives a round trip");
        for (int i = 0; i < ms::kNumFactoryPresets; ++i)
        {
            auto original = std::make_unique<ms::Project>();
            ms::fillFactoryProject (i, *original);
            auto loaded = std::make_unique<ms::Project>();
            expect (ser::treeToProject (ser::projectToTree (*original, "F"), *loaded));
            expectEquals (xmlOf (*loaded), xmlOf (*original));
        }

        beginTest ("defaults are omitted, bad data is clamped, garbage is rejected");
        {
            auto empty = std::make_unique<ms::Project>();
            ms::fillFactoryProject (1, *empty);
            const auto compact = xmlOf (*empty);
            expect (! compact.contains ("STEP"));   // an empty project stores no steps at all

            auto tree = ser::projectToTree (*empty, "X");
            auto scene = tree.getChildWithName ("SCENE");
            auto track = scene.getChild (0);
            ValueTree_setProperty (track, "out", 9999);
            ValueTree_setProperty (track, "count", -3);
            ValueTree_setProperty (track, "ch", 40);
            auto loaded = std::make_unique<ms::Project>();
            expect (ser::treeToProject (tree, *loaded));
            expectEquals (loaded->scenes[0].tracks[0].outNote, 127);
            expectEquals (loaded->scenes[0].tracks[0].stepCount, 1);
            expectEquals (loaded->scenes[0].tracks[0].outChannel, 16);

            expect (! ser::treeToProject (juce::ValueTree ("SOMETHING_ELSE"), *loaded));
            expect (! ser::treeToProject (juce::ValueTree ("PROJECT"), *loaded));   // no scenes
        }

        beginTest ("user scales and migration");
        {
            std::vector<ser::NamedScale> scales { { "Hirajoshi", ms::maskOf ({ 0, 2, 3, 7, 8 }) }, { "Mine", 0x123 } };
            const auto back = ser::treeToScales (ser::scalesToTree (scales));
            expectEquals ((int) back.size(), 2);
            expectEquals (back[0].name, juce::String ("Hirajoshi"));
            expectEquals ((int) back[1].mask, 0x123);

            juce::ValueTree old ("PARAMS");   // a version-1 session has no version property at all
            ser::migrate (old);
            expectEquals ((int) old.getProperty ("stateVersion"), ser::kStateVersion);
        }

        beginTest ("MIDI export has the right shape and timing");
        {
            auto p = std::make_unique<ms::Project>();
            ms::fillFactoryProject (0, *p);
            ms::Global g = p->global;

            const auto one = midiexport::build (*p, g, 0);
            expectEquals (one.getTimeFormat(), midiexport::kTicksPerQuarter);
            expectEquals (one.getNumTracks(), 2);   // conductor + notes

            const auto* notes = one.getTrack (1);
            int ons = 0, offs = 0;
            for (int i = 0; i < notes->getNumEvents(); ++i)
            {
                const auto& m = notes->getEventPointer (i)->message;
                ons += m.isNoteOn() ? 1 : 0;
                offs += m.isNoteOff() ? 1 : 0;
            }
            expect (ons > 0);
            expectEquals (ons, offs);   // nothing left hanging

            // kick (5 pulses, 16th steps): first hit at tick 0, and every event stays inside the cycle
            expectEquals (notes->getEventPointer (0)->message.getTimeStamp(), 0.0);
            expect (notes->getEndTime() <= 4.0 * midiexport::kTicksPerQuarter + 1.0);

            const auto all = midiexport::build (*p, g, -1);
            expect (all.getTrack (1)->getEndTime() > one.getTrack (1)->getEndTime());   // scenes are chained
        }
    }

    static void ValueTree_setProperty (juce::ValueTree& t, const char* name, int value)
    {
        t.setProperty (juce::Identifier (name), value, nullptr);
    }
};

static SerializerTests serializerTests;
} // namespace

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;
    return failures == 0 ? 0 : 1;
}
