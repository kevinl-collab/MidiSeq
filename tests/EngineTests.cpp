#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Directions.h"
#include "Engine/Euclid.h"
#include "Engine/Repeats.h"
#include "Engine/Scale.h"
#include "Engine/Factory.h"
#include "Engine/Generator.h"
#include "Engine/Metronome.h"
#include "Engine/Pattern.h"
#include "Engine/Recording.h"
#include "Engine/Sequencer.h"
#include "Engine/Scheduler.h"
#include "Engine/Swing.h"
#include "Engine/TripleBuffer.h"

#include <cmath>
#include <cstdlib>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace ms;
using Catch::Approx;

namespace
{
std::string toString (const std::array<bool, kMaxSteps>& p, int n)
{
    std::string s;
    for (int i = 0; i < n; ++i) s += p[size_t (i)] ? 'x' : '.';
    return s;
}

std::vector<int> walk (Direction d, int len, int count, uint32_t seed = 7)
{
    DirectionWalker w;
    Rng rng;
    rng.seed (seed);
    std::vector<int> out;
    for (int n = 0; n < count; ++n) out.push_back (w.index (d, n, len, rng));
    return out;
}

struct Hit { int64_t sample; NoteEvent ev; };

struct Harness
{
    static constexpr double sr = 48000.0;
    Scheduler sched;
    Scene scene;
    Global g;
    EventList list;
    std::vector<Hit> events;
    int64_t samplePos = 0;

    Harness() { scene.numTracks = 1; }

    Track& track (int i = 0) { return scene.tracks[size_t (i)]; }

    void block (double ppq, double bpm, int n, bool playing = true)
    {
        list.clear();
        BlockInfo bi;
        bi.ppqStart = ppq; bi.bpm = bpm; bi.sampleRate = sr; bi.numSamples = n; bi.playing = playing;
        sched.process (bi, scene, g, list);
        for (int i = 0; i < list.size(); ++i)
        {
            REQUIRE (list[i].sampleOffset >= 0);
            REQUIRE (list[i].sampleOffset < n);
            events.push_back ({ samplePos + list[i].sampleOffset, list[i] });
        }
        samplePos += n;
    }

    /** Constant tempo, contiguous blocks. */
    void run (double bpm, int blockSize, int numBlocks)
    {
        for (int b = 0; b < numBlocks; ++b)
            block (double (samplePos) * bpm / 60.0 / sr, bpm, blockSize);
    }

    std::vector<int64_t> times (bool on, int note = -1) const
    {
        std::vector<int64_t> v;
        for (auto& h : events)
            if (h.ev.isOn == on && (note < 0 || h.ev.note == note)) v.push_back (h.sample);
        return v;
    }
};

bool closeTo (int64_t a, int64_t b, int64_t tol = 1) { return std::llabs (a - b) <= tol; }
} // namespace

// ---------------------------------------------------------------- Euclid
TEST_CASE ("Euclid patterns", "[euclid]")
{
    CHECK (toString (euclidPattern (3, 8, 0), 8) == "x..x..x.");
    CHECK (toString (euclidPattern (5, 8, 0), 8) == "x.xx.xx.");
    CHECK (toString (euclidPattern (4, 16, 0), 16) == "x...x...x...x...");
    CHECK (toString (euclidPattern (0, 8, 0), 8) == "........");
    CHECK (toString (euclidPattern (8, 8, 0), 8) == "xxxxxxxx");
    CHECK (toString (euclidPattern (12, 8, 0), 8) == "xxxxxxxx");   // pulses clamp to steps
    CHECK (toString (euclidPattern (3, 8, 1), 8) == ".x..x..x");
    CHECK (toString (euclidPattern (3, 8, -1), 8) == "..x..x.x");
    CHECK (toString (euclidPattern (3, 8, 8), 8) == "x..x..x.");

    for (int steps = 1; steps <= kMaxSteps; ++steps)
        for (int pulses = 0; pulses <= steps; ++pulses)
        {
            const auto p = euclidPattern (pulses, steps, 0);
            int c = 0;
            for (int i = 0; i < steps; ++i) c += p[size_t (i)] ? 1 : 0;
            REQUIRE (c == pulses);
        }
}

TEST_CASE ("applyEuclid writes active flags and step count", "[euclid]")
{
    Track t;
    t.euclid = { 3, 8, 0, false };
    applyEuclid (t);
    CHECK (t.stepCount == 8);
    CHECK (t.steps[0].active);
    CHECK_FALSE (t.steps[1].active);
    CHECK (t.steps[3].active);
    CHECK (t.steps[6].active);
    CHECK_FALSE (t.steps[8].active);
}

// ---------------------------------------------------------------- Directions
TEST_CASE ("Direction sequences", "[directions]")
{
    CHECK (walk (Direction::Forward, 4, 8)  == std::vector<int> { 0, 1, 2, 3, 0, 1, 2, 3 });
    CHECK (walk (Direction::Reverse, 4, 8)  == std::vector<int> { 3, 2, 1, 0, 3, 2, 1, 0 });
    CHECK (walk (Direction::PingPong, 4, 8) == std::vector<int> { 0, 1, 2, 3, 2, 1, 0, 1 });
    CHECK (walk (Direction::Pendulum, 4, 9) == std::vector<int> { 0, 1, 2, 3, 3, 2, 1, 0, 0 });
    CHECK (walk (Direction::PingPong, 1, 4) == std::vector<int> { 0, 0, 0, 0 });
    CHECK (walk (Direction::Forward, 1, 3)  == std::vector<int> { 0, 0, 0 });

    for (int i : walk (Direction::Random, 5, 200)) { CHECK (i >= 0); CHECK (i < 5); }

    const auto rw = walk (Direction::RandomWalk, 6, 500);
    for (size_t i = 0; i < rw.size(); ++i)
    {
        CHECK (rw[i] >= 0);
        CHECK (rw[i] < 6);
        if (i > 0) CHECK (std::abs (rw[i] - rw[i - 1]) == 1);
    }
}

// ---------------------------------------------------------------- Timing
TEST_CASE ("Step lengths incl. triplet and dotted", "[timing]")
{
    CHECK (stepLengthPpq ({ StepDivision::Quarter, StepModifier::Straight }) == Approx (1.0));
    CHECK (stepLengthPpq ({ StepDivision::Sixteenth, StepModifier::Straight }) == Approx (0.25));
    CHECK (stepLengthPpq ({ StepDivision::ThirtySecond, StepModifier::Straight }) == Approx (0.125));
    CHECK (stepLengthPpq ({ StepDivision::Eighth, StepModifier::Triplet }) == Approx (1.0 / 3.0));
    CHECK (stepLengthPpq ({ StepDivision::Sixteenth, StepModifier::Triplet }) == Approx (1.0 / 6.0));
    CHECK (stepLengthPpq ({ StepDivision::Eighth, StepModifier::Dotted }) == Approx (0.75));
    CHECK (stepLengthPpq ({ StepDivision::Sixteenth, StepModifier::Dotted }) == Approx (0.375));
}

TEST_CASE ("Swing offsets", "[swing]")
{
    const double len = 0.25;
    for (auto t : { ShuffleType::Accurate, ShuffleType::MPC, ShuffleType::EightOhEight })
    {
        CHECK (swingOffsetPpq (t, 100.0f, 0, len) == Approx (0.0));   // even steps stay on the grid
        CHECK (swingOffsetPpq (t, 0.0f, 1, len) == Approx (0.0));     // 0% = straight
    }
    CHECK (swingOffsetPpq (ShuffleType::Accurate, 100.0f, 1, len) == Approx (len / 3.0));   // triplet feel
    CHECK (swingOffsetPpq (ShuffleType::Accurate, 50.0f, 1, len) == Approx (len / 6.0));
    CHECK (swingOffsetPpq (ShuffleType::MPC, 100.0f, 1, len) == Approx (len * 0.5));        // 75%
    CHECK (swingOffsetPpq (ShuffleType::MPC, 50.0f, 3, len) == Approx (len * 0.25));
    CHECK (swingOffsetPpq (ShuffleType::EightOhEight, 100.0f, 1, len) == Approx (len / 3.0));
    // 808 snaps to discrete levels: 5% and 0% both land on level 0
    CHECK (swingOffsetPpq (ShuffleType::EightOhEight, 5.0f, 1, len) == Approx (0.0));
}

// ---------------------------------------------------------------- Scale
TEST_CASE ("Scale quantization", "[scale]")
{
    const auto major = kBuiltInScales[0].mask;
    CHECK (quantizeToScale (60, 0, major) == 60);
    CHECK (quantizeToScale (61, 0, major) == 60);   // tie -> down
    CHECK (quantizeToScale (66, 0, major) == 65);
    CHECK (quantizeToScale (63, 2, major) == 62);   // D major: D# -> D
    CHECK (quantizeToScale (61, 0, 0) == 61);       // empty mask = off
    CHECK (quantizeToScale (61, 0, 0xFFF) == 61);   // chromatic

    const auto minPent = maskOf ({ 0, 3, 5, 7, 10 });
    CHECK (quantizeToScale (62, 0, minPent) == 63);  // 61 not in scale, 63 is
    CHECK (quantizeToScale (64, 0, minPent) == 63);
    CHECK (quantizeToScale (-1, 0, major) == -1);    // B, in scale (negative input must not UB)

    CHECK (fitToRange (100, 36, 72) == 64);
    CHECK (fitToRange (20, 36, 72) == 44);
    CHECK (fitToRange (80, 60, 64) == 64);           // sub-octave range clamps
}

// ---------------------------------------------------------------- Repeats
TEST_CASE ("Repeat timing curves", "[repeats]")
{
    Rng rng;
    RepeatHit h[kMaxRepeats + 1];
    Step s;

    s.reps = 0;
    CHECK (computeRepeats (s, rng, h) == 1);
    CHECK (h[0].offsetPpq == Approx (0.0));

    s.reps = 3; s.interval = 0.25f; s.curve = 0.0f;
    REQUIRE (computeRepeats (s, rng, h) == 4);
    for (int k = 0; k < 4; ++k) CHECK (h[k].offsetPpq == Approx (0.25 * k));

    s.curve = 100.0f;   // accelerate: gaps shrink, same overall span
    computeRepeats (s, rng, h);
    CHECK (h[3].offsetPpq == Approx (0.75));
    CHECK ((h[1].offsetPpq - h[0].offsetPpq) > (h[2].offsetPpq - h[1].offsetPpq));
    CHECK ((h[2].offsetPpq - h[1].offsetPpq) > (h[3].offsetPpq - h[2].offsetPpq));

    s.curve = -100.0f;  // decelerate: gaps grow
    computeRepeats (s, rng, h);
    CHECK (h[3].offsetPpq == Approx (0.75));
    CHECK ((h[1].offsetPpq - h[0].offsetPpq) < (h[2].offsetPpq - h[1].offsetPpq));
    CHECK ((h[2].offsetPpq - h[1].offsetPpq) < (h[3].offsetPpq - h[2].offsetPpq));

    s.curve = 0.0f; s.shape = RepeatShape::RampUp;
    computeRepeats (s, rng, h);
    CHECK (h[0].velScale < h[1].velScale);
    CHECK (h[1].velScale < h[3].velScale);
    CHECK (h[3].velScale == Approx (1.0f));

    s.shape = RepeatShape::RampDown;
    computeRepeats (s, rng, h);
    CHECK (h[0].velScale > h[3].velScale);

    s.reps = 20;   // clamped to 8
    CHECK (computeRepeats (s, rng, h) == kMaxRepeats + 1);
}

// ---------------------------------------------------------------- Scheduler
TEST_CASE ("Steps land on exact samples and are independent of block size", "[scheduler]")
{
    auto render = [] (int blockSize, int numBlocks)
    {
        Harness h;
        for (auto& s : h.track().steps) s.active = true;   // 16 steps of 1/16
        h.run (120.0, blockSize, numBlocks);
        return h;
    };

    // 120 bpm @ 48k: 1/16 = 0.25 ppq = 6000 samples, default gate 0.5 -> 3000 samples
    const auto a = render (512, 100);
    const auto ons = a.times (true), offs = a.times (false);
    REQUIRE (ons.size() == 9);
    REQUIRE (offs.size() == 9);
    for (size_t i = 0; i < ons.size(); ++i)
    {
        CHECK (closeTo (ons[i], int64_t (i) * 6000));
        CHECK (closeTo (offs[i] - ons[i], 3000));
    }

    for (int bs : { 1, 64, 333, 1000 })
    {
        const auto b = render (bs, 51200 / bs + 1);
        const auto bo = b.times (true);
        for (size_t i = 0; i < 9; ++i)
        {
            REQUIRE (bo.size() > i);
            CHECK (closeTo (bo[i], int64_t (i) * 6000));
        }
    }
}

TEST_CASE ("Starting mid-step waits for the next step", "[scheduler]")
{
    Harness h;
    for (auto& s : h.track().steps) s.active = true;
    h.block (0.1, 120.0, 4800);   // 0.1 ppq in; next step boundary is 0.25 ppq = 3600 samples later
    h.block (0.1 + 4800 / 24000.0, 120.0, 6000);
    const auto ons = h.times (true);
    REQUIRE (! ons.empty());
    CHECK (closeTo (ons[0], 3600));
    CHECK (closeTo (ons[1], 3600 + 6000));
}

TEST_CASE ("Tempo change keeps events aligned to ppq", "[scheduler]")
{
    Harness h;
    for (auto& s : h.track().steps) s.active = true;
    h.run (120.0, 480, 100);   // 48000 samples = 2.0 ppq
    for (int b = 0; b < 20; ++b)
        h.block (2.0 + double (h.samplePos - 48000) * 240.0 / 60.0 / 48000.0, 240.0, 480);

    // at 240 bpm 1 ppq = 12000 samples: 2.25 ppq -> 51000, 2.5 ppq -> 54000
    const auto ons = h.times (true);
    bool a = false, b = false;
    for (auto t : ons) { a = a || closeTo (t, 51000); b = b || closeTo (t, 54000); }
    CHECK (a);
    CHECK (b);
}

TEST_CASE ("Loop wrap flushes hanging notes at the jump and re-aligns", "[scheduler]")
{
    Harness h;
    auto& tr = h.track();
    tr.steps[15].active = true;
    tr.steps[15].length = 2.0f;   // gate 0.5 ppq: starts at 3.75, would end at 4.25 - after the loop end (4.0)

    // 1 bar loop = 4 ppq = 96000 samples; host blocks of 500 samples never straddle the wrap
    for (int b = 0; b <= 192; ++b)
        h.block (double ((int64_t (b) * 500) % 96000) / 24000.0, 120.0, 500);

    const auto ons = h.times (true), offs = h.times (false);
    REQUIRE (ons.size() == 1);
    REQUIRE (offs.size() == 1);
    CHECK (closeTo (ons[0], 90000));
    CHECK (offs[0] == 96000);   // flushed at sample 0 of the first block after the wrap

    // and the loop plays again from the top
    for (int b = 193; b < 400; ++b)
        h.block (double ((int64_t (b) * 500) % 96000) / 24000.0, 120.0, 500);
    CHECK (h.times (true).size() == 2);
    CHECK (closeTo (h.times (true)[1], 96000 + 90000));
}

TEST_CASE ("Transport jump backwards re-aligns steps", "[scheduler]")
{
    Harness h;
    for (auto& s : h.track().steps) s.active = true;
    h.block (8.0, 120.0, 1000);          // starts on a step boundary
    const size_t before = h.times (true).size();
    h.block (1.0, 120.0, 1000);          // jump back to 1.0 (also a boundary)
    const auto ons = h.times (true);
    REQUIRE (ons.size() == before + 1);
    CHECK (ons.back() == 1000);          // fires at the first sample of the new position
}

TEST_CASE ("Stopping the transport releases sounding notes", "[scheduler]")
{
    Harness h;
    h.track().steps[0].active = true;
    h.track().steps[0].length = 8.0f;   // 2 ppq gate
    h.run (120.0, 512, 10);
    REQUIRE (h.times (true).size() == 1);
    REQUIRE (h.times (false).empty());

    const int64_t stopAt = h.samplePos;
    h.block (0.0, 120.0, 512, false);
    REQUIRE (h.times (false).size() == 1);
    CHECK (h.times (false)[0] == stopAt);

    h.block (0.0, 120.0, 512, false);   // stays silent while stopped
    CHECK (h.times (false).size() == 1);
}

TEST_CASE ("Requesting a resync (pattern change) flushes notes", "[scheduler]")
{
    Harness h;
    h.track().steps[0].active = true;
    h.track().steps[0].length = 8.0f;
    h.run (120.0, 512, 4);
    h.sched.requestResync();
    const int64_t at = h.samplePos;
    h.run (120.0, 512, 1);
    REQUIRE (h.times (false).size() == 1);
    CHECK (h.times (false)[0] == at);
}

TEST_CASE ("Ratchets cross block boundaries", "[scheduler]")
{
    Harness h;
    h.track().stepLength = { StepDivision::Quarter, StepModifier::Straight };
    h.track().linkStepLength = false;
    auto& s = h.track().steps[0];
    s.active = true; s.reps = 3; s.interval = 0.25f;
    h.run (120.0, 512, 70);   // long enough for the last hit's 0.5 ppq gate to end
    const auto ons = h.times (true);
    REQUIRE (ons.size() == 4);
    for (size_t i = 0; i < 4; ++i) CHECK (closeTo (ons[i], int64_t (i) * 6000));
    CHECK (h.times (false).size() == 4);
}

TEST_CASE ("Shuffle delays odd steps", "[scheduler]")
{
    Harness h;
    for (auto& s : h.track().steps) s.active = true;
    h.g.shuffleType = ShuffleType::Accurate;
    h.g.shuffleAmount = 100.0f;
    h.run (120.0, 512, 40);
    const auto ons = h.times (true);
    REQUIRE (ons.size() >= 3);
    CHECK (closeTo (ons[0], 0));
    CHECK (closeTo (ons[1], 6000 + 2000));
    CHECK (closeTo (ons[2], 12000));
}

TEST_CASE ("Polymetric tracks realign after the common cycle", "[scheduler]")
{
    Harness h;
    h.scene.numTracks = 2;
    h.track (0).outNote = 60; h.track (0).stepCount = 3; h.track (0).steps[0].active = true;
    h.track (1).outNote = 62; h.track (1).stepCount = 4; h.track (1).steps[0].active = true;
    h.run (120.0, 500, 160);   // 80000 samples

    const auto a = h.times (true, 60), b = h.times (true, 62);
    std::vector<int64_t> coincide;
    for (auto x : a) for (auto y : b) if (closeTo (x, y)) coincide.push_back (x);
    REQUIRE (coincide.size() == 2);
    CHECK (closeTo (coincide[0], 0));
    CHECK (closeTo (coincide[1], 72000));   // lcm(3,4) = 12 steps * 6000
    CHECK (a.size() == 5);               // 0, 3, 6, 9, 12 steps
    CHECK (b.size() == 4);               // 0, 4, 8, 12 steps
}

TEST_CASE ("Choke group cuts the other track before the new note", "[scheduler]")
{
    Harness h;
    h.scene.numTracks = 2;
    h.track (0).outNote = 60; h.track (0).chokeMode = 2;
    h.track (0).steps[0].active = true; h.track (0).steps[0].length = 8.0f;   // 2 ppq gate
    h.track (1).outNote = 64; h.track (1).chokeMode = 2;
    h.track (1).steps[2].active = true;                                       // at 0.5 ppq = 12000 samples
    h.run (120.0, 500, 150);   // 75000 samples

    const auto offs60 = h.times (false, 60);
    REQUIRE (offs60.size() == 1);   // exactly one off: the cancelled scheduled off must not fire later
    CHECK (closeTo (offs60[0], 12000));

    size_t iOff = 0, iOn = 0;
    for (size_t i = 0; i < h.events.size(); ++i)
    {
        if (! h.events[i].ev.isOn && h.events[i].ev.note == 60) iOff = i;
        if (h.events[i].ev.isOn && h.events[i].ev.note == 64) iOn = i;
    }
    CHECK (iOff < iOn);
    CHECK (h.events[iOff].sample == h.events[iOn].sample);
}

TEST_CASE ("Mute, solo, probability, conditions", "[scheduler]")
{
    SECTION ("mute")
    {
        Harness h;
        h.track().steps[0].active = true;
        h.track().mute = true;
        h.run (120.0, 500, 50);
        CHECK (h.events.empty());
    }
    SECTION ("solo")
    {
        Harness h;
        h.scene.numTracks = 2;
        h.track (0).outNote = 60; h.track (0).steps[0].active = true;
        h.track (1).outNote = 62; h.track (1).steps[0].active = true; h.track (1).solo = true;
        h.run (120.0, 500, 10);
        CHECK (h.times (true, 60).empty());
        CHECK (h.times (true, 62).size() == 1);
    }
    SECTION ("probability 0")
    {
        Harness h;
        for (auto& s : h.track().steps) { s.active = true; s.probability = 0.0f; }
        h.run (120.0, 500, 50);
        CHECK (h.events.empty());
    }
    SECTION ("first-loop-only and every-2nd-loop")
    {
        // a 16-step loop of 1/16 = 4 ppq = 96000 samples; run 2.2 loops
        Harness first;
        first.track().steps[0].active = true;
        first.track().steps[0].condition = Condition::First;
        first.run (120.0, 1200, 176);
        CHECK (first.times (true).size() == 1);

        Harness every;
        every.track().steps[0].active = true;
        every.track().steps[0].condition = Condition::EveryN;
        every.track().steps[0].conditionN = 2;
        every.run (120.0, 1200, 250);   // 300000 samples = 3.1 loops: plays on loops 0 and 2
        CHECK (every.times (true).size() == 2);
    }
    SECTION ("fill / not-fill")
    {
        Harness h;
        h.track().steps[0].active = true;
        h.track().steps[0].condition = Condition::Fill;
        h.run (120.0, 500, 10);
        CHECK (h.events.empty());
        h.g.fill = true;
        h.sched.requestResync();
        h.samplePos = 0;
        h.run (120.0, 500, 10);
        CHECK (h.times (true).size() == 1);
    }
}

TEST_CASE ("Transpose, scale snapping and note range", "[scheduler]")
{
    Harness h;
    h.track().outNote = 61;
    h.track().steps[0].active = true;
    h.g.transpose = 3;
    h.run (120.0, 500, 2);
    REQUIRE (h.events.size() >= 1);
    CHECK (h.events[0].ev.note == 64);

    Harness fixed;
    fixed.track().outNote = 61;
    fixed.track().followTranspose = false;
    fixed.track().steps[0].active = true;
    fixed.g.transpose = 3;
    fixed.run (120.0, 500, 2);
    CHECK (fixed.events[0].ev.note == 61);

    Harness snap;
    snap.track().outNote = 61;
    snap.track().steps[0].active = true;
    snap.g.scaleEnabled = true;
    snap.g.scaleMask = kBuiltInScales[0].mask;   // C major
    snap.run (120.0, 500, 2);
    CHECK (snap.events[0].ev.note == 60);
}


// ---------------------------------------------------------------- Stage 3 pieces
TEST_CASE ("TripleBuffer hands over the newest snapshot", "[exchange]")
{
    TripleBuffer<int> tb;
    CHECK_FALSE (tb.acquire());          // nothing published yet
    tb.writeBuffer() = 1; tb.publish();
    tb.writeBuffer() = 2; tb.publish();  // supersedes 1 before the reader looked
    REQUIRE (tb.acquire());
    CHECK (tb.read() == 2);
    CHECK_FALSE (tb.acquire());          // nothing new
    CHECK (tb.read() == 2);              // read() stays stable
}

TEST_CASE ("TripleBuffer never exposes a torn snapshot across threads", "[exchange]")
{
    struct Payload { int a = 0; int pad[64] {}; int b = 0; };
    TripleBuffer<Payload> tb;
    std::atomic<bool> done { false };

    std::thread producer ([&]
    {
        for (int i = 1; i <= 200000; ++i)
        {
            auto& w = tb.writeBuffer();
            w.a = i;
            for (auto& x : w.pad) x = i;
            w.b = i;
            tb.publish();
        }
        done = true;
    });

    int last = 0;
    bool torn = false, backwards = false;
    while (! done.load())
        if (tb.acquire())
        {
            const auto& r = tb.read();
            if (r.a != r.b || r.pad[0] != r.a || r.pad[63] != r.a) torn = true;
            if (r.a < last) backwards = true;
            last = r.a;
        }
    producer.join();
    CHECK_FALSE (torn);
    CHECK_FALSE (backwards);
}

TEST_CASE ("Scheduler reports each track's last triggered step", "[scheduler]")
{
    Harness h;
    for (auto& s : h.track().steps) s.active = true;
    CHECK (h.sched.lastStepIndex (0) == -1);
    h.run (120.0, 500, 14);   // 7000 samples: steps 0 (t=0) and 1 (t=6000) have fired
    CHECK (h.sched.lastStepIndex (0) == 1);
}

TEST_CASE ("Demo pattern is playable", "[scheduler]")
{
    Harness h;
    const auto demo = makeDemoPattern();
    h.scene = demo.scene;
    h.g = demo.global;
    h.run (120.0, 512, 400);   // ~4.3 s
    CHECK (h.times (true, 36).size() > 0);                 // kick
    for (auto& e : h.events) CHECK (isInScale (e.ev.note, 0, demo.global.scaleMask));   // scale snapping applies to every track
}
TEST_CASE ("Per-track transpose offset shifts the output note", "[scheduler]")
{
    Harness h;
    h.track().outNote = 60;
    h.track().transposeOffset = -5;
    h.track().steps[0].active = true;
    h.run (120.0, 500, 2);
    REQUIRE (! h.events.empty());
    CHECK (h.events[0].ev.note == 55);
}
// ---------------------------------------------------------------- Stage 5: scenes, recording, transpose input
namespace
{
struct SeqHarness
{
    static constexpr double sr = 48000.0;
    Sequencer seq;
    Project project;
    EventList list;
    std::vector<Hit> events;
    int64_t samplePos = 0;

    SeqHarness()
    {
        project.numScenes = 2;
        for (auto& s : project.scenes) s.numTracks = 1;
        for (auto& st : project.scenes[0].tracks[0].steps) st.active = true;
        project.scenes[0].tracks[0].outNote = 60;
        for (auto& st : project.scenes[1].tracks[0].steps) st.active = true;
        project.scenes[1].tracks[0].outNote = 72;
    }

    void block (int n, bool playing = true, double bpm = 120.0)
    {
        list.clear();
        BlockInfo bi;
        bi.ppqStart = double (samplePos) * bpm / 60.0 / sr; bi.bpm = bpm; bi.sampleRate = sr; bi.numSamples = n; bi.playing = playing;
        seq.process (bi, project, project.global, 4.0, list);
        for (int i = 0; i < list.size(); ++i)
        {
            REQUIRE (list[i].sampleOffset >= 0);
            REQUIRE (list[i].sampleOffset < n);
            events.push_back ({ samplePos + list[i].sampleOffset, list[i] });
        }
        samplePos += n;
    }

    std::vector<int64_t> times (bool on, int note) const
    {
        std::vector<int64_t> v;
        for (auto& h : events) if (h.ev.isOn == on && h.ev.note == note) v.push_back (h.sample);
        return v;
    }
};
} // namespace

TEST_CASE ("Scene launch waits for the next bar and splits the block exactly", "[scenes]")
{
    SeqHarness h;
    // a note that would ring past the bar line: step 14 of scene A, gate 4 steps = 1 ppq
    for (auto& st : h.project.scenes[0].tracks[0].steps) st.active = false;
    h.project.scenes[0].tracks[0].steps[14].active = true;
    h.project.scenes[0].tracks[0].steps[14].length = 4.0f;

    for (int b = 0; b < 8; ++b) h.block (700);
    h.seq.requestScene (1);
    CHECK (h.seq.activeScene() == 0);
    for (int b = 0; b < 200; ++b) h.block (700);   // 140000 samples, past the bar line at 96000

    CHECK (h.seq.activeScene() == 1);
    CHECK (h.seq.queuedScene() == -1);

    const auto on72 = h.times (true, 72);
    REQUIRE (! on72.empty());
    CHECK (closeTo (on72[0], 96000));              // scene B's first step lands exactly on the bar line
    for (auto t : h.times (true, 60)) CHECK (t < 96000);

    const auto off60 = h.times (false, 60);
    REQUIRE (off60.size() >= 1);
    CHECK (closeTo (off60[0], 96000));             // hanging note released at the switch point, not later
}

TEST_CASE ("Immediate and end-of-pattern launch quantize", "[scenes]")
{
    SECTION ("immediate")
    {
        SeqHarness h;
        h.project.global.launchQuantize = LaunchQuantize::Immediate;
        h.block (500);
        h.seq.requestScene (1);
        h.block (500);
        CHECK (h.seq.activeScene() == 1);
    }
    SECTION ("end of pattern uses the longest track cycle")
    {
        SeqHarness h;
        h.project.global.launchQuantize = LaunchQuantize::EndOfPattern;
        h.project.scenes[0].tracks[0].stepCount = 8;   // 8 x 1/16 = 2 ppq = 48000 samples
        for (int b = 0; b < 10; ++b) h.block (500);
        h.seq.requestScene (1);
        for (int b = 0; b < 100; ++b) h.block (500);
        const auto on72 = h.times (true, 72);
        REQUIRE (! on72.empty());
        CHECK (closeTo (on72[0], 48000));
    }
    SECTION ("a stopped transport switches at once; bad requests are ignored")
    {
        SeqHarness h;
        h.seq.requestScene (1);
        h.block (500, false);
        CHECK (h.seq.activeScene() == 1);
        h.seq.requestScene (7);   // only 2 scenes exist
        h.block (500, false);
        CHECK (h.seq.activeScene() == 1);
        CHECK (h.seq.queuedScene() == -1);
    }
}

TEST_CASE ("Metronome clicks on the beat and is silent between", "[metronome]")
{
    Metronome m;
    std::vector<float> out (60000, 0.0f);
    for (int b = 0; b < 120; ++b)
    {
        BlockInfo bi;
        bi.ppqStart = double (b * 500) * 120.0 / 60.0 / 48000.0; bi.bpm = 120.0; bi.sampleRate = 48000.0;
        bi.numSamples = 500; bi.playing = true;
        m.process (bi, 4, out.data() + b * 500, nullptr, 500);
    }
    auto energy = [&] (int a, int z) { float e = 0; for (int i = a; i < z; ++i) e += std::abs (out[size_t (i)]); return e; };
    CHECK (energy (1, 1000) > 0.1f);          // beat 0
    CHECK (energy (2000, 23900) == 0.0f);     // gap
    CHECK (energy (24001, 25000) > 0.1f);     // beat 1 (24000 samples at 120 bpm)
    CHECK (energy (48001, 49000) > 0.1f);
}

TEST_CASE ("Recording quantization keeps or removes timing feel", "[recording]")
{
    const auto full = quantizeToStep (1.1, 0.25, 1.0f);
    CHECK (full.step == 4);
    CHECK (full.shift == Approx (0.0f));
    const auto none = quantizeToStep (1.1, 0.25, 0.0f);
    CHECK (none.step == 4);
    CHECK (none.shift == Approx (0.4f).margin (1e-5));
    CHECK (quantizeToStep (1.1, 0.25, 0.5f).shift == Approx (0.2f).margin (1e-5));
    const auto early = quantizeToStep (1.2, 0.25, 0.0f);   // 4.8 steps -> step 5, played early
    CHECK (early.step == 5);
    CHECK (early.shift == Approx (-0.2f).margin (1e-5));

    CHECK (patternIndexForStep (17, 0, 16) == 1);
    CHECK (patternIndexForStep (17, 2, 16) == 3);
    CHECK (patternIndexForStep (0, -1, 16) == 15);

    Track t; t.outNote = 60; t.transposeOffset = 0;
    recordHit (t, 3, 90, 0.1f, 67);
    CHECK (t.steps[3].active);
    CHECK (t.steps[3].velocity == 90);
    CHECK (t.steps[3].shift == Approx (0.1f));
    CHECK (t.steps[3].pitch == 7);
    recordHit (t, 4, 500, 0.9f);           // clamps; drum hit leaves pitch alone
    CHECK (t.steps[4].velocity == 127);
    CHECK (t.steps[4].shift == Approx (0.5f));
    CHECK (t.steps[4].pitch == 0);
    clearTrackSteps (t);
    CHECK_FALSE (t.steps[3].active);
}

TEST_CASE ("MIDI transpose: scale snapping, latch and hold", "[transpose]")
{
    const auto major = kBuiltInScales[0].mask;
    MidiTranspose mt;
    mt.noteOn (64, 0, false, 0xFFF);
    CHECK (mt.semitones() == 4);                     // E3 against C3 reference
    mt.noteOn (61, 0, true, major);                  // C# snaps down to C in C major
    CHECK (mt.semitones() == 0);
    mt.noteOn (67, 7, false, 0xFFF);                 // reference moves with the root: G3 is zero for root G
    CHECK (mt.semitones() == 0);
    mt.reset();
    mt.noteOn (120, 0, false, 0xFFF);
    CHECK (mt.semitones() == 24);                    // clamped

    MidiTranspose latch;
    latch.noteOn (64, 0, false, 0xFFF);
    latch.noteOff (64, TransposeMode::Latch);
    CHECK (latch.semitones() == 4);                  // latch keeps it

    MidiTranspose hold;
    hold.noteOn (64, 0, false, 0xFFF);
    hold.noteOn (67, 0, false, 0xFFF);
    CHECK (hold.semitones() == 7);
    hold.noteOff (67, TransposeMode::Hold);
    CHECK (hold.semitones() == 4);                   // falls back to the key still held
    hold.noteOff (64, TransposeMode::Hold);
    CHECK (hold.semitones() == 0);
}

TEST_CASE ("Demo project has distinct scenes", "[scenes]")
{
    const auto p = makeDemoProject();
    CHECK (p.numScenes == 4);
    auto count = [] (const Scene& s, int track) { int c = 0; for (int i = 0; i < s.tracks[size_t (track)].stepCount; ++i) c += s.tracks[size_t (track)].steps[size_t (i)].active ? 1 : 0; return c; };
    CHECK (count (p.scenes[0], 0) == 5);
    CHECK (count (p.scenes[1], 0) == 7);
    CHECK (count (p.scenes[3], 0) == 0);              // scene D is empty
    CHECK (p.scenes[3].numTracks == p.scenes[0].numTracks);
}
// ---------------------------------------------------------------- Stage 6: generator and factory presets
TEST_CASE ("Generator stays in scale and range, and is deterministic", "[generator]")
{
    Global g;
    g.scaleEnabled = true; g.root = 0; g.scaleMask = maskOf ({ 0, 3, 5, 7, 10 });
    Track a;
    a.outNote = 48;
    auto cfg = presetSettings (GenPreset::Bassline, 7);
    generateTrack (a, g, cfg);

    int active = 0;
    for (int i = 0; i < a.stepCount; ++i)
    {
        const auto& s = a.steps[size_t (i)];
        if (! s.active) continue;
        ++active;
        const int note = a.outNote + s.pitch;
        CHECK (isInScale (note, g.root, g.scaleMask));
        CHECK (s.pitch >= cfg.lowSemis);
        CHECK (s.pitch <= cfg.highSemis);
        CHECK (s.velocity >= 1);
        CHECK (s.velocity <= 127);
    }
    CHECK (active > 0);
    CHECK (active < a.stepCount);

    Track b;
    b.outNote = 48;
    generateTrack (b, g, cfg);
    for (int i = 0; i < a.stepCount; ++i)
    {
        CHECK (a.steps[size_t (i)].active == b.steps[size_t (i)].active);
        CHECK (a.steps[size_t (i)].pitch == b.steps[size_t (i)].pitch);
    }

    Track c;
    c.outNote = 48;
    cfg.seed = 8;
    generateTrack (c, g, cfg);
    bool differs = false;
    for (int i = 0; i < a.stepCount; ++i)
        differs = differs || a.steps[size_t (i)].active != c.steps[size_t (i)].active || a.steps[size_t (i)].pitch != c.steps[size_t (i)].pitch;
    CHECK (differs);
}

TEST_CASE ("Generator density and root emphasis", "[generator]")
{
    Global g;
    g.scaleEnabled = true; g.root = 0; g.scaleMask = maskOf ({ 0, 2, 4, 5, 7, 9, 11 });
    Track t;
    t.outNote = 48;
    t.stepCount = 64;

    auto cfg = presetSettings (GenPreset::Custom, 3);
    cfg.density = 0.0f;
    generateTrack (t, g, cfg);
    for (int i = 0; i < 64; ++i) CHECK_FALSE (t.steps[size_t (i)].active);

    cfg.density = 1.0f; cfg.rootEmphasis = 1.0f;
    generateTrack (t, g, cfg);
    for (int i = 0; i < 64; ++i)
    {
        CHECK (t.steps[size_t (i)].active);
        CHECK (posMod (t.outNote + t.steps[size_t (i)].pitch - g.root, 12) == 0);   // always the root
    }
}

TEST_CASE ("Arpeggio walks the chord in the chosen pattern", "[generator]")
{
    Global g;
    g.scaleEnabled = true; g.root = 0; g.scaleMask = maskOf ({ 0, 2, 4, 5, 7, 9, 11 });
    Track t;
    t.outNote = 60;
    auto cfg = presetSettings (GenPreset::Arpeggio, 1);
    cfg.chordDegrees = 0x15;   // root, third, fifth
    cfg.highSemis = 12;

    cfg.arp = ArpPattern::Up;
    generateTrack (t, g, cfg);
    // C major triad from C3: 0, 4, 7, then the octave (12) as the range allows
    CHECK (t.steps[0].pitch == 0);
    CHECK (t.steps[1].pitch == 4);
    CHECK (t.steps[2].pitch == 7);
    CHECK (t.steps[3].pitch == 12);
    CHECK (t.steps[4].pitch == 0);   // cycles

    cfg.arp = ArpPattern::Down;
    generateTrack (t, g, cfg);
    CHECK (t.steps[0].pitch == 12);
    CHECK (t.steps[3].pitch == 0);

    cfg.arp = ArpPattern::UpDown;
    generateTrack (t, g, cfg);
    CHECK (t.steps[0].pitch == 0);
    CHECK (t.steps[3].pitch == 12);
    CHECK (t.steps[4].pitch == 7);   // coming back down
    CHECK (t.steps[5].pitch == 4);
    CHECK (t.steps[6].pitch == 0);
}

TEST_CASE ("Every factory preset builds, plays, and has valid data", "[factory]")
{
    for (int i = 0; i < kNumFactoryPresets; ++i)
    {
        Harness h;
        auto project = std::make_unique<Project>();
        fillFactoryProject (i, *project);
        REQUIRE (project->numScenes >= 1);
        REQUIRE (project->scenes[0].numTracks >= 1);
        h.scene = project->scenes[0];
        h.g = project->global;
        h.run (120.0, 512, 400);
        if (i != 1)   // "Empty" is silent by design
            CHECK (! h.times (true).empty());
        else
            CHECK (h.times (true).empty());
        for (auto& e : h.events) { CHECK (e.ev.note >= 0); CHECK (e.ev.note <= 127); CHECK (e.ev.channel >= 1); CHECK (e.ev.channel <= 16); }
    }
}