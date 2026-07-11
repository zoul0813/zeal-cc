#!/usr/bin/env python3

import json
import os
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DEPLOY = ROOT / ".deploy"
HOSTFS = DEPLOY / "hostfs"
EMULATOR_RELEASE = os.environ.get("EMULATOR_RELEASE", "latest")
EMULATOR_ARCHIVE = (
    "https://github.com/Zeal8bit/Zeal-NativeEmulator/releases/download/"
    f"{EMULATOR_RELEASE}/zeal-native-wasm.zip"
)
EMULATOR_FILES = (
    "zeal.elf.js",
    "zeal.elf.wasm",
    "libnative/indexeddb.js",
    "libnative/disk-images.js",
    "libnative/zeal-native.js",
)


def stage_hostfs():
    for name in ("bin", "runtime", "tests"):
        source = ROOT / name
        destination = HOSTFS / name
        destination.mkdir(parents=True, exist_ok=True)
        for path in source.iterdir():
            if path.is_file():
                shutil.copy2(path, destination / path.name)

    paths = sorted(
        path.relative_to(HOSTFS).as_posix()
        for path in HOSTFS.rglob("*")
        if path.is_file() and path.name != "index.json"
    )
    with (HOSTFS / "index.json").open("w", encoding="utf-8") as handle:
        json.dump(paths, handle)


def stage_emulator():
    with tempfile.TemporaryDirectory(prefix="zeal-native-") as temporary:
        archive_path = Path(temporary) / "zeal-native-wasm.zip"
        print(f"Downloading Zeal-NativeEmulator release {EMULATOR_RELEASE}...")
        subprocess.run(
            [
                "curl",
                "--fail",
                "--location",
                "--retry",
                "3",
                "--output",
                str(archive_path),
                EMULATOR_ARCHIVE,
            ],
            check=True,
        )

        with zipfile.ZipFile(archive_path) as archive:
            available = set(archive.namelist())
            missing = set(EMULATOR_FILES) - available
            if missing:
                raise RuntimeError(
                    f"Emulator archive missing required files: {', '.join(sorted(missing))}"
                )
            for name in EMULATOR_FILES:
                destination = DEPLOY / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(name) as source, destination.open("wb") as output:
                    shutil.copyfileobj(source, output)


if __name__ == "__main__":
    stage_hostfs()
    stage_emulator()
