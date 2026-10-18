# Multi Burst NT / nt_emu patches

Open **Multi_Burst_NT-nt_emu.vcv** in VCV Rack 2 with Core, Fundamental and
nt_emu installed. The patch loads the project's Windows library,
`dist/nt_emu/multi_burst.dll`. Set **Engine → Sample rate to 48 kHz**, matching
the rate reported by nt_emu 2.0.2.

**Multi_Burst_NT-motion.vcv** demonstrates two looping generators, three slow
bipolar LFOs and shape morphing. The reference patch provides a simpler
single-voice test with five outputs.

## 16-scene pack

The [preset ZIP](../downloads/v0.4/multi_burst_motion_presets.zip) contains 16
patches, a JSON settings catalog, a guide and source settings. Scenes include
short envelopes, ratchets, level patterns, slow loops and musical ratios.
See the [scene guide](../docs/preset-pack.md).

Each patch plays both voices: ENV MIX on NT output 8 controls a sine-wave VCA.
PULSES 1 starts, PULSES 2 resets, and PULSES 3 advances one burst. The third
button is useful for Hand Steps, whose internal clock is stopped. Slow LFOs
are connected, but their CV inputs start at None so the base sound plays first.
Enable Width CV input=4, Interval CV input=5 or Shape CV input=6 to add motion.

An external 2.7 Hz clock is available on input 2. In Absolute scenes, set
Clock input=2 and Clock source=Internal + CV to combine it with the internal
rate. Clock input=None leaves each scene at its own timing.

On the physical NT, select **Preset pack → Pack preset → Load pack preset**.
This loads both voices and shared settings while preserving routing and both
DROID user banks.

## Motion demo

1. Open **Multi_Burst_NT-motion.vcv** and press **PULSES 2**.
2. Press **PULSES 1** to start both voices. G1 plays eight bursts and G2 thirteen;
   both loop with different slopes and durations.
3. Select an audio driver/device in AUDIO 2 to hear G1 controlling a sine-wave
   VCA. Initial volume is 15%.
4. Watch the changing shapes, spacing and width. PULSES 2 stops both sequences;
   PULSES 1 restarts them.

Absolute mode uses continuous timing. The external 2.7 Hz clock combines with
variable internal rates, initially 4 Hz and 1.6 Hz. CV response=Bipolar additive
preserves the base setting at 0 V; Shape response=Morph blends profiles without
stepped selection. The three modulation LFOs are free-running and continue after
sequence reset.

| NT input | Source | Destination | G1 / G2 depth |
| --- | --- | --- | --- |
| 1 | PULSES 1 | Trigger all | — |
| 2 | 2.7 Hz LFO clock | Both voice clocks | — |
| 3 | PULSES 2 | Reset all | — |
| 4 | 0.125 Hz bipolar triangle | Width | 35% / 20% |
| 5 | 0.088 Hz bipolar sine | Interval / Rate | 35% / 20% |
| 6 | 0.051 Hz bipolar triangle | Shape | 35% / 25% |

The first three scopes show G1 ENV/Gate, Active/EOC and Clock/external clock.
Two more show G2 ENV/Shape CV and Width CV/Interval CV. G2 ENV uses NT output 6.

To compare responses, change G1 Shape response from Morph to DROID for stepped
shapes. Set Time CV depth to zero to stabilize durations and internal rates.
CV response=DROID reference uses positive targets, with negative voltages held
at the lower bound. See the [user guide](../docs/user-guide.md).

## Reference patch

1. Press **PULSES 2** to reset the plugin and LFO.
2. Press **PULSES 1** to trigger a sequence.
3. Watch eight envelopes rising from 4% to 100% of 8 V.
4. Select an audio driver/device in AUDIO 2 to listen. The right-hand VCA sets
   volume, initially 15%.

Initial settings: eight bursts, 100 ms width, +100% slope and a 4 Hz CV clock.
The sequence stops after the last envelope; clock pulses continue between
sequences.

| Scope | X | Y |
| --- | --- | --- |
| 1 | Envelope, NT output 1 | Gate, NT output 2 |
| 2 | Active, NT output 3 | EOC, NT output 4 |
| 3 | Clock, NT output 5 | External LFO clock |

The second row provides a roughly 130.8 Hz sine wave, envelope VCA, volume VCA
and AUDIO 2. Both audio channels receive the same signal. No audio device is
preselected.

Change Shape, Width and Level during playback. Slope and Bursts are captured
at the trigger, so edits apply to the next sequence. Slope 0 gives equal levels;
−100% gives a falling sequence. ONE ignores retriggers; RET restarts.

Clock source=Internal rate uses Rate. Internal BPM uses Tempo and Burst division.
Internal + CV combines Rate and the LFO. Rate=0 stops the internal clock.
Press the NT's right encoder or select Step in the menu for manual advancement.
PULSES 2 must clear Envelope, Gate and Active without EOC; only natural
completion produces EOC.

For MIDI, select input/output devices in nt_emu's menu. On channel 1, notes
60/61/62 trigger/reset/step, and CC 20–26 control Bursts, Shape, Width, Rate,
Slope, Level and Tempo. The patch enables burst notes, envelope CC 11, clock
and transport output, without selecting devices. For incoming MIDI Clock,
choose Clock source=MIDI and send Start plus F8. Avoid routing MIDI output back
into the plugin's input.

## Rebuild or relocate

The ARM `.o` is for the physical NT; Rack uses the Windows `.dll`. Patches store
an absolute DLL path. After moving the project, rebuild and regenerate them:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-emu.ps1
python scripts/create-rack-patch.py
python scripts/create-rack-patch.py --motion
python scripts/export-preset-pack.py
```

Routes stay within nt_emu 2.0.2's 28-bus range. The desktop adapter preserves
the plugin DSP while adapting its host contract; the DLL cannot load on the
physical NT.

Pack export compiles a small C++17 program using
`include/multi_burst/performance_presets.h`, so scene values are not duplicated
in Python. Native clang++ or g++ is required. Export produces 16 `.vcv` files,
settings catalog, guide, source settings and a SHA-256 manifest. Regenerate
after moving or rebuilding the DLL.

## Verification

Tests run the installed Rack, Fundamental and nt_emu DSP libraries. They check
parameter restoration, five reference outputs, eight 100 ms envelopes,
crescendo, 5 ms EOC, CV reset and CV/internal clocks. The sine/VCA path produces
audio. User-bank testing saves two sounds, replaces them, reloads them through
nt_emu's real JSON parser, and recalls the original values.

The 12-second Motion test checks three bipolar CVs, both voices, varying Gate
segments and irregular clock intervals. No external audio or MIDI devices were
activated. GUI operation and external MIDI remain interactive checks. See the
[validation report](../docs/validation.md).

The reference patch starts with G2 disabled. Enable G2, assign its output buses
and load a preset to explore two-voice behavior. The
[user guide](../docs/user-guide.md) covers musical timing, loops and glide.

Format and port references: [VCV Rack](https://github.com/VCVRack/Rack/blob/v2/src/patch.cpp),
[VCV Fundamental](https://github.com/VCVRack/Fundamental/tree/v2/src),
[nt_emu](https://github.com/thorinside/nt_emu/tree/ad2aa4b610f39c32903e3b7403cf2f2335508ca1/vcv-plugin/src).
