#!/usr/bin/env python3
"""Independent subprocess tests for evidence failure propagation."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest


_RUNNER = pathlib.Path(__file__).with_name("run_evidence.py")


class EvidenceTest(unittest.TestCase):
    """Exercise real subprocess/CTest results and filesystem failures."""

    def setUp(self):
        # TestCase owns cleanup on Python 3.8, including failed assertions.
        # pylint: disable-next=consider-using-with
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = pathlib.Path(temporary.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.build = self.root / "build"
        self.build.mkdir()
        self.records = self.root / "evidence"

    def run_command(self, command, *, ctest=False, records=None):
        """Run the CLI in an isolated source tree and return its process result."""
        arguments = [
            sys.executable,
            str(_RUNNER),
            "--source-root",
            str(self.source),
            "--record-dir",
            str(records or self.records),
            "--cwd",
            str(self.build if ctest else self.source),
            "--configuration",
            "test",
            "--id",
            "test.independent",
        ]
        if ctest:
            arguments.append("--ctest")
        return subprocess.run(
            [*arguments, "--", *command],
            check=False,
            capture_output=True,
            text=True,
        )

    def read_record(self):
        """Return the JSON written by the executed CLI, not an internal helper."""
        return json.loads((self.records / "record.json").read_text())

    def test_help(self):
        """The documented help path succeeds without running a command."""
        result = subprocess.run(
            [sys.executable, str(_RUNNER), "--help"],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("--ctest", result.stdout)

    def test_command_failure_preserved(self):
        """A failed tool retains its exact nonzero exit."""
        result = self.run_command([sys.executable, "-c", "raise SystemExit(7)"])
        self.assertEqual(result.returncode, 7)
        self.assertEqual(self.read_record()["command_exit_code"], 7)

    def test_success_and_log_hash(self):
        """A successful command records its actual output and matching hash."""
        result = self.run_command(
            [
                sys.executable,
                "-c",
                "import sys; sys.stdout.buffer.write(b'observed\\n')",
            ],
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        log = (self.records / "command.log").read_bytes()
        self.assertEqual(log, b"observed\n")
        self.assertEqual(
            self.read_record()["artifacts"]["command.log"],
            hashlib.sha256(log).hexdigest(),
        )

    def test_logging_failure_prevents_command(self):
        """An unusable log destination fails before command execution."""
        blocker = self.root / "file"
        blocker.write_text("not a directory")
        result = self.run_command(
            [sys.executable, "-c", "print('ran')"], records=blocker / "record"
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.records.exists())

    def test_in_source_evidence_rejected(self):
        """Source trees cannot contain evidence artifacts."""
        result = self.run_command(
            [sys.executable, "-c", "print('ran')"],
            records=self.source / "evidence",
        )
        self.assertNotEqual(result.returncode, 0)

    def test_existing_record_not_overwritten(self):
        """Reusing an evidence directory never overwrites previous output."""
        self.records.mkdir()
        marker = self.records / "marker"
        marker.write_text("retained")
        result = self.run_command([sys.executable, "-c", "print('ran')"])
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(marker.read_text(), "retained")

    def test_source_mutation_invalidates_evidence(self):
        """A command cannot award evidence to changing source inputs."""
        result = self.run_command(
            [
                sys.executable,
                "-c",
                "from pathlib import Path; Path('changed').write_text('mutation')",
            ]
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.read_record()["command_exit_code"], 0)
        self.assertIn(
            "source content changed while command ran",
            self.read_record()["evidence_failures"],
        )

    def test_ctest_success_failure_and_skip(self):
        """Mixed CTest outcomes are counted separately and propagate failure."""
        test_file = self.build / "CTestTestfile.cmake"
        test_file.write_text(
            f'add_test(pass "{sys.executable}" "-c" "pass")\n'
            f'add_test(fail "{sys.executable}" "-c" "raise SystemExit(1)")\n'
            f'add_test(skip "{sys.executable}" "-c" "raise SystemExit(77)")\n'
            "set_tests_properties(skip PROPERTIES SKIP_RETURN_CODE 77)\n"
        )
        result = self.run_command(["ctest", "--output-on-failure"], ctest=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(
            self.read_record()["test_counts"],
            {
                "selected": 3,
                "executed": 2,
                "passed": 1,
                "failed": 1,
                "skipped": 1,
            },
        )

    def test_successful_ctest(self):
        """A real nonempty passing CTest run produces successful evidence."""
        (self.build / "CTestTestfile.cmake").write_text(
            f'add_test(pass "{sys.executable}" "-c" "pass")\n'
        )
        result = self.run_command(["ctest"], ctest=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            self.read_record()["test_counts"],
            {
                "selected": 1,
                "executed": 1,
                "passed": 1,
                "failed": 0,
                "skipped": 0,
            },
        )

    def test_skip_invalidates_successful_ctest_exit(self):
        """CTest's successful process exit does not excuse skipped tests."""
        (self.build / "CTestTestfile.cmake").write_text(
            f'add_test(skip "{sys.executable}" "-c" "raise SystemExit(77)")\n'
            "set_tests_properties(skip PROPERTIES SKIP_RETURN_CODE 77)\n"
        )
        result = self.run_command(["ctest"], ctest=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.read_record()["command_exit_code"], 0)
        self.assertEqual(self.read_record()["test_counts"]["skipped"], 1)

    def test_zero_tests_rejected(self):
        """An empty CTest selection cannot produce successful evidence."""
        (self.build / "CTestTestfile.cmake").write_text("")
        result = self.run_command(["ctest"], ctest=True)
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
