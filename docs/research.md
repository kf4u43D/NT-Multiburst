# References and design choices

These sources distinguish published manufacturer behavior from the independent
implementation choices used by this plugin. They were reviewed in October 2026.

## Firmware and SDK

The validation target is disting NT firmware **1.18.0**, released on
**4 September 2026**, according to the
[official firmware table](https://www.expert-sleepers.co.uk/distingNTfirmwareupdates.html).
Firmware and SDK API versions are separate compatibility contracts.

The [official SDK](https://github.com/expertsleepersltd/distingNT_API) snapshot is
pinned to **f440d20c2a96a9ca43892c7418a1d8185ad47eb5**, dated **3 August 2026**.
Its [api.h](https://github.com/expertsleepersltd/distingNT_API/blob/f440d20c2a96a9ca43892c7418a1d8185ad47eb5/include/distingnt/api.h)
defines `kNT_apiVersionCurrent = kNT_apiVersion13`. The hardware plugin explicitly
advertises API 13, supported since firmware **1.15.0**. API 14, introduced in the
SDK on 29 September 2026, must not be inferred to work with firmware 1.18.0.

Pinned revisions make builds reproducible. Changing the target firmware or SDK
requires fresh checks of signatures, imports and ARM object loading. See the
[validation report](validation.md) for software results; hardware testing is
pending.

## Published AJH Multi Burst Envelopes behavior

The [official AJH Synth page](https://ajhsynth.com/MultiBurst.html) describes:

| Feature | Published behavior |
| --- | --- |
| Sequence | 1–40 envelopes; one burst acts as an AD envelope. |
| Shapes, in order | Sine, Log Down, Log Up, Expo Down, Ramp Up, Triangle, Ramp Down, Pulse. |
| Width | 20 ms–2.5 s, with CV control. |
| Internal rate | Off, then 0.24–51 Hz, with CV control. |
| Amplitude | 0 to +10 V; rising sequence from 4 to 100%, falling sequence from 100 to 4%, or uniform level. |
| External clock | Each pulse advances one envelope; irregular timing and combination with the internal LFO are supported. |
| Triggering | ONE ignores triggers during a sequence; RET interrupts and restarts. |
| Modulation | Shape, width and rate can change during the sequence; slope is captured at the trigger. |
| Shape selection | 0 to +5 V CV replaces the selector. |
| Completion and gate | The sequence remains active until the final envelope ends; gate behavior depends on count, width and rate. |

AJH credits Allan Hall and Tom Wiltshire of Electric Druid. The page publishes
no curve equations or sample tables. Its linked
[official video guide](https://player.vimeo.com/video/703216004) was not analyzed.
The [official manual catalog](https://ajhsynth.com/downloads.html) had no Multi
Burst Envelopes PDF at the time of review.

## Scope and behavior

The plugin implements the published musical principle: finite envelope
sequences, amplitude progression and several timing sources. Curve names and
intent follow the reference, but the curves are independent implementations.
No equivalence of tables, equations, resolution or analog behavior is claimed.

The published sources do not fully specify inter-burst slope laws, count
capture, mid-envelope shape changes, first-event timing, envelope overlap or
clock collisions. Closer equivalence would require measurements on an AJH
module. This plugin uses these explicit conventions:

- **Count and slope:** captured at the trigger; edits affect the next sequence.
- **Shape, width and level:** live during playback. Shape can change during the
  envelope, width changes its phase speed, and level changes its output voltage.
- **First burst:** selectable immediate trigger response or next-clock start.
- **Overlap:** a new burst restarts the envelope; envelopes do not sum.
- **ONE / RET:** ONE protects the sequence through the final envelope; RET
  restarts it from the beginning.
- **Reset:** wins over simultaneous trigger and clock events, cancels the
  sequence, clears phases/counters/gates and releases the plugin's MIDI notes.
- **EOC:** marks only the natural end of the final envelope, not reset or
  interruption.

Dedicated reset, MIDI, Active/EOC and clock outputs are NT extensions.

## CV, MIDI and timing

The plugin provides per-voice and shared actions, plus six shared CV inputs:
Count, Width, Interval, Slope, Shape and Amplitude. The DROID response retains
per-voice targets and depths; native NT CV/MIDI mappings remain available.

Timing sources are internal, CV, MIDI and internal + CV. The combined source
keeps two independent clocks for interlocking rhythms; it does not phase-lock
the internal LFO. MIDI clock and transport are system messages, independent of
the note/CC channel.

Outputs include envelope, burst gate, sequence activity, EOC and clock. Active
stays high while an armed sequence waits for a clock. Optional buses can be
disabled; enabled outputs respect Add/Replace settings.

MIDI input covers trigger notes, control CCs, clock and transport. Output
includes burst notes, envelope level CC and clock. CC emission is rate-limited
and change-driven; it does not have CV timing or amplitude resolution. Routing
changes, reset and stop must preserve consistent note ownership.

## Modulation choices

AJH's published live shape, width and rate control, and irregular external clock
combined with its LFO, inform the Absolute and Internal + CV modes. The source
does not establish an exact CV response equation.

The supplied DROID patch quantizes timing to 16 ratios and captures count and
slope at the trigger, while width, shape and amplitude stay live. Mainly
unipolar targets and multiplicative slope can restrict a bipolar LFO's range.
The [reference comparison](reference-comparison.md) details these differences.

Bipolar additive response applies signed voltages around the control setting,
with exponential timing modulation in Absolute mode. Shape response=Morph
crossfades neighboring curves. These are independent NT design choices, not
claimed AJH features. DROID and Legacy responses remain available. The shared
CV response is appended to the parameter list; voice banks retain 25 fields,
with Morph stored in the existing curve field.

## SDK and real-time constraints

`pluginEntry` advertises hardware API 13 and supplies `_NT_factory`. Callbacks
declare memory requirements, construct the instance in host allocations,
process parameters and blocks, and receive MIDI. Parameter-definition and
page pointers must be initialized at construction. The
[pinned SDK signatures](https://github.com/expertsleepersltd/distingNT_API/blob/f440d20c2a96a9ca43892c7418a1d8185ad47eb5/include/distingnt/api.h)
take precedence over secondary examples.

There are **64 logical buses**: 12 inputs, 8 outputs and 44 auxiliaries. User
bus numbers are one-based; buffer indices are zero-based. Validate disabled
routing value 0 before subtracting. The historical `step()` comment referring
to 28 buses does not match the current constants.

`numFramesBy4 * 4` is the block frame count. Each contiguous bus holds that many
samples. Timing uses `NT_globals.sampleRate`, without assuming 48 kHz or a fixed
block size.

The [official example Makefile](https://github.com/expertsleepersltd/distingNT_API/blob/f440d20c2a96a9ca43892c7418a1d8185ad47eb5/examples/Makefile)
uses Cortex-M7, Thumb, `fpv5-d16`, hard-float ABI and PIC, without RTTI or
exceptions. The deliverable is a relocatable ARM `.o`; inspect its format,
`pluginEntry`, sections and external references after building.

Real-time implementation rules:

- Use fixed-capacity state/queues and request persistent storage through
  `calculateRequirements()`.
- Avoid allocation, disk/network access, waits, logging and unbounded loops in
  audio processing and control callbacks.
- Recalculate coefficients on input changes; use constant tables and simple
  interpolation where useful.
- Bound indices, counters, CV, frequencies and MIDI; handle non-finite values.
- Detect CV edges with hysteresis and process reset, trigger, then clock.
- Preserve published parameter indices and append new parameters.
- Treat native tests, ARM inspection, emulation and hardware tests as distinct
  evidence.

The [thorinside disting-nt-cpp-plugin-writer skill](https://github.com/thorinside/nt_mcp/blob/3047bb232d9487fe075d48d12f48115383adf728/claude-plugin/skills/disting-nt-cpp-plugin-writer/SKILL.md),
revision **3047bb232d9487fe075d48d12f48115383adf728**, informed these practices.
Its API 13 baseline matches the hardware contract; official SDK declarations
remain authoritative. Polyphonic-allocation advice applies only to an actual
polyphonic architecture.

## Hardware checks still required

No physical disting NT validation or measured AJH comparison has been performed.
Hardware testing must confirm loading, CPU use alongside other algorithms,
CV thresholds/output voltages, external timing and MIDI. Boundary cases include
1/40 bursts, extreme slopes, ONE/RET through the final envelope, live edits,
missing/combined clocks, simultaneous events, reset, stop and routing changes.
