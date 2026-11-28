#!/usr/bin/env python3

from contextlib import contextmanager
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
    "libnative/disk-images.js",
    "libnative/zeal-native.js",
)


DIST = ROOT / "dist/zeal-cc"
FILES = {
    "bin": ("cc", "cc_parse", "cc_semantic", "cc_codegen", "ast_dump"),
    "lib": ("crt0.asm", "zeal8bit.asm", "math_8.asm", "math_16.asm"),
}


def media(name, variable):
    generated = ROOT / "build/test-media" / name
    return Path(os.environ.get(variable, generated if generated.is_file() else ROOT / ".zeal8bit" / name))


@contextmanager
def replacement(destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent) as work:
        tree = Path(work) / "tree"
        tree.mkdir()
        yield tree
        previous = Path(work) / "previous"
        if destination.exists():
            destination.rename(previous)
        try:
            tree.rename(destination)
        except OSError:
            if previous.exists():
                previous.rename(destination)
            raise


def stage_dist():
    with replacement(DIST) as tree:
        for folder, names in FILES.items():
            (tree / folder).mkdir(parents=True)
            for name in names:
                source = ROOT / ("runtime" if folder == "lib" else folder) / name
                shutil.copyfile(source, tree / folder / name)


def stage_example(tree):
    directory = Path("examples/hexdump")
    (tree / directory).mkdir(parents=True)
    for name in ("hexdump.c", "sample.dat"):
        shutil.copyfile(ROOT / directory / name, tree / directory / name)


def disk(image, *args):
    subprocess.run(["zdt", "--yes", "-i", image, *args], check=True,
                   stdout=subprocess.DEVNULL)


def stage_tf(destination, dist, *, base=None):
    with tempfile.TemporaryDirectory(dir=destination.parent) as directory:
        image = Path(directory) / "tf.img"
        if base is None:
            subprocess.run(["zdt", "--yes", "image", "create", image, "--size", "4MiB", "--mbr"],
                           check=True, stdout=subprocess.DEVNULL)
            disk(image, "partition", "create")
            existing = set()
        else:
            shutil.copyfile(base, image)
            listing = subprocess.check_output(["zdt", "--json", "-i", image, "ls", "/"])
            existing = {entry["name"] for entry in json.loads(listing)["data"] if entry["directory"]}
        for folder, names in FILES.items():
            if folder not in existing:
                disk(image, "mkdir", f"/{folder}")
            disk(image, "import", *(dist / folder / name for name in names), f"/{folder}/")
        disk(image, "import", media("zealasm", "ZEALASM"), "/bin/zealasm")
        image.replace(destination)


def stage_hostfs():
    with replacement(HOSTFS) as tree:
        stage_example(tree)
        paths = sorted(path.relative_to(tree).as_posix() for path in tree.rglob("*") if path.is_file())
        (tree / "index.json").write_text(json.dumps(paths))


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
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--dist", action="store_true")
    if parser.parse_args().dist:
        stage_dist()
    else:
        stage_tf(DEPLOY / "compiler-tf.img", DIST, base=DEPLOY / "tf.img")
        stage_hostfs()
        stage_emulator()
