# Multi Burst NT

A dual burst envelope generator for the Expert Sleepers disting NT, inspired by
the AJH Synth Multi Burst Envelopes and the DROID implementation.

- Two independent generators with 1–40 bursts, eight envelope shapes and continuous shape morphing.
- Envelope level and slope, loop, mute, and separate rise/fall glide.
- Free timing or musical divisions, with internal, CV and MIDI clocks.
- Shared or independent trigger, stop, step and reset controls.
- CV modulation, including bipolar response; MIDI notes, envelope CC and clock output.
- Individual envelope, gate, activity, end-of-cycle and clock outputs, plus an envelope mix.
- 16 dual-generator presets and separate factory/user banks for each generator.

See the [user guide](docs/user-guide.md) and [preset guide](docs/preset-pack.md).

## Installation

Requires disting NT firmware **1.15 or later** (API 13).

1. Download **multi_burst_nt.zip** from the [latest NT release](https://github.com/kf4u43D/NT-Multiburst/releases/latest).
2. Extract it to the root of the MicroSD card. The file must be at `programs/plug-ins/multi_burst.o`.
3. Insert the card into the disting NT and restart it.
4. Add **Multi Burst NT** from the plug-in algorithm list.

The 16 presets are built in. Open **Preset pack**, select **Pack preset**, press
**Load pack preset**, then **Trigger all**.
