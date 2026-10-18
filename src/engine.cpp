#include "multi_burst/engine.h"

namespace multi_burst {
namespace {

// 48 fractional bits keep long envelopes and slow clocks accurate to a sample.
// The Cortex-M7 handles the per-sample add/compare as pairs of integer operations;
// conversions and divisions happen only when a parameter or measured period changes.
constexpr uint64_t kPhaseScale = uint64_t{1} << 48;
constexpr float kPhaseToFloat = 1.0f / 281474976710656.0f;

float bounded(float value, float low, float high, float fallback) {
    if (value != value) return fallback;
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

uint64_t incrementFor(double cyclesPerSample) {
    if (cyclesPerSample <= 0.0) return 0;
    const double scaled = cyclesPerSample * static_cast<double>(kPhaseScale);
    if (scaled < 1.0) return 1;
    if (scaled >= static_cast<double>(kPhaseScale)) return kPhaseScale;
    const double rounded = scaled + 0.5;
    const uint32_t high = static_cast<uint32_t>(rounded / 4294967296.0);
    const uint32_t low = static_cast<uint32_t>(rounded - static_cast<double>(high) * 4294967296.0);
    return (static_cast<uint64_t>(high) << 32) | low;
}

bool advance(uint64_t& phase, uint64_t increment) {
    phase += increment;
    if (phase < kPhaseScale) return false;
    phase -= kPhaseScale;
    return true;
}

uint16_t midiDivision(uint16_t value) {
    switch (value) {
        case 3: case 6: case 12: case 24: case 48: case 96: case 192:
            return value;
        default: return 6;
    }
}

uint8_t cvPpqn(uint8_t value) {
    if (value == 0) return 1;
    return value > 48 ? 48 : value;
}

bool cvEdgeIsMidiTick(uint8_t ppqn) {
    return ppqn <= 24 && 24 % ppqn == 0;
}

Config::ClockMode clockMode(Config::ClockMode value) {
    return value <= Config::InternalAndCv ? value : Config::InternalRate;
}

float referenceLog(float value) {
    // log10(value), value in [1,10]. Range reduction keeps |z| <= 1/3;
    // the fixed odd series has absolute error < 0.000001 on this interval.
    float exponent = 0.0f;
    if (value >= 8.0f) { value *= 0.125f; exponent = 3.0f; }
    else if (value >= 4.0f) { value *= 0.25f; exponent = 2.0f; }
    else if (value >= 2.0f) { value *= 0.5f; exponent = 1.0f; }
    const float z = (value - 1.0f) / (value + 1.0f);
    const float z2 = z * z;
    const float ln = 2.0f * z * (1.0f + z2 * (1.0f / 3.0f + z2 *
        (1.0f / 5.0f + z2 * (1.0f / 7.0f + z2 * (1.0f / 9.0f)))));
    return (ln + exponent * 0.6931471805599453f) * 0.4342944819032518f;
}

float shapeValue(Shape shape, float phase, bool referenceCurves) {
    switch (shape) {
        case Shape::Sine: {
            // Seventh-order sine approximation on [0, pi/2], mirrored about 0.5.
            // Max absolute error < 0.00016; no transcendental call or lookup memory.
            const float x = (phase < 0.5f ? phase : 1.0f - phase) * 3.14159265358979323846f;
            const float x2 = x * x;
            return x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f - x2 / 5040.0f)));
        }
        case Shape::LogDown: return referenceCurves ? referenceLog(10.0f - 9.0f * phase) : 1.0f - phase * phase * phase;
        case Shape::LogUp: {
            if (referenceCurves) return referenceLog(1.0f + 9.0f * phase);
            const float reverse = 1.0f - phase;
            return 1.0f - reverse * reverse * reverse;
        }
        case Shape::ExpoDown: {
            const float reverse = 1.0f - phase;
            return reverse * reverse * reverse;
        }
        case Shape::RampUp: return phase;
        case Shape::Triangle: return phase < 0.5f ? 2.0f * phase : 2.0f * (1.0f - phase);
        case Shape::RampDown: return 1.0f - phase;
        case Shape::Pulse: return 1.0f;
        default: return 0.0f;
    }
}

float envelopeValue(const Config& config, float phase) {
    if (config.shapePosition < 0.0f || config.shapePosition != config.shapePosition)
        return shapeValue(config.shape, phase, config.referenceCurves);
    const float position = bounded(config.shapePosition, 0.0f, 7.0f, 0.0f);
    const auto lower = static_cast<uint8_t>(position);
    constexpr Shape morphShapes[] = {Shape::Sine, Shape::LogDown, Shape::LogUp, Shape::ExpoDown,
        Shape::RampUp, Shape::Triangle, Shape::RampDown, Shape::Pulse};
    const float first = shapeValue(morphShapes[lower], phase, config.referenceCurves);
    if (lower >= 7) return first;
    const float second = shapeValue(morphShapes[lower + 1], phase, config.referenceCurves);
    return first + (second - first) * (position - static_cast<float>(lower));
}

} // namespace

Engine::Engine(float sampleRate) {
    setSampleRate(sampleRate);
}

void Engine::setSampleRate(float sampleRate) {
    sampleRate_ = bounded(sampleRate, 1000.0f, 384000.0f, 48000.0f);
    cachedWidth_ = cachedRate_ = cachedBpm_ = -1.0f;
    cachedGlideRise_ = cachedGlideFall_ = -1.0f;
    previousCvValid_ = false;
    cvPeriodValid_ = false;
    cachedCvPpqn_ = 0;
}

void Engine::reset() {
    stop();
    ratePhase_ = midiPhase_ = cvMidiPhase_ = 0;
    tempoTicks_ = midiBurstTicks_ = 0;
    cvBurstTicks_ = 0;
    sampleCounter_ = previousCvSample_ = 0;
    cvPeriodSamples_ = 0;
    previousCvValid_ = cvPeriodValid_ = false;
    cachedCvPpqn_ = 0;
    midiRunning_ = true;
}

void Engine::stop() {
    sequenceActive_ = envelopeActive_ = false;
    burstStartedPending_ = burstEndedPending_ = false;
    startedBursts_ = index_ = 0;
    loopPending_ = false;
    progress_ = 0.0f;
    envelopePhase_ = 0;
    eocRemaining_ = clockRemaining_ = burstRemaining_ = 0;
    incomingMidiTicks_ = 0;
    midiStartPending_ = midiContinuePending_ = midiStopPending_ = midiResetPending_ = false;
}

void Engine::trigger(const Config& config, float velocity) {
    if (config.retrigger == Config::Ignore && sequenceActive_) return;
    if (envelopeActive_) burstEndedPending_ = true;
    totalBursts_ = config.bursts == 0 ? 1 : (config.bursts > 40 ? 40 : config.bursts);
    slope_ = bounded(config.slope, -1.0f, 1.0f, 0.0f);
    velocity_ = config.velocityEnabled ? bounded(velocity, 0.0f, 1.0f, 1.0f) : 1.0f;
    startedBursts_ = index_ = 0;
    sequenceActive_ = true;
    loopPending_ = false;
    progress_ = 0.0f;
    envelopeActive_ = false;
    burstStartedPending_ = false;
    envelopePhase_ = 0;
    eocRemaining_ = 0;
    if (!config.preserveClockPhase) {
        ratePhase_ = 0;
        tempoTicks_ = midiBurstTicks_ = 0;
        cvBurstTicks_ = 0;
    }
    if (!config.waitForClock) startBurst();
}

void Engine::ingestMidiRealtime(uint8_t message) {
    switch (message) {
        case 0xf8:
            if (incomingMidiTicks_ != 0xffff) ++incomingMidiTicks_;
            break;
        case 0xfa:
            midiStopPending_ = midiContinuePending_ = false;
            incomingMidiTicks_ = 0; // Clocks preceding Start belong to the old transport.
            midiStartPending_ = true;
            break;
        case 0xfb:
            midiStopPending_ = false;
            midiContinuePending_ = true;
            break;
        case 0xfc:
            midiStartPending_ = midiContinuePending_ = false;
            incomingMidiTicks_ = 0;
            midiStopPending_ = true;
            break;
        case 0xff: midiResetPending_ = true; break;
        default: break;
    }
}

void Engine::startBurst() {
    if (startedBursts_ >= totalBursts_) return;
    if (envelopeActive_) burstEndedPending_ = true;
    index_ = startedBursts_++;
    envelopePhase_ = 0;
    envelopeActive_ = true;
    burstStartedPending_ = true;
    if (totalBursts_ == 1) {
        burstGain_ = 1.0f;
    } else {
        const float position = static_cast<float>(index_) / static_cast<float>(totalBursts_ - 1);
        const float amount = slope_ < 0.0f ? -slope_ : slope_;
        const float extreme = slope_ >= 0.0f
            ? 0.04f + 0.96f * position : 1.0f - 0.96f * position;
        burstGain_ = 1.0f + amount * (extreme - 1.0f);
    }
    if (index_ == 0) glideGain_ = burstGain_;
    progress_ = totalBursts_ > 1 ? 10.0f * static_cast<float>(index_) / static_cast<float>(totalBursts_ - 1) : 0.0f;
    burstRemaining_ = static_cast<uint32_t>(sampleRate_ * 0.001f + 0.5f);
    if (burstRemaining_ == 0) burstRemaining_ = 1;
}

float Engine::currentBurstGain() const {
    return glideGain_ * velocity_;
}

uint32_t Engine::pulseSamples(const Config& config) const {
    const float duration = bounded(config.pulseSeconds, 0.0001f, 1.0f, 0.005f);
    const uint32_t samples = static_cast<uint32_t>(duration * sampleRate_ + 0.5f);
    return samples == 0 ? 1 : samples;
}

void Engine::updateTiming(const Config& config) {
    const float width = bounded(config.widthSeconds, 0.0001f, 960.0f, 0.1f);
    if (width != cachedWidth_) {
        widthIncrement_ = incrementFor(1.0 / (static_cast<double>(width) * sampleRate_));
        cachedWidth_ = width;
    }
    const float rise = bounded(config.glideRiseSeconds, 0.0f, 960.0f, 0.0f);
    const float fall = bounded(config.glideFallSeconds, 0.0f, 960.0f, 0.0f);
    if (rise != cachedGlideRise_) {
        glideRiseStep_ = rise <= 0.0f ? 1.0f : 1.0f / (rise * sampleRate_);
        cachedGlideRise_ = rise;
    }
    if (fall != cachedGlideFall_) {
        glideFallStep_ = fall <= 0.0f ? 1.0f : 1.0f / (fall * sampleRate_);
        cachedGlideFall_ = fall;
    }
    const float maxRate = sampleRate_ / 48.0f < 100.0f ? sampleRate_ / 48.0f : 100.0f;
    const float rate = config.rateHz <= 0.0f ? 0.0f : bounded(config.rateHz, 0.01f, maxRate, 4.0f);
    const float bpm = bounded(config.bpm, 1.0f, 600.0f, 120.0f);
    const Config::ClockMode mode = clockMode(config.clockMode);
    if (rate != cachedRate_ || bpm != cachedBpm_ || mode != cachedMode_) {
        rateIncrement_ = incrementFor(static_cast<double>(rate) / sampleRate_);
        const double quarterHz = mode == Config::InternalTempo
            ? static_cast<double>(bpm) / 60.0 : static_cast<double>(rate);
        midiIncrement_ = incrementFor(quarterHz * 24.0 / sampleRate_);
        cachedRate_ = rate;
        cachedBpm_ = bpm;
        cachedMode_ = mode;
    }
    const uint8_t ppqn = cvPpqn(config.cvPpqn);
    if (cvPeriodValid_ && ppqn != cachedCvPpqn_) {
        cvMidiIncrement_ = incrementFor(24.0 / (static_cast<double>(cvPeriodSamples_) * ppqn));
        cachedCvPpqn_ = ppqn;
    }
}

void Engine::observeCvClock(const Config& config) {
    if (previousCvValid_) {
        const uint64_t elapsed = sampleCounter_ - previousCvSample_;
        if (elapsed != 0) {
            cvPeriodSamples_ = elapsed > 0xffffffffULL ? 0xffffffffU : static_cast<uint32_t>(elapsed);
            cvMidiIncrement_ = incrementFor(24.0 / (static_cast<double>(cvPeriodSamples_) * cvPpqn(config.cvPpqn)));
            const bool acquired = !cvPeriodValid_;
            cvPeriodValid_ = true;
            cachedCvPpqn_ = cvPpqn(config.cvPpqn);
            // Below 24 PPQN each CV edge is also a MIDI tick. At 48 PPQN
            // retain the oscillator phase so consecutive edges produce half as many ticks.
            if (acquired || cvEdgeIsMidiTick(cachedCvPpqn_)) cvMidiPhase_ = 0;
        }
    }
    previousCvSample_ = sampleCounter_;
    previousCvValid_ = true;
}

Sample Engine::tick(const Config& config, bool triggerEdge, bool resetEdge, bool cvClockEdge,
                    bool manualClockEdge, bool stopEdge) {
    Sample output;
    // A reset wins over all events in this sample, including queued transport/clock.
    if (resetEdge || midiResetPending_) {
        reset();
        ++sampleCounter_;
        return output;
    }
    if (stopEdge || midiStopPending_) {
        const bool midiStop = midiStopPending_;
        stop();
        if (midiStop) midiRunning_ = false;
        ++sampleCounter_;
        return output;
    }
    if (loopPending_) {
        loopPending_ = false;
        if (config.loop && !triggerEdge) {
            const uint32_t eoc = eocRemaining_;
            trigger(config, velocity_);
            eocRemaining_ = eoc; // Looping must retain the full EOC pulse.
        }
    }
    const Config::ClockMode mode = clockMode(config.clockMode);
    const bool start = midiStartPending_ && mode == Config::Midi;
    const bool resume = midiContinuePending_ && mode == Config::Midi;
    midiStartPending_ = midiContinuePending_ = false;
    if (start) {
        const uint16_t clocks = incomingMidiTicks_;
        reset();
        incomingMidiTicks_ = clocks;
        trigger(config);
    } else if (resume) {
        midiRunning_ = true;
    }
    if (triggerEdge && !start) trigger(config);
    updateTiming(config);

    bool ended = burstEndedPending_;
    bool started = burstStartedPending_;
    burstEndedPending_ = burstStartedPending_ = false;
    if (envelopeActive_ && !started && advance(envelopePhase_, widthIncrement_)) {
        envelopeActive_ = false;
        ended = true;
        if (startedBursts_ >= totalBursts_) {
            sequenceActive_ = false;
            output.sequenceEnded = true;
            eocRemaining_ = pulseSamples(config);
            loopPending_ = config.loop;
        }
    }

    bool receivedMidiClock = false;
    if (incomingMidiTicks_ != 0) {
        --incomingMidiTicks_;
        receivedMidiClock = true;
    }
    bool sourceClock = false;
    const bool acquiredCvPeriod = !cvPeriodValid_;
    if (cvClockEdge) observeCvClock(config);

    switch (mode) {
        case Config::InternalRate:
        case Config::InternalAndCv:
            sourceClock = (config.preserveClockPhase || !started) && advance(ratePhase_, rateIncrement_);
            output.midiClock = advance(midiPhase_, midiIncrement_);
            break;
        case Config::InternalTempo:
            output.midiClock = advance(midiPhase_, midiIncrement_);
            if (output.midiClock && ++tempoTicks_ >= midiDivision(config.midiTicksPerBurst)) {
                tempoTicks_ = 0;
                sourceClock = true;
            }
            break;
        case Config::Cv: {
            if (cvPeriodValid_) {
                const uint64_t elapsed = sampleCounter_ - previousCvSample_;
                const uint64_t minimumTimeout = static_cast<uint32_t>(sampleRate_ * 2.0f);
                const uint64_t periodTimeout = static_cast<uint64_t>(cvPeriodSamples_) * 3;
                const uint64_t timeout = periodTimeout > minimumTimeout ? periodTimeout : minimumTimeout;
                if (elapsed >= timeout) {
                    cvPeriodValid_ = false;
                    previousCvValid_ = false;
                } else {
                    output.midiClock = cvEdgeIsMidiTick(cvPpqn(config.cvPpqn))
                        ? cvClockEdge || advance(cvMidiPhase_, cvMidiIncrement_)
                        : (cvClockEdge && acquiredCvPeriod) || advance(cvMidiPhase_, cvMidiIncrement_);
                }
            }
            break;
        }
        case Config::Midi:
            output.midiClock = receivedMidiClock;
            if (receivedMidiClock && midiRunning_ && ++midiBurstTicks_ >= midiDivision(config.midiTicksPerBurst)) {
                midiBurstTicks_ = 0;
                sourceClock = true;
            }
            break;
    }

    if (cvClockEdge && (mode == Config::Cv || mode == Config::InternalAndCv)) {
        const uint8_t divider = config.cvDivider == 0 ? 1 : config.cvDivider;
        if (++cvBurstTicks_ >= divider) {
            cvBurstTicks_ = 0;
            sourceClock = true;
        }
    }
    sourceClock = sourceClock || manualClockEdge;
    if (sourceClock) {
        clockRemaining_ = pulseSamples(config);
        if (sequenceActive_ && !started && startedBursts_ < totalBursts_) {
            startBurst();
            started = burstStartedPending_;
            ended = ended || burstEndedPending_;
            burstStartedPending_ = burstEndedPending_ = false;
        }
    }

    if (glideGain_ < burstGain_) {
        glideGain_ += glideRiseStep_;
        if (glideGain_ > burstGain_) glideGain_ = burstGain_;
    } else if (glideGain_ > burstGain_) {
        glideGain_ -= glideFallStep_;
        if (glideGain_ < burstGain_) glideGain_ = burstGain_;
    }

    const float level = bounded(config.level, 0.0f, 10.0f, 8.0f);
    const float trim = bounded(config.levelTrim, 0.0f, 1.0f, 1.0f);
    output.index = index_;
    output.burstLevel = level * trim * currentBurstGain();
    output.levels = sequenceActive_ ? 10.0f * glideGain_ : 0.0f;
    output.progress = progress_;
    output.clockTick = sourceClock;
    output.burstStarted = started;
    output.burstEnded = ended;
    if (envelopeActive_ && !config.mute) {
        const float phase = static_cast<float>(static_cast<uint32_t>(envelopePhase_ >> 32)) * (1.0f / 65536.0f)
            + static_cast<float>(static_cast<uint32_t>(envelopePhase_)) * kPhaseToFloat;
        output.envelope = output.burstLevel * envelopeValue(config, phase);
        output.gate = 5.0f;
    }
    output.active = sequenceActive_ ? 5.0f : 0.0f;
    if (eocRemaining_ != 0) {
        output.eoc = 5.0f;
        --eocRemaining_;
    }
    if (clockRemaining_ != 0) {
        output.clock = 5.0f;
        --clockRemaining_;
    }
    if (burstRemaining_ != 0) {
        output.burst = 5.0f;
        --burstRemaining_;
    }
    ++sampleCounter_;
    return output;
}

namespace {
constexpr uint8_t ratioNumerators[] = {1, 1, 1, 1, 1, 1, 1, 2, 1, 3, 2, 3, 4, 6, 8, 16};
constexpr uint8_t ratioDenominators[] = {16, 12, 8, 6, 4, 3, 2, 3, 1, 2, 1, 1, 1, 1, 1, 1};

float phaseFloat(uint64_t phase) {
    const float value = static_cast<float>(static_cast<uint32_t>(phase >> 32)) * (1.0f / 65536.0f)
        + static_cast<float>(static_cast<uint32_t>(phase)) * kPhaseToFloat;
    return value < 1.0f ? value : 0.99999994f;
}

bool freshClock(uint64_t now, uint64_t last, uint32_t quarterPeriod,
                uint8_t ppqn, float sampleRate, bool seen) {
    if (!seen) return false;
    const uint64_t minimum = static_cast<uint32_t>(sampleRate * 2.0f);
    const uint64_t measured = static_cast<uint64_t>(quarterPeriod / ppqn) * 3;
    return now - last < (measured > minimum ? measured : minimum);
}

uint32_t quarterPeriod(uint64_t elapsed, uint8_t ppqn) {
    if (elapsed > 0xffffffffU / ppqn) return 0xffffffffU;
    return static_cast<uint32_t>(elapsed) * ppqn;
}

float elapsedFloat(uint64_t now, uint64_t last) {
    const uint64_t elapsed = now - last;
    return static_cast<float>(elapsed > 0xffffffffULL ? 0xffffffffU : static_cast<uint32_t>(elapsed));
}
}

float musicalRatio(uint8_t index) {
    if (index > 15) index = 15;
    return static_cast<float>(ratioNumerators[index]) / static_cast<float>(ratioDenominators[index]);
}

float glideRatio(uint8_t index) {
    return index == 0 ? 0.0f : musicalRatio(static_cast<uint8_t>(index - 1));
}

float modulationScale(float octaves) {
    const float amount = bounded(octaves, -4.0f, 4.0f, 0.0f);
    const int whole = static_cast<int>(amount >= 0.0f ? amount + 0.5f : amount - 0.5f);
    constexpr float powers[] = {0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
    const float x = (amount - static_cast<float>(whole)) * 0.6931471805599453f;
    // exp(x) on [-ln(2)/2,ln(2)/2], relative error below 0.000001.
    const float fractional = 1.0f + x * (1.0f + x * (0.5f + x * (1.0f / 6.0f + x *
        (1.0f / 24.0f + x * (1.0f / 120.0f + x / 720.0f)))));
    return powers[whole + 4] * fractional;
}

MusicalClock::MusicalClock(float sampleRate) { setSampleRate(sampleRate); }

void MusicalClock::setSampleRate(float sampleRate) {
    const uint16_t queued = midiQueued_;
    const bool start = startPending_, stop = stopPending_;
    const bool resume = continuePending_, systemReset = resetPending_;
    sampleRate_ = bounded(sampleRate, 1000.0f, 384000.0f, 48000.0f);
    cachedBpm_ = -1.0f;
    reset();
    midiQueued_ = queued;
    startPending_ = start;
    stopPending_ = stop;
    continuePending_ = resume;
    resetPending_ = systemReset;
}

void MusicalClock::reset() {
    internalPhase_ = midiPhase_ = cvMidiPhase_ = 0;
    samples_ = cvLast_ = midiLast_ = 0;
    cvPeriod_ = midiPeriod_ = 0;
    lastCvPeriodSeconds_ = lastMidiPeriodSeconds_ = 0.0f;
    cvTicks_ = midiTicks_ = 0;
    midiQueued_ = 0;
    cvSeen_ = midiSeen_ = midiRunning_ = false;
    startPending_ = stopPending_ = continuePending_ = resetPending_ = false;
    cachedPpqn_ = 0;
    ++epoch_;
}

void MusicalClock::ingestMidiRealtime(uint8_t message) {
    switch (message) {
        case 0xf8: if (midiQueued_ != 0xffff) ++midiQueued_; break;
        case 0xfa:
            midiQueued_ = 0;
            stopPending_ = continuePending_ = false;
            startPending_ = true;
            break;
        case 0xfb: stopPending_ = false; continuePending_ = true; break;
        case 0xfc:
            midiQueued_ = 0;
            startPending_ = continuePending_ = false;
            stopPending_ = true;
            break;
        case 0xff: resetPending_ = true; break;
        default: break;
    }
}

ReferenceSample MusicalClock::tick(const ReferenceConfig& config, bool cvClockEdge, bool resetEdge) {
    ReferenceSample result;
    if (resetEdge || resetPending_) {
        reset();
        result.reset = true;
        result.epoch = epoch_;
        result.periodSeconds = 60.0f / bounded(config.bpm, 20.0f, 300.0f, 120.0f);
        return result;
    }
    const float bpm = bounded(config.bpm, 20.0f, 300.0f, 120.0f);
    if (bpm != cachedBpm_) {
        internalIncrement_ = incrementFor(static_cast<double>(bpm) / (60.0 * sampleRate_));
        midiIncrement_ = incrementFor(static_cast<double>(bpm) * 24.0 / (60.0 * sampleRate_));
        cachedBpm_ = bpm;
    }
    const uint8_t ppqn = cvPpqn(config.cvPpqn);
    if (ppqn != cachedPpqn_) {
        cvSeen_ = false;
        cvPeriod_ = 0;
        cvTicks_ = 0;
        cvMidiPhase_ = 0;
        cachedPpqn_ = ppqn;
        ++epoch_;
    }
    const bool start = startPending_;
    const bool stop = stopPending_;
    const bool cvAvailable = cvClockEdge || freshClock(samples_, cvLast_, cvPeriod_, ppqn, sampleRate_, cvSeen_);
    const bool midiTransportSelected = config.source == ReferenceConfig::Midi ||
        (config.source == ReferenceConfig::Auto && !cvAvailable);
    if (start) {
        midiRunning_ = true;
        midiTicks_ = 0;
        midiSeen_ = false;
        midiPeriod_ = 0;
        lastMidiPeriodSeconds_ = 0.0f;
        midiLast_ = samples_;
        if (midiTransportSelected) ++epoch_;
    } else if (stop) {
        midiRunning_ = false;
    } else if (continuePending_) {
        midiRunning_ = true;
        if (!midiSeen_) midiLast_ = samples_;
    }
    startPending_ = stopPending_ = continuePending_ = false;

    if (!freshClock(samples_, cvLast_, cvPeriod_, ppqn, sampleRate_, cvSeen_)) {
        cvSeen_ = false;
        cvPeriod_ = 0;
    }
    if (!freshClock(samples_, midiLast_, midiPeriod_, 24, sampleRate_, midiSeen_)) {
        midiSeen_ = false;
        midiPeriod_ = 0;
    }

    bool cvBeat = false;
    if (cvClockEdge) {
        if (cvSeen_ && samples_ > cvLast_) {
            cvPeriod_ = quarterPeriod(samples_ - cvLast_, ppqn);
            lastCvPeriodSeconds_ = static_cast<float>(cvPeriod_) / sampleRate_;
            cvMidiIncrement_ = incrementFor(24.0 / cvPeriod_);
        }
        if (!cvSeen_ || ++cvTicks_ >= ppqn) cvTicks_ = 0;
        cvBeat = cvTicks_ == 0;
        cvLast_ = samples_;
        cvSeen_ = true;
        if (cvEdgeIsMidiTick(ppqn)) cvMidiPhase_ = 0;
    }
    bool midiTick = false, midiBeat = false;
    if (midiQueued_ != 0) {
        --midiQueued_;
        midiTick = true;
        if (midiSeen_ && samples_ > midiLast_) {
            midiPeriod_ = quarterPeriod(samples_ - midiLast_, 24);
            lastMidiPeriodSeconds_ = static_cast<float>(midiPeriod_) / sampleRate_;
        }
        midiLast_ = samples_;
        midiSeen_ = true;
        if (++midiTicks_ >= 24) { midiTicks_ = 0; midiBeat = true; }
    }
    const bool cvFresh = freshClock(samples_, cvLast_, cvPeriod_, ppqn, sampleRate_, cvSeen_);
    const bool midiFresh = freshClock(samples_, midiLast_, midiPeriod_, 24, sampleRate_, midiSeen_);
    const bool midiAwaitingClock = midiRunning_ && !midiSeen_ && samples_ - midiLast_ < static_cast<uint32_t>(sampleRate_ * 2.0f);
    ReferenceConfig::Source source = config.source <= ReferenceConfig::Auto ? config.source : ReferenceConfig::Internal;
    if (source == ReferenceConfig::Auto) {
        source = cvFresh ? ReferenceConfig::Cv : (midiFresh || midiAwaitingClock) && midiRunning_ ? ReferenceConfig::Midi : ReferenceConfig::Internal;
    }
    if (source != selected_) { selected_ = source; ++epoch_; }
    const bool internalBeat = advance(internalPhase_, internalIncrement_);
    const bool internalMidi = advance(midiPhase_, midiIncrement_);
    result.periodSeconds = 60.0f / bpm;
    result.epoch = epoch_;
    const bool useMidiTransport = source == ReferenceConfig::Midi ||
        (config.source == ReferenceConfig::Auto && !cvFresh);
    result.start = start && useMidiTransport;
    result.stop = stop && useMidiTransport;
    switch (source) {
        case ReferenceConfig::Internal:
            result.beat = internalBeat;
            result.midiClock = internalMidi;
            result.fraction = phaseFloat(internalPhase_);
            break;
        case ReferenceConfig::Cv:
            if (lastCvPeriodSeconds_ > 0.0f) result.periodSeconds = lastCvPeriodSeconds_;
            result.beat = cvBeat;
            result.ready = cvPeriod_ != 0 && cvFresh;
            result.running = cvFresh;
            if (cvPeriod_ != 0) {
                const float age = elapsedFloat(samples_, cvLast_) / static_cast<float>(cvPeriod_);
                const float withinEdge = bounded(age, 0.0f, 0.999999f / static_cast<float>(ppqn), 0.0f);
                result.fraction = static_cast<float>(cvTicks_) / static_cast<float>(ppqn) + withinEdge;
                if (cvFresh) {
                    if (ppqn == 48) result.midiClock = cvClockEdge && cvTicks_ % 2 == 0;
                    else if (cvEdgeIsMidiTick(ppqn)) result.midiClock = cvClockEdge || advance(cvMidiPhase_, cvMidiIncrement_);
                    else result.midiClock = advance(cvMidiPhase_, cvMidiIncrement_);
                }
            }
            break;
        case ReferenceConfig::Midi:
            if (lastMidiPeriodSeconds_ > 0.0f) result.periodSeconds = lastMidiPeriodSeconds_;
            result.beat = midiBeat && midiRunning_;
            result.midiClock = midiTick;
            result.ready = midiPeriod_ != 0 && midiFresh;
            result.running = midiRunning_ && midiFresh;
            if (midiPeriod_ != 0) {
                const float age = elapsedFloat(samples_, midiLast_) / static_cast<float>(midiPeriod_);
                result.fraction = static_cast<float>(midiTicks_) / 24.0f + bounded(age, 0.0f, 0.999999f / 24.0f, 0.0f);
            }
            break;
        case ReferenceConfig::Auto: break;
    }
    ++samples_;
    return result;
}

void MusicalTiming::reset() {
    initialised_ = false;
    interval_ = counter_ = 0;
    bucket_ = 0;
}

bool MusicalTiming::tick(const ReferenceSample& reference, uint8_t interval) {
    if (interval > 16) interval = 16;
    const bool changed = !initialised_ || epoch_ != reference.epoch || interval_ != interval;
    if (changed) {
        epoch_ = reference.epoch;
        interval_ = interval;
        counter_ = 0;
        bucket_ = 0;
        initialised_ = true;
    }
    if (interval == 0 || reference.reset || !reference.running) return false;
    const uint8_t numerator = ratioNumerators[interval - 1];
    const uint8_t denominator = ratioDenominators[interval - 1];
    if (reference.beat && !changed) counter_ = static_cast<uint8_t>((counter_ + 1) % numerator);
    const float fraction = reference.ready ? bounded(reference.fraction, 0.0f, 0.999999f, 0.0f) : 0.0f;
    const int bucket = static_cast<int>((static_cast<float>(counter_) + fraction) * static_cast<float>(denominator) / static_cast<float>(numerator));
    if (changed) {
        bucket_ = bucket;
        return reference.beat;
    }
    const bool event = bucket != bucket_ || (reference.beat && counter_ == 0);
    bucket_ = bucket;
    return event && (reference.ready || reference.beat);
}

} // namespace multi_burst
