"""Export the C++ performance pack as Rack patches, JSON and a ZIP archive."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "include/multi_burst/performance_presets.h"
PRESETS = ROOT / "include/multi_burst/presets.h"
PACK_ID = "multi-burst-motion-16"

# Host bindings describe the serialization contract, not the sounds. All
# numerical sound values are obtained by evaluating performancePreset() in C++.
VOICE_PARAMETERS = {
    "bursts": "Bursts", "shape": "EnvelopeShape", "slope": "Slope",
    "level": "Level", "retrigger": "RetriggerMode", "loop": "Loop",
    "mute": "Mute", "widthRatio": "WidthRatio", "intervalRatio": "IntervalRatio",
    "glideUp": "GlideRiseRatio", "glideDown": "GlideFallRatio",
    "cvEnabled": "CvEnabled", "velocityEnabled": "VelocityEnabled",
    "cvDepth": "CvDepth", "levelTrim": "LevelTrim", "countDepth": "CountCvDepth",
    "timeDepth": "TimeCvDepth", "shapeDepth": "ShapeCvDepth",
    "slopeDepth": "SlopeCvDepth", "widthMilliseconds": "Width",
    "rateCentihz": "Rate", "timingMode": "TimingMode",
    "referenceCurves": "ReferenceCurves", "glideRiseMs": "GlideRise",
    "glideFallMs": "GlideFall",
}
SHARED_PARAMETERS = {
    "clockSource": "ClockSource", "sharedClockSource": "SharedClockSource",
    "tempo": "Tempo", "cvResponse": "CvResponse",
    "generator2Enabled": "Generator2Enabled", "cvPpqn": "CvPpqn",
    "pulseWidth": "PulseWidth",
}
ARRAY_PARAMETERS = {
    "division": "Division", "cvDivider": "CvDivider", "firstBurst": "FirstBurst",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def preset_fields() -> list[str]:
    declaration = re.search(r"struct Preset\s*\{(.*?)\n\};", PRESETS.read_text(encoding="utf-8"), re.S)
    if declaration is None:
        raise ValueError("Cannot find the voice Preset contract")
    fields = re.findall(r"int16_t\s+(\w+)\s*=", declaration.group(1))
    if len(fields) != 25 or set(fields) != set(VOICE_PARAMETERS):
        raise ValueError("The 25-value voice serialization contract changed; update bindings")
    return fields


def dump_source(fields: list[str]) -> tuple[str, list[str]]:
    shared = list(SHARED_PARAMETERS)
    shared.extend(f"{name}[{voice}]" for name in ARRAY_PARAMETERS for voice in range(2))
    lines = [
        '#include <cstdio>',
        '#include "multi_burst/performance_presets.h"',
        'int main() {',
        '  using namespace multi_burst;',
        '  for (unsigned slot = 0; slot < performancePresetCount; ++slot) {',
        '    const auto scene = performancePreset(slot);',
        '    std::printf("%u\\t%s", slot, performancePresetNames[slot]);',
    ]
    lines.extend(f'    std::printf("\\t%d", static_cast<int>(scene.{name}));' for name in shared)
    lines.append('    for (unsigned voice = 0; voice < 2; ++voice) {')
    lines.extend(f'      std::printf("\\t%d", static_cast<int>(scene.voices[voice].{name}));' for name in fields)
    lines.extend(['    }', '    std::puts("");', '  }', '}', ''])
    return "\n".join(lines), shared


def compiler_candidates(requested: str | None) -> list[str]:
    if requested:
        candidate = shutil.which(requested) or requested
        if not Path(candidate).is_file():
            raise ValueError(f"Compiler not found: {requested}")
        return [candidate]
    candidates = [os.environ.get("CXX"), shutil.which("clang++"), shutil.which("g++")]
    if os.name == "nt":
        candidates.extend([
            r"C:\Program Files\LLVM\bin\clang++.exe",
            r"C:\msys64\ucrt64\bin\g++.exe", r"C:\msys64\mingw64\bin\g++.exe",
            r"C:\mingw64\bin\g++.exe",
        ])
    return list(dict.fromkeys(str(Path(value).resolve()) for value in candidates
                              if value and Path(value).is_file()))


def read_cpp_pack(build: Path, compiler: str | None) -> tuple[list[dict], str]:
    if not SOURCE.is_file():
        raise ValueError(f"Missing performance pack source: {SOURCE}")
    fields = preset_fields()
    source, shared_fields = dump_source(fields)
    build.mkdir(parents=True, exist_ok=True)
    source_path = build / "export_presets.cpp"
    executable = build / ("export_presets.exe" if os.name == "nt" else "export_presets")
    source_path.write_text(source, encoding="utf-8")
    failures = []
    for candidate in compiler_candidates(compiler):
        environment = dict(os.environ)
        environment["PATH"] = str(Path(candidate).parent) + os.pathsep + environment.get("PATH", "")
        command = [candidate, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                   "-I", str(ROOT / "include"), str(source_path), "-o", str(executable)]
        compilation = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                                     errors="replace", env=environment, cwd=build, timeout=60)
        if compilation.returncode:
            failures.append(f"{candidate}:\n{compilation.stderr.strip()}")
            continue
        run = subprocess.run([str(executable)], capture_output=True, text=True,
                             encoding="utf-8", errors="strict", env=environment,
                             cwd=build, timeout=30)
        if run.returncode:
            failures.append(f"{candidate}: compiled helper failed ({run.returncode})")
            continue
        scenes = []
        for line in run.stdout.splitlines():
            columns = line.split("\t")
            if len(columns) != 2 + len(shared_fields) + len(fields) * 2:
                raise ValueError("Unexpected C++ pack export row")
            slot = int(columns[0])
            numeric = [int(value) for value in columns[2:]]
            shared = {name: numeric[index] for index, name in enumerate(shared_fields)}
            voices = []
            for voice in range(2):
                offset = len(shared_fields) + voice * len(fields)
                voices.append(dict(zip(fields, numeric[offset:offset + len(fields)])))
            scenes.append({"slot": slot, "number": slot + 1, "name": columns[1],
                           "shared": shared, "voices": voices})
        if len(scenes) != 16 or [scene["slot"] for scene in scenes] != list(range(16)):
            raise ValueError("Expected the 16 consecutive performance pack slots")
        return scenes, candidate
    raise ValueError("No working C++17 compiler/runtime found:\n" + "\n".join(failures))


def load_patch_generator():
    spec = importlib.util.spec_from_file_location("multi_burst_rack_patch", ROOT / "scripts/create-rack-patch.py")
    if spec is None or spec.loader is None:
        raise ValueError("Cannot load the Rack patch generator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def make_patch(generator, library: Path, scene: dict) -> tuple[dict, dict]:
    patch = generator.create_patch(library, motion=True)
    names = generator.parameter_names()
    nt = next(module for module in patch["modules"] if module["plugin"] == "nt_emu")
    values = nt["data"]["parameterValues"]["routingMatrix"][:len(names)]
    assignments = {
        "ClockInput": 0, "TriggerInput": 0, "ResetInput": 0,
        "G2TriggerInput": 0, "G2ResetInput": 0, "TriggerAllInput": 1,
        "ResetAllInput": 3, "StepAllInput": 7,
        "WidthCvInput": 0, "IntervalCvInput": 0, "ShapeCvInput": 0,
        "G2EnvelopeOutput": 18, "G2EnvelopeOutputMode": 1,
        "MixOutput": 20, "MixOutputMode": 1,
        "PackPreset": scene["slot"], "LoadPackPreset": 0,
    }
    for field, parameter in SHARED_PARAMETERS.items():
        assignments[parameter] = scene["shared"][field]
    for voice in range(2):
        prefix = "G2" if voice else ""
        for field, parameter in VOICE_PARAMETERS.items():
            value = scene["voices"][voice][field]
            assignments[prefix + parameter] = value + 1 if field == "intervalRatio" else value
        for field, parameter in ARRAY_PARAMETERS.items():
            assignments[prefix + parameter] = scene["shared"][f"{field}[{voice}]"]
    for name, value in assignments.items():
        values[names.index(name)] = value
    nt["data"]["routing"] = [{"param": index, "bus": value} for index, value in enumerate(values)]
    nt["data"]["parameterValues"]["routingMatrix"] = values + [0] * (256 - len(values))
    nt["data"]["presetPackSlot"] = scene["slot"]
    # The mix auditions both voices through the existing VCA and first scope.
    for cable in patch["cables"]:
        if cable["outputModuleId"] == 4 and cable["outputId"] == 0 and cable["inputId"] == 0:
            if cable["inputModuleId"] in (5, 9):
                cable["outputId"] = 7
    patch["cables"].append({"id": max(cable["id"] for cable in patch["cables"]) + 1,
                            "outputModuleId": 3, "outputId": 2,
                            "inputModuleId": 4, "inputId": 6, "color": "#55a6dc"})
    modules = {module["id"]: module for module in patch["modules"]}
    modules[1]["data"]["text"] = (
        f"MULTI BURST NT\nPACK {scene['number']:02d} / 16\n{scene['name']}\n\n"
        "Engine : 48 kHz\n\nPULSES :\n1 = TRIGGER ALL\n2 = RESET ALL\n3 = STEP ALL\n\n"
        "SCOPES 1-3 :\nMIX / G1 GATE\nG1 BUSY / EOC\nG1 CLOCK / LFO\n\n"
        "CV de modulation\ndesactives au depart\n\nClock CV : None")
    modules[12]["data"]["text"] = (
        "ECOUTE DES 2 VOIX\nENV MIX -> VCA\n-> volume -> AUDIO\n\nChoisir driver\net sortie audio.\nVolume : 15 %.\n\n"
        "SCOPES 4-5 :\nG2 ENV / Shape CV\nWidth / Interval CV\n\n"
        "POUR MODULER :\nWidth CV input = 4\nInterval CV input = 5\nShape CV input = 6\n\n"
        "CLOCK EXTERNE :\nClock input = 2\nClock source =\nInternal + CV\nLFO externe 2.7 Hz\n\n"
        "PULSES 1 : lancer\nPULSES 2 : arreter\nPULSES 3 : avancer")
    return patch, dict(zip(names, values))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, default=ROOT / "dist/nt_emu/multi_burst.dll")
    parser.add_argument("--output", type=Path, default=ROOT / "dist/preset-pack")
    parser.add_argument("--zip", type=Path, dest="archive", default=ROOT / "dist/multi_burst_motion_presets.zip")
    parser.add_argument("--compiler", help="Path/name of a native clang++ or g++ compiler.")
    args = parser.parse_args()
    library = args.library.resolve()
    if not library.is_file():
        parser.error("Build the desktop DLL first with scripts/build-emu.ps1.")
    scenes, compiler = read_cpp_pack(ROOT / "build/preset-pack", args.compiler)
    generator = load_patch_generator()
    destination = args.output.resolve()
    (destination / "patches").mkdir(parents=True, exist_ok=True)
    (destination / "source").mkdir(parents=True, exist_ok=True)
    files = []
    for scene in scenes:
        slug = re.sub(r"[^a-z0-9]+", "-", scene["name"].lower()).strip("-")
        if not slug:
            raise ValueError("A scene has an empty filename")
        relative = Path("patches") / f"{scene['number']:02d}-{slug}.vcv"
        patch, parameters = make_patch(generator, library, scene)
        write_json(destination / relative, patch)
        scene["patch"] = relative.as_posix()
        scene["parameters"] = parameters
        files.append(relative.as_posix())
    for source in (SOURCE, PRESETS):
        relative = Path("source") / source.name
        shutil.copyfile(source, destination / relative)
        files.append(relative.as_posix())
    guide = ROOT / "docs/preset-pack.md"
    if guide.is_file():
        shutil.copyfile(guide, destination / "guide.md")
        files.append("guide.md")
    catalogue = {
        "schemaVersion": 1, "id": PACK_ID, "title": "Multi Burst NT - Motion Presets",
        "source": {"repositoryPath": SOURCE.relative_to(ROOT).as_posix(),
                   "path": "source/performance_presets.h", "sha256": sha256(SOURCE)},
        "voiceFieldOrder": preset_fields(), "parameterCount": len(generator.parameter_names()),
        "library": {"path": library.as_posix(), "sha256": sha256(library)},
        "presets": scenes,
    }
    write_json(destination / "catalog.json", catalogue)
    files.append("catalog.json")
    readme = """# Multi Burst NT : 16 scènes de modulation

Ouvrir un des 16 fichiers dans `patches/` avec VCV Rack 2, Fundamental et nt_emu.
Régler le moteur à 48 kHz. PULSES 1 lance les voix, 2 les réinitialise et 3
avance d'un burst. Les deux voix passent dans ENV MIX puis le VCA audio ; choisir
le pilote et le périphérique dans AUDIO 2 pour écouter. Le volume commence à 15 %.

Les modulations sont désactivées pour entendre chaque scène de base. Pour les
ajouter : Width CV input=4, Interval CV input=5 et Shape CV input=6. Les trois LFO
bipolaires correspondants tournent à 0,125 / 0,088 / 0,051 Hz. Clock input=None
laisse les scènes autonomes ; Clock input=2 et Clock source=Internal + CV ajoutent
le LFO externe 2,7 Hz en Absolute. Hand Steps attend PULSES 3 après son premier burst.

Le [catalogue JSON](catalog.json) contient les 25 valeurs par voix, les valeurs
communes et tous les paramètres réellement exportés. La source évaluée est
[performance_presets.h](source/performance_presets.h) ; les banques DROID restent
indépendantes. Le [manifeste](manifest.json) donne les empreintes des fichiers.

Les patches enregistrent le chemin absolu de la DLL Windows utilisée lors de
l'export. Si le projet ou la DLL change de dossier, régénérer depuis le projet :

```powershell
python scripts/export-preset-pack.py --library dist/nt_emu/multi_burst.dll
```
"""
    if guide.is_file():
        readme += "\nLe [guide des 16 scènes](guide.md) décrit leurs intentions et leur rappel sur NT.\n"
    (destination / "README.md").write_text(readme, encoding="utf-8")
    files.append("README.md")
    manifest = {
        "schemaVersion": 1, "id": PACK_ID, "presetCount": len(scenes),
        "library": catalogue["library"], "source": catalogue["source"],
        "files": [{"path": path, "bytes": (destination / path).stat().st_size,
                   "sha256": sha256(destination / path)} for path in sorted(files)],
    }
    write_json(destination / "manifest.json", manifest)
    args.archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.archive, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(files + ["manifest.json"]):
            archive.write(destination / path, arcname=path)
    print(f"Evaluated C++ source with {compiler}")
    print(f"Exported {len(scenes)} Rack patches and catalog: {destination}")
    print(f"Archive: {args.archive.resolve()}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f"Preset export failed: {error}", file=sys.stderr)
        raise SystemExit(1)
