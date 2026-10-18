#pragma once

#include <cstdint>

namespace multi_burst {

// Voice-local values only. Shared clock, CV routing and MIDI routing belong to
// the NT host preset. Ratios use the DROID grid, independently of host enums.
struct Preset {
    int16_t bursts = 8;
    int16_t shape = 0;
    int16_t slope = 0;             // Legacy NT sign: positive rises.
    int16_t level = 80;            // Tenths of a volt, 0..100.
    int16_t retrigger = 0;         // 0 Restart / 1 Ignore.
    int16_t loop = 0;
    int16_t mute = 0;
    int16_t widthRatio = 3;        // 0..15: 1/16 .. 16 quarter notes.
    int16_t intervalRatio = 5;     // -1 Off; otherwise the same 0..15 grid.
    int16_t glideUp = 0;           // 0 Off; 1..16: 1/16 .. 16 quarters.
    int16_t glideDown = 0;
    int16_t cvEnabled = 1;
    int16_t velocityEnabled = 1;
    int16_t cvDepth = 100;         // All depths and trim are percentages.
    int16_t levelTrim = 100;
    int16_t countDepth = 100;
    int16_t timeDepth = 100;
    int16_t shapeDepth = 100;
    int16_t slopeDepth = 100;

    // NT extensions make an absolute-timing sound round-trip as well.
    int16_t widthMilliseconds = 100;
    int16_t rateCentihz = 400;
    int16_t timingMode = 0;        // 0 Absolute / 1 Musical.
    int16_t referenceCurves = 0;   // 0 Legacy / 1 DROID / 2 continuous Morph.
    int16_t glideRiseMs = 0;
    int16_t glideFallMs = 0;
};

namespace preset_detail {
constexpr int16_t clamp(int value, int minimum, int maximum) {
    return static_cast<int16_t>(value < minimum ? minimum : value > maximum ? maximum : value);
}

constexpr int16_t numerator[16] = {1, 1, 1, 1, 1, 1, 1, 2, 1, 3, 2, 3, 4, 6, 8, 16};
constexpr int16_t denominator[16] = {16, 12, 8, 6, 4, 3, 2, 3, 1, 2, 1, 1, 1, 1, 1, 1};

// Exact factory data from ref/droid-v2.3.1.ini. Count includes the encoder's
// +1 output offset; width/interval positions are quantised to their 16 slots.
constexpr int16_t counts[2][16] = {
    {8, 6, 12, 10, 7, 4, 16, 40, 8, 5, 7, 12, 8, 6, 12, 7},
    {5, 9, 7, 11, 6, 3, 13, 32, 5, 9, 9, 7, 6, 4, 8, 5}
};
constexpr int16_t shapes[16] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 0};
constexpr int16_t droidSlopes[2][16] = {
    {0, 100, -100, 100, -100, 0, 100, 0, 0, -100, 100, 100, -70, 0, 75, 80},
    {0, -100, 100, -100, 100, 0, -100, 0, 0, -100, 100, -100, 70, 0, -75, -80}
};
constexpr int16_t widths[2][16] = {
    {3, 3, 2, 2, 5, 7, 1, 0, 5, 4, 0, 7, 4, 1, 5, 8},
    {5, 4, 6, 4, 5, 8, 2, 0, 6, 5, 0, 7, 4, 3, 6, 8}
};
constexpr int16_t intervals[2][16] = {
    {5, 4, 3, 3, 5, 8, 2, 0, 6, 5, 2, 5, 6, 5, 5, 8},
    {6, 5, 7, 4, 6, 9, 3, 1, 7, 6, 2, 6, 9, 7, 6, 8}
};
constexpr int16_t loops[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1};

constexpr int16_t millisecondsAt120(int ratio) {
    return clamp((500 * numerator[ratio] + denominator[ratio] / 2) / denominator[ratio], 20, 2500);
}
constexpr int16_t glideMillisecondsAt120(int glide) {
    return glide == 0 ? 0 : static_cast<int16_t>(
        (500 * numerator[glide - 1] + denominator[glide - 1] / 2) / denominator[glide - 1]);
}
} // namespace preset_detail

constexpr Preset boundedPreset(Preset p) {
    using preset_detail::clamp;
    p.bursts = clamp(p.bursts, 1, 40);
    p.shape = clamp(p.shape, 0, 7);
    p.slope = clamp(p.slope, -100, 100);
    p.level = clamp(p.level, 0, 100);
    p.retrigger = clamp(p.retrigger, 0, 1);
    p.loop = clamp(p.loop, 0, 1);
    p.mute = clamp(p.mute, 0, 1);
    p.widthRatio = clamp(p.widthRatio, 0, 15);
    p.intervalRatio = clamp(p.intervalRatio, -1, 15);
    p.glideUp = clamp(p.glideUp, 0, 16);
    p.glideDown = clamp(p.glideDown, 0, 16);
    p.cvEnabled = clamp(p.cvEnabled, 0, 1);
    p.velocityEnabled = clamp(p.velocityEnabled, 0, 1);
    p.cvDepth = clamp(p.cvDepth, 0, 100);
    p.levelTrim = clamp(p.levelTrim, 0, 100);
    p.countDepth = clamp(p.countDepth, 0, 100);
    p.timeDepth = clamp(p.timeDepth, 0, 100);
    p.shapeDepth = clamp(p.shapeDepth, 0, 100);
    p.slopeDepth = clamp(p.slopeDepth, 0, 100);
    p.widthMilliseconds = clamp(p.widthMilliseconds, 20, 2500);
    p.rateCentihz = clamp(p.rateCentihz, 0, 5100);
    p.timingMode = clamp(p.timingMode, 0, 1);
    p.referenceCurves = clamp(p.referenceCurves, 0, 2);
    p.glideRiseMs = clamp(p.glideRiseMs, 0, 30000);
    p.glideFallMs = clamp(p.glideFallMs, 0, 30000);
    return p;
}

constexpr Preset factoryPreset(unsigned generator, unsigned slot) {
    using namespace preset_detail;
    generator = generator < 2 ? generator : 1;
    slot = slot < 16 ? slot : 15;
    Preset p;
    p.bursts = counts[generator][slot];
    p.shape = shapes[slot];
    // DROID uses positive = falling; the NT's existing slope sign is retained.
    p.slope = static_cast<int16_t>(-droidSlopes[generator][slot]);
    p.level = static_cast<int16_t>(slot == 5 ? 65 : slot == 6 ? 85 : slot == 7 ? 50 : 100);
    p.retrigger = slot < 12 ? 1 : 0;
    p.loop = loops[slot];
    p.widthRatio = widths[generator][slot];
    p.intervalRatio = intervals[generator][slot];
    p.glideUp = static_cast<int16_t>(generator == 0 ? (slot == 9 ? 5 : 0) : (slot == 9 ? 6 : slot == 15 ? 7 : 0));
    p.glideDown = static_cast<int16_t>(generator == 0 ? (slot == 10 ? 4 : slot == 15 ? 7 : 0) : (slot == 10 ? 6 : 0));
    p.widthMilliseconds = millisecondsAt120(p.widthRatio);
    p.rateCentihz = static_cast<int16_t>((200 * denominator[p.intervalRatio] + numerator[p.intervalRatio] / 2) / numerator[p.intervalRatio]);
    p.glideRiseMs = glideMillisecondsAt120(p.glideUp);
    p.glideFallMs = glideMillisecondsAt120(p.glideDown);
    p.timingMode = 1;
    p.referenceCurves = 1;
    return p;
}

// No allocation and no SDK dependency. Only user overrides occupy saved slots;
// untouched/restored slots always resolve to their exact factory definition.
class PresetBank {
public:
    static constexpr unsigned generatorCount = 2;
    static constexpr unsigned slotsPerGenerator = 16;

    Preset load(unsigned generator, unsigned slot) const {
        return isSaved(generator, slot) ? values_[generator][slot] : factoryPreset(generator, slot);
    }
    bool save(unsigned generator, unsigned slot, const Preset& preset) {
        if (!valid(generator, slot)) return false;
        values_[generator][slot] = boundedPreset(preset);
        saved_[generator] = static_cast<uint16_t>(saved_[generator] | (uint16_t{1} << slot));
        return true;
    }
    bool restoreFactory(unsigned generator, unsigned slot) {
        if (!valid(generator, slot)) return false;
        saved_[generator] = static_cast<uint16_t>(saved_[generator] & ~(uint16_t{1} << slot));
        return true;
    }
    bool isSaved(unsigned generator, unsigned slot) const {
        return valid(generator, slot) && (saved_[generator] & (uint16_t{1} << slot)) != 0;
    }

private:
    static constexpr bool valid(unsigned generator, unsigned slot) {
        return generator < generatorCount && slot < slotsPerGenerator;
    }
    Preset values_[generatorCount][slotsPerGenerator]{};
    uint16_t saved_[generatorCount]{};
};

} // namespace multi_burst
