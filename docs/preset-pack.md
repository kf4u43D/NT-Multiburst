# Multi Burst NT: 16-scene preset pack

These scenes pair complementary generators for percussion, repetitions and
slow modulation. Twelve use free-running Absolute timing; four use Musical
ratios. They work without external CV. Hand Steps requires steps after its
first attack. The settings explore the NT's capabilities and are not a
measured reproduction of the AJH Multi Burst.

Select **Pack preset**, then activate **Load pack preset**. Loading sets both
voices and their clock, enables G2 and resets both sequences. It does not
trigger an envelope. Then press the right encoder or use **Trigger all** to
hear both voices; **Stop all** stops loops. G1 and G2 can also be triggered
and stopped separately.

CV/MIDI routing stays as configured in the current patch. To hear the pair,
route both Envelope outputs to their destinations, or route Envelope mix to
the chosen VCA. Assign G2 or mix if its route is still None. The pack does not
overwrite any of the 32 user slots in the DROID banks. To keep a variation,
use Save preset on each voice, then save the global NT preset, which also
stores the shared settings.

In the table, `N × duration / frequency` gives the envelope count, width and
burst-start rate. In Musical scenes, width and interval are expressed as
fractions of a quarter note. The numbers below run from 1 to 16; the internal
pack index runs from 0 to 15.

| No. / scene | G1 / G2 timing | Behaviour and controls | Optional modulation suggestion |
| --- | --- | --- | --- |
| 1 — Drum Pair | 1 × 140 ms / 1 × 70 ms | Falling expo percussion and a gentler rising log accent. Trigger all plays the pair once. | Slow CV or an envelope into Width CV; master depth 25%, Time 50%. Count depth=0 keeps one attack per voice. |
| 2 — Bead Cascade | 6 × 90 ms / 6 Hz; 5 × 75 ms / 4 Hz | Two short strings: a fading sine and a falling ramp with rising levels. Plays once per trigger. | Slow Count CV changes phrase lengths at the next trigger; Width CV separates or joins the beads. |
| 3 — Eight Ratchet | 8 × 20 ms / 24 Hz; 4 × 45 ms / 12 Hz | Eight fast pulses with four expo accents. Finishes in less than 0.35 s without CV. | Moderate Interval CV tightens or spreads the burst; Count depth=0 preserves the stated counts. |
| 4 — Flam Duo | 2 × 55 ms / 12.5 Hz; 1 × 220 ms | Two expo attacks 80 ms apart on G1, with a gentle triangle on G2. Trigger all plays the flam and its contour. | Width CV; Count depth=0 keeps the two-attack flam. Interval CV changes their spacing. |
| 5 — Velvet Crescendo | 8 × 100 ms / 5 Hz; 6 × 160 ms / 3.75 Hz | Two finite phrases with rising levels, using Log Up and Sine, with 70/120 ms rise glides. | Slow Slope CV is captured at the next trigger. Amplitude CV varies the level during the phrase. |
| 6 — Falling Sparks | 12 × 30 ms / 18 Hz; 6 × 95 ms / 7 Hz | Fast expo showers and more widely spaced Log Down accents; both voices fade. | Width CV or discrete Shape CV varies the tails. Repeated triggers restart the shower. |
| 7 — Crossing Levels | 8 × 220 ms / 2.4 Hz; 8 × 320 ms / 1.7 Hz | Loops with opposite slopes and 180/260 ms glides. Their approximately 3.14/4.44 s cycles drift apart naturally. Trigger all starts them; Stop all ends them. | Slow Slope CV or Amplitude CV brings each voice forward in turn. The drift is deterministic. |
| 8 — Slow Glide | 6 × 1.6 s / 0.60 Hz; 7 × 2.2 s / 0.47 Hz | Slow Triangle and Sine loops, with 1.4/1.8 s level smoothing. Allow 30 s to hear two cycles of each voice. | Very slow Width CV; Time depth=50% limits its range. Levels works well for a filter sweep. |
| 9 — Morph Breathing | 7 × 850 ms / 1.1 Hz; 9 × 550 ms / 1.7 Hz | Log Up and Triangle loops with 400/300 ms glides; both voices use Shape response=Morph. | A slow LFO into Shape CV reveals intermediate shapes. Without CV, the base shapes remain fixed. |
| 10 — Pulse And Sweep | 9 × 65 ms / 8 Hz; 5 × 950 ms / 1.2 Hz | A short pulse pattern beneath a slower rising sweep; two independent loops. | Width CV lengthens the pulses or ramps; set each voice's Time depth to suit its role. |
| 11 — Hand Steps | 4 × 180 ms / Rate Off; 4 × 300 ms / Rate Off | Trigger all plays the first envelope. Three Step all actions play the remaining three; gestures determine the rhythm. No automatic loop. | Play Step manually or route Step all input to a gate. Shape/Width CV changes contours without setting the rhythm. |
| 12 — Clock Weave | 13 × 95 ms / 3.3 Hz; 9 × 150 ms / 2.1 Hz | Morph loops using Internal + CV. The two internal rates work without an external clock; each CV edge adds a burst event. | An independent clock into Clock input adds attack placements. Shape CV is another option. No randomness is generated. |
| 13 — Triplet Weave | 108 BPM; 12 × 1/3 quarter note / 8 × 1/2 quarter note | Looping Expo and Sine: triplets against eighth notes, with an approximately four-quarter-note phrase on each voice without modulation. | Amplitude CV preserves the rhythmic pattern. Time CV moves ratios along the grid and changes it. |
| 14 — Seven Against Five | 96 BPM; 7 × 1 quarter note / 5 × 1 quarter note | Rising Triangle and fading Sine: seven- and five-beat loops, with a half-quarter-note glide on their levels. | Very slow Slope CV renews each phrase. Avoid Count/Time CV to keep the 7:5 relationship. |
| 15 — Dotted Lanterns | 120 BPM; 4 × 3/2 quarter note / 6 × 1 quarter note | Dotted Log Down accents against triangles with rising levels. Two six-quarter-note phrases, with 1/4- and 1/3-quarter-note glides. | Amplitude or Shape CV preserves both phrase lengths; Time CV changes them. |
| 16 — Long Tide | 60 BPM; 4 × 2 quarter notes / 3 × 3 quarter notes | Log Up and Ramp Down loops, with Morph and 1.5/2 s glides. Two tides of approximately eight and nine seconds. | Very slow Shape CV crossfades profiles; Time depth=50% keeps ratio movements smaller. |

## Setting modulation

The pack selects **CV response=Bipolar additive**: 0 V preserves the setting,
a negative voltage reduces it and a positive voltage increases it. CV inputs
can stay at None. The pack does not generate its own LFOs.

Master depth is usually 35%. Destination depths multiply it: for example,
Time depth=70% gives an effective depth of 24.5%. At ±5 V, this approaches
one octave of frequency or duration variation in Absolute. Percussion scenes
reduce some depths; Drum Pair, Eight Ratchet, Flam Duo and Hand Steps retain
their counts under Count CV. Amplitude uses master depth directly.

Count and Slope are captured at an accepted trigger, including the start of
a new loop cycle. Width, Interval/Rate, Shape and Amplitude can change during
the sequence. Morph only moves when Shape CV or a host mapping changes its
position.

The two voices' base levels total no more than 9 V. Modulation can increase
their sum; Envelope mix limits the plug-in contribution to 10 V. Use separate
Envelope outputs to keep each voice's dynamics independent. In Add mode,
other algorithms' contributions are added to that voltage.

The four Musical scenes use Internal BPM as their reference. To sync them to
an external source, select Musical clock=CV or MIDI and set its resolution.
Loading another pack scene restores the internal reference. In Clock Weave,
Clock input accepts an optional additional clock.
