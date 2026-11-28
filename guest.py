"""Prepare private media and a HostFS installation for the Zeal headless frontend."""
from contextlib import contextmanager
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from stage import DIST, ROOT, media


@contextmanager
def workspace(cases):
    parent = ROOT / "tests/audit"
    parent.mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="guest-", dir=parent))
    host = work / "host"
    try:
        shutil.copytree(DIST, host)
        shutil.copyfile(media("zealasm", "ZEALASM"), host / "bin/zealasm")
        shutil.copyfile(media("headless.img", "ZEAL_CC_ROM"), work / "rom.img")
        (host / "tests").mkdir()
        for case in cases:
            shutil.copyfile(ROOT / f"tests/{case}.c", host / f"tests/{case}.c")
        shutil.copyfile(ROOT / "tests/zealos.txt", host / "tests/zealos.txt")
        # The UART kernel disables TF. An explicit private image suppresses auto-discovery.
        with (work / "tf.img").open("wb") as image:
            image.truncate(4 * 1024 * 1024)
        image = work / "eeprom.img"
        for args in (["image", "create", image, "--size", "64KiB"],
                     ["-i", image, "partition", "create"],
                     ["-i", image, "import", ROOT / "autoexec.zs", "/"]):
            subprocess.run(["zdt", "--yes", *args], check=True, stdout=subprocess.DEVNULL)
        yield work
    except BaseException:
        print(f"Guest workspace retained at {work}", flush=True)
        raise
    else:
        shutil.rmtree(work)


def command(work, *, console=False):
    exe = shutil.which("zeal-headless")
    if not exe:
        raise FileNotFoundError("zeal-headless not found on PATH")
    return [exe, *(["--console"] if console else []), "--no-reset", "--hostfs", str(work / "host"),
            "--rom", str(work / "rom.img"), "--eeprom", str(work / "eeprom.img"),
            "--tf", str(work / "tf.img")]


def script(work, commands):
    (work / "host/test.zs").write_text("\n".join([*commands, "reset", ""]))


def debug(work, case, stage, smoke, batch):
    host = work / "host/tests"
    source, ast, asm = (f"{case}.{ext}" for ext in ("c", "ast", "asm"))
    steps = {"parse": f"cc_parse {source} {ast}", "semantic": f"cc_semantic {ast}",
             "codegen": f"cc_codegen {ast} {asm}"}
    if stage != "parse":
        prerequisites = [steps["parse"]]
        if stage == "codegen":
            prerequisites.append("? " + steps["semantic"])
        script(work, ["cd h:/tests", *prerequisites, "? echo READY"])
        status, log = batch(work)
        if status or "READY" not in log.splitlines() or not (host / ast).is_file():
            raise ValueError("Failed to prepare debugger input")
    script(work, ["cd h:/tests", steps[stage]])
    argv = command(work, console=True)
    mapfile = work / f"cc_{stage}.map"
    shutil.copyfile(ROOT / "debug" / mapfile.name, mapfile)
    argv.extend(["--map", str(mapfile)])
    (work / "debug-launch.txt").write_text(" ".join(argv) + "\n")
    print(" ".join(argv), flush=True)
    if not smoke:
        with (work / "console.log").open("wb") as log, subprocess.Popen(
            argv, cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        ) as proc:
            for chunk in iter(lambda: proc.stdout.read1(4096), b""):
                log.write(chunk)
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
            if proc.wait():
                raise ValueError("Debugger exited with an error")
        return
    name = "_parser_parse_next"
    address = int(re.search(r"^\s*([0-9A-F]+)\s+" + name + r"\s", mapfile.read_text(), re.M)[1], 16)
    binary = (work / "host/bin/cc_parse").read_bytes()
    expected = binary[address - 0x4000:address - 0x4000 + 8].hex(" ")
    commands = f"bp {name}\nrun 200000000\nregs\nmmu\nrb {name} 8\nbc {name}\nrun 50000000\nquit\n"
    (work / "commands.txt").write_text(commands)
    with (work / "console.log").open("w+") as log:
        proc = subprocess.run(argv, cwd=work, input=commands, text=True, stdout=log,
                              stderr=subprocess.STDOUT, timeout=30)
        log.seek(0)
        transcript = log.read().lower()
    if (proc.returncode or len(expected) != 23 or f"pc={address:04x}" not in transcript
            or f"{address:04x}: {expected}" not in transcript or "8000 ->" not in transcript
            or not (host / ast).stat().st_size):
        raise ValueError("Debugger checkpoint or parser output missing")
    print(f"Verified {name} at {address:04x}, mapped instructions and fresh AST")
