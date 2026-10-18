"""Create the VCV Rack test patch using installed Core/Fundamental/nt_emu modules."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def parameter_names() -> list[str]:
    """Read the host's append-only parameter contract."""
    root = Path(__file__).resolve().parent.parent
    header = (root / "include/multi_burst/parameters.h").read_text(encoding="utf-8")
    header = re.sub(r"//[^\n]*", "", header)
    enum = re.search(r"enum Parameter\s*:\s*uint8_t\s*\{([^}]+)\}", header)
    if enum is None:
        raise ValueError("Cannot read persisted parameter indices")
    names = [name.strip() for name in enum.group(1).split(",") if name.strip()]
    if names[-1] != "ParameterCount" or names.index("ManualStep") != 41:
        raise ValueError("Unexpected persisted parameter contract")
    names.pop()
    return names


def persisted_defaults() -> list[int]:
    """Keep legacy indices and initialise every appended host parameter."""
    names = parameter_names()
    values = [0] * len(names)
    legacy = [8, 0, 100, 100, 80, 400, 1200, 2, 1, 1, 1, 0, 0, 5,
              1, 2, 3, 13, 1, 14, 1, 15, 1, 16, 1, 17, 1,
              1, 60, 61, 62, 4, 1, 60, 1, 11, 50, 1, 1, 0, 0, 0]
    values[:len(legacy)] = legacy
    features = {"WidthRatio": 3, "IntervalRatio": 6, "VelocityEnabled": 1,
                "LevelTrim": 100, "CvEnabled": 1, "CvDepth": 100,
                "CountCvDepth": 100, "TimeCvDepth": 100,
                "ShapeCvDepth": 100, "SlopeCvDepth": 100, "PresetSlot": 1}
    defaults = {**features, **{"G2" + key: value for key, value in features.items()},
                "MidiStopNote": 63, "G2Bursts": 5, "G2Width": 100,
                "G2Level": 80, "G2Rate": 400, "G2Division": 1, "G2CvDivider": 1,
                "G2MidiInputChannel": 1, "G2MidiTriggerNote": 64, "G2MidiResetNote": 65,
                "G2MidiStepNote": 66, "G2MidiStopNote": 67, "G2MidiOutputChannel": 2,
                "G2MidiOutputNote": 64, "G2MidiEnvelopeCc": -1, "G2MidiCcRate": 50}
    for name, value in defaults.items():
        values[names.index(name)] = value
    if len(values) > 256:
        raise ValueError("nt_emu routing matrix has only 256 entries")
    return values


def create_patch(library: Path, motion: bool = False) -> dict:
    def module(identifier, plugin, model, x, y, values=(), data=None):
        result = {"id": identifier, "plugin": plugin, "model": model,
                  "version": "2.0.2" if plugin == "nt_emu" else "2.6.4",
                  "params": [{"id": i, "value": v} for i, v in enumerate(values)],
                  "pos": [x, y]}
        if data is not None:
            result["data"] = data
        return result

    # Plugin parameter indices are persisted by the host (parameters.h).
    values = persisted_defaults()
    if motion:
        names = parameter_names()
        settings = {
            "Generator2Enabled": 1, "ClockSource": 4, "CvResponse": 1,
            "TriggerInput": 0, "ResetInput": 0, "TriggerAllInput": 1,
            "ResetAllInput": 3, "WidthCvInput": 4, "IntervalCvInput": 5,
            "ShapeCvInput": 6, "Bursts": 8, "EnvelopeShape": 3,
            "Width": 100, "Slope": -75, "Level": 80, "Rate": 400,
            "RetriggerMode": 1, "Loop": 1, "ReferenceCurves": 2,
            "TimeCvDepth": 35, "ShapeCvDepth": 35,
            "MidiNotes": 0, "MidiEnvelopeCc": -1, "MidiClockOutput": 0,
            "MidiTransportOutput": 0, "G2Bursts": 13, "G2EnvelopeShape": 5,
            "G2Width": 280, "G2Slope": 50, "G2Level": 70, "G2Rate": 160,
            "G2RetriggerMode": 1, "G2Loop": 1, "G2ReferenceCurves": 2,
            "G2TimeCvDepth": 20, "G2ShapeCvDepth": 25,
            "G2EnvelopeOutput": 18, "G2EnvelopeOutputMode": 1,
        }
        for name, value in settings.items():
            values[names.index(name)] = value
    nt_data = {
        "algorithmIndex": 0,
        "pluginPath": library.resolve().as_posix(),
        "params": [0, 0, 0],
        "routing": [{"param": i, "bus": value} for i, value in enumerate(values)],
        "parameterValues": {"currentPageIndex": 0, "currentParamIndex": 0,
                            "routingMatrix": values + [0] * (256 - len(values))},
        "midiInput": {"driver": -1, "channel": -1},
        "midiOutput": {"driver": -1, "channel": 0},
    }
    scope_values = [1, -4, 1, -2.5, -2, 0, 0, 1]
    modules = [
        module(1, "Core", "Notes", 0, 0, data={"text":
            "MULTI BURST NT\nCV / MIDI TEST\n\nEngine : 48 kHz\n\nPULSES :\n1 = TRIGGER\n2 = RESET\n\n8 bursts / 4 Hz\nWidth : 100 ms\nSlope : +100 %\nLevel : 8 V\n\nSCOPES :\n1 Envelope / Gate\n2 Active / EOC\n3 Clock / LFO\n\nRIGHT NT KNOB :\npress = STEP\n\nAUDIO : select\ndriver and output"}),
        module(2, "Fundamental", "LFO", 16, 0, [1, 0, 2, 0, 0, .05, 0]),
        module(3, "Fundamental", "Pulses", 25, 0, [0] * 10),
        module(4, "nt_emu", "nt_emu", 33, 0, [0] * 12, nt_data),
        module(5, "Fundamental", "Scope", 47, 0, scope_values),
        module(6, "Fundamental", "Scope", 60, 0, [1, -2.5, 1, -2.5, -2, 0, 0, 1]),
        module(7, "Fundamental", "Scope", 73, 0, [1, -2.5, 1, -5, -2, 0, 0, 1]),
        module(8, "Fundamental", "VCO", 16, 1, [0, 1, -12, 0, 0, .5, 0, 0]),
        module(9, "Fundamental", "VCA-1", 25, 1, [1, 1]),
        module(10, "Fundamental", "VCA-1", 28, 1, [.15, 1]),
        module(11, "Core", "AudioInterface2", 31, 1, [1], {
            "audio": {"driver": -1, "sampleRate": 48000, "blockSize": 256,
                      "inputOffset": 0, "outputOffset": 0}, "dcFilter": True}),
        module(12, "Core", "Notes", 0, 1, data={"text":
            "LISTEN\nSine -> ENV VCA\n-> volume -> AUDIO\n\nTESTS :\nSlope 0 : uniform\nSlope -100 : falling\nONE : ignore trigger\nRET : restart\nReset : silence\nRate 0 : internal off\nSTEP : manual\n\nNT MENU :\nClock source\nCV / internal / MIDI\nShape / Width\n\nMIDI channel 1\nNote 60 : trigger\n61 : reset / 62 : step\nCC 20-26 : control\nMIDI output : menu\nselect device"}),
    ]
    connections = [
        (2, 3, 4, 1, "#e6c84c"),     # External CV clock.
        (3, 0, 4, 0, "#e64b40"),     # Trigger.
        (3, 1, 4, 2, "#9956d6"),     # Reset plugin and external oscillator together.
        (3, 1, 2, 2, "#9956d6"),
        (4, 0, 5, 0, "#55bd83"),
        (4, 1, 5, 1, "#e69a40"),
        (4, 2, 6, 0, "#55a6dc"),
        (4, 3, 6, 1, "#9956d6"),
        (4, 4, 7, 0, "#e6c84c"),
        (2, 3, 7, 1, "#8a8a8a"),
        (8, 0, 9, 1, "#b0b0b0"),
        (4, 0, 9, 0, "#55bd83"),
        (9, 0, 10, 1, "#b0b0b0"),
        (10, 0, 11, 0, "#b0b0b0"),
        (10, 0, 11, 1, "#b0b0b0"),
    ]
    if motion:
        modules[0]["data"]["text"] = (
            "MULTI BURST NT\nMOTION / 2 VOICES\n\nEngine : 48 kHz\n\nPULSES :\n1 = TRIGGER ALL\n2 = RESET ALL\n\nG1 : 8 bursts\nG2 : 13 bursts\nLoop / ONE\n\nAbsolute\nInternal + CV\nCV : Bipolar\nShape : Morph\n\nSCOPES 1-3 :\nENV / GATE\nBUSY / EOC\nCLOCK / LFO")
        # A slow external clock combines with each voice's independently
        # modulated internal oscillator. The modulation LFOs run freely.
        modules[1]["params"][2]["value"] = 1.4329594073  # 2**pitch = 2.7 Hz.
        modules[11]["data"]["text"] = (
            "LISTEN TO G1\nSine -> ENV VCA\n-> volume -> AUDIO\n\nSelect driver\nand audio output.\nVolume : 15 %.\n\n3 SLOW LFOs\nBipolar -5/+5 V\nWidth : 0.125 Hz\nInterval : 0.088 Hz\nShape : 0.051 Hz\n\nSCOPES 4-5 :\nG2 ENV / Shape CV\nWidth / Interval CV\n\nCompare :\nCV response DROID\nShape response DROID\nTime depth = 0\n\nPULSES 2 : stop\nPULSES 1 : restart")
        modules.extend([
            module(13, "Fundamental", "LFO", 47, 1, [0, 0, -3, 0, 0, .5, 0]),
            module(14, "Fundamental", "LFO", 56, 1, [0, 0, -3.5, 0, 0, .5, 0]),
            module(15, "Fundamental", "LFO", 65, 1, [0, 0, -4.3, 0, 0, .5, 0]),
            module(16, "Fundamental", "Scope", 74, 1, [1, -3.5, 0, 0, -2, 0, 0, 1]),
            module(17, "Fundamental", "Scope", 87, 1, [0, 0, 0, 0, -4.3, 0, 0, 1]),
        ])
        connections.extend([
            (13, 1, 4, 3, "#55a6dc"),  # Width: slow bipolar triangle.
            (14, 0, 4, 4, "#e6c84c"),  # Rate: a different slow sine.
            (15, 1, 4, 5, "#9956d6"),  # Shape: continuous bipolar triangle.
            (4, 5, 16, 0, "#55bd83"),  # G2 envelope on physical output 6.
            (15, 1, 16, 1, "#9956d6"),
            (13, 1, 17, 0, "#55a6dc"),
            (14, 0, 17, 1, "#e6c84c"),
        ])
    cables = [{"id": i + 1, "outputModuleId": source, "outputId": out,
               "inputModuleId": target, "inputId": port, "color": colour}
              for i, (source, out, target, port, colour) in enumerate(connections)]
    return {"version": "2.6.4", "zoom": .85, "gridOffset": [0, 0],
            "modules": modules, "cables": cables}


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, default=root / "dist/nt_emu/multi_burst.dll")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--motion", action="store_true",
                        help="Create the two-voice bipolar CV / shape morphing demo.")
    args = parser.parse_args()
    if not args.library.is_file():
        parser.error("Build the desktop DLL first with scripts/build-emu.ps1.")
    patch = create_patch(args.library, motion=args.motion)
    output = args.output or root / "patches" / (
        "Multi_Burst_NT-motion.vcv" if args.motion else "Multi_Burst_NT-nt_emu.vcv")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(patch, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Created {output} ({len(patch['modules'])} modules, {len(patch['cables'])} cables)")


if __name__ == "__main__":
    main()
