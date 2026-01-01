#!/usr/bin/env python3
import argparse
import os
import platform
import re
import select
import shutil
import subprocess
import sys
import time
from pathlib import Path


RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
BLUE = "\033[0;34m"
NC = "\033[0m"
HEADLESS_TIMEOUT_SEC = 120

TESTS_RUN = 0
TESTS_PASSED = 0
TESTS_FAILED = 0

EXPECTED_RESULTS = {
    "assign": "15",
    "array": "EF",
    "char": "41",
    "comp": "4E",
    "compares": "3F",
    "do_while": None,
    "expr": "1C",
    "for": "0A",
    "global": "0A",
    "if": "2A",
    "bitwise": "E4",
    "math": "3A",
    "params": "14",
    "pointer": "86",
    "simple_return": "0C",
    "return16": "EF",
    "struct": None,
    "signs": "EE",
    "ternary": None,
    "unary": "AA",
    "while": "0A",
    "zealos": "EA",
}


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

def print_expected_fail(test_name: str) -> None:
    global TESTS_RUN, TESTS_PASSED
    TESTS_RUN += 1
    print(f"{YELLOW}⚠{NC} {test_name}")
    TESTS_PASSED += 1


def status_label(status: str) -> str:
    if status == "ok":
        return "Check"
    if status == "expected_fail":
        return "Expected"
    if status == "unexpected_pass":
        return "Unexpected"
    if status == "missing":
        return "Missing"
    return "Fail"


def format_status(status: str, width: int) -> str:
    label = f"{status_label(status):<{width}}"
    if status == "ok":
        return f"{GREEN}{label}{NC}"
    if status == "expected_fail":
        return f"{YELLOW}{label}{NC}"
    if status == "unexpected_pass":
        return f"{RED}{label}{NC}"
    if status == "missing":
        return f"{YELLOW}{label}{NC}"
    return f"{RED}{label}{NC}"


def update_counts(status: str) -> None:
    global TESTS_RUN, TESTS_PASSED, TESTS_FAILED
    if status == "missing":
        return
    TESTS_RUN += 1
    if status in ("ok", "expected_fail"):
        TESTS_PASSED += 1
    else:
        TESTS_FAILED += 1


def print_results_table(host_results, zeal_results, zeal_messages) -> None:
    tests = sorted({*host_results.keys(), *zeal_results.keys()})
    header_test = "test"
    header_host = "host"
    header_zeal = "zeal-native"
    test_width = max(len(header_test), max((len(t) for t in tests), default=0))
    host_width = max(len(header_host), len("Unexpected"))
    zeal_width = max(len(header_zeal), len("Unexpected"))

    print(f"{header_test:<{test_width}} | {header_host:<{host_width}} | {header_zeal:<{zeal_width}}")
    print(f"{'-' * test_width}-+-{'-' * host_width}-+-{'-' * zeal_width}")

    for test in tests:
        host_status = host_results.get(test, "missing")
        zeal_status = zeal_results.get(test, "missing")
        update_counts(host_status)
        update_counts(zeal_status)
        host_label = format_status(host_status, host_width)
        zeal_label = format_status(zeal_status, zeal_width)
        print(f"{test:<{test_width}} | {host_label} | {zeal_label}")
        if host_status in ("fail", "unexpected_pass"):
            print(f"  > host: compile failed")
        if zeal_status in ("fail", "unexpected_pass"):
            msg = zeal_messages.get(test, "failure")
            print(f"  > zeal-native: {msg}")

def run_cmd(cmd, quiet=False):
    try:
        if quiet:
            return subprocess.run(cmd, check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return subprocess.run(cmd, check=False)
    except FileNotFoundError:
        return None


def run_cmd_capture(cmd):
    try:
        return subprocess.run(cmd, check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    except FileNotFoundError:
        return None


def host_arch() -> str:
    sys_name = platform.system()
    if sys_name == "Darwin":
        return "darwin"
    if sys_name == "Linux":
        return "linux"
    return sys_name


def test_compile(compiler: str, test_file: Path, test_name: str) -> None:
    if not test_file.exists():
        print(f"{YELLOW}⚠{NC}  Skipping {test_name} (file not found: {test_file})")
        return
    asm_file = test_file.with_suffix(".asm")
    result = run_cmd([compiler, str(test_file), str(asm_file)])
    status = 1 if result is None else result.returncode
    stem = test_file.stem
    expected = EXPECTED_RESULTS.get(stem)
    if stem in EXPECTED_RESULTS and expected is None:
        if status == 0:
            print_result(f"{test_name} (unexpected pass)", 1)
        else:
            print_expected_fail(f"{test_name} (expected fail)")
        return
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

def clean_test_artifacts() -> None:
    tests_dir = Path("tests")
    if not tests_dir.exists():
        return
    for pattern in ("*.asm", "*.bin", "*.ast"):
        for path in tests_dir.glob(pattern):
            path.unlink(missing_ok=True)


def parse_return_results(log: str):
    exec_file = ""
    current_test = ""
    pairs = []
    exec_re = re.compile(r"Exec\s+'([^']+)'")
    return_re = re.compile(r"Returned\s+\$([0-9A-Fa-f]+)")
    test_re = re.compile(r"TEST:\s+(\S+)")
    for raw in log.splitlines():
        line = raw.rstrip("\r").lstrip()
        test_match = test_re.search(line)
        if test_match:
            current_test = test_match.group(1)
            continue
        exec_match = exec_re.search(line)
        if exec_match:
            exec_file = exec_match.group(1)
            continue
        ret_match = return_re.search(line)
        if ret_match:
            hex_val = ret_match.group(1)
            if hex_val:
                path = exec_file or current_test
                if path:
                    pairs.append((normalize_test_path(path), hex_val))
                    exec_file = ""

    if not pairs:
        return []

    results = []
    for path, hex_val in pairs:
        base = Path(path).stem
        expected = EXPECTED_RESULTS.get(base, "")
        hex_val = hex_val.upper()
        ok = True
        if base in EXPECTED_RESULTS and expected is None:
            ok = False
        elif expected and hex_val != expected:
            ok = False
        results.append((path, expected, hex_val, ok))
    return results


def normalize_failure_path(path: str, current_test: str) -> str:
    if current_test:
        return normalize_test_path(current_test)
    normalized = normalize_test_path(path)
    if normalized.endswith((".ast", ".asm", ".bin")):
        return str(Path(normalized).with_suffix(".c"))
    return normalized


def parse_compile_failures(log: str):
    failures = []
    seen = set()
    test_re = re.compile(r"TEST:\s+(\S+)")
    patterns = [
        ("parse", re.compile(r"Failed to parse\s+(\S+)")),
        ("codegen", re.compile(r"Failed to codegen\s+(\S+)")),
        ("assemble", re.compile(r"Failed to assemble\s+(\S+)")),
        ("compile", re.compile(r"Failed to compile\s+(\S+)")),
    ]
    current_test = ""
    for raw in log.splitlines():
        line = raw.rstrip("\r").lstrip()
        test_match = test_re.search(line)
        if test_match:
            current_test = test_match.group(1)
            continue
        for reason, pattern in patterns:
            match = pattern.search(line)
            if match:
                path = normalize_failure_path(match.group(1), current_test)
                if path not in seen:
                    failures.append((path, reason))
                    seen.add(path)
                current_test = ""
                break
    return failures


def normalize_test_path(path: str) -> str:
    if path.startswith("h:/tests/"):
        return "tests/" + path[len("h:/tests/"):]
    if path.startswith("H:/tests/"):
        return "tests/" + path[len("H:/tests/"):]
    return path


def normalize_test_case(path: str) -> str:
    normalized = normalize_test_path(path)
    if normalized.endswith((".ast", ".asm", ".bin")):
        return str(Path(normalized).with_suffix(".c"))
    if normalized.endswith(".c"):
        return normalized
    return str(Path(normalized).with_suffix(".c"))


def parse_zeal_test_results(log: str):
    results = {}
    messages = {}
    failures = parse_compile_failures(log)
    for path, reason in failures:
        path = normalize_test_case(path)
        stem = Path(path).stem
        expected = EXPECTED_RESULTS.get(stem)
        if stem in EXPECTED_RESULTS and expected is None:
            results[path] = "expected_fail"
            messages[path] = f"failed to {reason} (expected)"
        else:
            results[path] = "fail"
            messages[path] = f"failed to {reason}"

    returns = parse_return_results(log)
    for path, expected, actual, ok in returns:
        path = normalize_test_case(path)
        if path in results and results[path] in ("fail", "expected_fail"):
            continue
        if Path(path).stem in EXPECTED_RESULTS and expected is None:
            results[path] = "unexpected_pass"
            messages[path] = f"unexpected pass (returned ${actual})"
            continue
        if ok:
            results[path] = "ok"
        else:
            results[path] = "fail"
            if expected:
                messages[path] = f"expected ${expected}, got ${actual}"
            else:
                messages[path] = f"returned ${actual}"
    return results, messages


def parse_host_test_results(log: str, all_tests: list[str] | None = None):
    tests = []
    results = {}
    test_re = re.compile(r"TEST:\s+(\S+)")
    fail_re = re.compile(r"Failed to compile\s+(\S+)")
    ok_re = re.compile(r"OK:\s+(\S+)")
    for raw in log.splitlines():
        line = raw.rstrip("\r").lstrip()
        test_match = test_re.search(line)
        if test_match:
            path = normalize_test_path(test_match.group(1))
            tests.append(path)
            continue
        ok_match = ok_re.search(line)
        if ok_match:
            path = normalize_test_path(ok_match.group(1))
            results[path] = "ok"
            continue
        fail_match = fail_re.search(line)
        if fail_match:
            path = normalize_test_path(fail_match.group(1))
            results[path] = "fail"
            continue
        if line == "Out of memory":
            last = tests[-1] if tests else ""
            if last:
                results[last] = "fail"
    if not tests and all_tests:
        tests = list(all_tests)
    output = []
    seen = set()
    for path in tests:
        if path in seen:
            continue
        seen.add(path)
        status = results.get(path)
        if status is None:
            status = "missing"
        stem = Path(path).stem
        expected = EXPECTED_RESULTS.get(stem)
        if status == "ok" and stem in EXPECTED_RESULTS and expected is None:
            status = "unexpected_pass"
        if status == "fail" and stem in EXPECTED_RESULTS and expected is None:
            status = "expected_fail"
        output.append((path, status))
    return output


def run_headless_emulator(
    img: str,
    eeprom: str,
    test_name: str,
    show_log: bool = False,
    log_path: Path | None = None,
) -> tuple[int | None, str]:
    proc = None
    streaming = show_log or log_path
    if shutil.which("zeal-native") is None:
        print(f"{YELLOW}⚠{NC}  Skipping {test_name} (zeal-native not found)")
        return None, ""
    if not Path(img).exists() or not Path(eeprom).exists():
        print(f"{YELLOW}⚠{NC}  Skipping {test_name} (image(s) missing)")
        return None, ""

    try:
        zealasm_src = Path(".zeal8bit/zealasm")
        zealasm_dst = Path("bin/zealasm")
        if zealasm_src.exists() and zealasm_src.is_file():
            shutil.copyfile(zealasm_src, zealasm_dst)

        if streaming:
            proc = subprocess.Popen(
                ["zeal-native", "--headless", "--no-reset", "-r", img, "-e", eeprom],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
            )
            log_parts = []
            start = time.monotonic()
            output = proc.stdout
            log_handle = log_path.open("w", encoding="utf-8") if log_path else None
            try:
                while True:
                    elapsed = time.monotonic() - start
                    if elapsed > HEADLESS_TIMEOUT_SEC:
                        raise TimeoutError
                    if output is None:
                        break
                    ready, _, _ = select.select([output], [], [], 0.1)
                    if ready:
                        line = output.readline()
                        if line:
                            log_parts.append(line)
                            if show_log:
                                print(line, end="")
                            if log_handle:
                                log_handle.write(line)
                                log_handle.flush()
                            continue
                    if proc.poll() is not None:
                        break
                if output is not None:
                    remainder = output.read()
                    if remainder:
                        log_parts.append(remainder)
                        if show_log:
                            print(remainder, end="")
                        if log_handle:
                            log_handle.write(remainder)
            finally:
                if log_handle:
                    log_handle.close()
            status = proc.wait()
            log = "".join(log_parts)
        else:
            proc = subprocess.run(
                ["zeal-native", "--headless", "--no-reset", "-r", img, "-e", eeprom],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=False,
                timeout=HEADLESS_TIMEOUT_SEC,
            )
            log = proc.stdout or ""
            status = proc.returncode
    except TimeoutError:
        print(
            f"{RED}X{NC} {test_name} timed out after "
            f"{HEADLESS_TIMEOUT_SEC}s (possible hang/reset loop)"
        )
        try:
            proc.kill()
            proc.wait(timeout=1)
        except Exception:
            pass
        return 1, ""
    except Exception as exc:
        print(f"{RED}X{NC} {test_name} failed to run zeal-native: {exc}")
        return 1, ""

    if not streaming:
        if log_path:
            log_path.write_text(log)
        if show_log and log:
            print(log)

    return status, log


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run Zeal C compiler tests")
    parser.add_argument(
        "--headless-only",
        action="store_true",
        help="Only run the Zeal-native headless test",
    )
    parser.add_argument(
        "--headless-log",
        action="store_true",
        help="Print the Zeal-native headless output",
    )
    parser.add_argument(
        "--headless-log-file",
        type=Path,
        help="Write the Zeal-native headless output to this file",
    )
    args = parser.parse_args(argv)
    print(f"{BLUE}╔════════════════════════════════════════╗{NC}")
    print(f"{BLUE}║     Zeal C Compiler Test Suite         ║{NC}")
    print(f"{BLUE}╚════════════════════════════════════════╝{NC}")

    print_header("Building ZOS Target")
    zos_build = run_cmd(["zde", "cmake"], quiet=True)
    zos_status = 1 if zos_build is None else zos_build.returncode
    print_result("ZOS compilation (bin/cc)", zos_status)

    mac_status = 0
    arch = host_arch()
    cc_parse = Path(f"bin/cc_parse_{arch}")
    cc_codegen = Path(f"bin/cc_codegen_{arch}")
    host_results = {}
    zeal_results = {}
    zeal_messages = {}

    if not args.headless_only:
        print_header("Building Host Target")
        run_cmd(["make", "clean"], quiet=True)
        mac_build = run_cmd(["make"], quiet=True)
        mac_status = 1 if mac_build is None else mac_build.returncode
        print_result("Host compilation (make)", mac_status)

        print_header("Running Host Tests")
        if cc_parse.exists() and cc_codegen.exists():
            clean_test_artifacts()
            host_tests = run_cmd_capture(["./test.sh"])
            if host_tests is None:
                print_result("Host test.sh", 1)
            else:
                host_log = (host_tests.stdout or "") + (host_tests.stderr or "")
                all_tests = [str(p) for p in sorted(Path("tests").glob("*.c"))]
                results = parse_host_test_results(host_log, all_tests=all_tests)
                if results:
                    host_results = {path: status for path, status in results}
                else:
                    print_result("Host test.sh", host_tests.returncode)
        else:
            print(f"{RED}✗{NC} host binaries not found, skipping tests")

        print_header("Verifying Build Artifacts")
        print_result("ZOS binary exists (bin/cc)", 0 if Path("bin/cc").exists() else 1)
        print_result(f"Host cc_parse exists ({cc_parse})", 0 if cc_parse.exists() else 1)
        print_result(f"Host cc_codegen exists ({cc_codegen})", 0 if cc_codegen.exists() else 1)
        print_result("ZOS debug symbols exist", 0 if Path("debug/cc.cdb").exists() else 1)

    print_header("Zeal-native Headless Smoke Test")
    if zos_status == 0:
        clean_test_artifacts()
        ensure_reset_in_test_zs()
        zeal_status, zeal_log = run_headless_emulator(
            ".zeal8bit/headless.img",
            ".zeal8bit/eeprom.img",
            "zeal-native headless boot",
            show_log=args.headless_log,
            log_path=args.headless_log_file,
        )
        if zeal_status is not None:
            print_result("zeal-native headless boot", zeal_status)
        if zeal_log:
            zeal_results, zeal_messages = parse_zeal_test_results(zeal_log)
        comment_reset_in_test_zs()
    else:
        print(f"{YELLOW}⚠{NC}  Skipping zeal-native headless boot (ZOS build failed)")

    print_header("Test Results")
    print_results_table(host_results, zeal_results, zeal_messages)

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
