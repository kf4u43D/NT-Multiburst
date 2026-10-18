#pragma once
#include <cstdint>

namespace multi_burst {
// Persisted by the host: append future parameters, never reorder these indices.
enum Parameter : uint8_t {
    Bursts, EnvelopeShape, Width, Slope, Level, Rate, Tempo, ClockSource,
    Division, CvDivider, CvPpqn, RetriggerMode, FirstBurst, PulseWidth,
    TriggerInput, ClockInput, ResetInput,
    EnvelopeOutput, EnvelopeOutputMode, GateOutput, GateOutputMode,
    ActiveOutput, ActiveOutputMode, EocOutput, EocOutputMode,
    ClockOutput, ClockOutputMode,
    MidiInputChannel, MidiTriggerNote, MidiResetNote, MidiStepNote,
    MidiDestination, MidiOutputChannel, MidiOutputNote, MidiNotes,
    MidiEnvelopeCc, MidiCcRate, MidiClockOutput, MidiTransportOutput,
    ManualTrigger, ManualReset, ManualStep,
    // Version 0.2 additions. The original 42 indices above remain unchanged.
    Generator2Enabled, FocusGenerator, SharedClockSource,
    TimingMode, WidthRatio, IntervalRatio, GlideRise, GlideFall,
    GlideRiseRatio, GlideFallRatio, Loop, Mute, VelocityEnabled, LevelTrim,
    CvEnabled, CvDepth, CountCvDepth, TimeCvDepth, ShapeCvDepth, SlopeCvDepth,
    ReferenceCurves, StopInput, StepInput, ManualStop, MidiStopNote,
    LevelsOutput, LevelsOutputMode, ProgressOutput, ProgressOutputMode,
    BurstOutput, BurstOutputMode, PresetSlot, PresetLoad, PresetSave, PresetFactory,
    G2Bursts, G2EnvelopeShape, G2Width, G2Slope, G2Level, G2Rate,
    G2Division, G2CvDivider, G2RetriggerMode, G2FirstBurst,
    G2TimingMode, G2WidthRatio, G2IntervalRatio, G2GlideRise, G2GlideFall,
    G2GlideRiseRatio, G2GlideFallRatio, G2Loop, G2Mute, G2VelocityEnabled,
    G2LevelTrim, G2CvEnabled, G2CvDepth, G2CountCvDepth, G2TimeCvDepth,
    G2ShapeCvDepth, G2SlopeCvDepth, G2ReferenceCurves,
    G2TriggerInput, G2ResetInput, G2StopInput, G2StepInput,
    G2EnvelopeOutput, G2EnvelopeOutputMode, G2GateOutput, G2GateOutputMode,
    G2ActiveOutput, G2ActiveOutputMode, G2EocOutput, G2EocOutputMode,
    G2ClockOutput, G2ClockOutputMode, G2LevelsOutput, G2LevelsOutputMode,
    G2ProgressOutput, G2ProgressOutputMode, G2BurstOutput, G2BurstOutputMode,
    G2MidiInputChannel, G2MidiTriggerNote, G2MidiResetNote, G2MidiStepNote,
    G2MidiStopNote, G2MidiDestination, G2MidiOutputChannel, G2MidiOutputNote,
    G2MidiNotes, G2MidiEnvelopeCc, G2MidiCcRate,
    G2ManualTrigger, G2ManualReset, G2ManualStep, G2ManualStop,
    G2PresetSlot, G2PresetLoad, G2PresetSave, G2PresetFactory,
    CountCvInput, WidthCvInput, IntervalCvInput, SlopeCvInput, ShapeCvInput,
    AmplitudeCvInput, TriggerAllInput, ResetAllInput, StopAllInput, StepAllInput,
    ManualTriggerAll, ManualResetAll, ManualStopAll, ManualStepAll,
    MixOutput, MixOutputMode, BusyAllOutput, BusyAllOutputMode,
    ReferenceClockOutput, ReferenceClockOutputMode,
    FocusProgressOutput, FocusProgressOutputMode,
    // Version 0.3: shared CV law; previous host-preset indices stay unchanged.
    CvResponse,
    // Version 0.4: read-only performance pack, separate from user banks.
    PackPreset, LoadPackPreset, ParameterCount
};
static_assert(ParameterCount < 256, "SDK parameter page indices are eight-bit");
constexpr uint16_t divisionTicks[] = {3, 6, 12, 24, 48, 96, 192};
constexpr uint8_t ppqnValues[] = {1, 2, 4, 8, 12, 24, 48};
}
