#pragma once

#include <cstdint>

namespace multi_burst {

enum class Shape : uint8_t {
    Sine, LogDown, LogUp, ExpoDown, RampUp, Triangle, RampDown, Pulse
};

struct Config {
    enum ClockMode : uint8_t { InternalRate, InternalTempo, Cv, Midi, InternalAndCv };
    enum Retrigger : uint8_t { Restart, Ignore };

    uint8_t bursts = 8;
    Shape shape = Shape::Sine;
    float widthSeconds = 0.1f;
    float slope = 0.0f;
    float level = 8.0f;
    float rateHz = 4.0f;
    float bpm = 120.0f;
    uint16_t midiTicksPerBurst = 6;
    uint8_t cvPpqn = 1;
    uint8_t cvDivider = 1;
    ClockMode clockMode = InternalRate;
    Retrigger retrigger = Restart;
    float pulseSeconds = 0.005f;
    bool waitForClock = false;
    bool loop = false;
    bool mute = false;
    bool velocityEnabled = true;
    bool preserveClockPhase = false;
    bool referenceCurves = false;
    float shapePosition = -1.0f; // Negative selects shape; 0..7 morphs adjacent shapes.
    float glideRiseSeconds = 0.0f;
    float glideFallSeconds = 0.0f;
    float levelTrim = 1.0f;
};

struct Sample {
    float envelope = 0.0f;
    float gate = 0.0f;
    float active = 0.0f;
    float eoc = 0.0f;
    float clock = 0.0f;
    float levels = 0.0f;   // 0..10 V slope gain, independent of level/velocity/mute.
    float progress = 0.0f; // 0..10 V; retained after natural completion.
    float burst = 0.0f;    // 1 ms event pulse, independent of mute.
    bool clockTick = false;
    bool burstStarted = false;
    bool burstEnded = false;
    bool sequenceEnded = false;
    bool midiClock = false;
    float burstLevel = 0.0f; // Volts, including latched slope and trigger velocity.
    uint8_t index = 0;      // Zero-based index of the current burst.
};

// Fixed-storage, sample-driven engine. Call all methods on the audio thread.
// Trigger count/slope/velocity are latched; width/shape/level are live in tick().
class Engine {
public:
    explicit Engine(float sampleRate = 48000.0f);
    void setSampleRate(float sampleRate);
    void trigger(const Config& config, float velocity = 1.0f);
    void reset();
    void stop();
    void ingestMidiRealtime(uint8_t message);
    Sample tick(const Config& config, bool triggerEdge = false,
                bool resetEdge = false, bool cvClockEdge = false,
                bool manualClockEdge = false, bool stopEdge = false);

private:
    void startBurst();
    void updateTiming(const Config& config);
    void observeCvClock(const Config& config);
    float currentBurstGain() const;
    uint32_t pulseSamples(const Config& config) const;

    float sampleRate_ = 48000.0f;
    uint64_t widthIncrement_ = 0;
    float cachedWidth_ = -1.0f;
    float cachedRate_ = -1.0f;
    float cachedBpm_ = -1.0f;
    float cachedGlideRise_ = -1.0f, cachedGlideFall_ = -1.0f;
    float glideRiseStep_ = 1.0f, glideFallStep_ = 1.0f;
    Config::ClockMode cachedMode_ = Config::InternalRate;
    uint64_t rateIncrement_ = 0;
    uint64_t midiIncrement_ = 0;
    uint64_t ratePhase_ = 0;
    uint64_t midiPhase_ = 0;
    uint16_t tempoTicks_ = 0;
    uint16_t incomingMidiTicks_ = 0;
    uint16_t midiBurstTicks_ = 0;
    uint8_t cvBurstTicks_ = 0;

    uint64_t sampleCounter_ = 0;
    uint64_t previousCvSample_ = 0;
    uint32_t cvPeriodSamples_ = 0;
    uint64_t cvMidiIncrement_ = 0;
    uint64_t cvMidiPhase_ = 0;
    uint8_t cachedCvPpqn_ = 0;
    bool previousCvValid_ = false;
    bool cvPeriodValid_ = false;

    uint64_t envelopePhase_ = 0;
    float slope_ = 0.0f;
    float velocity_ = 1.0f;
    float burstGain_ = 1.0f;
    float glideGain_ = 1.0f;
    float progress_ = 0.0f;
    uint8_t totalBursts_ = 8;
    uint8_t startedBursts_ = 0;
    uint8_t index_ = 0;
    bool sequenceActive_ = false;
    bool envelopeActive_ = false;
    bool burstStartedPending_ = false;
    bool burstEndedPending_ = false;
    bool loopPending_ = false;
    bool midiRunning_ = true;
    bool midiStartPending_ = false;
    bool midiContinuePending_ = false;
    bool midiStopPending_ = false;
    bool midiResetPending_ = false;
    uint32_t eocRemaining_ = 0;
    uint32_t clockRemaining_ = 0;
    uint32_t burstRemaining_ = 0;
};

// The musical grid is measured in quarter notes. Interval 0 is Off; interval
// 1..16 uses this grid. Width uses 0..15; glide uses 0=Off and 1..16.
float musicalRatio(uint8_t index);
float glideRatio(uint8_t index);
// Continuous time/frequency modulation, clamped to +/-4 octaves; no libm call.
float modulationScale(float octaves);

struct ReferenceConfig {
    enum Source : uint8_t { Internal, Cv, Midi, Auto };
    Source source = Internal;
    float bpm = 120.0f;
    uint8_t cvPpqn = 1;
};

struct ReferenceSample {
    bool beat = false, midiClock = false;
    bool start = false, stop = false, reset = false;
    bool running = true, ready = true;
    float periodSeconds = 0.5f, fraction = 0.0f;
    uint32_t epoch = 0;
};

// One clock per algorithm, shared by both generators. External interpolation is
// capped at the next expected edge, so a missing clock cannot run bursts forever.
class MusicalClock {
public:
    explicit MusicalClock(float sampleRate = 48000.0f);
    void setSampleRate(float sampleRate);
    void reset();
    void ingestMidiRealtime(uint8_t message);
    ReferenceSample tick(const ReferenceConfig& config, bool cvClockEdge = false,
                         bool resetEdge = false);

private:
    float sampleRate_ = 48000.0f;
    float cachedBpm_ = -1.0f;
    float lastCvPeriodSeconds_ = 0.0f, lastMidiPeriodSeconds_ = 0.0f;
    uint64_t internalPhase_ = 0, internalIncrement_ = 0;
    uint64_t midiPhase_ = 0, midiIncrement_ = 0;
    uint64_t cvMidiPhase_ = 0, cvMidiIncrement_ = 0;
    uint64_t samples_ = 0, cvLast_ = 0, midiLast_ = 0;
    uint32_t cvPeriod_ = 0, midiPeriod_ = 0;
    uint16_t midiQueued_ = 0;
    uint8_t cvTicks_ = 0, midiTicks_ = 0;
    uint8_t cachedPpqn_ = 0;
    bool cvSeen_ = false, midiSeen_ = false, midiRunning_ = false;
    bool startPending_ = false, stopPending_ = false;
    bool continuePending_ = false, resetPending_ = false;
    ReferenceConfig::Source selected_ = ReferenceConfig::Internal;
    uint32_t epoch_ = 0;
};

class MusicalTiming {
public:
    bool tick(const ReferenceSample& reference, uint8_t interval);
    void reset();
private:
    uint32_t epoch_ = 0;
    uint8_t interval_ = 0, counter_ = 0;
    int bucket_ = 0;
    bool initialised_ = false;
};

} // namespace multi_burst
