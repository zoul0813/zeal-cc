#!/usr/bin/env python3
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
BLUE = "\033[0;34m"
NC = "\033[0m"

TESTS_RUN = 0
TESTS_PASSED = 0
TESTS_FAILED = 0


def print_header(title: str) -> None:
    print(f"\n{BLUE}========================================{NC}")
    print(f"{BLUE}{title}{NC}")
    print(f"{BLUE}========================================{NC}\n")


def print_result(test_name: str, result: int) -> None:
    global TESTS_RUN, TESTS_PASSED, TESTS_FAILED
    TESTS_RUN += 1
    if result == 0:
        print(f"{GREEN}✓{NC} {test_name}")
        TESTS_PASSED += 1
    else:
        print(f"{RED}✗{NC} {test_name}")
        TESTS_FAILED += 1


def run_cmd(cmd, quiet=False):
    try:
        if quiet:
            return subprocess.run(cmd, check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return subprocess.run(cmd, check=False)
    except FileNotFoundError:
        return None


def test_compile(compiler: str, test_file: Path, test_name: str) -> None:
    if not test_file.exists():
        print(f"{YELLOW}⚠{NC}  Skipping {test_name} (file not found: {test_file})")
        return
    asm_file = test_file.with_suffix(".asm")
    result = run_cmd([compiler, str(test_file), str(asm_file)])
    status = 1 if result is None else result.returncode
    print_result(test_name, status)


def ensure_reset_in_test_zs() -> None:
    path = Path("test.zs")
    if not path.exists():
        return
    lines = path.read_text().splitlines()
    while lines and lines[-1].strip() == "":
        lines.pop()
    while lines and lines[-1].strip().lstrip(";").strip() == "reset":
        lines.pop()
    lines.append("reset")
    path.write_text("\n".join(lines) + "\n")


def comment_reset_in_test_zs() -> None:
    path = Path("test.zs")
    if not path.exists():
        return
    lines = path.read_text().splitlines()
    while lines and lines[-1].strip() == "":
        lines.pop()
    while lines and lines[-1].strip().lstrip(";").strip() == "reset":
        lines.pop()
    lines.append("; reset")
    path.write_text("\n".join(lines) + "\n")


def expected_return_hex(base: str) -> str:
    return {
        "test1": "0C",
        "test2": "0F",
        "test_add": "0F",
        "test_comp": "78",
        "test_div": "03",
        "test_expr": "1C",
        "test_for": "0A",
        "test_if": "2A",
        "test_mod": "01",
        "test_mul": "0F",
        "test_params": "05",
        "test_while": "0A",
    }.get(base, "")


def parse_return_results(log: str):
    exec_file = ""
    pairs = []
    exec_re = re.compile(r"Exec\s+'([^']+)'")
    return_re = re.compile(r"Returned\s+\$([0-9A-Fa-f]+)")
    for raw in log.splitlines():
        line = raw.rstrip("\r").lstrip()
        exec_match = exec_re.search(line)
        if exec_match:
            exec_file = exec_match.group(1)
            continue
        ret_match = return_re.search(line)
        if ret_match:
            hex_val = ret_match.group(1)
            if exec_file and hex_val:
                pairs.append((exec_file, hex_val))
                exec_file = ""

    if not pairs:
        return []

    results = []
    for path, hex_val in pairs:
        base = Path(path).stem
        expected = expected_return_hex(base)
        hex_val = hex_val.upper()
        ok = True
        if expected and hex_val != expected:
            ok = False
        results.append((path, expected, hex_val, ok))
    return results


def run_headless_emulator(img: str, eeprom: str, test_name: str) -> None:
    if shutil.which("zeal-native") is None:
        print(f"{YELLOW}⚠{NC}  Skipping {test_name} (zeal-native not found)")
        return
    if not Path(img).exists() or not Path(eeprom).exists():
        print(f"{YELLOW}⚠{NC}  Skipping {test_name} (image(s) missing)")
        return

    try:
        proc = subprocess.run(
            ["zeal-native", "--headless", "--no-reset", "-r", img, "-e", eeprom],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=30,
        )
        log = proc.stdout
        status = proc.returncode
    except subprocess.TimeoutExpired as exc:
        print(f"{RED}X{NC} {test_name} timed out after 30s (possible hang/reset loop)")
        if exc.stdout:
            print(exc.stdout)
        print_result(test_name, 1)
        return

    if "error occurred" in log.lower():
        print(log)
        print_result(test_name, 1)
        return

    print_result(test_name, status)

    results = parse_return_results(log)
    if results:
        for path, expected, actual, ok in results:
            if expected:
                msg = f"{path} expected ${expected}, got ${actual}"
            else:
                msg = f"{path} got ${actual}"
            print_result(msg, 0 if ok else 1)


def main() -> int:
    print(f"{BLUE}╔════════════════════════════════════════╗{NC}")
    print(f"{BLUE}║     Zeal C Compiler Test Suite         ║{NC}")
    print(f"{BLUE}╚════════════════════════════════════════╝{NC}")

    print_header("Building ZOS Target")
    zos_build = run_cmd(["zde", "cmake"], quiet=True)
    zos_status = 1 if zos_build is None else zos_build.returncode
    print_result("ZOS compilation (bin/cc)", zos_status)

    print_header("Building macOS Target")
    run_cmd(["make", "clean"], quiet=True)
    mac_build = run_cmd(["make"], quiet=True)
    mac_status = 1 if mac_build is None else mac_build.returncode
    print_result("macOS compilation (bin/cc_darwin)", mac_status)

    print_header("Running Compiler Tests (macOS)")
    cc_darwin = Path("bin/cc_darwin")
    if cc_darwin.exists():
        for test_file in sorted(Path("tests").glob("*.c")):
            test_compile(str(cc_darwin), test_file, f"Compile {test_file.name}")
        if TESTS_RUN == 2:
            with tempfile.NamedTemporaryFile("w", delete=False, suffix=".c") as tmp:
                tmp.write("int main() {\n    int x = 42;\n    return 0;\n}\n")
                tmp_path = Path(tmp.name)
            test_compile(str(cc_darwin), tmp_path, "Inline test code")
            tmp_path.unlink(missing_ok=True)
    else:
        print(f"{RED}✗{NC} macOS binary not found, skipping tests")

    print_header("Verifying Build Artifacts")
    print_result("ZOS binary exists (bin/cc)", 0 if Path("bin/cc").exists() else 1)
    print_result("macOS binary exists (bin/cc_darwin)", 0 if cc_darwin.exists() else 1)
    print_result("ZOS debug symbols exist", 0 if Path("debug/cc.cdb").exists() else 1)

    print_header("Zeal-native Headless Smoke Test")
    if zos_status == 0:
        ensure_reset_in_test_zs()
        run_headless_emulator(".zeal8bit/headless.img", ".zeal8bit/eeprom.img", "zeal-native headless boot")
        comment_reset_in_test_zs()
    else:
        print(f"{YELLOW}⚠{NC}  Skipping zeal-native headless boot (ZOS build failed)")

    print_header("Test Summary")
    print(f"Total tests run:    {BLUE}{TESTS_RUN}{NC}")
    print(f"Tests passed:       {GREEN}{TESTS_PASSED}{NC}")
    print(f"Tests failed:       {RED}{TESTS_FAILED}{NC}")

    if TESTS_FAILED == 0:
        print(f"\n{GREEN}╔════════════════════════════════════════╗{NC}")
        print(f"{GREEN}║          All Tests Passed! ✓           ║{NC}")
        print(f"{GREEN}╚════════════════════════════════════════╝{NC}\n")
        return 0

    print(f"\n{RED}╔════════════════════════════════════════╗{NC}")
    print(f"{RED}║         Some Tests Failed ✗            ║{NC}")
    print(f"{RED}╚════════════════════════════════════════╝{NC}\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
