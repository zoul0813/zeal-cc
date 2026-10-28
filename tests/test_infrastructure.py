"""Failure-oriented checks for the regression runner and stage I/O contracts."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import resource
import signal
import subprocess
import tempfile
import time
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("zeal_tests", ROOT / "test.py")
suite = importlib.util.module_from_spec(spec)
spec.loader.exec_module(suite)


def execution_log(name="simple_return", value="12"):
    return f"""TEST: h:/tests/{name}.c
h:/tests/{name}.c -> h:/tests/{name}.ast
h:/tests/{name}.ast OK
h:/tests/{name}.ast -> h:/tests/{name}.asm
Exec 'h:/tests/{name}.bin'
Returned  ${value}
!!! Complete !!!
"""


class ResultTests(unittest.TestCase):
    def test_decimal_return_format(self):
        results, _ = suite.parse_zeal_test_results(execution_log(), {"simple_return": 12})
        self.assertEqual(results, {"tests/simple_return.c": "ok"})
        results, _ = suite.parse_zeal_test_results(execution_log(value="177"), {"simple_return": 0xB1})
        self.assertEqual(results["tests/simple_return.c"], "ok")

    def test_incomplete_or_inconsistent_execution_fails(self):
        good = execution_log()
        variants = {
            "empty": "",
            "no completion": good.replace("!!! Complete !!!", ""),
            "compile only": good.replace("Returned  $12\n", ""),
            "no execution attribution": good.replace("Exec 'h:/tests/simple_return.bin'\n", ""),
            "wrong binary": good.replace("simple_return.bin", "other.bin"),
            "missing semantic": good.replace("h:/tests/simple_return.ast OK\n", ""),
            "wrong return": good.replace("$12", "$13"),
            "hex format": good.replace("$12", "$0C"),
            "overflow": good.replace("$12", "$268"),
            "duplicate return": good.replace("Returned  $12", "Returned  $12\nReturned  $12"),
            "duplicate test": good.replace("!!! Complete !!!", good),
            "failure then success": good.replace("h:/tests/simple_return.ast OK", "Failed to validate h:/tests/simple_return.ast\nh:/tests/simple_return.ast OK"),
        }
        for label, log in variants.items():
            with self.subTest(label=label):
                results, _ = suite.parse_zeal_test_results(log, {"simple_return": 12})
                self.assertTrue(any(status not in ("ok", "expected_fail") for status in results.values()))

    def test_manifest_cases_cannot_disappear(self):
        results, _ = suite.parse_zeal_test_results(execution_log(), {"simple_return": 12, "array": 0xEF})
        self.assertEqual(results["tests/array.c"], "missing")

    def test_expected_rejection_requires_correct_stage_and_diagnostic(self):
        prefix = "TEST: h:/tests/semantic.c\nh:/tests/semantic.c -> h:/tests/semantic.ast\n"
        rejected = prefix + "Semantic validation failed\nFailed to validate h:/tests/semantic.ast\n!!! Complete !!!\n"
        results, _ = suite.parse_zeal_test_results(rejected, {"semantic": "semantic"})
        self.assertEqual(results["tests/semantic.c"], "expected_fail")
        for log in (rejected.replace("Semantic validation failed\n", ""), rejected.replace("Failed to validate", "Failed to parse"), execution_log("semantic")):
            results, _ = suite.parse_zeal_test_results(log, {"semantic": "semantic"})
            self.assertEqual(results["tests/semantic.c"], "fail")

    def test_expected_parse_failure_allows_cascading_missing_file_errors(self):
        log = """TEST: h:/tests/struct.c
Parsing failed
Failed to parse h:/tests/struct.c
Failed to validate h:/tests/struct.ast
Failed to codegen h:/tests/struct.ast
Exec 'h:/tests/struct.bin'
error occurred 0004
Failed to assemble h:/tests/struct.asm
!!! Complete !!!
"""
        results, _ = suite.parse_zeal_test_results(log, {"struct": "parse"})
        self.assertEqual(results["tests/struct.c"], "expected_fail")
        results, _ = suite.parse_zeal_test_results(log.replace("error occurred 0004", "Returned  $0"), {"struct": "parse"})
        self.assertEqual(results["tests/struct.c"], "fail")

    @mock.patch.object(suite, "clean_test_artifacts")
    def test_host_process_exit_is_authoritative(self, clean):
        for stage, expected in (("parse", "parse"), ("semantic", "semantic")):
            calls = [subprocess.CompletedProcess([], 0, "")] * suite.STAGES.index(stage)
            for code, diagnostic, status in ((1, suite.REJECTION_DIAGNOSTICS[stage], "expected_fail"), (-11, suite.REJECTION_DIAGNOSTICS[stage], "fail"), (1, "Failed to open input", "fail"), (0, "", "unexpected_pass")):
                with self.subTest(stage=stage, code=code, diagnostic=diagnostic):
                    with mock.patch.object(suite.subprocess, "run", side_effect=[*calls, subprocess.CompletedProcess([], code, diagnostic)]):
                        results, _ = suite.run_host_tests({"example": expected})
                        self.assertEqual(results["tests/example.c"], status)
        with mock.patch.object(suite.subprocess, "run", side_effect=FileNotFoundError("missing compiler")):
            results, _ = suite.run_host_tests({"example": 0})
            self.assertEqual(results["tests/example.c"], "fail")

    def test_missing_results_and_failed_emulator_change_main_exit(self):
        with contextlib.ExitStack() as stack:
            stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
            stack.enter_context(mock.patch.object(suite, "validate_inventory"))
            stack.enter_context(mock.patch.object(suite, "clean_test_artifacts"))
            stack.enter_context(mock.patch.object(suite, "reset_test_script", contextlib.nullcontext))
            stack.enter_context(mock.patch.object(Path, "is_file", return_value=True))
            stack.enter_context(mock.patch.object(suite.shutil, "copyfile"))
            stack.enter_context(mock.patch.object(suite, "run_host_tests", return_value=({"test": "missing"}, {})))
            self.assertEqual(suite.main(["--host-only", "--no-build"]), 1)
            with mock.patch.object(suite, "run_headless_emulator", return_value=(0, "!!! Complete !!!\n")):
                self.assertEqual(suite.main(["--headless-only", "--no-build"]), 1)
            with mock.patch.object(suite, "run_headless_emulator", return_value=(1, execution_log())), mock.patch.object(suite, "parse_zeal_test_results", return_value=({"test": "ok"}, {})):
                self.assertEqual(suite.main(["--headless-only", "--no-build"]), 1)

    def test_build_failure_never_runs_stale_compiler(self):
        with mock.patch.object(suite, "validate_inventory"), mock.patch.object(suite.subprocess, "run", return_value=subprocess.CompletedProcess([], 2)), mock.patch.object(suite, "run_host_tests") as run:
            self.assertEqual(suite.main(["--host-only"]), 1)
            run.assert_not_called()


class TemporaryArtifacts(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="infrastructure-", dir=ROOT / "tests")
        self.work = Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)


class RunnerProcessTests(TemporaryArtifacts):
    def test_inherited_ignored_signal_is_preserved(self):
        previous = signal.signal(signal.SIGHUP, signal.SIG_IGN)
        try:
            with suite.termination_signals():
                self.assertEqual(signal.getsignal(signal.SIGHUP), signal.SIG_IGN)
            self.assertEqual(signal.getsignal(signal.SIGHUP), signal.SIG_IGN)
        finally:
            signal.signal(signal.SIGHUP, previous)

    def test_termination_signals_restore_script_and_reap_emulator(self):
        executable = self.work / "zeal-native"
        executable.write_text("#!/usr/bin/env python3\nimport os,time\nfrom pathlib import Path\nPath('emulator.pid').write_text(str(os.getpid()))\ntime.sleep(30)\n")
        executable.chmod(0o755)
        image = self.work / "image"
        image.touch()
        script = self.work / "test.zs"
        original = b"cd h:/\r\n; reset\r\n"
        command = (
            "import runpy,signal; "
            "signal.signal(signal.SIGTERM,signal.SIG_DFL); "
            "signal.signal(signal.SIGHUP,signal.SIG_DFL); "
            f"s=runpy.run_path({str(ROOT / 'test.py')!r}); "
            "exec(\"with s['termination_signals'](), s['reset_test_script']():\\n"
            " s['run_headless_emulator']('image','image','test',timeout=10)\")"
        )
        for signum in (signal.SIGTERM, signal.SIGHUP):
            with self.subTest(signal=signum):
                script.write_bytes(original)
                marker = self.work / "emulator.pid"
                marker.unlink(missing_ok=True)
                env = {**os.environ, "PATH": str(self.work) + os.pathsep + os.environ["PATH"]}
                proc = subprocess.Popen([sys.executable, "-c", command], cwd=self.work, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                emulator_pid = None
                try:
                    deadline = time.monotonic() + 3
                    while not marker.exists() and time.monotonic() < deadline and proc.poll() is None:
                        time.sleep(0.01)
                    self.assertTrue(marker.exists(), "emulator never started")
                    emulator_pid = int(marker.read_text())
                    proc.send_signal(signum)
                    stdout, stderr = proc.communicate(timeout=3)
                    self.assertEqual(proc.returncode, 128 + signum, stdout + stderr)
                    self.assertEqual(script.read_bytes(), original)
                    with self.assertRaises(ProcessLookupError):
                        os.kill(emulator_pid, 0)
                finally:
                    if proc.poll() is None:
                        proc.kill()
                    proc.communicate()
                    if emulator_pid is not None:
                        try:
                            os.kill(emulator_pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass

    def test_reset_script_restored_on_interrupt(self):
        path = self.work / "test.zs"
        original = b"cd h:/\r\n; reset\r\n\r\n"
        path.write_bytes(original)
        with mock.patch.object(suite, "Path", return_value=path):
            with self.assertRaises(KeyboardInterrupt):
                with suite.reset_test_script():
                    self.assertTrue(path.read_bytes().endswith(b"reset\n"))
                    raise KeyboardInterrupt
        self.assertEqual(path.read_bytes(), original)

    def test_unterminated_output_cannot_bypass_timeout(self):
        executable = self.work / "zeal-native"
        executable.write_text("#!/usr/bin/env python3\nimport os,time\nos.write(1,b'partial')\ntime.sleep(5)\n")
        executable.chmod(0o755)
        image = self.work / "image"
        image.touch()
        for stream in (False, True):
            with self.subTest(stream=stream), mock.patch.dict(os.environ, {"PATH": str(self.work) + os.pathsep + os.environ["PATH"]}), contextlib.redirect_stdout(io.StringIO()):
                start = time.monotonic()
                code, log = suite.run_headless_emulator(str(image), str(image), "test", show_log=stream, timeout=0.3)
                self.assertNotEqual(code, 0)
                self.assertIn("partial", log)
                self.assertIn("timed out", log)
                self.assertLess(time.monotonic() - start, 3)


class CompilerIOTests(TemporaryArtifacts):
    def setUp(self):
        super().setUp()
        self.parser = ROOT / f"bin/cc_parse_{suite.host_arch()}"
        self.codegen = ROOT / f"bin/cc_codegen_{suite.host_arch()}"
        self.source = self.work / "input.c"
        self.source.write_text("int main(){return 12;}\n")
        self.ast = self.work / "input.ast"
        proc = subprocess.run([self.parser, self.source, self.ast], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)

    def assert_stage_failure(self, command, **kwargs):
        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=10, **kwargs)
        self.assertEqual(proc.returncode, 1, proc.stdout)
        self.assertNotIn(" -> ", proc.stdout)
        return proc.stdout

    def test_read_error_is_not_empty_source(self):
        text = self.assert_stage_failure([self.parser, self.work, self.work / "directory.ast"])
        self.assertIn("Failed to read source", text)

    def test_output_open_failure_is_not_a_crash(self):
        self.assert_stage_failure([self.codegen, self.ast, self.work / "missing/output.asm"])

    def test_empty_ast_failure_is_not_a_crash(self):
        empty = self.work / "empty.ast"
        empty.touch()
        self.assert_stage_failure([self.codegen, empty, self.work / "empty.asm"])

    def test_missing_or_unreadable_runtime_is_not_success(self):
        text = self.assert_stage_failure([self.codegen, self.ast, self.work / "missing.asm"], cwd=self.work)
        self.assertIn("Failed to open runtime file", text)
        runtime = self.work / "runtime"
        runtime.mkdir()
        (runtime / "crt0.asm").mkdir()  # Opens on POSIX, but read fails.
        self.assert_stage_failure([self.codegen, self.ast, self.work / "unreadable.asm"], cwd=self.work)

    def test_missing_late_runtime_is_not_success(self):
        runtime = self.work / "runtime"
        runtime.mkdir()
        for name in ("crt0.asm", "zeal8bit.asm", "math_8.asm"):
            (runtime / name).symlink_to(ROOT / "runtime" / name)
        self.assert_stage_failure([self.codegen, self.ast, self.work / "late.asm"], cwd=self.work)

    def test_write_and_final_flush_failures(self):
        def limit_output():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (1, 1))
        text = self.assert_stage_failure([self.parser, self.source, self.work / "limited.ast"], preexec_fn=limit_output)
        self.assertIn("Failed to close AST output", text)
        self.assert_stage_failure([self.codegen, self.ast, self.work / "limited.asm"], preexec_fn=limit_output)
        normal = self.work / "normal.asm"
        proc = subprocess.run([self.codegen, self.ast, normal], capture_output=True)
        self.assertEqual(proc.returncode, 0)
        limit = normal.stat().st_size - 1

        def limit_final_flush():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))

        text = self.assert_stage_failure([self.codegen, self.ast, self.work / "flush.asm"], preexec_fn=limit_final_flush)
        self.assertIn("Failed to close assembly output", text)

    def test_parser_midstream_write_failure(self):
        self.source.write_text("\n".join(f"int value{i:03}=0;" for i in range(480)))

        def limit_output():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (1, 1))

        text = self.assert_stage_failure([self.parser, self.source, self.work / "midstream.ast"], preexec_fn=limit_output)
        self.assertIn("Failed to write AST", text)

    def test_nested_writer_failures_and_semantic_read_errors(self):
        makefile = (ROOT / "Makefile").read_text().replace("\\\n", "")
        import re
        parse_sources = re.search(r"^PARSE_SRCS = (.*)$", makefile, re.M).group(1).split()
        for name in ("semantic_io", "ast_writer"):
            with self.subTest(harness=name):
                binary = self.work / name
                command = ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-Iinclude", "-DCC_POOL_SIZE=32768", f"tests/infrastructure/{name}_test.c"]
                if name == "ast_writer":
                    command += [source for source in parse_sources if source != "src/parser/main.c"]
                build = subprocess.run([*command, "-o", str(binary)], capture_output=True, text=True, cwd=ROOT)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                proc = subprocess.run([binary, self.work / "writer.ast"], capture_output=True, text=True)
                self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)

    def test_adapter_fault_injection(self):
        input_file = self.work / "byte"
        input_file.write_bytes(b"x")
        for target in ("modern", "zos"):
            with self.subTest(target=target):
                binary = self.work / target
                command = ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-Iinclude"]
                if target == "zos":
                    command += ["-Itests/infrastructure/zos_mock"]
                command += [f"tests/infrastructure/{target}_io_test.c", "-o", str(binary)]
                build = subprocess.run(command, capture_output=True, text=True, cwd=ROOT)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                proc = subprocess.run([binary, input_file, self.work / "output"], capture_output=True, text=True)
                self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)


if __name__ == "__main__":
    unittest.main()
