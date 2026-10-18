#pragma once

#include "multi_burst/presets.h"

namespace multi_burst {

constexpr unsigned performancePresetCount = 16;
constexpr const char* performancePresetNames[performancePresetCount] = {
    "Drum Pair", "Bead Cascade", "Eight Ratchet", "Flam Duo",
    "Velvet Crescendo", "Falling Sparks", "Crossing Levels", "Slow Glide",
    "Morph Breathing", "Pulse And Sweep", "Hand Steps", "Clock Weave",
    "Triplet Weave", "Seven Against Five", "Dotted Lanterns", "Long Tide"
};

// Read-only scenes. Values use NT parameter units and enum indices. Loading a
// scene changes the live controls, never the two editable DROID preset banks.
// Routing, MIDI assignments, focus and the user's saved slots are not included.
struct PerformancePreset {
    Preset voices[2];
    int16_t clockSource = 0;
    int16_t sharedClockSource = 0;
    int16_t tempo = 1200;
    int16_t cvResponse = 1;
    int16_t generator2Enabled = 0;
    int16_t division[2] = {1, 1};
    int16_t cvDivider[2] = {1, 1};
    int16_t firstBurst[2] = {0, 0};
    int16_t cvPpqn = 0;
    int16_t pulseWidth = 5;
};

namespace performance_detail {
constexpr Preset absoluteVoice(int16_t count, int16_t shape, int16_t widthMs,
                               int16_t rateCentihz, int16_t slope,
                               int16_t level, int16_t loop = 0) {
    Preset p;
    p.bursts = count;
    p.shape = shape;
    p.widthMilliseconds = widthMs;
    p.rateCentihz = rateCentihz;
    p.slope = slope;
    p.level = level;
    p.loop = loop;
    p.referenceCurves = 1;
    // Bipolar modulation is optional. Effective destination depth is the
    // product of master and destination depths (e.g. time = 24.5 percent).
    p.cvDepth = 35;
    p.countDepth = 20;
    p.timeDepth = 70;
    p.shapeDepth = 60;
    p.slopeDepth = 50;
    return p;
}

constexpr int16_t ratioMilliseconds(int ratio, int16_t tempo, int maximum) {
    const int numerator = preset_detail::numerator[ratio];
    const int denominator = tempo * preset_detail::denominator[ratio];
    return preset_detail::clamp((600000 * numerator + denominator / 2) / denominator, 0, maximum);
}

constexpr Preset musicalVoice(int16_t count, int16_t shape, int16_t widthRatio,
                              int16_t intervalRatio, int16_t slope,
                              int16_t level, int16_t tempo) {
    Preset p = absoluteVoice(count, shape, 100, 400, slope, level, 1);
    p.timingMode = 1;
    p.widthRatio = widthRatio;
    p.intervalRatio = intervalRatio;
    p.widthMilliseconds = preset_detail::clamp(ratioMilliseconds(widthRatio, tempo, 2500), 20, 2500);
    const int numerator = preset_detail::numerator[intervalRatio];
    const int denominator = preset_detail::denominator[intervalRatio];
    p.rateCentihz = preset_detail::clamp((tempo * denominator + 3 * numerator) / (6 * numerator), 0, 5100);
    return p;
}

constexpr bool namesFitDisplay() {
    for (const char* name : performancePresetNames) {
        unsigned length = 0;
        while (name[length] != '\0') {
            if (name[length] < 32 || name[length] > 126) return false;
            ++length;
        }
        if (length == 0 || length >= 24) return false;
    }
    return true;
}
static_assert(namesFitDisplay(), "Performance preset labels must be short printable ASCII");
} // namespace performance_detail

constexpr PerformancePreset performancePreset(unsigned slot) {
    using performance_detail::absoluteVoice;
    using performance_detail::musicalVoice;
    slot = slot < performancePresetCount ? slot : performancePresetCount - 1;
    PerformancePreset scene;
    scene.generator2Enabled = 1;
    switch (slot) {
    case 0: // A short decay and a softer rising accent.
        scene.voices[0] = absoluteVoice(1, 3, 140, 400, 0, 50);
        scene.voices[1] = absoluteVoice(1, 2, 70, 400, 0, 35);
        scene.voices[0].cvDepth = scene.voices[1].cvDepth = 25;
        scene.voices[0].countDepth = scene.voices[1].countDepth = 0;
        scene.voices[0].timeDepth = scene.voices[1].timeDepth = 50;
        break;
    case 1: // Distinct finite bead strings, six against five attacks.
        scene.voices[0] = absoluteVoice(6, 0, 90, 600, -40, 45);
        scene.voices[1] = absoluteVoice(5, 6, 75, 400, 45, 35);
        break;
    case 2: // Eight fast gates with four slower decaying accents.
        scene.voices[0] = absoluteVoice(8, 7, 20, 2400, -25, 40);
        scene.voices[1] = absoluteVoice(4, 3, 45, 1200, -65, 40);
        scene.pulseWidth = 2;
        scene.voices[0].countDepth = scene.voices[1].countDepth = 0;
        break;
    case 3: // Two attacks 80 ms apart over a single soft contour.
        scene.voices[0] = absoluteVoice(2, 3, 55, 1250, -30, 50);
        scene.voices[1] = absoluteVoice(1, 5, 220, 400, 0, 35);
        scene.voices[0].countDepth = scene.voices[1].countDepth = 0;
        scene.voices[0].cvDepth = scene.voices[1].cvDepth = 25;
        break;
    case 4:
        scene.voices[0] = absoluteVoice(8, 2, 100, 500, 90, 50);
        scene.voices[1] = absoluteVoice(6, 0, 160, 375, 60, 40);
        scene.voices[0].glideRiseMs = 70;
        scene.voices[1].glideRiseMs = 120;
        break;
    case 5:
        scene.voices[0] = absoluteVoice(12, 3, 30, 1800, -95, 50);
        scene.voices[1] = absoluteVoice(6, 1, 95, 700, -65, 40);
        scene.voices[0].glideFallMs = 15;
        scene.voices[1].glideFallMs = 60;
        break;
    case 6: // Independent repeated crescendos and decrescendos.
        scene.voices[0] = absoluteVoice(8, 0, 220, 240, 90, 45, 1);
        scene.voices[1] = absoluteVoice(8, 1, 320, 170, -90, 45, 1);
        scene.voices[0].glideRiseMs = 180;
        scene.voices[1].glideFallMs = 260;
        break;
    case 7:
        scene.voices[0] = absoluteVoice(6, 5, 1600, 60, 90, 45, 1);
        scene.voices[1] = absoluteVoice(7, 0, 2200, 47, -85, 40, 1);
        scene.voices[0].glideRiseMs = 1400;
        scene.voices[1].glideFallMs = 1800;
        scene.voices[0].timeDepth = scene.voices[1].timeDepth = 50;
        break;
    case 8: // Integer profiles sound unaided; Shape CV opens up Morph.
        scene.voices[0] = absoluteVoice(7, 2, 850, 110, 35, 45, 1);
        scene.voices[1] = absoluteVoice(9, 5, 550, 170, -40, 40, 1);
        scene.voices[0].referenceCurves = scene.voices[1].referenceCurves = 2;
        scene.voices[0].shapeDepth = scene.voices[1].shapeDepth = 70;
        scene.voices[0].glideRiseMs = 400;
        scene.voices[1].glideFallMs = 300;
        break;
    case 9:
        scene.voices[0] = absoluteVoice(9, 7, 65, 800, 0, 35, 1);
        scene.voices[1] = absoluteVoice(5, 4, 950, 120, 65, 45, 1);
        scene.voices[1].glideRiseMs = 650;
        break;
    case 10: // First attack is immediate; subsequent attacks require Step.
        scene.voices[0] = absoluteVoice(4, 6, 180, 0, -65, 45);
        scene.voices[1] = absoluteVoice(4, 4, 300, 0, 65, 40);
        scene.voices[0].countDepth = scene.voices[1].countDepth = 0;
        break;
    case 11: // CV clock adds optional events to already-running free clocks.
        scene.clockSource = 4;
        scene.voices[0] = absoluteVoice(13, 3, 95, 330, 35, 45, 1);
        scene.voices[1] = absoluteVoice(9, 2, 150, 210, -40, 40, 1);
        scene.voices[0].referenceCurves = scene.voices[1].referenceCurves = 2;
        scene.voices[0].glideRiseMs = 60;
        scene.voices[1].glideFallMs = 100;
        break;
    case 12: // Twelve triplet attacks and eight duplet attacks per four beats.
        scene.tempo = 1080;
        scene.voices[0] = musicalVoice(12, 3, 5, 5, 0, 45, scene.tempo);
        scene.voices[1] = musicalVoice(8, 0, 6, 6, -30, 40, scene.tempo);
        break;
    case 13: // Seven- and five-beat phrases share a one-beat interval.
        scene.tempo = 960;
        scene.voices[0] = musicalVoice(7, 5, 8, 8, 70, 45, scene.tempo);
        scene.voices[1] = musicalVoice(5, 0, 8, 8, -70, 40, scene.tempo);
        scene.voices[0].glideUp = 7;
        scene.voices[1].glideDown = 7;
        break;
    case 14: // Four dotted beats against six plain beats in the same phrase.
        scene.voices[0] = musicalVoice(4, 1, 9, 9, -45, 45, scene.tempo);
        scene.voices[1] = musicalVoice(6, 5, 8, 8, 55, 40, scene.tempo);
        scene.voices[0].glideDown = 5;
        scene.voices[1].glideUp = 6;
        break;
    case 15: // Long, overlapping phrase cycles; Morph remains CV-optional.
        scene.tempo = 600;
        scene.voices[0] = musicalVoice(4, 2, 10, 10, 80, 45, scene.tempo);
        scene.voices[1] = musicalVoice(3, 6, 11, 11, -80, 40, scene.tempo);
        scene.voices[0].referenceCurves = scene.voices[1].referenceCurves = 2;
        scene.voices[0].glideUp = 10;
        scene.voices[1].glideDown = 11;
        scene.voices[0].timeDepth = scene.voices[1].timeDepth = 50;
        break;
    default:
        break; // The slot is clamped above; retain a defensive default.
    }
    // Populate the absolute glide controls too, so switching timing modes
    // retains useful values. Musical glide indices include their leading Off.
    for (auto& voice : scene.voices) {
        if (voice.timingMode != 0) {
            if (voice.glideUp != 0) voice.glideRiseMs = performance_detail::ratioMilliseconds(voice.glideUp - 1, scene.tempo, 30000);
            if (voice.glideDown != 0) voice.glideFallMs = performance_detail::ratioMilliseconds(voice.glideDown - 1, scene.tempo, 30000);
        }
        voice = boundedPreset(voice);
    }
    return scene;
}

} // namespace multi_burst
