import math
import os
from pathlib import Path
import random
import re
import resource
import signal
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
PARENT = Path(os.environ.get("LAB2_PARENT", ROOT / "build/parent"))


class Lab2Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="lab2-")
        self.addCleanup(self.directory.cleanup)
        self.folder = Path(self.directory.name)
        self.output = self.folder / "result with spaces.txt"

    def run_commands(self, commands, child=None, output=None):
        data = str(output or self.output).encode() + b"\n"
        data += commands.encode() if isinstance(commands, str) else commands
        result = subprocess.run(
            [str(PARENT), str(child or PARENT.with_name("child"))], input=data,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10,
        )
        self.assertEqual(result.stdout, b"", result)
        return result

    def values(self):
        return [float(line) for line in self.output.read_text().splitlines()]

    def test_sequential_float_division(self):
        result = self.run_commands("100 2 5\n7.5 2.5\n-24 2 -3\n0 5\n.5\n1e2 2e1\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.values(), [10, 3, 4, 0, .5, 5])
        self.assertRegex(result.stderr.decode(), r"\[parent pid=\d+\]")
        self.assertRegex(result.stderr.decode(), r"\[child pid=\d+\]")
        self.assertIn(b"reaped", result.stderr)

    def test_default_child_path_from_project_root(self):
        data = str(self.output).encode() + b"\n12 2 3\n"
        result = subprocess.run([str(PARENT)], cwd=ROOT.parent, input=data,
                                capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.values(), [2])

    def test_zero_stops_before_next_command(self):
        for zero in ("0", "-0", "0.0", "0e5"):
            with self.subTest(zero=zero):
                result = self.run_commands(f"12 2\n8 {zero}\n99 3\n")
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(self.values(), [6])
                self.assertIn(b"division by zero", result.stderr)
                self.assertIn(b"status=2; reaped", result.stderr)

    def test_invalid_and_nonfinite_numbers_are_rejected(self):
        result = self.run_commands(
            b"garbage\n4x 2\n1,5 2\nnan\ninf\n1e100\n1e-100\n"
            b"3.4e38 1e-10\n12\x00 3\n9 3\n"
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.values(), [3])
        self.assertEqual(result.stderr.count(b"skipped"), 9)

    def test_empty_input_and_whitespace(self):
        result = self.run_commands("\n \t\r\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.read_bytes(), b"")

    def test_final_line_without_newline(self):
        result = self.run_commands("12 2 3")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.values(), [2])

    def test_long_line_exceeds_pipe_capacity(self):
        result = self.run_commands("42 " + "1 " * 100000 + "\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.values(), [42])

    def test_many_commands_and_float_rounding(self):
        rng = random.Random(42)
        rows, expected = [], []
        for _ in range(500):
            a, b, c = (rng.randint(1, 100) for _ in range(3))
            rows.append(f"{a} {b} {c}\n")
            expected.append(a / b / c)
        result = self.run_commands("".join(rows))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(self.values()), len(expected))
        for actual, target in zip(self.values(), expected):
            self.assertTrue(math.isclose(actual, target, rel_tol=3e-7), (actual, target))

    def test_empty_or_missing_filename(self):
        for data in (b"", b"\n", b"bad\x00name\n"):
            result = subprocess.run([str(PARENT)], input=data, capture_output=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)

    def test_unwritable_output(self):
        result = self.run_commands("8 2\n", output=self.folder / "missing/result")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"open output", result.stderr)
        self.assertIn(b"reaped", result.stderr)

    @unittest.skipUnless(Path("/dev/full").exists(), "Linux /dev/full is unavailable")
    def test_full_output_device(self):
        result = self.run_commands("8 2\n", output="/dev/full")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"write output", result.stderr)

    def test_missing_child_executable(self):
        result = self.run_commands("8 2\n", child=self.folder / "missing-child")
        self.assertEqual(result.returncode, 127, result.stderr)
        self.assertIn(b"execvp", result.stderr)
        self.assertIn(b"reaped", result.stderr)

    def test_pipe_descriptor_exhaustion(self):
        def limit_descriptors():
            resource.setrlimit(resource.RLIMIT_NOFILE, (5, 5))
        result = subprocess.run([str(PARENT)], input=str(self.output).encode() + b"\n",
                                capture_output=True, timeout=5, preexec_fn=limit_descriptors)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"pipe2", result.stderr)

    def test_peer_disappears_after_ready(self):
        fake = self.folder / "fake-child"
        fake.write_text("#!/bin/sh\nprintf A\nexit 7\n")
        fake.chmod(0o755)
        result = self.run_commands("42 " + "1 " * 100000 + "\n", child=fake)
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertIn(b"reaped", result.stderr)

    def interactive(self):
        process = subprocess.Popen([str(PARENT), str(PARENT.with_name("child"))], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, start_new_session=True)

        def cleanup():
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=5)
            for stream in (process.stdin, process.stdout, process.stderr):
                stream.close()
        self.addCleanup(cleanup)
        process.stdin.write(str(self.output).encode() + b"\n")
        process.stdin.flush()
        logs = b""
        import select
        deadline = time.monotonic() + 5
        while b"; ready\n" not in logs:
            self.assertLess(time.monotonic(), deadline, logs)
            ready, _, _ = select.select([process.stderr], [], [], .1)
            if ready:
                chunk = os.read(process.stderr.fileno(), 4096)
                self.assertTrue(chunk, logs)
                logs += chunk
        return process, logs

    def test_zero_terminates_with_input_still_open(self):
        process, logs = self.interactive()
        process.stdin.write(b"8 0\n")
        process.stdin.flush()
        self.assertEqual(process.wait(timeout=5), 2)
        self.assertIn(b"reaped", logs + process.stderr.read())

    def test_parent_detects_dead_child_while_waiting_for_input(self):
        process, logs = self.interactive()
        child_pid = int(re.search(rb"\[child pid=(\d+)\]", logs)[1])
        os.kill(child_pid, signal.SIGKILL)
        self.assertNotEqual(process.wait(timeout=5), 0)
        self.assertIn(b"reaped", logs + process.stderr.read())

    def test_interrupt_reaps_child(self):
        process, logs = self.interactive()
        os.kill(process.pid, signal.SIGTERM)
        self.assertEqual(process.wait(timeout=5), 128 + signal.SIGTERM)
        self.assertIn(b"reaped", logs + process.stderr.read())


if __name__ == "__main__":
    unittest.main(verbosity=2)
