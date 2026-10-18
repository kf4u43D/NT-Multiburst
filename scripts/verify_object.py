#!/usr/bin/env python3
"""Inspect the ARM object before packaging; this does not prove hardware execution.

Host symbols must be declared by the imported official SDK. The only default
runtime imports are memcpy, memset and memmove; no libm, allocation, C++ runtime,
or ARM helper is accepted implicitly. An additional runtime symbol requires an
explicit --allow-runtime after its availability on the target firmware is checked.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


DEFAULT_RUNTIME = frozenset({"memcpy", "memset", "memmove"})
# GNU PIC emits this linker pseudo-symbol; it is not a firmware function import.
LINKER_SYMBOLS = frozenset({"_GLOBAL_OFFSET_TABLE_"})
# The official JSON API is C++, unlike the NT_* exports. Accept only exact
# signatures also declared by the pinned SDK. No constructors or C++ runtime.
SERIALISATION_METHODS = {
    "_NT_jsonStream::openArray()": "voidopenArray(void);",
    "_NT_jsonStream::closeArray()": "voidcloseArray(void);",
    "_NT_jsonStream::openObject()": "voidopenObject(void);",
    "_NT_jsonStream::closeObject()": "voidcloseObject(void);",
    "_NT_jsonStream::addMemberName(char const*)": "voidaddMemberName(constchar*str);",
    "_NT_jsonStream::addNumber(int)": "voidaddNumber(intv);",
    "_NT_jsonStream::addNumber(float)": "voidaddNumber(floatv);",
    "_NT_jsonStream::addString(char const*)": "voidaddString(constchar*str);",
    "_NT_jsonStream::addBoolean(bool)": "voidaddBoolean(boolv);",
    "_NT_jsonStream::addNull()": "voidaddNull(void);",
    "_NT_jsonParse::numberOfArrayElements(int&)": "boolnumberOfArrayElements(int&num);",
    "_NT_jsonParse::numberOfObjectMembers(int&)": "boolnumberOfObjectMembers(int&num);",
    "_NT_jsonParse::matchName(char const*)": "boolmatchName(constchar*name);",
    "_NT_jsonParse::skipMember()": "boolskipMember(void);",
    "_NT_jsonParse::number(int&)": "boolnumber(int&v);",
    "_NT_jsonParse::number(float&)": "boolnumber(float&v);",
    "_NT_jsonParse::string(char const*&)": "boolstring(constchar*&str);",
    "_NT_jsonParse::boolean(bool&)": "boolboolean(bool&v);",
    "_NT_jsonParse::null()": "boolnull(void);",
}


def command_output(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(f"{' '.join(command)} failed:\n{result.stderr.strip()}")
    return result.stdout


def inspect_object(args: argparse.Namespace) -> dict[str, object]:
    object_path = args.object.resolve(strict=True)
    sdk_path = args.sdk_header.resolve(strict=True)
    header = command_output([args.tool_prefix + "readelf", "-h", str(object_path)])
    attributes = command_output([args.tool_prefix + "readelf", "-A", str(object_path)])
    symbols = command_output([args.tool_prefix + "readelf", "-W", "-s", str(object_path)])
    undefined = sorted(set(command_output([args.tool_prefix + "nm", "-u", "-j", str(object_path)]).split()))
    sizes_text = command_output([args.tool_prefix + "size", "-A", str(object_path)])

    required = {
        "ELF32": r"Class:\s+ELF32\b",
        "little endian": r"Data:.*little endian",
        "relocatable": r"Type:\s+REL\b",
        "ARM": r"Machine:\s+ARM\b",
    }
    for description, pattern in required.items():
        if not re.search(pattern, header):
            raise ValueError(f"Object must be {description}.")
    flags_match = re.search(r"Flags:\s+(0x[0-9a-fA-F]+)", header)
    if not flags_match or int(flags_match.group(1), 16) & 0xFF000000 != 0x05000000:
        raise ValueError("Object must use EABI version 5.")
    # GNU relocatable objects normally record hard-float in ARM attributes,
    # even when the ELF header lacks the hard-float flag used by executables.
    if not re.search(r"Tag_ABI_VFP_args:\s+VFP registers", attributes):
        raise ValueError("Object must use the hard-float ABI (VFP argument registers).")
    if "soft-float ABI" in header:
        raise ValueError("Contradictory soft-float ELF flags.")
    if not re.search(r"^\s*\d+:\s+\S+\s+\d+\s+FUNC\s+GLOBAL\s+\S+\s+(?!UND\b)\S+\s+pluginEntry\s*$", symbols, re.MULTILINE):
        raise ValueError("pluginEntry must be a defined GLOBAL function with C linkage.")

    sdk_text = sdk_path.read_text(encoding="utf-8")
    # Function declarations and public globals both use this namespace. Strip
    # comments and macros before harvesting names so prose/defines do not grant imports.
    sdk_text = re.sub(r"/\*.*?\*/|//[^\n]*", "", sdk_text, flags=re.DOTALL)
    sdk_text = re.sub(r"^\s*#.*(?:\\\n.*)*$", "", sdk_text, flags=re.MULTILINE)
    host_symbols = set(re.findall(r"\b(NT_[A-Za-z0-9_]+)\s*(?:\(|\[|;)", sdk_text))
    serialisation_path = sdk_path.with_name("serialisation.h")
    serialisation_symbols: set[str] = set()
    if serialisation_path.is_file() and undefined:
        serialisation_text = serialisation_path.read_text(encoding="utf-8")
        serialisation_text = re.sub(r"/\*.*?\*/|//[^\n]*", "", serialisation_text, flags=re.DOTALL)
        serialisation_text = re.sub(r"\s+", "", serialisation_text)
        demangled = command_output([args.tool_prefix + "c++filt", *undefined]).splitlines()
        if len(demangled) != len(undefined):
            raise ValueError("Cannot match undefined symbols to demangled signatures.")
        for symbol, signature in zip(undefined, demangled):
            declaration = SERIALISATION_METHODS.get(signature)
            if declaration and declaration in serialisation_text:
                serialisation_symbols.add(symbol)
    host_symbols.update(serialisation_symbols)
    runtime_symbols = DEFAULT_RUNTIME | set(args.allow_runtime)
    unexpected = sorted(set(undefined) - host_symbols - runtime_symbols - LINKER_SYMBOLS)
    if unexpected:
        raise ValueError("Unexpected undefined symbols: " + ", ".join(unexpected))

    sections: dict[str, int] = {}
    for line in sizes_text.splitlines():
        match = re.match(r"\s*(\.\S+)\s+(\d+)\s+\d+\s*$", line)
        if match:
            sections[match.group(1)] = int(match.group(2))
    if not sections:
        raise ValueError("Cannot parse object section sizes.")
    totals = {
        "code_bytes": sum(size for name, size in sections.items() if name.startswith(".text")),
        "readonly_bytes": sum(size for name, size in sections.items() if name.startswith(".rodata")),
        "initialized_data_bytes": sum(size for name, size in sections.items() if name.startswith(".data")),
        "zero_initialized_bytes": sum(size for name, size in sections.items() if name.startswith(".bss")),
    }
    print("PASS: ELF32 little-endian ARM REL, EABI5, hard-float, GLOBAL pluginEntry")
    print("Undefined symbols (SDK host API or explicitly allowed runtime):")
    print("  " + (", ".join(undefined) or "none"))
    print(sizes_text.strip())
    print("Section totals: " + ", ".join(f"{name}={size}" for name, size in totals.items()))
    return {
        "object_sha256": hashlib.sha256(object_path.read_bytes()).hexdigest(),
        "sdk_header_sha256": hashlib.sha256(sdk_path.read_bytes()).hexdigest(),
        "sdk_serialisation_sha256": hashlib.sha256(serialisation_path.read_bytes()).hexdigest() if serialisation_symbols else None,
        "serialisation_symbols": sorted(serialisation_symbols),
        "format": "ELF32 little-endian ARM REL EABI5 hard-float",
        "entry": "GLOBAL FUNC pluginEntry",
        "undefined_symbols": undefined,
        "allowed_runtime_symbols": sorted(runtime_symbols),
        "sections": sections,
        "totals": totals,
        "hardware_tested": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("object", type=Path)
    parser.add_argument("--tool-prefix", default="arm-none-eabi-", help="Executable prefix, including its directory when necessary.")
    parser.add_argument("--sdk-header", type=Path, default=Path(__file__).resolve().parents[1] / "third_party/distingNT_API/include/distingnt/api.h")
    parser.add_argument("--allow-runtime", action="append", default=[], help="Additional symbol independently checked against target firmware exports.")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        report = inspect_object(args)
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    except (OSError, RuntimeError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
