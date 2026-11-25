#!/usr/bin/env python3
"""Host generation checks and ZOS assembly/execution regression tests."""
import argparse
import codecs
from contextlib import contextmanager
import os
from pathlib import Path
import platform
import re
import selectors
import shutil
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent
HEADLESS_TIMEOUT_SEC = 120
# One manifest for both runners: integer = ZOS exit byte; string = rejection stage.
TEST_CASES = {
    "break": 0xB1, "assign": 0x15, "array": 0xEF, "char": 0x41,
    "comp": 0x4E, "compares": 0x3F, "do_while": "parse", "expr": 0x1C,
    "for": 0x0A, "goto": 0xB2, "global": 0x0A, "if": 0x2A,
    "bitwise": 0xE4, "math": 0x3A, "params": 0x14, "pointer": 0x86,
    "simple_return": 0x0C, "return16": 0xEF, "struct": "parse", "signs": 0xEE,
    "semantic": "semantic", "ternary": "parse", "unary": 0xAA,
    "while": 0x0A, "zealos": 0xEA,
}
STAGES = ("parse", "semantic", "codegen")
REJECTION_DIAGNOSTICS = {"parse": "Parsing failed", "semantic": "Semantic validation failed"}


def host_arch():
    return {"Darwin": "darwin", "Linux": "linux"}.get(platform.system(), platform.system())


def clean_test_artifacts(cases=TEST_CASES):
    for name in cases:
        for suffix in (".ast", ".asm", ".bin"):
            Path(f"tests/{name}{suffix}").unlink(missing_ok=True)


def validate_inventory():
    sources = {p.stem for p in Path("tests").glob("*.c")}
    if sources != set(TEST_CASES):
        raise ValueError(f"Test manifest/source mismatch: {sorted(sources ^ set(TEST_CASES))}")
    script_cases = re.findall(r"^h:/tests/(\w+)\.zs$", Path("test.zs").read_text(), re.M)
    if len(script_cases) != len(TEST_CASES) or set(script_cases) != set(TEST_CASES):
        raise ValueError("test.zs must run every manifest case exactly once")
    for name in TEST_CASES:
        if not Path(f"tests/{name}.zs").is_file():
            raise ValueError(f"Missing target test script: tests/{name}.zs")


def run_host_tests(cases=TEST_CASES):
    """Observe child exits directly; a diagnostic or partial log cannot pass a case."""
    clean_test_artifacts(cases)
    results, messages = {}, {}
    for name, expected in cases.items():
        path = f"tests/{name}.c"
        ast, asm = f"tests/{name}.ast", f"tests/{name}.asm"
        results[path] = "missing"
        commands = ((path, ast), (ast,), (ast, asm))
        for stage, args in zip(STAGES, commands):
            try:
                proc = subprocess.run(
                    [f"bin/cc_{stage}_{host_arch()}", *args],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                    timeout=HEADLESS_TIMEOUT_SEC, check=False,
                )
            except (OSError, subprocess.TimeoutExpired) as exc:
                results[path], messages[path] = "fail", f"{stage}: {exc}"
                break
            if proc.returncode != 0:
                diagnostic = REJECTION_DIAGNOSTICS.get(stage, "")
                if expected == stage and proc.returncode == 1 and diagnostic and diagnostic in proc.stdout:
                    results[path], messages[path] = "expected_fail", f"rejected by {stage} as expected"
                else:
                    results[path], messages[path] = "fail", f"{stage} exited {proc.returncode}: {proc.stdout.strip()}"
                break
            if expected == stage:
                results[path], messages[path] = "unexpected_pass", f"{stage} accepted an expected rejection"
                break
        else:
            results[path] = "ok"
    return results, messages


def parse_zeal_test_results(log, cases=TEST_CASES):
    """Require ordered stage successes and an attributed decimal ZOS exit byte."""
    results = {f"tests/{name}.c": "missing" for name in cases}
    messages = {path: "no test result" for path in results}
    records, current = {}, None
    complete = False
    global_errors = []
    ansi = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
    for raw in log.splitlines():
        line = re.sub(r"[hH]:/tests/", "tests/", ansi.sub("", raw).strip())
        if line.startswith("TEST: "):
            path = line[6:]
            if complete or path in records or path not in results:
                global_errors.append(f"unexpected or duplicate test marker: {path}")
                current = None
                continue
            current = path
            records[path] = []
        elif line == "!!! Complete !!!":
            if complete:
                global_errors.append("duplicate suite completion marker")
            complete = True
            current = None
        elif current:
            records[current].append(line)
    if not complete:
        global_errors.append("missing suite completion marker")
    for path, lines in records.items():
        name = Path(path).stem
        expected = cases[name]
        ast = str(Path(path).with_suffix(".ast"))
        asm = str(Path(path).with_suffix(".asm"))
        binary = str(Path(path).with_suffix(".bin"))
        successes = [f"{path} -> {ast}", f"{ast} OK", f"{ast} -> {asm}", f"Exec '{binary}'"]
        position, failure, returned, problem = 0, None, None, None
        for line in lines:
            if line in successes:
                index = successes.index(line)
                if index == 3 and failure is not None and isinstance(expected, str):
                    continue  # coreutils prints Exec before reporting a missing binary.
                if index != position:
                    problem = "missing, duplicate, or out-of-order stage result"
                position = index + 1
            elif line.startswith("Exec '"):
                problem = "execution belongs to another test"
            match = re.fullmatch(r"Failed to (parse|validate|codegen|assemble|compile) (\S+)", line)
            if match and failure is None:
                stage = {"validate": "semantic"}.get(match[1], match[1])
                target_path = str(Path(match[2]).with_suffix(".c"))
                if target_path != path:
                    problem = "failure belongs to another test"
                failure = stage
            if line.startswith("Returned"):
                match = re.fullmatch(r"Returned\s+\$(\d+)", line)
                if match is None or returned is not None or position != len(successes):
                    problem = "invalid, duplicate, or unattributed execution result"
                else:
                    returned = int(match[1], 10)  # coreutils return uses put_u8, despite '$'.
                    if not 0 <= returned <= 255:
                        problem = "execution result is outside the ZOS exit-byte range"
        if isinstance(expected, str):
            required_position = STAGES.index(expected)
            diagnostic = REJECTION_DIAGNOSTICS[expected]
            # Later cascading errors/attempted execs are allowed; successful later stages are not.
            prior_successes = successes[:required_position]
            seen_successes = [line for line in lines if line in successes[:3]]
            if not problem and failure == expected and diagnostic in lines and seen_successes == prior_successes and returned is None:
                results[path], messages[path] = "expected_fail", f"rejected by {expected} as expected"
            else:
                results[path], messages[path] = "fail", f"expected rejection by {expected}, observed {failure or 'no rejection'}"
        elif not problem and failure is None and position == len(successes) and returned == expected:
            results[path], messages[path] = "ok", f"returned {returned} (0x{returned:02X})"
        else:
            results[path] = "fail"
            messages[path] = problem or (f"stage failure: {failure}" if failure else f"expected {expected}, got {returned}")
    if global_errors:
        results["suite"] = "fail"
        messages["suite"] = "; ".join(global_errors)
    return results, messages


@contextmanager
def termination_signals():
    """Turn catchable process termination into normal stack unwinding."""
    def terminate(signum, frame):
        raise SystemExit(128 + signum)

    previous = {}
    try:
        for signum in (signal.SIGTERM, signal.SIGHUP):
            if signal.getsignal(signum) == signal.SIG_IGN:
                continue
            previous[signum] = signal.signal(signum, terminate)
        yield
    finally:
        for signum, handler in previous.items():
            signal.signal(signum, handler)


@contextmanager
def reset_test_script():
    """Restore the exact original bytes even on timeout, exceptions, or interruption."""
    path = Path("test.zs")
    original = path.read_bytes()
    lines = original.decode().splitlines()
    while lines and (not lines[-1].strip() or lines[-1].strip().lstrip(";").strip() == "reset"):
        lines.pop()
    try:
        path.write_text("\n".join([*lines, "reset"]) + "\n")
        yield
    finally:
        path.write_bytes(original)


def run_headless_emulator(img, eeprom, test_name, show_log=False, log_path=None, timeout=HEADLESS_TIMEOUT_SEC):
    executable = shutil.which("zeal-headless")
    proc, log_handle, chunks = None, None, []
    decoder = codecs.getincrementaldecoder("utf-8")("replace")

    def record(text):
        chunks.append(text)
        if show_log:
            print(text, end="", flush=True)
        if log_handle:
            log_handle.write(text)
            log_handle.flush()

    try:
        if log_path:
            log_handle = Path(log_path).open("w", encoding="utf-8")
        if not executable:
            message = f"{test_name}: zeal-headless not found on PATH\n"
        elif missing := [str(path) for path in (img, eeprom) if not Path(path).is_file()]:
            message = f"{test_name}: missing image(s): {', '.join(missing)}\n"
        else:
            message = None
        if message:
            record(message)
            if not show_log:
                print(message, end="", file=sys.stderr)
            return 1, "".join(chunks)
        command = [executable, "--headless", "--no-reset", "-H", str(ROOT), "-r", img, "-e", eeprom]
        # Avoid the emulator's user-global TF default; the headless kernel disables TF.
        if Path(".zeal8bit/tf.img").is_file():
            command += ["-t", ".zeal8bit/tf.img"]
        proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
        os.set_blocking(proc.stdout.fileno(), False)
        deadline = time.monotonic() + timeout
        with selectors.DefaultSelector() as selector:
            selector.register(proc.stdout, selectors.EVENT_READ)
            while selector.get_map():
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise subprocess.TimeoutExpired(command, timeout)
                for key, _ in selector.select(min(remaining, 0.1)):
                    data = os.read(key.fd, 65536)
                    if not data:
                        selector.unregister(key.fileobj)
                    else:
                        record(decoder.decode(data))
            record(decoder.decode(b"", final=True))
        return proc.wait(timeout=max(0.001, deadline - time.monotonic())), "".join(chunks)
    except subprocess.TimeoutExpired:
        record(f"\n{test_name} timed out after {timeout}s\n")
        return 1, "".join(chunks)
    except OSError as exc:
        record(f"\n{test_name} failed: {exc}\n")
        return 1, "".join(chunks)
    finally:
        if proc:
            # Own process group only; also closes pipes held open by descendants.
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()
            proc.stdout.close()
        if log_handle:
            log_handle.close()


def print_results(label, results, messages):
    print(f"\n{label}")
    for path, status in results.items():
        print(f"{status:16} {path}: {messages.get(path, '')}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--host-only", action="store_true", help="Check host generation only; no ZOS build/execution")
    modes.add_argument("--headless-only", action="store_true", help="Build and test ZOS only")
    parser.add_argument("--no-build", action="store_true", help="Explicitly use existing compiler binaries")
    parser.add_argument("--headless-log", action="store_true")
    parser.add_argument("--headless-log-file", type=Path)
    args = parser.parse_args(argv)
    os.chdir(ROOT)
    results, checks = [], []
    try:
        validate_inventory()
        for label, enabled, command in (
            ("Host build", not args.headless_only, ["make", "all"]),
            ("ZOS build", not args.host_only, ["zde", "--no-tty", "cmake"]),
        ):
            if enabled and not args.no_build:
                try:
                    status = subprocess.run(command, check=False).returncode
                except OSError as exc:
                    print(f"{label}: {exc}")
                    status = 1
                checks.append(status == 0)
                if status != 0:
                    return 1  # Never run stale artifacts after a failed build.
        if not args.headless_only:
            host, messages = run_host_tests()
            print_results("Host generation (no execution)", host, messages)
            results.extend(host.values())
        if not args.host_only:
            for name in STAGES:
                if not Path(f"bin/cc_{name}").is_file():
                    raise ValueError(f"Missing ZOS stage: bin/cc_{name}")
            if Path(".zeal8bit/zealasm").is_file():
                shutil.copyfile(".zeal8bit/zealasm", "bin/zealasm")
            clean_test_artifacts()
            with termination_signals(), reset_test_script():
                status, log = run_headless_emulator(
                    ".zeal8bit/headless.img", ".zeal8bit/eeprom.img", "ZOS execution",
                    args.headless_log, args.headless_log_file,
                )
            checks.append(status == 0)
            zeal, messages = parse_zeal_test_results(log)
            print_results("ZOS assembly and execution", zeal, messages)
            results.extend(zeal.values())
            if status != 0:
                print(f"Emulator failed (exit {status}); see --headless-log or --headless-log-file")
        failed = sum(value not in ("ok", "expected_fail") for value in results)
        print(f"\n{len(results)} test outcomes; {failed} failed or missing")
        return 0 if results and failed == 0 and all(checks) else 1
    except (OSError, ValueError) as exc:
        print(f"Test infrastructure failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
