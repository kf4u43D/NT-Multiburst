# Development

## Sources and dependencies

```powershell
git clone https://github.com/kf4u43D/NT-Multiburst.git
cd NT-Multiburst
```

The repository includes the plugin sources, tests, build scripts, presets and a
minimal official SDK snapshot: five public headers and the
[MIT license](../third_party/distingNT_API/LICENSE) in `third_party/distingNT_API`.
The snapshot is pinned to API 13, commit
`f440d20c2a96a9ca43892c7418a1d8185ad47eb5`. No submodule or additional download is
required to build the plugin or run native tests. Hardware firmware must be
1.15.0 or later; the validation target is 1.18.0.

`sdk.lock.json` records the upstream commit, API contract and SHA-256 of `api.h`
with LF line endings. The ARM build checks the API and hash, and also the commit
when the SDK directory is a full Git checkout. Object inspection checks host
imports against `api.h` and exact JSON method signatures against
`serialisation.h`.

Optional upstream repositories can be cloned beside the project for examples
and emulator development:

```powershell
git clone https://github.com/expertsleepersltd/distingNT_API.git ../distingNT_API-full
git -C ../distingNT_API-full checkout --detach f440d20c2a96a9ca43892c7418a1d8185ad47eb5
git clone https://github.com/thorinside/nt_emu.git ../nt_emu
git -C ../nt_emu checkout --detach ad2aa4b610f39c32903e3b7403cf2f2335508ca1
git clone https://github.com/thorinside/nt_mcp.git ../nt_mcp
git -C ../nt_mcp checkout --detach 3047bb232d9487fe075d48d12f48115383adf728
```

`nt_mcp` and its `disting-nt-cpp-plugin-writer` skill were development references,
not build dependencies. The DLL build connects to the installed emulator and
does not use the `nt_emu` source checkout. External manuals are not redistributed;
official links are in [research.md](research.md).

Published artifacts are in [`downloads/v0.4`](../downloads/v0.4/). `build` and
`dist` are generated local directories excluded from Git. The DROID reference
for factory sounds is `ref/droid-v2.3.1.ini`.

## Architecture

- `include/multi_burst/engine.h` and `src/engine.cpp`: host-independent engine.
- `src/multi_burst.cpp`: SDK metadata, parameters, buses, MIDI and display.
- `include/multi_burst/parameters.h`: persistent parameter indices and divisions.
- `include/multi_burst/presets.h`: DROID factory sounds and per-voice bank fields.
- `include/multi_burst/performance_presets.h`: 16 complete two-voice scenes.

Two engines share `MusicalClock`, with a separate `MusicalTiming` for each
voice's interval ratio. Absolute timing remains available. The
[reference comparison](reference-comparison.md) explains the preserved
conventions and Auto clock detection on NT buses.

The instance is constructed with placement new in host-provided DTC memory.
Audio processing performs no dynamic allocation. UI events use atomic flags;
MIDI callbacks and the engine run on the audio thread. Display data uses atomic
snapshots. All CV inputs for a sample are read before any outputs are written,
including when input and output buses overlap.

The 32 user slots use fixed atomic storage. Audio reads are bounded: a recall
keeps the current parameters if a concurrent write prevents a consistent read.
Non-audio serialization waits for a coherent snapshot. Versioned JSON stores
flat records of 27 numbers: voice, slot and 25 fields. This works with nt_emu
2.0.2; invalid documents cannot partially replace the banks.

Shapes use bounded polynomials and integer Q48 phases. Timing conversions and
divisions are recalculated when their inputs change. There is no CMSIS
dependency or library trigonometric/exponential call in processing. Optional
DROID logarithms use range reduction and a fixed-length series. Glide smooths
only the slope gain. Count, slope and velocity are captured at the trigger;
mute leaves events and progression running. EOC retains its width across loops.

`CvResponse` is a shared parameter. `Legacy / DROID / Morph` reuse the existing
curve field without changing bank records. Morph evaluates neighboring shapes
at the same phase and interpolates their outputs. Bipolar timing uses a bounded
approximation of `2^x` across ±4 octaves. Absolute Width and Rate CV preserve
fractional timing and phase; Musical timing keeps quantized ratios. Interval CV
is blended in normalized position before selecting Off.

`PackPreset` and `LoadPackPreset` occupy indices 167 and 168. Loading a scene
applies both voices and shared settings on the audio thread, resets sequences
without triggering them, and clears queued trigger, step and save events.
Routing, focus and user banks remain intact. Rack export reads the same C++
header; scene values are not duplicated in Python.

SDK MIDI clock callbacks have block-level timing without sample timestamps.
CV clock edges are detected per sample; outgoing MIDI also depends on firmware
queuing. Native timing results do not measure hardware jitter.

## Build and test on Windows

Requirements: Python 3, Arm GNU Toolchain (`arm-none-eabi-g++` and binutils),
and Clang++ with a Windows C++ runtime or MinGW. The validated toolchain uses
ARM GCC 14.3 and LLVM Clang 22.1.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test.ps1
clang-tidy -p build/native src/engine.cpp src/multi_burst.cpp
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
```

The test script runs engine and SDK integration suites and records compiler
arguments in `build/native/compile_commands.json`. `.clang-tidy` documents SDK
and bounded positive-rounding exceptions. The build script creates and inspects
the relocatable ARM object, then packages it for MicroSD. Warnings are errors.

Outputs:

- `build/native/engine_tests.exe` and `plugin_tests.exe`;
- `build/arm/object_report.json`: sections, symbols and hashes;
- `dist/programs/plug-ins/multi_burst.o`;
- `dist/multi_burst_nt.zip`, containing exactly that path;
- `dist/build_manifest.json`: tools and SHA-256 hashes.

The ARM target is Cortex-M7, Thumb, `fpv5-d16`, hard float, `-fPIC`, `-Os`, without
RTTI, exceptions or static initialization runtime. Two units are linked with
`-r -nostdlib`. Inspection requires ELF32 little-endian ARM REL EABI5, VFP ABI and
a global `pluginEntry`. Unexpected imports are rejected. `_GLOBAL_OFFSET_TABLE_`
is the GNU PIC linker pseudo-symbol; permitted C memory routines remain visible
in the report. The nine JSON methods are checked by exact demangled signature
against the pinned SDK, without permitting other C++ imports. The Windows bridge
resolves these methods explicitly from emulator exports.

## Publish an NT release

Run `python scripts/prepare-downloads.py`, set the version in
`.github/nt-release.json`, then push to `main`. Changing that configuration
triggers **Publish NT release**; it can also be run manually from GitHub Actions.

The workflow verifies the NT ZIP layout and the ZIP/object hashes against the
download manifest, then attaches `multi_burst_nt.zip`, `multi_burst.o` and
`SHA256SUMS.txt` to the release. Use a new version for a new release: an already
published release is left unchanged.

## Persistent contracts

Keep GUID `MbNt` and existing parameter indices. Append new parameters, then
arrange them through pages. The ARM object must continue to declare API 13
explicitly unless compatibility is deliberately changed. Updating the SDK alone
must not change the API advertised to the host.

Apply Add/Replace independently to each output bus. Handle None before `bus - 1`.
Use `kNT_lastBus` for the highest legal bus, currently 64. Reset wins over
simultaneous events. Outgoing MIDI notes keep their original destination until
Note Off, even if routing changes.

Independent curve choices and undocumented AJH behavior are described in
[research.md](research.md).

## Rack and hardware validation

The [Rack guide](../patches/README.md) covers the desktop patches.
`emu/multi_burst_compat.h` and `emu/nt_emu_bridge.cpp` adapt the same engine and
firmware adapter sources. With nt_emu 2.0.2, the DLL advertises API 8 and limits
routing to 28 buses. The ARM object advertises API 13 and supports 64 buses.

The bridge resolves Windows exports and forwards parameter writes and callbacks
to the correct instance. It is self-contained. nt_emu 2.0.2 reports 48 kHz
globally, so use that Rack sample rate. NT hardware MIDI destinations map to the
emulator's selected MIDI port.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-emu.ps1
python scripts/create-rack-patch.py
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-rack.ps1
```

Install VCV Rack 2, Fundamental, nt_emu and the matching Windows Rack SDK
separately. Supply `-RackSdkPath` or `RACK_SDK_PATH`. Otherwise the script searches
`Rack-SDK`, `third_party/Rack-SDK`, then `third_party/nt_emu/vcv-plugin/Rack-SDK`
under the project root. The SDK must contain `include/rack.hpp` and
`libRack.dll.a`. `-RackDirectory`, `-RackPluginsPath` and `-Compiler` override the
other installation paths.

Regenerate patches after moving or cloning the project so they store the local
DLL path. To export and verify all scenes:

```powershell
python scripts/create-rack-patch.py --motion
python scripts/export-preset-pack.py --library dist/nt_emu/multi_burst.dll
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-rack.ps1 -PresetPack
python scripts/prepare-downloads.py
```

The Rack test runs installed DSP libraries without opening Rack or replacing
its settings/autosave. Results are in `build/rack-smoke/result.txt` and
`rack-smoke.log`. Native tests exercise transitions, timing and routing through
a simulated API; ARM inspection checks format and dependencies; Rack tests
exercise a separate host implementation. Loading, physical UI, voltages,
external jitter, MIDI transport, CPU use and coexistence with other algorithms
still require a physical disting NT.

For each update, preserve persisted contracts, rebuild artifacts and update the
[validation report](validation.md).
