#include "multi_burst/engine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

using multi_burst::Config;
using multi_burst::Engine;
using multi_burst::MusicalClock;
using multi_burst::MusicalTiming;
using multi_burst::ReferenceConfig;
using multi_burst::ReferenceSample;
using multi_burst::Sample;
using multi_burst::Shape;

namespace {

int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; \
} } while (false)

bool near(float a, float b, float tolerance = 0.0001f) {
    return std::fabs(a - b) <= tolerance;
}

void testBurstCountAndFinalTail() {
    for (const int count : {1, 40}) {
        Engine engine(1000.0f);
        Config c;
        c.bursts = static_cast<uint8_t>(count);
        c.shape = Shape::Pulse;
        c.rateHz = 20.0f;
        c.widthSeconds = 0.08f; // Overlap: the final tail must still finish.
        int starts = 0;
        int ends = 0;
        int sequenceEnds = 0;
        int eocSamples = 0;
        int finalStart = -1;
        int completion = -1;
        engine.trigger(c);
        for (int i = 0; i < 2500; ++i) {
            const Sample s = engine.tick(c);
            if (s.burstStarted) {
                ++starts;
                CHECK(s.index == starts - 1);
                finalStart = i;
            }
            ends += s.burstEnded ? 1 : 0;
            if (s.sequenceEnded) {
                ++sequenceEnds;
                completion = i;
                CHECK(s.envelope == 0.0f);
                CHECK(s.gate == 0.0f);
                CHECK(s.active == 0.0f);
            }
            eocSamples += s.eoc > 0.0f ? 1 : 0;
            if (completion < 0) CHECK(s.active == 5.0f);
            else CHECK(!s.burstStarted);
        }
        CHECK(starts == count);
        CHECK(ends == count);
        CHECK(sequenceEnds == 1);
        CHECK(eocSamples == 5);
        CHECK(completion - finalStart >= 80);
        CHECK(completion - finalStart <= 81);
    }
}

void testSlopeAndLatching() {
    for (const float slope : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
        Engine engine(1000.0f);
        Config c;
        c.clockMode = Config::Cv;
        c.bursts = 5;
        c.slope = slope;
        c.shape = Shape::Pulse;
        engine.trigger(c, 0.5f);
        // These controls affect the following trigger, not the armed sequence.
        c.slope = -slope;
        c.bursts = 40;
        int starts = 0;
        for (int i = 0; i < 120; ++i) {
            const Sample s = engine.tick(c, false, false, false, i != 0 && i % 20 == 0);
            if (s.burstStarted) {
                const float fraction = static_cast<float>(starts) / 4.0f;
                const float extreme = slope >= 0.0f ? 0.04f + 0.96f * fraction : 1.0f - 0.96f * fraction;
                const float expected = 4.0f * (1.0f + std::fabs(slope) * (extreme - 1.0f));
                CHECK(near(s.burstLevel, expected));
                ++starts;
            }
        }
        CHECK(starts == 5);
    }
    Engine single(1000.0f);
    Config c;
    c.bursts = 1;
    c.slope = 1.0f;
    single.trigger(c);
    CHECK(near(single.tick(c).burstLevel, 8.0f));
}

void testResetStopAndRetrigger() {
    Engine engine(1000.0f);
    Config c;
    c.clockMode = Config::Cv;
    c.bursts = 3;
    engine.ingestMidiRealtime(0xfa);
    engine.ingestMidiRealtime(0xf8);
    Sample s = engine.tick(c, true, true, true, true);
    CHECK(!s.burstStarted && !s.burstEnded && !s.sequenceEnded);
    CHECK(!s.midiClock && s.envelope == 0.0f && s.active == 0.0f);
    CHECK(!engine.tick(c, false, false, true).burstStarted);

    engine.trigger(c);
    CHECK(engine.tick(c).burstStarted);
    c.retrigger = Config::Ignore;
    s = engine.tick(c, true);
    CHECK(!s.burstStarted && s.index == 0 && s.active == 5.0f);
    s = engine.tick(c, false, false, false, true);
    CHECK(s.burstStarted && s.burstEnded && s.index == 1);
    c.retrigger = Config::Restart;
    s = engine.tick(c, true, false, true, true);
    CHECK(s.burstStarted && s.burstEnded && s.index == 0);

    engine.trigger(c); // Pending start must be cancelled by Stop.
    for (int i = 0; i < 12; ++i) engine.ingestMidiRealtime(0xf8);
    engine.stop();
    s = engine.tick(c);
    CHECK(!s.burstStarted && s.active == 0.0f && s.gate == 0.0f && s.eoc == 0.0f);
    engine.trigger(c);
    engine.ingestMidiRealtime(0xfc);
    s = engine.tick(c, true, false, true, true);
    CHECK(!s.burstStarted && s.active == 0.0f);
    CHECK(!engine.tick(c).burstStarted);
    engine.ingestMidiRealtime(0xfa);
    CHECK(!engine.tick(c).burstStarted); // MIDI transport only starts MIDI-clock mode.
}

void testRateChangesAndOff() {
    Engine engine(1000.0f);
    Config c;
    c.bursts = 40;
    c.widthSeconds = 0.005f;
    c.rateHz = 4.0f;
    engine.trigger(c);
    int previous = -1;
    int starts = 0;
    for (int i = 0; i < 600; ++i) {
        if (i == 126) c.rateHz = 8.0f;
        const Sample s = engine.tick(c);
        if (s.burstStarted) {
            if (starts == 1) CHECK(i >= 187 && i <= 190);
            if (starts > 1) CHECK(i - previous >= 124 && i - previous <= 126);
            previous = i;
            ++starts;
        }
    }
    CHECK(starts >= 4);
    c.rateHz = 0.0f;
    for (int i = 0; i < 2000; ++i) {
        const Sample s = engine.tick(c);
        CHECK(!s.burstStarted && !s.midiClock);
    }
    CHECK(engine.tick(c, false, false, false, true).burstStarted);
    c.clockMode = Config::InternalAndCv;
    CHECK(engine.tick(c, false, false, true).burstStarted);
}

void testWaitingCvDividerAndManualStep() {
    Engine engine(1000.0f);
    Config c;
    c.clockMode = Config::Cv;
    c.waitForClock = true;
    c.bursts = 4;
    c.cvDivider = 2;
    c.cvPpqn = 1;
    engine.trigger(c);
    Sample s = engine.tick(c);
    CHECK(s.active == 5.0f && s.gate == 0.0f && !s.burstStarted);
    CHECK(!engine.tick(c, false, false, true).burstStarted);
    s = engine.tick(c, false, false, true);
    CHECK(s.burstStarted && s.index == 0);
    s = engine.tick(c, false, false, false, true);
    CHECK(s.burstStarted && s.index == 1);
    CHECK(!engine.tick(c, false, false, true).burstStarted);
    s = engine.tick(c, false, false, true, true);
    CHECK(s.burstStarted && s.index == 2); // Simultaneous clocks are merged.
    s = engine.tick(c, false, false, false, true);
    CHECK(s.burstStarted && s.index == 3);
    CHECK(!engine.tick(c, false, false, true, true).burstStarted);

    // PPQN changes MIDI-clock conversion, never synthesizes additional bursts.
    for (const uint8_t ppqn : {uint8_t{1}, uint8_t{4}, uint8_t{24}}) {
        engine.reset();
        c.cvDivider = 1;
        c.cvPpqn = ppqn;
        c.bursts = 40;
        engine.trigger(c);
        int starts = 0;
        for (int i = 0; i < 1000; ++i) starts += engine.tick(c, false, false, i % 250 == 0).burstStarted;
        CHECK(starts == 4);
    }

    engine.reset();
    c.clockMode = Config::InternalRate;
    engine.trigger(c);
    CHECK(engine.tick(c, false, false, false, true).burstStarted);
}

void testMidiRealtimeAndTempo() {
    Engine engine(1000.0f);
    Config c;
    c.clockMode = Config::Midi;
    c.waitForClock = true;
    c.midiTicksPerBurst = 6;
    engine.trigger(c);
    int starts = 0;
    int clocks = 0;
    for (int i = 0; i < 18; ++i) {
        engine.ingestMidiRealtime(0xf8);
        const Sample s = engine.tick(c);
        starts += s.burstStarted;
        clocks += s.midiClock;
    }
    CHECK(starts == 3 && clocks == 18);
    for (int i = 0; i < 24; ++i) engine.ingestMidiRealtime(0xf8);
    clocks = 0;
    for (int i = 0; i < 30; ++i) clocks += engine.tick(c).midiClock;
    CHECK(clocks == 24);

    c.waitForClock = false;
    engine.ingestMidiRealtime(0xfa);
    engine.ingestMidiRealtime(0xf8);
    Sample s = engine.tick(c);
    CHECK(s.burstStarted && s.index == 0 && s.midiClock);
    engine.ingestMidiRealtime(0xfc);
    engine.ingestMidiRealtime(0xfa); // The latest transport command wins.
    CHECK(engine.tick(c).burstStarted);
    engine.ingestMidiRealtime(0xfa);
    engine.ingestMidiRealtime(0xfc);
    CHECK(engine.tick(c, true).active == 0.0f);
    engine.ingestMidiRealtime(0xfb);
    CHECK(!engine.tick(c).burstStarted);
    engine.trigger(c);
    CHECK(engine.tick(c).burstStarted);
    engine.ingestMidiRealtime(0xff);
    CHECK(engine.tick(c, true).active == 0.0f);

    c.waitForClock = true;
    c.midiTicksPerBurst = 3;
    for (int i = 0; i < 3; ++i) engine.ingestMidiRealtime(0xf8);
    engine.ingestMidiRealtime(0xfa);
    for (int i = 0; i < 4; ++i) {
        s = engine.tick(c);
        CHECK(!s.midiClock && !s.burstStarted); // Start discards preceding clocks.
    }
    engine.ingestMidiRealtime(0xfa);
    engine.ingestMidiRealtime(0xfb); // Continue must retain a pending Start.
    for (int i = 0; i < 3; ++i) engine.ingestMidiRealtime(0xf8);
    CHECK(!engine.tick(c).burstStarted);
    CHECK(!engine.tick(c).burstStarted);
    CHECK(engine.tick(c).burstStarted); // Clocks following Start are preserved.
    engine.ingestMidiRealtime(0xff);
    engine.ingestMidiRealtime(0xfa);
    CHECK(!engine.tick(c).burstStarted); // System Reset still wins.

    c.clockMode = Config::InternalTempo;
    c.midiTicksPerBurst = 6;
    c.bpm = 120.0f;
    c.bursts = 40;
    c.waitForClock = true;
    engine.reset();
    engine.trigger(c);
    starts = clocks = 0;
    for (int i = 0; i < 1000; ++i) {
        c.rateHz = i < 500 ? 1.0f : 30.0f;
        s = engine.tick(c);
        starts += s.burstStarted;
        clocks += s.midiClock;
    }
    CHECK(clocks >= 47 && clocks <= 48);
    CHECK(starts >= 7 && starts <= 8);

    c.clockMode = Config::InternalRate;
    c.rateHz = 4.0f;
    engine.reset();
    clocks = 0;
    for (int i = 0; i < 1000; ++i) clocks += engine.tick(c).midiClock;
    CHECK(clocks >= 95 && clocks <= 96);
}

void testCvMidiClockTimeout() {
    Engine engine(1000.0f);
    Config c;
    c.clockMode = Config::Cv;
    c.cvPpqn = 1;
    CHECK(!engine.tick(c, false, false, true).midiClock);
    for (int i = 1; i < 500; ++i) CHECK(!engine.tick(c).midiClock);
    CHECK(engine.tick(c, false, false, true).midiClock);
    int clocks = 1;
    for (int i = 501; i < 1000; ++i) clocks += engine.tick(c).midiClock;
    CHECK(clocks == 24);
    CHECK(engine.tick(c, false, false, true).midiClock);
    int lastClockSample = -1;
    for (int i = 1001; i < 3500; ++i) {
        if (engine.tick(c).midiClock) lastClockSample = i;
    }
    CHECK(lastClockSample >= 2900 && lastClockSample < 3000);
    for (int i = 0; i < 100; ++i) CHECK(!engine.tick(c).midiClock);
    CHECK(!engine.tick(c, false, false, true).midiClock);
    for (int i = 1; i < 500; ++i) CHECK(!engine.tick(c).midiClock);
    CHECK(engine.tick(c, false, false, true).midiClock);

    // A 48-PPQN input must be divided, not forwarded at twice the MIDI rate.
    engine.reset();
    c.cvPpqn = 48;
    clocks = 0;
    for (int i = 0; i < 1000; ++i) clocks += engine.tick(c, false, false, i % 10 == 0).midiClock;
    CHECK(clocks >= 49 && clocks <= 50);
}

void testCvTwelvePpqn() {
    for (const int sampleRate : {1000, 48000}) {
        Engine engine(static_cast<float>(sampleRate));
        Config c;
        c.clockMode = Config::Cv;
        c.cvPpqn = 12;
        c.waitForClock = true;
        c.bursts = 40;
        const int period = sampleRate / 10;
        engine.trigger(c);
        Sample s = engine.tick(c, false, false, true);
        CHECK(s.burstStarted && !s.midiClock);
        for (int i = 1; i < period; ++i) CHECK(!engine.tick(c).midiClock);
        for (int cycle = 0; cycle < 5; ++cycle) {
            s = engine.tick(c, false, false, true);
            CHECK(s.burstStarted && s.midiClock);
            int interpolatedTicks = 0;
            for (int i = 1; i < period; ++i) {
                s = engine.tick(c);
                CHECK(!s.burstStarted);
                if (s.midiClock) {
                    ++interpolatedTicks;
                    CHECK(std::abs(i - period / 2) <= 1);
                }
            }
            CHECK(interpolatedTicks == 1); // 12 PPQN -> 2 MIDI ticks per CV period.
        }
    }
}

void testTempoClockPrecision() {
    // Compare each MIDI tick with ideal musical time, independently of engine phase.
    for (const int sampleRate : {44100, 48000, 96000, 192000}) {
        for (const float bpm : {73.125f, 120.0f, 123.456f, 240.0f}) {
            Engine engine(static_cast<float>(sampleRate));
            Config c;
            c.clockMode = Config::InternalTempo;
            c.bpm = bpm;
            const double clockHz = static_cast<double>(bpm) * 24.0 / 60.0;
            const int totalSamples = sampleRate * 3;
            int clocks = 0;
            for (int i = 0; i < totalSamples; ++i) {
                if (engine.tick(c).midiClock) {
                    ++clocks;
                    const int ideal = static_cast<int>(std::ceil(static_cast<double>(clocks) * sampleRate / clockHz));
                    CHECK(std::abs((i + 1) - ideal) <= 1);
                }
            }
            const int idealClocks = static_cast<int>(std::floor(3.0 * clockHz));
            CHECK(std::abs(clocks - idealClocks) <= 1);
        }
    }
}

void testShapesAndLiveControls() {
    float quarter[8] = {};
    for (int shape = 0; shape < 8; ++shape) {
        Engine engine(1000.0f);
        Config c;
        c.bursts = 1;
        c.widthSeconds = 1.0f;
        c.level = 1.0f;
        c.shape = static_cast<Shape>(shape);
        engine.trigger(c);
        for (int i = 0; i <= 1001; ++i) {
            const Sample s = engine.tick(c);
            CHECK(s.envelope >= 0.0f && s.envelope <= 1.0001f);
            if (i == 250) quarter[shape] = s.envelope;
            if (i == 500 && c.shape == Shape::Sine) CHECK(near(s.envelope, 1.0f, 0.0002f));
        }
    }
    CHECK(near(quarter[static_cast<int>(Shape::Sine)], std::sqrt(0.5f), 0.0002f));
    CHECK(quarter[static_cast<int>(Shape::LogDown)] > quarter[static_cast<int>(Shape::RampDown)]);
    CHECK(quarter[static_cast<int>(Shape::RampDown)] > quarter[static_cast<int>(Shape::ExpoDown)]);
    CHECK(quarter[static_cast<int>(Shape::LogUp)] > quarter[static_cast<int>(Shape::RampUp)]);
    CHECK(near(quarter[static_cast<int>(Shape::Triangle)], 0.5f));
    CHECK(near(quarter[static_cast<int>(Shape::Pulse)], 1.0f));

    Engine engine(1000.0f);
    Config c;
    c.clockMode = Config::Cv;
    c.bursts = 1;
    c.widthSeconds = 1.0f;
    c.shape = Shape::RampUp;
    engine.trigger(c);
    Sample s;
    for (int i = 0; i <= 250; ++i) s = engine.tick(c);
    CHECK(near(s.envelope, 2.0f));
    c.shape = Shape::Pulse;
    c.level = 3.0f;
    CHECK(near(engine.tick(c).envelope, 3.0f));
    c.widthSeconds = 0.01f;
    int remaining = 0;
    do { s = engine.tick(c); ++remaining; } while (s.active > 0.0f && remaining < 20);
    CHECK(remaining >= 7 && remaining <= 8);
}

void testSampleRatesAndInvalidInput() {
    for (const float rate : {1000.0f, 44100.0f, 48000.0f, 96000.0f, 192000.0f}) {
        Engine engine(rate);
        Config c;
        c.bursts = 1;
        c.widthSeconds = 0.025f;
        c.shape = Shape::Pulse;
        engine.trigger(c);
        int completed = -1;
        const int expected = static_cast<int>(rate * c.widthSeconds);
        for (int i = 0; i <= expected + 3; ++i) {
            if (engine.tick(c).sequenceEnded) completed = i;
        }
        CHECK(completed >= expected && completed <= expected + 1);
    }
    Engine engine(std::numeric_limits<float>::quiet_NaN());
    Config c;
    c.level = std::numeric_limits<float>::infinity();
    c.widthSeconds = std::numeric_limits<float>::quiet_NaN();
    c.slope = std::numeric_limits<float>::infinity();
    c.rateHz = -100.0f;
    c.bpm = std::numeric_limits<float>::quiet_NaN();
    c.shape = Shape::Pulse;
    c.bursts = 255;
    engine.trigger(c, std::numeric_limits<float>::quiet_NaN());
    for (int i = 0; i < 100; ++i) {
        const Sample s = engine.tick(c, false, false, false, i % 2 == 0);
        CHECK(std::isfinite(s.envelope) && s.envelope >= 0.0f && s.envelope <= 10.0f);
        CHECK(s.index < 40);
    }
    engine.setSampleRate(96000.0f);
    c = Config{};
    c.bursts = 1;
    c.shape = Shape::Pulse;
    c.widthSeconds = 0.001f;
    engine.trigger(c);
    int completion = -1;
    for (int i = 0; i < 100; ++i) if (engine.tick(c).sequenceEnded) completion = i;
    CHECK(completion >= 96 && completion <= 97);
}

void testLoopWaitsForTheFinalTailAndCanBeStopped() {
    Engine engine(1000.0f);
    Config c;
    c.loop = true;
    c.bursts = 2;
    c.clockMode = Config::Cv;
    c.widthSeconds = 0.02f;
    c.shape = Shape::Pulse;
    engine.trigger(c);
    CHECK(engine.tick(c).burstStarted);
    Sample s = engine.tick(c, false, false, false, true);
    CHECK(s.burstStarted && s.index == 1);
    int completedAt = -1;
    for (int i = 1; i < 30; ++i) {
        s = engine.tick(c, false, false, false, true);
        if (s.sequenceEnded) {
            completedAt = i;
            CHECK(!s.burstStarted && s.envelope == 0.0f && s.gate == 0.0f);
            CHECK(s.eoc == 5.0f && near(s.progress, 10.0f));
            break;
        }
        CHECK(!s.burstStarted); // Extra clocks cannot truncate the last envelope.
        CHECK(s.active == 5.0f);
    }
    CHECK(completedAt >= 19 && completedAt <= 21);
    s = engine.tick(c);
    CHECK(s.burstStarted && s.index == 0 && near(s.progress, 0.0f));
    CHECK(s.eoc == 5.0f);
    for (int i = 0; i < 3; ++i) CHECK(engine.tick(c).eoc == 5.0f);
    CHECK(engine.tick(c).eoc == 0.0f); // Automatic looping preserves the whole EOC pulse.

    // Stop wins over Trigger, clock, and a possible automatic restart.
    s = engine.tick(c, true, false, true, true, true);
    CHECK(!s.burstStarted && !s.sequenceEnded);
    CHECK(s.envelope == 0.0f && s.gate == 0.0f && s.active == 0.0f);
    CHECK(s.burst == 0.0f && s.eoc == 0.0f && s.progress == 0.0f);
    for (int i = 0; i < 50; ++i) CHECK(!engine.tick(c, false, false, true).burstStarted);

    engine.trigger(c);
    CHECK(engine.tick(c).burstStarted);
    s = engine.tick(c, true, true, true, true, true);
    CHECK(!s.burstStarted && s.active == 0.0f && s.progress == 0.0f);
    CHECK(!engine.tick(c).burstStarted);

    // Waiting loops re-arm after the tail, then wait for a new clock.
    c.waitForClock = true;
    c.bursts = 1;
    engine.trigger(c);
    CHECK(!engine.tick(c).burstStarted);
    CHECK(engine.tick(c, false, false, true).burstStarted);
    for (int i = 0; i < 25; ++i) engine.tick(c);
    s = engine.tick(c);
    CHECK(s.active == 5.0f && !s.burstStarted && s.gate == 0.0f);
    s = engine.tick(c, false, false, true);
    CHECK(s.burstStarted && s.index == 0);

    // Stop and Reset must also cancel the restart already queued by completion.
    for (const bool reset : {false, true}) {
        engine.reset();
        c.waitForClock = false;
        c.widthSeconds = 0.002f;
        engine.trigger(c);
        engine.tick(c);
        bool completed = false;
        for (int i = 0; i < 5 && !completed; ++i) completed = engine.tick(c).sequenceEnded;
        CHECK(completed);
        s = engine.tick(c, true, reset, true, true, !reset);
        CHECK(!s.burstStarted && !s.sequenceEnded && s.active == 0.0f);
        CHECK(s.envelope == 0.0f && s.gate == 0.0f && s.eoc == 0.0f && s.burst == 0.0f);
        for (int i = 0; i < 5; ++i) CHECK(!engine.tick(c).burstStarted);
    }
}

void testMuteAndAuxiliaryOutputs() {
    Engine audible(1000.0f), muted(1000.0f);
    Config c;
    c.clockMode = Config::Cv;
    c.bursts = 3;
    c.slope = 1.0f;
    c.level = 2.0f;
    c.shape = Shape::Pulse;
    c.widthSeconds = 0.005f;
    audible.trigger(c, 0.25f);
    muted.trigger(c, 0.25f);
    int starts = 0, burstPulseSamples = 0;
    Sample last;
    for (int i = 0; i < 50; ++i) {
        const bool step = i == 10 || i == 20;
        const Sample a = audible.tick(c, false, false, false, step);
        Config silence = c;
        silence.mute = true;
        const Sample b = muted.tick(silence, false, false, false, step);
        CHECK(b.envelope == 0.0f && b.gate == 0.0f);
        CHECK(near(a.levels, b.levels) && near(a.progress, b.progress));
        CHECK(a.burst == b.burst && a.active == b.active && a.eoc == b.eoc);
        CHECK(a.burstStarted == b.burstStarted && a.sequenceEnded == b.sequenceEnded);
        burstPulseSamples += b.burst > 0.0f;
        if (b.burstStarted) {
            const float expectedGain = 0.04f + 0.48f * starts;
            CHECK(near(b.levels, expectedGain * 10.0f));
            CHECK(near(b.progress, starts * 5.0f));
            CHECK(near(a.burstLevel, expectedGain * 0.5f));
            ++starts;
        }
        last = b;
    }
    CHECK(starts == 3 && burstPulseSamples == 3); // Burst is a 1 ms event.
    CHECK(near(last.progress, 10.0f) && last.active == 0.0f);
    c.level = 0.0f;
    last = muted.tick(c);
    CHECK(near(last.progress, 10.0f));
    last = muted.tick(c, false, true);
    CHECK(last.levels == 0.0f && last.progress == 0.0f && last.burst == 0.0f);
}

void testVelocityTrimAndGlide() {
    for (const bool velocityEnabled : {false, true}) {
        Engine engine(1000.0f);
        Config c;
        c.velocityEnabled = velocityEnabled;
        c.level = 4.0f;
        c.levelTrim = 0.5f;
        c.bursts = 1;
        c.shape = Shape::Pulse;
        engine.trigger(c, 0.25f);
        Sample s = engine.tick(c);
        CHECK(near(s.envelope, velocityEnabled ? 0.5f : 2.0f));
        CHECK(near(s.levels, 10.0f));
        c.levelTrim = 0.25f;
        s = engine.tick(c);
        CHECK(near(s.envelope, velocityEnabled ? 0.25f : 1.0f));
    }

    for (const float slope : {-1.0f, 1.0f}) {
        Engine engine(1000.0f);
        Config c;
        c.clockMode = Config::Cv;
        c.bursts = 3;
        c.widthSeconds = 0.5f;
        c.slope = slope;
        c.level = 1.0f;
        c.shape = Shape::Pulse;
        c.glideRiseSeconds = 0.1f;
        c.glideFallSeconds = 0.2f;
        engine.trigger(c);
        Sample s = engine.tick(c);
        const float first = slope > 0.0f ? 0.04f : 1.0f;
        CHECK(near(s.envelope, first)); // First burst never slews from zero.
        for (int i = 0; i < 20; ++i) CHECK(near(engine.tick(c).envelope, first));
        s = engine.tick(c, false, false, false, true);
        CHECK(s.burstStarted && s.index == 1);
        const float step = slope > 0.0f ? 0.01f : -0.005f;
        CHECK(std::fabs(s.envelope - first) <= std::fabs(step) + 0.0001f);
        float previous = s.envelope;
        for (int i = 0; i < 110; ++i) {
            s = engine.tick(c);
            const float expected = slope > 0.0f
                ? (previous + step < 0.52f ? previous + step : 0.52f)
                : (previous + step > 0.52f ? previous + step : 0.52f);
            CHECK(near(s.envelope, expected, 0.0002f));
            previous = s.envelope;
        }
        CHECK(near(s.envelope, 0.52f));
        c.glideRiseSeconds = c.glideFallSeconds = 0.0f;
        s = engine.tick(c, false, false, false, true);
        CHECK(near(s.envelope, slope > 0.0f ? 1.0f : 0.04f));
    }
}

void testReferenceEnvelopeCurvesAndClockPhase() {
    float legacy[2] = {}, reference[2] = {};
    for (int curve = 0; curve < 2; ++curve) {
        for (const bool referenceCurves : {false, true}) {
            Engine engine(1000.0f);
            Config c;
            c.referenceCurves = referenceCurves;
            c.bursts = 1;
            c.level = 1.0f;
            c.widthSeconds = 1.0f;
            c.shape = curve == 0 ? Shape::LogDown : Shape::LogUp;
            engine.trigger(c);
            Sample s;
            for (int i = 0; i <= 250; ++i) s = engine.tick(c);
            (referenceCurves ? reference : legacy)[curve] = s.envelope;
        }
    }
    CHECK(reference[0] < legacy[0]);
    CHECK(reference[1] < legacy[1]);
    CHECK(near(reference[0], std::log10(7.75f), 0.00001f));
    CHECK(near(reference[1], std::log10(3.25f), 0.00001f));
    CHECK(near(legacy[0], 1.0f - 0.25f * 0.25f * 0.25f));
    CHECK(near(legacy[1], 1.0f - 0.75f * 0.75f * 0.75f));

    for (const bool preserve : {false, true}) {
        Engine engine(1000.0f);
        Config c;
        c.preserveClockPhase = preserve;
        c.rateHz = 4.0f;
        c.waitForClock = true;
        for (int i = 0; i < 125; ++i) engine.tick(c);
        engine.trigger(c);
        int firstStart = -1;
        for (int i = 0; i < 260; ++i) {
            const Sample s = engine.tick(c);
            if (s.burstStarted) CHECK(s.clockTick && s.clock == 5.0f);
            if (s.burstStarted && firstStart < 0) firstStart = i;
        }
        CHECK(std::abs(firstStart - (preserve ? 124 : 249)) <= 1);
    }
}

void testContinuousModulationScale() {
    CHECK(near(multi_burst::modulationScale(0.0f), 1.0f));
    CHECK(near(multi_burst::modulationScale(-4.0f), 1.0f / 16.0f));
    CHECK(near(multi_burst::modulationScale(4.0f), 16.0f));
    CHECK(near(multi_burst::modulationScale(-100.0f), 1.0f / 16.0f));
    CHECK(near(multi_burst::modulationScale(100.0f), 16.0f));
    CHECK(near(multi_burst::modulationScale(-0.5f), std::sqrt(0.5f), 0.00001f));
    CHECK(near(multi_burst::modulationScale(0.5f), std::sqrt(2.0f), 0.00001f));
    CHECK(multi_burst::modulationScale(std::numeric_limits<float>::quiet_NaN()) == 1.0f);
    CHECK(multi_burst::modulationScale(-std::numeric_limits<float>::infinity()) == 1.0f / 16.0f);
    CHECK(multi_burst::modulationScale(std::numeric_limits<float>::infinity()) == 16.0f);
    float previous = 0.0f;
    for (int i = 0; i <= 8000; ++i) {
        const float octaves = -4.0f + static_cast<float>(i) / 1000.0f;
        const float scale = multi_burst::modulationScale(octaves);
        const float expected = std::exp2(octaves);
        CHECK(std::isfinite(scale) && scale > previous);
        CHECK(std::fabs(scale / expected - 1.0f) < 0.00001f);
        previous = scale;
    }

    // A fractional-octave CV must produce its actual period and duration,
    // rather than select the adjacent octave or a musical-grid interval.
    Engine engine(1000.0f);
    Config c;
    c.bursts = 8;
    c.shape = Shape::Pulse;
    c.rateHz = 4.0f * multi_burst::modulationScale(0.5f);
    c.widthSeconds = 0.01f;
    engine.trigger(c);
    int starts = 0;
    for (int i = 0; i < 1300; ++i) {
        if (engine.tick(c).burstStarted) {
            const int ideal = static_cast<int>(std::ceil(starts * 1000.0 / c.rateHz));
            CHECK(std::abs(i - ideal) <= 1);
            ++starts;
        }
    }
    CHECK(starts == 8);
    c.bursts = 1;
    c.widthSeconds = 0.1f * multi_burst::modulationScale(-0.5f);
    engine.trigger(c);
    int completion = -1;
    for (int i = 0; i < 100; ++i) if (engine.tick(c).sequenceEnded) completion = i;
    CHECK(completion >= 70 && completion <= 71);
}

void testShapeMorphMatchesReferenceCurves() {
    // Morph is an added continuous control. Its integer stops retain the
    // existing DROID curves; AJH's original Shape CV selects discrete curves.
    for (int shape = 0; shape < 8; ++shape) {
        Engine discrete(1000.0f), morph(1000.0f);
        Config c;
        c.clockMode = Config::Cv;
        c.referenceCurves = true;
        c.bursts = 1;
        c.widthSeconds = 1.0f;
        c.level = 1.0f;
        c.shape = static_cast<Shape>(shape);
        Config m = c;
        m.shapePosition = static_cast<float>(shape);
        m.shape = static_cast<Shape>((shape + 3) % 8); // Ignored in Morph mode.
        discrete.trigger(c);
        morph.trigger(m);
        for (int i = 0; i < 1002; ++i) {
            const Sample a = discrete.tick(c), b = morph.tick(m);
            CHECK(a.envelope == b.envelope);
            CHECK(a.gate == b.gate && a.active == b.active);
            CHECK(a.burstStarted == b.burstStarted && a.sequenceEnded == b.sequenceEnded);
        }
    }

    for (int shape = 0; shape < 7; ++shape) {
        Engine lower(1000.0f), upper(1000.0f), morph(1000.0f);
        Config c;
        c.clockMode = Config::Cv;
        c.referenceCurves = true;
        c.bursts = 1;
        c.widthSeconds = 1.0f;
        c.level = 1.0f;
        c.shape = static_cast<Shape>(shape);
        Config u = c, m = c;
        u.shape = static_cast<Shape>(shape + 1);
        m.shapePosition = static_cast<float>(shape) + 0.5f;
        lower.trigger(c);
        upper.trigger(u);
        morph.trigger(m);
        for (int i = 0; i < 1002; ++i) {
            const Sample a = lower.tick(c), b = upper.tick(u), s = morph.tick(m);
            CHECK(near(s.envelope, 0.5f * (a.envelope + b.envelope), 0.000001f));
            CHECK(s.envelope >= 0.0f && s.envelope <= 1.0f);
        }
    }
}

void testShapeMorphLivePositionAndInvalidInput() {
    Engine engine(1000.0f);
    Config c;
    c.clockMode = Config::Cv;
    c.referenceCurves = true;
    c.bursts = 1;
    c.widthSeconds = 1.0f;
    c.level = 10.0f;
    c.shapePosition = 4.0f; // RampUp.
    engine.trigger(c);
    Sample s;
    for (int i = 0; i <= 250; ++i) s = engine.tick(c);
    CHECK(near(s.envelope, 2.5f));
    c.shapePosition = 4.5f; // RampUp/Triangle, still at the current phase.
    s = engine.tick(c);
    CHECK(!s.burstStarted && s.index == 0 && s.active == 5.0f);
    CHECK(near(s.envelope, 3.765f));
    c.shapePosition = 6.25f; // RampDown/Pulse at phase 0.252.
    s = engine.tick(c);
    CHECK(!s.burstStarted && near(s.envelope, 8.11f));
    c.shapePosition = 100.0f; // Clamp to Pulse.
    CHECK(near(engine.tick(c).envelope, 10.0f));
    c.shapePosition = std::numeric_limits<float>::infinity();
    CHECK(near(engine.tick(c).envelope, 10.0f));

    // Negative sentinels and NaN keep the discrete shape path, including
    // legacy curve behavior when referenceCurves is disabled.
    for (const bool referenceCurves : {false, true}) {
        for (const float position : {-1.0f, -0.01f,
                -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            Engine discrete(1000.0f), fallback(1000.0f);
            Config d;
            d.clockMode = Config::Cv;
            d.referenceCurves = referenceCurves;
            d.bursts = 1;
            d.widthSeconds = 1.0f;
            d.shape = Shape::LogUp;
            Config f = d;
            f.shapePosition = position;
            discrete.trigger(d);
            fallback.trigger(f);
            for (int i = 0; i < 1002; ++i) {
                const Sample a = discrete.tick(d), b = fallback.tick(f);
                CHECK(a.envelope == b.envelope);
                CHECK(std::isfinite(b.envelope) && b.envelope >= 0.0f && b.envelope <= 10.0f);
            }
        }
    }
}

void testMusicalGridAndSharedTiming() {
    const float ratios[] = {1.0f / 16, 1.0f / 12, 1.0f / 8, 1.0f / 6,
        1.0f / 4, 1.0f / 3, 1.0f / 2, 2.0f / 3, 1, 1.5f, 2, 3, 4, 6, 8, 16};
    CHECK(multi_burst::glideRatio(0) == 0.0f);
    for (uint8_t index = 0; index < 16; ++index) {
        CHECK(near(multi_burst::musicalRatio(index), ratios[index]));
        CHECK(near(multi_burst::glideRatio(index + 1), ratios[index]));
        MusicalTiming first, second;
        ReferenceSample r;
        r.epoch = 1;
        int events = 0, previous = -1;
        constexpr int samplesPerBeat = 480;
        const float period = ratios[index] * samplesPerBeat;
        for (int i = 0; i < samplesPerBeat * 48; ++i) {
            r.beat = i % samplesPerBeat == 0;
            r.fraction = static_cast<float>(i % samplesPerBeat) / samplesPerBeat;
            const bool a = first.tick(r, index + 1);
            const bool b = second.tick(r, index + 1);
            CHECK(a == b); // Two generators consume the same reference independently.
            if (a) {
                if (previous >= 0) CHECK(std::fabs((i - previous) - period) <= 1.0f);
                previous = i;
                ++events;
            }
        }
        CHECK(std::abs(events - static_cast<int>(48.0f / ratios[index])) <= 1);
    }

    MusicalTiming timing;
    ReferenceSample r;
    r.beat = true;
    for (int i = 0; i < 10; ++i) CHECK(!timing.tick(r, 0));
    r.beat = false;
    r.fraction = 0.75f;
    CHECK(!timing.tick(r, 5)); // Selecting a ratio never invents a clock.
    ++r.epoch;
    CHECK(!timing.tick(r, 1));
    r.ready = false;
    r.beat = false;
    for (int i = 0; i < 100; ++i) {
        r.fraction = static_cast<float>(i) / 100;
        CHECK(!timing.tick(r, 1));
    }
}

void testMusicalReferenceSources() {
    MusicalClock clock(1000.0f);
    ReferenceConfig c;
    ReferenceSample r;
    int beats = 0, midiClocks = 0;
    for (int i = 0; i < 2000; ++i) {
        r = clock.tick(c);
        CHECK(r.running && r.ready && near(r.periodSeconds, 0.5f));
        CHECK(r.fraction >= 0.0f && r.fraction < 1.0f);
        beats += r.beat;
        midiClocks += r.midiClock;
    }
    CHECK(beats >= 3 && beats <= 4);
    CHECK(midiClocks >= 95 && midiClocks <= 96);

    clock.reset();
    c.source = ReferenceConfig::Cv;
    r = clock.tick(c, true);
    CHECK(r.beat && !r.ready);
    for (int i = 1; i < 400; ++i) clock.tick(c);
    r = clock.tick(c, true);
    CHECK(r.beat && r.ready && near(r.periodSeconds, 0.4f));
    MusicalTiming timing;
    timing.tick(r, 1);
    int events = 0;
    for (int i = 0; i < 2000; ++i) {
        r = clock.tick(c);
        CHECK(!r.beat); // No synthesized quarters after a missing external edge.
        events += timing.tick(r, 1);
    }
    CHECK(events <= 15); // Interpolation stops at the next expected edge.

    clock.reset();
    c.source = ReferenceConfig::Midi;
    CHECK(!clock.tick(c).running);
    clock.ingestMidiRealtime(0xfa);
    r = clock.tick(c);
    CHECK(r.start);
    beats = midiClocks = 0;
    for (int i = 0; i < 720; ++i) {
        if (i % 20 == 0) clock.ingestMidiRealtime(0xf8);
        r = clock.tick(c);
        beats += r.beat;
        midiClocks += r.midiClock;
    }
    CHECK(midiClocks == 36 && beats >= 1 && beats <= 2);
    CHECK(r.ready && near(r.periodSeconds, 0.48f));
    clock.ingestMidiRealtime(0xfc);
    r = clock.tick(c);
    CHECK(r.stop && !r.running);
    clock.ingestMidiRealtime(0xfa);
    clock.ingestMidiRealtime(0xff);
    r = clock.tick(c, true, true);
    CHECK(r.reset && !r.start); // Reset has priority over transport and CV.

    clock.reset();
    c.source = ReferenceConfig::Auto;
    CHECK(near(clock.tick(c).periodSeconds, 0.5f)); // No external activity yet.
    for (int i = 0; i < 100; ++i) {
        if (i % 10 == 0) clock.ingestMidiRealtime(0xf8);
        r = clock.tick(c);
    }
    CHECK(near(r.periodSeconds, 0.5f)); // MIDI ticks alone do not start its transport.
    clock.ingestMidiRealtime(0xfa);
    clock.tick(c);
    for (int i = 0; i < 100; ++i) {
        if (i % 10 == 0) clock.ingestMidiRealtime(0xf8);
        r = clock.tick(c);
    }
    CHECK(r.running && r.ready && near(r.periodSeconds, 0.24f));
    clock.tick(c, true);
    for (int i = 1; i <= 100; ++i) {
        if (i % 10 == 0) clock.ingestMidiRealtime(0xf8);
        r = clock.tick(c, i == 100);
    }
    CHECK(r.ready && near(r.periodSeconds, 0.1f)); // Recent CV takes priority.
    for (int i = 0; i < 2200; ++i) {
        if (i % 10 == 0) clock.ingestMidiRealtime(0xf8);
        r = clock.tick(c);
    }
    CHECK(near(r.periodSeconds, 0.24f)); // Missing CV falls back to live MIDI.
    clock.ingestMidiRealtime(0xfc);
    r = clock.tick(c);
    CHECK(near(r.periodSeconds, 0.5f)); // Stopped MIDI is not an Auto source.
    clock.ingestMidiRealtime(0xfa);
    clock.tick(c);
    for (int i = 0; i < 100; ++i) {
        if (i % 10 == 0) clock.ingestMidiRealtime(0xf8);
        r = clock.tick(c);
    }
    CHECK(near(r.periodSeconds, 0.24f));
    for (int i = 0; i < 2200; ++i) r = clock.tick(c);
    CHECK(near(r.periodSeconds, 0.5f)); // A running transport also needs recent ticks.
    clock.reset();
    c.source = ReferenceConfig::Cv;
    c.cvPpqn = 4;
    clock.tick(c, true);
    for (int i = 1; i < 125; ++i) clock.tick(c);
    r = clock.tick(c, true);
    CHECK(r.ready && near(r.periodSeconds, 0.5f));
}

void testMusicalCvPpqnAndMidiConversion() {
    // Every supported PPQN describes the same three quarters and 72 MIDI clocks.
    // This covers both interpolation below 24 PPQN and division at 48 PPQN.
    for (const uint8_t ppqn : {uint8_t{1}, uint8_t{2}, uint8_t{4}, uint8_t{8},
                              uint8_t{12}, uint8_t{24}, uint8_t{48}}) {
        MusicalClock clock(48000.0f);
        ReferenceConfig c;
        c.source = ReferenceConfig::Cv;
        c.cvPpqn = ppqn;
        constexpr int quarterSamples = 4800;
        const int edgeSamples = quarterSamples / ppqn;
        int beats = 0, clocks = 0;
        for (int i = 0; i <= quarterSamples * 3; ++i) {
            const ReferenceSample r = clock.tick(c, i % edgeSamples == 0);
            if (r.beat) {
                CHECK(i % quarterSamples == 0);
                ++beats;
            }
            clocks += r.midiClock;
            if (i >= edgeSamples) CHECK(r.ready && near(r.periodSeconds, 0.1f));
        }
        CHECK(beats == 4);
        // The first interval is needed to measure the input tempo.
        const int expected = 72 - 24 / ppqn + 1;
        CHECK(std::abs(clocks - expected) <= 1);
    }
}

void testMusicalTransportOnlyAffectsTheSelectedSource() {
    // A MIDI keyboard or DAW can send Start while CV or Internal is selected.
    // Its transport must not move the selected clock or invalidate its timings.
    for (const ReferenceConfig::Source source : {ReferenceConfig::Internal,
                                                ReferenceConfig::Cv, ReferenceConfig::Auto}) {
        MusicalClock subject(1000.0f), witness(1000.0f);
        ReferenceConfig c;
        c.source = source;
        c.cvPpqn = 4;
        const bool useCv = source != ReferenceConfig::Internal;
        for (int i = 0; i < 450; ++i) {
            const bool edge = useCv && i % 125 == 0;
            subject.tick(c, edge);
            witness.tick(c, edge);
        }
        subject.ingestMidiRealtime(0xfa);
        for (int i = 450; i < 1100; ++i) {
            const bool edge = useCv && i % 125 == 0;
            const ReferenceSample a = subject.tick(c, edge);
            const ReferenceSample b = witness.tick(c, edge);
            CHECK(!a.start && !a.stop);
            CHECK(a.epoch == b.epoch);
            CHECK(a.beat == b.beat && a.midiClock == b.midiClock);
            CHECK(a.ready == b.ready && a.running == b.running);
            CHECK(near(a.fraction, b.fraction));
            CHECK(near(a.periodSeconds, b.periodSeconds));
        }
    }

    // In Auto, Start without an active CV source selects MIDI immediately.
    // It must wait for F8 rather than run the internal reference between ticks.
    MusicalClock clock(1000.0f);
    ReferenceConfig c;
    c.source = ReferenceConfig::Auto;
    for (int i = 0; i < 125; ++i) clock.tick(c);
    clock.ingestMidiRealtime(0xfa);
    ReferenceSample r = clock.tick(c);
    CHECK(r.start && !r.ready && !r.beat && !r.midiClock);
    const uint32_t waitingEpoch = r.epoch;
    for (int i = 0; i < 200; ++i) {
        r = clock.tick(c);
        CHECK(!r.start && !r.ready && !r.beat && !r.midiClock);
        CHECK(r.epoch == waitingEpoch && r.fraction == 0.0f);
    }
    clock.ingestMidiRealtime(0xf8);
    r = clock.tick(c);
    CHECK(r.midiClock && !r.ready && r.epoch == waitingEpoch);
    for (int i = 1; i < 20; ++i) clock.tick(c);
    clock.ingestMidiRealtime(0xf8);
    r = clock.tick(c);
    CHECK(r.midiClock && r.ready && near(r.periodSeconds, 0.48f));
    CHECK(r.epoch == waitingEpoch);
}

void testMusicalSampleRateChangePreservesQueuedMidi() {
    MusicalClock clock(1000.0f);
    ReferenceConfig c;
    c.source = ReferenceConfig::Midi;
    clock.ingestMidiRealtime(0xfa);
    for (int i = 0; i < 3; ++i) clock.ingestMidiRealtime(0xf8);
    // The host sets its sample rate on the first audio block, after callbacks
    // may already have delivered Start and clock ticks.
    clock.setSampleRate(48000.0f);
    ReferenceSample r = clock.tick(c);
    CHECK(r.start && r.midiClock);
    int clocks = r.midiClock;
    for (int i = 0; i < 4; ++i) clocks += clock.tick(c).midiClock;
    CHECK(clocks == 3);

    clock.ingestMidiRealtime(0xfc);
    clock.setSampleRate(96000.0f);
    r = clock.tick(c);
    CHECK(r.stop && !r.running && !r.midiClock);
    clock.ingestMidiRealtime(0xfb);
    clock.ingestMidiRealtime(0xf8);
    clock.setSampleRate(48000.0f);
    r = clock.tick(c);
    CHECK(r.running && r.midiClock && !r.start && !r.stop);

    clock.ingestMidiRealtime(0xfa);
    clock.ingestMidiRealtime(0xf8);
    clock.ingestMidiRealtime(0xff);
    clock.setSampleRate(96000.0f);
    r = clock.tick(c);
    CHECK(r.reset && !r.start && !r.midiClock);
    CHECK(!clock.tick(c).midiClock); // Reset also discards the queued clock.
}

void testExternalTimeoutPreservesEnvelopeTiming() {
    // The last measured period still defines an envelope's width and glide
    // after input clocks disappear; freshness must stop scheduling separately.
    for (const ReferenceConfig::Source source : {ReferenceConfig::Cv, ReferenceConfig::Midi}) {
        MusicalClock clock(1000.0f);
        ReferenceConfig c;
        c.source = source;
        ReferenceSample r;
        const bool midi = source == ReferenceConfig::Midi;
        if (midi) {
            clock.ingestMidiRealtime(0xfa);
            clock.tick(c);
            clock.ingestMidiRealtime(0xf8);
            clock.tick(c);
            for (int i = 1; i < 20; ++i) clock.tick(c);
            clock.ingestMidiRealtime(0xf8);
            r = clock.tick(c);
        } else {
            clock.tick(c, true);
            for (int i = 1; i < 400; ++i) clock.tick(c);
            r = clock.tick(c, true);
        }
        const float measured = midi ? 0.48f : 0.4f;
        CHECK(r.ready && near(r.periodSeconds, measured));
        c.bpm = 100.0f;
        for (int i = 0; i < 2200; ++i) {
            r = clock.tick(c);
            CHECK(near(r.periodSeconds, measured));
        }
        CHECK(!r.ready && !r.running && !r.beat);
        if (midi) {
            clock.ingestMidiRealtime(0xfa);
            r = clock.tick(c);
            CHECK(r.start && !r.ready && near(r.periodSeconds, 0.6f));
        }
        r = clock.tick(c, false, true);
        CHECK(r.reset && near(r.periodSeconds, 0.6f));
        r = clock.tick(c);
        CHECK(!r.ready && near(r.periodSeconds, 0.6f));
    }
}

} // namespace

int main() {
    testBurstCountAndFinalTail();
    testSlopeAndLatching();
    testResetStopAndRetrigger();
    testRateChangesAndOff();
    testWaitingCvDividerAndManualStep();
    testMidiRealtimeAndTempo();
    testCvMidiClockTimeout();
    testCvTwelvePpqn();
    testTempoClockPrecision();
    testShapesAndLiveControls();
    testSampleRatesAndInvalidInput();
    testLoopWaitsForTheFinalTailAndCanBeStopped();
    testMuteAndAuxiliaryOutputs();
    testVelocityTrimAndGlide();
    testReferenceEnvelopeCurvesAndClockPhase();
    testContinuousModulationScale();
    testShapeMorphMatchesReferenceCurves();
    testShapeMorphLivePositionAndInvalidInput();
    testMusicalGridAndSharedTiming();
    testMusicalReferenceSources();
    testMusicalCvPpqnAndMidiConversion();
    testMusicalTransportOnlyAffectsTheSelectedSource();
    testMusicalSampleRateChangePreservesQueuedMidi();
    testExternalTimeoutPreservesEnvelopeTiming();
    if (failures != 0) {
        std::fprintf(stderr, "%d engine test failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("All multi-burst engine tests passed.");
    return EXIT_SUCCESS;
}
