# DROID reference and NT implementation

The functional reference is [droid-v2.3.1.ini](../ref/droid-v2.3.1.ini), the
MULTI BURST V2.3.1 export for DROID blue-7. The behaviour below follows the
patch's connections and formulas. Preset names identify settings; their names
alone do not establish a particular musical property.

| Function | DROID reference | NT implementation |
| --- | --- | --- |
| Generators | Two independent engines, G1 and G2 | Two voices in one instance, with a shared timing reference. |
| Burst count | 1–40, captured at an accepted trigger | Same behaviour. |
| First burst | Immediate | Immediate or on the next clock. |
| Retrigger | RET restarts; ONE waits until the final envelope tail ends | Same behaviour. |
| Slope and velocity | Captured at the trigger | Same behaviour; NT slope sign convention retained. |
| Width and shape | Adjustable during an envelope | Same behaviour. |
| Eight shapes | Sine, Log Down, Log Up, Expo Down, Ramp Up, Triangle, Ramp Down, Pulse | Same order; choice of Legacy profiles, DROID reference curves or Morph. |
| LOOP | Natural completion restarts the sequence | Loops after the final envelope; stop/reset interrupts. |
| MUTE | Silences ENV and GATE, retaining sequencing and BUSY | Same role. |
| GLIDE | Separate linear rise/fall slew between levels; first level is immediate | Absolute times or musical ratios. |
| Musical width | 16 ratios of the reference period | Musical mode uses the ratio grid; Absolute uses milliseconds. |
| Musical interval | Off or 16 ratios; subdivisions interpolated between CV/MIDI clocks | Musical grid or Absolute rate, MIDI/BPM division and CV divider. |
| Clock source | Internal, CV, MIDI, Auto | Internal rate, Internal BPM, CV, MIDI, Internal + CV; shared Musical clock also supports Auto. |
| Actions | Trigger/stop/step per voice, shared trigger and actions on the focused voice | Actions and focus available through NT controls and parameter pages. |
| Levels | Smoothed slope gain × BUSY, excluding amplitude and velocity | LEVELS output per voice. |
| Progress | `(index−1)/max(count−1,1)` | PROGRESS output; holds its final level after natural completion. |
| Events | GATE, CLOCK, EOC, BURST and BUSY per voice | Gate, Clock, EOC, Burst and Active per voice. |
| Shared outputs | ENV mix, reference CLOCK, logical-OR BUSY and focused voice's progress | Integrated shared outputs. |
| Amplitude | Live main amplitude × level trim | Live Level and independent trim. |
| Velocity | Independently enabled per voice | Independently enabled per voice. |
| CV modulation | Six shared destinations, with per-voice enable/depth | Reference modulation controls plus native NT parameter mappings. |
| Presets | 16 factory/user slots per voice | Two 16-slot banks saved with the host preset. |
| MIDI | Notes 36/37, channel 10; transport; internal clock output | Configurable notes/channels/routing, CC control, note and clock outputs. |

## Timing grid

Width and interval share these ratios, expressed in quarter notes of the
shared reference:

`1/16, 1/12, 1/8, 1/6, 1/4, 1/3, 1/2, 2/3, 1, 3/2, 2, 3, 4, 6, 8, 16`.

Glide adds Off before this grid. Interval also has Off, which disables
automatic advancement while leaving manual stepping available. Patch positions
are rounded to this grid. The Ratchet 40 preset's `0.01` interval setting
selects `1/16`, since it exceeds the `0.005` Off threshold.

## Conventions

In the patch, gain is `1 − 0.96 × abs(slope) × position`, where `position`
is progress for a positive slope and its complement for a negative slope.
A positive DROID slope therefore falls from 100% to 4%, opposite to the NT's
convention. Imported presets invert the sign; NT Slope remains positive for
rising levels. COUNT=1 always retains full gain.

The reference logarithmic profiles are `log10(1 + 9p)` and `log10(10 − 9p)`,
for phase `p` from 0 to 1. The falling exponential is `(1−p)^3`. Choosing
reference curves affects shapes, without changing the slope sign.

MUTE resets neither count nor phase. LEVELS excludes captured velocity,
amplitude, level trim and mute. PROGRESS holds its final level after a natural
end; stop clears the index. A single-burst sequence always has zero progress.

The patch emits a 5 ms CLOCK pulse on clock events, even while the generator
is idle; BURST is a 1 ms pulse at the actual start of an envelope. Stop does
not emit EOC. These events therefore serve different purposes.

## Reference modulation

Each voice has CV enable, CV depth, count depth, time depth, shape depth and
slope depth. Master depth and CV enable multiply destination depths. Width
and Interval share time depth. An unassigned input has no effect. Shared
`CV response=DROID reference` uses this behaviour.

Reference modulation blends a base value with a bounded CV target. This
differs from native NT additive mapping:

| Destination | Target at full depth |
| --- | --- |
| Count | `1 + (Count−1) × clamp(CV/5, 0, 1)`, then rounded. |
| Width and Interval | Position `clamp(CV/5, 0, 1)` on the grid. |
| Slope | `Slope × clamp(CV/5, −1, 1)`. |
| Shape | `min(floor(8 × clamp(CV/5, 0, 1)), 7)`, then the blend is rounded. |
| Amplitude | `Amplitude × clamp(CV/5, 0, 1)`. |

For Interval, blending acts on the normalised position before adding the
NT interface's Off-choice offset. The resulting position is Off at or below
`0.005`; otherwise its musical index is `round(15 × position)`. This preserves
the DROID thresholds, including at partial depth.

## AJH comparison and NT modulation options

The DROID patch is an adaptation. The [official AJH description](https://ajhsynth.com/MultiBurst.html)
highlights changes to width, rate and shape during a sequence, irregular
external clocks and their combination with the internal LFO. For the latter
behaviour, use NT `Absolute` and `Internal + CV`: both sources remain
independent. DROID-style Musical mode provides synchronised ratios on a grid.

The DROID path modulates towards a positive target for five destinations:
a −5…+5 V LFO spends half its period at the minimum target. Multiplicative
slope does not vary when its base setting is zero. These laws do not establish
the behaviour of the AJH's analogue CV inputs.

Two optional NT choices extend the reference:

| NT option | Behaviour |
| --- | --- |
| CV response=Bipolar additive | 0 V is neutral; signed changes affect Count, Slope, Shape and Amplitude. In Absolute, Width and Rate scale by `2^(4 × CV/5 × depth)`. In Musical, timing remains on the grid. |
| Shape response=Morph | Interpolates adjacent DROID profiles at equal phase, with a continuous CV-controlled position. |

These laws are NT extensions, not claims about DROID or AJH behaviour. Count
and Slope are still captured at the trigger; Width, Rate, Shape and Amplitude
remain active during the sequence. NT's Slope sign convention is unchanged.

## Factory banks

Both banks follow the patch's 16 sets of settings: Classic Sine, Positive Log,
Negative Log, Expo Percussion, Rising Ramp, Slow Triangle, Falling Ramp,
Ratchet, Sine Loop, Rise Glide, Fall Glide, Expo Loop, Binary/Dotted, Velocity
Triplets, 3:2 Polyrhythm and 7:5 Polymeter. G1 uses Ratchet 40 and Binary;
G2 uses Ratchet 32 and Dotted.

The 19 per-generator values are retained: count, shape, slope, amplitude,
retrigger, loop, mute, width, interval, glide up/down, CV enable, velocity
enable, CV depth, level trim and the four modulation depths. Six NT fields
also store Absolute timing values, timing mode and curve choice. The factory
sounds' Absolute values correspond to their grid values at 120 BPM, within
NT parameter limits.

## Comparison limits

DROID Auto detects a connected CV cable, then uses MIDI if its transport is
running, then the internal clock. The NT receives a bus and cannot directly
detect physical cable insertion; its Auto selection therefore uses incoming
activity. The patch retains a measured external period and limits its fraction
to `0.9999`; this is not a continuously predictive clock when the source stops.

DROID DB8E/B32/P2B8/E4 controllers, their LEDs, short-press recall and 1.5 s
hold-to-save are hardware choices. NT pages, parameters and buttons provide
their functions without reproducing that physical control chain. The INI
supports software comparison; it does not replace measurements on both devices.
