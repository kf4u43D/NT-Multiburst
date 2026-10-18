# Validation

Software verification completed on **18 October 2026**. The hardware build uses
API 13, pinned SDK commit `f440d20c2a96a9ca43892c7418a1d8185ad47eb5`.
Minimum firmware: 1.15.0. Validation target: 1.18.0.
Physical hardware testing is still pending.

| Check | Result |
| --- | --- |
| Native engine | Passed: sequence behavior, loop/EOC, mute/glide, curves, phase-continuous morphing and continuous timing modulation. |
| Musical clock | Passed: 16 ratios, triplets, two shared-clock timings, CV/MIDI/Auto, PPQN 1–48, transport and sample-rate changes. |
| SDK integration | 34 cases passed at 44.1, 48 and 96 kHz, covering modulation, transport, banks and all 16 pack scenes. |
| Factory presets | Independent comparison of 32 × 19 values against the DROID INI passed; slope sign follows the NT convention. |
| Persistence | 169 parameters; all 167 preceding indices unchanged. Banks retain their 25-field JSON records; scene loading preserves user slots. |
| Native build | Clang 22.1, C++17, warnings treated as errors. |
| Static analysis | clang-tidy reported no errors in either project source; external-header diagnostics filtered. |
| ARM build | Passed with Arm GNU GCC 14.3, Cortex-M7, Thumb, hard float and PIC. |
| ARM inspection | ELF32 little-endian ARM REL, EABI5, VFP arguments, global pluginEntry and explicit API 13 contract. |
| NT archive | Exactly one file, programs/plug-ins/multi_burst.o; content and hash verified. |
| Emulator | Windows DLL rebuilt; installed Rack/Fundamental/nt_emu DSP and JSON bank round-trip verified. |
| Physical disting NT | Pending: loading, interface, voltages, jitter, external MIDI and CPU use. |

The supplied ARM object is **61,600 bytes**, including ELF metadata and
relocations. Sections contain **18,728 bytes of code**, **6,190 bytes of
constants**, **3,444 bytes of initialized metadata**, and no static `.bss`.
Voice and bank state is allocated per instance by the host. These sizes do not
measure CPU use.

Object SHA-256:

`17448fc85a4fc62a9ff9182037c0dec1e3d6972dc9a2b33afbb78033daf6a9ec`

The verifier permits only pinned-SDK NT exports and the exact signatures of
nine JSON methods declared in `serialisation.h`. No additional 64-bit
conversion/division helper, libm, allocator or C++ runtime is imported.
`memcpy`, `memset` and the PIC pseudo-symbol remain visible. Header hashes are
included in the ARM report.

## Rack / nt_emu

Tests use the installed libRack, Fundamental 2.6.4 and nt_emu 2.0.2 libraries,
without a GUI, audio device or user-profile changes. The DLL advertises API 8
for this emulator and limits routing to 28 buses.

The reference patch restores all 169 parameters. The test checks eight 100 ms
envelopes, a crescendo from 0.320 to 7.999 V, a 5 ms EOC and silent reset.
CV and internal clock periods are 12,000 samples, within one sample; the
observed CV-chain delay is four samples. The sine/VCA path produces about
0.097 V RMS.

Two different user sounds are saved, replaced, reloaded through nt_emu's real
JSON parser, then recalled. Both banks recover their values. Flat records avoid
this emulator parser's nested-array limitation; the firmware uses the same
schema. External MIDI and the physical display are outside these tests.

The Motion patch also runs for 12 seconds through the installed libraries.
Three modulation inputs receive bipolar CV, both voices stay active and their
outputs remain within 0–10 V. G2 reaches 7 V. Clock edge intervals range from
290 to 17,776 samples, and high Gate segments from 1,830 to 46,720 samples.
Gate segments may contain several retriggered envelopes and do not measure
Width. This verifies timing variation without measuring an AJH module's sound.

The local Motion log is `build/rack-motion-smoke/result.txt`. The
[interactive patch](../patches/Multi_Burst_NT-motion.vcv) includes scopes and
leaves audio-device selection to the user.

## 16-scene preset pack

Plugin scenes and Rack exports use the same C++ header. Native tests play each
scene without external CV and check both voices, bounded mix, silent loading
and Stop. A separate case replaces voices with held MIDI notes: Note Off is
sent, queued triggers and saves are cleared, and routing and saved banks remain
unchanged.

All 16 exported patches were instantiated in the installed Rack libraries with
169-parameter restoration and port checks. Nine were also played: Drum Pair,
Crossing Levels, Slow Glide, Morph Breathing, Hand Steps, Triplet Weave, Seven
Against Five, Dotted Lanterns and Long Tide. Voice and mix outputs stay bounded,
the audio path is nonzero, loops continue, and Stop/Reset emit no EOC. Hand Steps
advances with three actual impulses from the third PULSES button after its first
attack.

Local logs are in `build/rack-pack-smoke/`. The extracted ZIP was checked for
16 VCV files, catalog, guide and sources, including matching manifest, header
and DLL SHA-256 hashes. The [catalog](preset-pack.md) describes each scene and
optional modulation.

Published reports: [ARM object inspection](../downloads/v0.4/object_report.json)
and [portable manifest](../downloads/v0.4/manifest.json). The full local build
manifest is `dist/build_manifest.json`; the reference Rack log is
`build/rack-smoke/result.txt`. Functional choices are documented in the
[reference comparison](reference-comparison.md).
