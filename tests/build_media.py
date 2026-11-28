"""Build a private UART test kernel and Zealasm with ZDE's installed tools."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    zos = Path(os.environ["ZOS_PATH"])
    for name in ("zshell", "echo", "return"):
        if not (Path("/mnt/romdisk") / name).is_file():
            raise SystemExit(f"Missing /mnt/romdisk/{name}. Stage coreutils and zshell with ZDE.")
    output = ROOT / "build/test-media"
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output) as temp:
        work = Path(temp)
        source = work / "os"
        shutil.copytree(zos, source, ignore=shutil.ignore_patterns(".git", "build", "osconfig.asm", "romdisk_info_h.asm"))
        shutil.copyfile(ROOT / "headless.conf", source / "os.conf")
        env = {**os.environ, "ZOS_PATH": str(source), "GIT_CEILING_DIRECTORIES": str(work)}
        env.pop("EXTRA_ROMDISK_FILES", None)
        build = work / "build"
        subprocess.run(["cmake", "-S", source, "-B", build], env=env, check=True)
        subprocess.run(["cmake", "--build", build], env=env, check=True)
        asm = work / "asm"
        original = ROOT / "examples/Zealasm"
        shutil.copytree(original, asm, ignore=shutil.ignore_patterns(".git", "bin"))
        version = subprocess.check_output(["git", "-C", original, "describe", "--always"], text=True)
        (asm / "version.txt").write_text(version.strip() + " ")
        subprocess.run(["make", "-o", "version.txt", f"ZOS_INCLUDE={zos}/sdk/z88dk-z80asm"],
                       cwd=asm, check=True)
        (asm / "bin/zealasm.bin").replace(output / "zealasm")
        (build / "os_with_romdisk.img").replace(output / "headless-tf.img")


if __name__ == "__main__":
    main()
