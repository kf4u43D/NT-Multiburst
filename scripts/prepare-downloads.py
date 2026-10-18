"""Prepare the small, versioned downloads committed alongside the source."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parent.parent
VERSION = "0.4"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    distribution = ROOT / "dist"
    build = json.loads((distribution / "build_manifest.json").read_text(encoding="utf-8-sig"))
    hardware = distribution / "multi_burst_nt.zip"
    with zipfile.ZipFile(hardware) as archive:
        if archive.namelist() != ["programs/plug-ins/multi_burst.o"]:
            raise ValueError("Unexpected hardware archive layout")
        object_digest = hashlib.sha256(archive.read(archive.namelist()[0])).hexdigest()
    if object_digest != build["object_sha256"] or digest(hardware) != build["archive_sha256"]:
        raise ValueError("Hardware archive does not match the inspected build")
    inspection = ROOT / "build/arm/object_report.json"
    if json.loads(inspection.read_text(encoding="utf-8"))["object_sha256"] != object_digest:
        raise ValueError("Inspection report does not match the hardware object")
    library = distribution / "nt_emu/multi_burst.dll"
    presets = distribution / "multi_burst_motion_presets.zip"
    with zipfile.ZipFile(presets) as archive:
        pack = json.loads(archive.read("manifest.json"))
        if pack["presetCount"] != 16 or pack["library"]["sha256"] != digest(library):
            raise ValueError("Preset pack does not match the current native library")
        for entry in pack["files"]:
            if hashlib.sha256(archive.read(entry["path"])).hexdigest() != entry["sha256"]:
                raise ValueError(f"Preset pack checksum mismatch: {entry['path']}")
    destination = ROOT / "downloads" / f"v{VERSION}"
    destination.mkdir(parents=True, exist_ok=True)
    files = []
    for source in (hardware, presets, library, inspection):
        target = destination / source.name
        shutil.copyfile(source, target)
        files.append({"path": target.name, "bytes": target.stat().st_size, "sha256": digest(target)})
    manifest = {
        "version": VERSION,
        "target_firmware": build["target_firmware"],
        "minimum_firmware": build["minimum_firmware"],
        "required_api_version": build["required_api_version"],
        "sdk_commit": build["sdk_commit"],
        "sdk_header_sha256_lf": build["sdk_header_sha256_lf"],
        "object_sha256": object_digest,
        "hardware_tested": False,
        "files": files,
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    sums = "\n".join(f"{entry['sha256']}  {entry['path']}" for entry in files)
    (destination / "SHA256SUMS.txt").write_text(sums + "\n", encoding="utf-8")
    print(f"Verified {len(files)} downloads in {destination}")


if __name__ == "__main__":
    main()
