# Multi Burst NT user guide

Multi Burst NT generates two independent envelope sequences for VCAs, filters
and other CV destinations. Each generator is monophonic: a burst arriving before
the previous envelope ends restarts that envelope. Both generators can share a
musical clock and modulation inputs. G2 is disabled by default; enable it with
`G2 enabled`.

## Installation

Extract `multi_burst_nt.zip` to the root of the MicroSD card to obtain
`/programs/plug-ins/multi_burst.o`, then restart the NT. Check the scan under
`Misc / Plug-ins / View info…`. In `Add algorithm`, select `Multi Burst NT`,
load it, then add it. The plug-in uses API 13, requiring firmware **1.15.0** or
later. The target firmware is **1.18.0**; operation on hardware remains to be
verified.

These steps follow the [official 1.18 manual, pages 69–70](https://www.expert-sleepers.co.uk/downloads/manuals/disting_NT_user_manual_1.18.pdf).

## Getting started

Physical inputs 1, 2 and 3 receive trigger, clock and reset respectively.
The default internal rate is 4 Hz: a trigger starts eight 100 ms envelopes with
a maximum level of 8 V. Use the NT's usual controls to adjust parameters.
Press the left pot to trigger, the centre pot to reset, and the right pot to
advance the selected generator by one burst. Turn the left encoder to select
G1 or G2; press it to stop that generator. Press the right encoder to trigger
both enabled generators.

The `Trigger`, `Reset` and `Step` parameter actions are momentary: they return
to Off after activation. Step works with every clock source and bypasses the
CV divider.

The G1 and G2 pages control each voice. The Performance and Shared inputs pages
also provide Trigger/Reset/Stop/Step all. Stop cancels the sequence without EOC;
reset also clears its counters. Additional inputs and outputs default to None:
assign them to the required buses before use.

## Envelope

| Parameter | Range | Default | Effect |
| --- | --- | --- | --- |
| Bursts | 1–40 | 8 | Number of bursts, captured at the trigger. |
| Shape | 8 shapes | Sine | Shape of the current envelope. |
| Width | 20–2500 ms | 100 ms | Duration; changing it alters the speed of the envelope already in progress. |
| Slope | −100 to +100% | 0% | Sequence level slope, captured at the trigger. |
| Level | 0–10 V | 8 V | Maximum level, adjustable during playback. |
| Retrigger | Restart (RET), Ignore (ONE) | Restart | RET restarts the sequence; ONE ignores triggers until the final envelope has ended. |
| First burst | Immediate, On clock | Immediate | Starts an envelope immediately or arms the sequence for the next clock. |

Shape order: Sine, Log Down, Log Up, Expo Down, Ramp Up, Triangle, Ramp Down,
Pulse. Sine and Triangle rise and fall; ramps are linear, while the log/expo
shapes use separate curved profiles. Pulse stays high for Width. Shape and
level changes, as well as restarts, can introduce CV discontinuities.

At Slope +100%, levels rise from 4% to 100%; at −100%, they fall from 100% to
4%. Progression between bursts is linear. At 0%, every burst has the same
level. A single burst always uses the full level. MIDI trigger velocity
multiplies these levels.

Each generator also provides:

| Parameter | Effect |
| --- | --- |
| Loop | Restarts after the final envelope ends naturally. Stop/reset interrupts the current loop; another trigger can start it again. |
| Mute | Silences Envelope and Gate; counters, Busy, Levels, Progress, Clock and Burst continue. |
| MIDI velocity | Allows MIDI trigger velocity to scale amplitude; Off uses maximum velocity. |
| Level trim | Multiplies Level, from 0 to 100%. |
| Glide rise / fall | Smooths changes in slope gain between bursts, with separate rise and fall times. The first level is immediate. |
| Shape response | Legacy uses the original profiles; DROID uses the reference patch's logarithmic profiles; Morph adds continuous CV crossfading between adjacent shapes. |

Glide affects successive levels and the Levels output, rather than the time
profile of each envelope. Level changes remain immediate. Positive NT Slope
means rising levels; imported DROID presets convert the sign automatically.

## Musical timing and two generators

Each timing page has an `Absolute` / `Musical` choice. Absolute uses Width in
milliseconds, Rate, divisions and glide times in milliseconds. Musical uses a
shared reference selected by `Musical clock`: Internal BPM, CV, MIDI or Auto.
Tempo and CV clock PPQN describe this shared reference; each voice keeps its
own width and interval ratios.

The Width ratio and Interval ratio grid, in reference quarter notes, is:

`1/16, 1/12, 1/8, 1/6, 1/4, 1/3, 1/2, 2/3, 1, 3/2, 2, 3, 4, 6, 8, 16`.

Interval ratio also has Off: only manual/external steps then advance the
sequence. Glide rise/fall ratio adds Off to the same grid. At 120 BPM, a ratio
of 1 is 500 ms, 1/3 is about 167 ms, and 3/2 is 750 ms. Width may exceed the
interval; the next burst then restarts that voice's envelope.

The external period is measured to place subdivisions between clock edges.
Auto prioritises recent CV clock activity, then recent MIDI clock activity
with an active transport, then the internal clock. It uses bus activity
because the NT does not expose DROID-style cable detection to the plug-in.
The ratio grid supports triplets and 3:2 relationships as well as binary
divisions.

If an explicitly selected external source stops, the last measured period
continues to determine the envelopes and glides already in progress.
Automatic advancement waits for the clock to resume. In Auto, switching
sources may change that period.

## Generator presets

Each voice has 16 independent slots on its presets page: select Preset, then
Load preset to recall it, Save preset to store it, or Factory preset to restore
that slot's factory settings. These actions are momentary. The user bank is
stored in the NT's global preset; save that preset to retain edits after
recalling it.

The factory sounds follow the two DROID reference banks: sine, logarithmic,
percussion, ramp, triangle, ratchet, loop, glide, triplet, 3:2 and 7:5 settings.
The eighth slot is Ratchet 40 on G1 and Ratchet 32 on G2. Factory sounds use
Musical timing and Shape response=DROID. A user slot also stores Absolute
timing and its values when that mode is selected. Shared tempo, routing and
CV response belong to the global NT preset. Shape response, including Morph,
is stored per voice.

## Pack of 16 scenes

The **Preset pack** page provides 16 complete G1/G2 scenes, separate from the
user banks: select **Pack preset**, then **Load pack preset**. Loading stops
both sequences without triggering them; use **Trigger all** or the shared
trigger button to play the scene. **Stop all** interrupts loops. Hand Steps
waits for Step commands after its first burst.

The pack sets both sounds, their enabled states, rates and shared clock/CV
parameters. It preserves CV/MIDI routing, focus and all user memories. To hear
both voices, assign G2 Envelope output or Envelope mix output to an available
output. Existing active modulation routes can continue to alter the scene's
base values.

See the [16-scene catalogue](preset-pack.md) for timings, behaviour and
modulation suggestions. The supplied Rack patches let you hear the mix of
both voices directly, with modulation CV initially disabled.

## Absolute clocks and reset

These settings apply to Absolute mode. For Musical, use the ratios and shared
reference described above.

| Parameter | Range | Default |
| --- | --- | --- |
| Clock source | Internal rate, Internal BPM, CV, MIDI, Internal + CV | Internal rate |
| Rate | Off or 0.01–51 Hz | 4 Hz |
| Tempo | 20–300 BPM | 120 BPM |
| Burst division | 1/32, 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars | 1/16 |
| CV divider | 1–64 edges per burst | 1 |
| CV clock PPQN | 1, 2, 4, 8, 12, 24, 48 | 1 |
| Pulse width | 1–20 ms | 5 ms |

In **Internal rate**, Rate directly sets the burst frequency; MIDI Clock output
treats each period as a quarter note. Rate Off stops this clock. The range below
0.24 Hz extends beyond the AJH module's range. In **Internal BPM**, Tempo sets
quarter notes and Burst division sets bursts. Bar divisions assume four quarter
notes per bar.

In **CV**, every edge advances the CV divider; a burst event occurs every
CV divider edges. Burst division does not multiply CV clocks. CV clock PPQN
specifies the source resolution used to reconstruct MIDI Clock output from
two measured edges. Reconstruction stops after a gap longer than the greater
of 2 seconds or three measured periods. Two new edges are needed to estimate
the tempo again.

In **MIDI**, F8 clock arrives at 24 pulses per quarter note; Burst division
selects 3, 6, 12, 24, 48, 96 or 192 F8 messages per burst. Start (FA) resets and
starts a sequence; Continue (FB) resumes the clock after Stop. Stop (FC)
cancels the sequence and notes; Continue does not restore that sequence.
System Reset (FF) resets the generator. Channel filtering applies to notes
and CCs; real-time messages are system messages.

In **Internal + CV**, both sources advance the same sequence. The internal
clock remains independent. Two clocks on the same sample are merged; separate
events are retained. MIDI Clock output follows the internal clock in this mode.

A **reset** clears the sequence, counters and phases; it takes priority over
a trigger or clock on the same sample. It releases the plug-in's MIDI notes
and does not produce EOC.

## CV routing

Select buses 1–64, or None (0). Inputs 1–12 are physical; buses 13–20 correspond
to physical outputs 1–8; later buses are auxiliary. Rising edges are detected
at 1 V or above, with rearming at 0.2 V or below. Holding an input high does
not retrigger. Changing a route to an already high signal requires a new edge.

| Output | Default bus | Jack | Signal |
| --- | --- | --- | --- |
| Envelope | 13 | Output 1 | 0 to Level volts, including slope and velocity. |
| Gate | 14 | Output 2 | 5 V throughout each envelope. |
| Active | 15 | Output 3 | 5 V while the sequence is armed or playing, including the final envelope. |
| EOC | 16 | Output 4 | 5 V pulse after the final envelope ends naturally. |
| Clock | 17 | Output 5 | 5 V pulses at the burst-event rate, even without an active sequence. |

Pulse width sets Clock and EOC pulse duration; Gate follows Width. Choose
separate output buses. **Add**, the default, adds the contribution to that of
preceding algorithms; **Replace** replaces it. The 0–10 V limit applies to the
plug-in's contribution: an Add sum can exceed this range.

G2 provides the same outputs. Each voice also has Levels, Progress and Burst.
These buses, and the shared output buses, initially use None.

| Additional output | Signal |
| --- | --- |
| Levels | 0–10 V representing smoothed slope gain while Busy, excluding Level, trim, velocity and mute. |
| Progress | 0–10 V progress from the first to the last burst; holds the final level after a natural end and returns to zero on stop/reset. A single burst produces 0 V. |
| Burst | 1 ms, 5 V pulse at the actual start of each envelope, including when muted. |
| ENV mix | Sum of both enabled voices' envelopes, with the plug-in contribution limited to 10 V. |
| Busy all | 10 V while at least one enabled generator is busy. |
| Reference clock | 5 V pulses at the shared quarter-note rate; duration follows Pulse width. |
| Focus progress | Progress of the generator selected on screen. |

Per-generator Stop and Step inputs complement Trigger/Reset. Shared
Trigger/Reset/Stop/Step all inputs act on enabled voices.

## CV modulation

Shared inputs provides Count, Width, Interval, Slope, Shape and Amplitude CV
buses. Both generators share these inputs, with separate CV enable, CV depth
and Count/Time/Shape/Slope depths per voice. Time depth affects both Width and
Interval. An input set to None has no effect. Shared **CV response** selects
DROID reference or Bipolar additive.

In **DROID reference**, a 0–5 V signal selects a width/interval position or a
shape. Count varies from 1 to the set count; Amplitude varies from zero to the
set level. Slope uses −5 to +5 V to multiply, invert or cancel the base slope.
Depth blends the base setting with that target, then discrete selections are
rounded. A bipolar LFO's negative half is therefore clamped to zero for
destinations other than Slope. At Slope=0, multiplicative modulation remains
zero.

In **Bipolar additive**, 0 V preserves the setting, negative voltages reduce it
and positive voltages increase it. Input voltage is limited to −5…+5 V. Depth
scales the following ranges:

| Destination | Range at ±5 V, 100% depth |
| --- | --- |
| Count | ±39 bursts, limited to 1–40 and rounded. |
| Slope | ±100 percentage points, limited to −100…+100%. |
| Amplitude | ±10 V, limited to 0–10 V before Level trim. |
| Shape | ±7 positions, limited to the eight shapes. |
| Width / Interval, Absolute | Duration / frequency multiplied by 1/16 to 16; Width stays within 20–2500 ms and Rate at or below 51 Hz. Rate Off stays Off. |
| Width / Interval, Musical | Movement along the ratio grid, with musical rounding and Off available for the interval. |

For example, Time depth=25% gives a one-octave range: at −5 V, Width and Rate
are halved; at +5 V, they double. Their input voltages can differ because
Width and Interval have separate inputs. Count and Slope are captured at the
trigger in both modes. Width, Rate, Shape and Amplitude act during the sequence.

**Shape response=Morph** retains a fractional Shape CV position and crossfades
between adjacent profiles at the same phase. It produces intermediate envelopes
without selection jumps. Integer positions match the DROID shapes. This is an
NT extension; the DROID patch and [AJH module](https://ajhsynth.com/MultiBurst.html)
select eight discrete shapes. Bipolar additive is also an NT option, rather
than a claim about the AJH's modulation law.

For a first listen, open the [Motion patch](../patches/README.md): two loops,
three slow LFOs, Absolute timing, Internal + CV clocks and Morph. For less
regular phrases with an external clock, use Absolute and Internal + CV:
each external edge adds a burst to the independent internal clock. To modulate
one voice with the other on hardware, send Progress G1 and Levels G2 to two
physical outputs, then patch them into the inputs chosen for Width CV and
Amplitude CV. An output bus in the same instance does not itself provide an
internal feedback loop. Start with 20–30% depth and CV response=DROID reference
for these positive signals. Half attenuation brings their 0–10 V range down
to 0–5 V to use the full modulation range.

Native parameter mappings are also available. In the NT mappings, choose the
parameter, source bus, Volts and Delta. CV adds `CV × Delta / Volts` to the base
value. For 0–5 V shape selection, set Shape to Sine, then map Shape with Volts=5
and Delta=7. For Bursts, use base=1, Volts=5, Delta=39. For Width, use base=20 ms,
Volts=5, Delta=2480 ms. Modulation goes through the host's parameter processing;
trigger/clock/reset edges are observed on every sample.

Mappings are saved with the preset. See the
[official 1.18 manual, pages 45–48](https://www.expert-sleepers.co.uk/downloads/manuals/disting_NT_user_manual_1.18.pdf).

## MIDI control

MIDI input channel defaults to **1**. **Off** disables the plug-in's own
notes/CCs; **Omni** accepts all sixteen channels. Default notes are 60 (trigger),
61 (reset), 62 (step) and 63 (stop) for G1, and 64, 65, 66 and 67 for G2.
Each voice has its own channel and notes, configurable from 0 to 127.
Zero-velocity Note On and Note Off do not trigger; the sequence continues
after releasing the trigger note.

| G1 CC | G2 CC | Destination | Values 0–127 |
| --- | --- | --- | --- |
| 20 | 30 | Bursts | 1–40 |
| 21 | 31 | Shape | Sine to Pulse |
| 22 | 32 | Width | 20–2500 ms in Absolute |
| 23 | 33 | Rate | 0=Off; 1–127=0.24–51 Hz in Absolute |
| 24 | 34 | Slope | −100 to +100% |
| 25 | 35 | Level | 0–10 V |
| 26 | — | Shared Tempo | 20–300 BPM |
| 115 | 110 | Trigger | Value ≥64 |
| 116 | 111 | Reset | Value ≥64 |
| 117 | 112 | Step | Value ≥64 |
| 118 | 113 | Stop | Value ≥64 |
| 120, 123 | 120, 123 | Cancel/reset | Any value on a channel accepted by the voice |

Native NT MIDI mappings can assign parameters to other CCs. Avoid assigning
the same CCs to other plug-in parameters at the same time. The SDK's MIDI input
callback does not identify the originating port: avoid a loop between the
outgoing MIDI destination and the incoming source.

## MIDI outputs

| Parameter | Range | Default |
| --- | --- | --- |
| MIDI destination | Off, Breakout, Select Bus, USB and combinations | Off |
| MIDI output channel | 1–16 | 1 |
| MIDI output note | 0–127 | 60 |
| MIDI notes | Off/On | Off |
| Envelope MIDI CC | Off or 0–119 | Off |
| MIDI CC rate | 1–100 Hz | 50 Hz |
| MIDI clock output | Off/On | Off |
| MIDI transport out | Off/On | Off |

Each burst with a nonzero level emits Note On, followed by Note Off when its
envelope ends or before the next burst. Velocity represents the peak level,
mapping 0–10 V to 0–127, with a minimum of 1 for Note On. A zero peak level
does not emit a note. Changing note, channel or destination, or disabling
notes, releases the note on its previous route. Mute also silences that voice's
notes. G2 has its own note and envelope CC routing; its defaults are channel 2
and note 64.

Envelope CC also maps 0–10 V to 0–127. Only changes are sent, at the selected
maximum rate. The final return to zero is sent without waiting for the next
interval. CV retains a higher resolution and update rate.

MIDI clock output and MIDI transport out use G1's MIDI destination. Clock sends
F8 at 24 PPQN: internally generated, reconstructed from CV or forwarded from
MIDI, depending on the source. When any enabled voice uses Musical timing,
output follows the shared reference; otherwise it follows G1's Absolute clock.

Transport out sends Start on the first burst and Stop when both sequences
have stopped or finished. Clock can continue between sequences. A RET retrigger
sends Stop then Start if the other voice is not busy. If Stop and Start arrive
before the same audio block, the last message determines transport state;
System Reset retains priority.

## Hardware verification

Software checks have passed; physical hardware validation remains pending.
On the first hardware test, check plug-in scanning, output voltages, ONE/RET,
Loop/Mute/Glide, simultaneous resets, Absolute and Musical clocks, the user
bank and Note Off messages. The [DROID comparison](reference-comparison.md)
explains adapted conventions and fidelity limits. Hardware measurements are
still needed for a precise comparison of the curves.
