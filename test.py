#!/usr/bin/env python3
"""Host generation checks and ZOS assembly/execution regression tests."""
import guest
import stage as distribution
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
    "while": 0x0A, "zealos": 0xEA, "literals": 0xA7, "bad_literal": "parse",
    "nested": 33, "truth": 41, "widen": 42,
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
        binary = str(Path(path).with_suffix(".bin"))
        successes = [f"{name}.c -> cc-work/input.ast", "cc-work/input.ast OK",
                     "cc-work/input.ast -> cc-work/output.asm", f"Exec '{binary}'"]
        position, failure, returned, problem = 0, None, None, None
        for line in lines:
            if failure and line:
                problem = "unexpected output after driver failure"
            if line in successes:
                index = successes.index(line)
                if index != position:
                    problem = "missing, duplicate, or out-of-order stage result"
                position = index + 1
            elif line.startswith("Exec '"):
                problem = "execution belongs to another test"
            if line.startswith("cc: failed "):
                match = re.fullmatch(r"cc: failed /bin/cc_(parse|semantic|codegen) status 1", line)
                if failure or not match:
                    problem = "unexpected stage failure"
                failure = match[1] if match else "unknown"
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
            prior_successes = successes[:required_position]
            seen_successes = [line for line in lines if line in successes[:3]]
            if not problem and failure == expected and diagnostic in lines and seen_successes == prior_successes and returned is None and position == required_position:
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


def run_headless_emulator(work, test_name, show_log=False, log_path=None, timeout=HEADLESS_TIMEOUT_SEC):
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
        command = guest.command(work)
        proc = subprocess.Popen(command, cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
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
        message = f"\n{test_name} failed: {exc}\n"
        record(message)
        if not show_log:
            print(message, end="", file=sys.stderr)
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


def check_driver_failures(work, batch):
    """Exercise installed components and verify an existing output survives each failure."""
    host = work / "host"
    output = host / "tests/simple_return.bin"
    previous = output.read_bytes()
    if not previous:
        raise ValueError("Missing executable before driver failure checks")
    for missing, stage in (("bin/cc_semantic", "cc_semantic"), ("runtime/crt0.asm", "cc_codegen"),
                           ("bin/zealasm", "zealasm")):
        component = host / missing
        component.rename(work / "removed")
        guest.script(work, ["cd h:/tests", "cc simple_return.c", ": echo EXPECTED-FAILURE"])
        try:
            status, log = batch(work, stage)
            if (status or f"cc: failed /bin/{stage} status " not in log
                    or "EXPECTED-FAILURE" not in log.splitlines() or output.read_bytes() != previous
                    or (host / "tests/cc-work").exists()):
                raise ValueError(f"Driver failure check failed for {missing}")
        finally:
            (work / "removed").rename(component)

    reserved = host / "tests/cc-work"
    reserved.mkdir()
    marker = reserved / "owner"
    marker.write_bytes(b"reserved")
    guest.script(work, ["cd h:/tests", "cc simple_return.c", ": echo RESERVED"])
    status, log = batch(work, "reserved")
    if (status or "RESERVED" not in log.splitlines() or "cc: cannot create cc-work" not in log
            or output.read_bytes() != previous or marker.read_bytes() != b"reserved"):
        raise ValueError("Driver touched an existing workspace or output")
    marker.unlink()
    reserved.rmdir()

    directory = host / "tests/directory.asm"
    directory.mkdir()
    guest.script(work, ["cd h:/tests", "cc simple_return.c directory.asm", ": echo DIRECTORY"])
    status, log = batch(work, "directory")
    if (status or "DIRECTORY" not in log.splitlines() or "cc: could not publish output" not in log
            or not directory.is_dir() or reserved.exists()):
        raise ValueError("Driver did not preserve an output directory")

    source = host / "tests/simple_return.c"
    source.write_text("int main() { return 93; }\n")
    guest.script(work, ["cd h:/tests", "cc -S simple_return.c", "? cc simple_return.c h:/tests/listing.asm",
                        "? cc simple_return.c", "? return h:/tests/simple_return.bin", "? echo REBUILT"])
    status, log = batch(work, "rebuild")
    asm = (host / "tests/simple_return.asm").read_bytes()
    rebuilt = output.read_bytes()
    if (status or "REBUILT" not in log.splitlines() or "Returned  $93" not in log.splitlines()
            or not asm or asm != (host / "tests/listing.asm").read_bytes() or rebuilt == previous):
        raise ValueError("Assembly or executable rebuild failed")

    # Disk B is the private ZealFS EEPROM. First publish to a missing path.
    guest.script(work, ["cd h:/tests", "cc simple_return.c b:/listing.asm", "? echo EEPROM"])
    status, log = batch(work, "eeprom")
    disk = ["zdt", "--yes", "-i", str(work / "eeprom.img")]
    saved = work / "listing.asm"
    if status or "EEPROM" not in log.splitlines():
        raise ValueError("Could not publish assembly to ZealFS")
    subprocess.run([*disk, "export", "/listing.asm", saved], check=True)
    if saved.read_bytes() != asm:
        raise ValueError("ZealFS output differs from HostFS assembly")

    source.write_text("int main( {\n")
    guest.script(work, ["cd h:/tests", "cc simple_return.c", ": echo REJECTED"])
    status, log = batch(work, "rejected-rebuild")
    if (status or "REJECTED" not in log.splitlines() or "cc: failed /bin/cc_parse status 1" not in log
            or output.read_bytes() != rebuilt or (host / "tests/cc-work").exists()):
        raise ValueError("Rejected rebuild failed to preserve the last successful binary")


def target_run(args):
    cases = [args.debug_case] if args.debug_case else TEST_CASES
    with termination_signals(), guest.workspace(cases) as work:
        def batch(work, label="batch"):
            return run_headless_emulator(work, "ZOS execution",
                                         args.headless_log, (args.headless_log_file if label == "batch" and args.headless_log_file else work / f"{label}.log"))

        cmd = guest.command(work)
        version = subprocess.check_output([cmd[0], "--version"], text=True).strip()
        (work / "launch.txt").write_text(version + "\n" + " ".join(cmd) + "\n")
        print(version)
        if args.debug_case:
            guest.debug(work, args.debug_case, args.stage, args.debug_smoke, batch)
            return
        binary_fixture = work / "host/tests/bytes"
        binary_fixture.mkdir()
        data = bytes(range(240, 256)) + b"\x00"
        (binary_fixture / "sample.dat").write_bytes(data)
        commands = ["cd h:/tests"]
        for name in cases:
            commands += [f"echo TEST: h:/tests/{name}.c", f"cc {name}.c",
                         f"? return h:/tests/{name}.bin"]
        # The end marker separates corpus attribution from the installed example.
        commands += ["echo !!! Complete !!!", "cd h:/examples/hexdump", "cc hexdump.c",
                     "? return h:/examples/hexdump/hexdump.bin", "cd h:/tests/bytes",
                     "return h:/examples/hexdump/hexdump.bin", "cd h:/tests",
                     "return h:/examples/hexdump/hexdump.bin", "? echo EXAMPLE-END"]
        guest.script(work, commands)
        status, log = batch(work)
        results, messages = parse_zeal_test_results(log)
        print_results("ZOS assembly and execution", results, messages)
        example = log.partition("!!! Complete !!!")[2].replace("\r", "")
        golden = "Exec 'h:/examples/hexdump/hexdump.bin'\n5A 65 61 6C 0A\nReturned  $0"
        binary_dump = data[:16].hex(" ").upper() + "\n00\nReturned  $0"
        missing = "Exec 'h:/examples/hexdump/hexdump.bin'\nCannot open sample.dat\nReturned  $2"
        if (status or any(v not in ("ok", "expected_fail") for v in results.values())
                or not (0 <= example.find(golden) < example.find(binary_dump) < example.find(missing) < example.find("EXAMPLE-END"))):
            raise ValueError("Installed target acceptance failed")
        check_driver_failures(work, batch)
        print(f"{len(results)} target cases, hex dump and driver failure checks passed")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--host-only", action="store_true", help="Check host generation only; no ZOS build/execution")
    modes.add_argument("--headless-only", action="store_true", help="Build and test ZOS only")
    parser.add_argument("--no-build", action="store_true", help="Explicitly use existing compiler binaries")
    parser.add_argument("--headless-log", action="store_true")
    parser.add_argument("--headless-log-file", type=Path)
    parser.add_argument("--debug-case", choices=TEST_CASES)
    parser.add_argument("--stage", choices=STAGES, default="parse")
    parser.add_argument("--debug-smoke", action="store_true")
    args = parser.parse_args(argv)
    if args.host_only and (args.debug_case or args.debug_smoke):
        parser.error("--host-only cannot run the guest debugger")
    if args.debug_smoke:
        args.debug_case, args.stage = "simple_return", "parse"
    if args.debug_case:
        args.headless_only = True
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
                    status = subprocess.run(command, check=False, env={**os.environ, "ZDE_STRICT_EXIT": "1"}).returncode
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
            if not args.no_build:
                distribution.stage_dist()
            target_run(args)
            checks.append(True)
        failed = sum(value not in ("ok", "expected_fail") for value in results)
        if results:
            print(f"\n{len(results)} host outcomes; {failed} failed or missing")
        return 0 if failed == 0 and all(checks) else 1
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f"Test infrastructure failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
