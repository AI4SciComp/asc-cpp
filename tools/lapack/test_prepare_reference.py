# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Provider preparation selection and evidence-preservation regressions."""

import pathlib
import subprocess
import sys
import tempfile
import unittest

import prepare_reference as preparation
import validate_coverage as coverage


class PreparationTest(unittest.TestCase):
    """Synthetic command evidence gives no provider or ASC numerical credit."""

    def test_exact_selection(self):
        """The actual executed set, not a root profile count, must match."""
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "tests.xml"
            path.write_text(
                '<testsuite tests="999"><testcase name="a" '
                'status="run"/></testsuite>',
                encoding="utf-8")
            preparation.check_selection(
                {"tests": [{
                    "name": "a",
                    "command": ["test"]
                }]}, path)
            for tests in ([], [{
                    "name": "a"
            }], [{
                    "name": "b",
                    "command": ["test"]
            }], [{
                    "name": "a",
                    "command": ["test"]
            }] * 2):
                with self.subTest(tests=tests):
                    with self.assertRaises(coverage.ValidationError):
                        preparation.check_selection({"tests": tests}, path)

    def test_listing_footer_is_preserved_not_counted(self):
        """The pinned callback's human summary is distinct from CTest JSON."""
        document = '{"kind":"ctestInfo","tests":[]}'
        footer = '\n\t-->   LAPACK TESTING SUMMARY  <--\nNO TESTS WERE ANALYZED\n'
        listing, observed = preparation.read_listing(document + footer)
        self.assertEqual(observed, footer)
        self.assertEqual(listing["tests"], [])
        for suffix in ('\n{"tests":[]}', '\nUnexpected error'):
            with self.subTest(suffix=suffix):
                with self.assertRaisesRegex(coverage.ValidationError,
                                            "Unexpected output"):
                    preparation.read_listing(document + suffix)

    def test_failed_skipped_duplicate_or_missing_execution(self):
        """A configured binary cannot hide absent or nonpassing execution."""
        fragments = ('', '<testcase name="a"><failure/></testcase>',
                     '<testcase name="a"><skipped/></testcase>',
                     '<testcase name="a" status="run"/>' * 2)
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "tests.xml"
            for fragment in fragments:
                with self.subTest(fragment=fragment):
                    path.write_text('<testsuite>' + fragment + '</testsuite>',
                                    encoding="utf-8")
                    with self.assertRaises(coverage.ValidationError):
                        preparation.check_selection(
                            {"tests": [{
                                "name": "a",
                                "command": ["test"]
                            }]}, path)

    def test_occupied_work_directory_is_preserved(self):
        """A real CLI refuses existing evidence before executing any command."""
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            sentinel = root / "sealed.log"
            sentinel.write_text("historical execution\n", encoding="utf-8")
            result = subprocess.run([
                sys.executable, "-B", preparation.__file__, "--work-dir",
                str(root), "--integer-bits", "32", "--linkage", "shared"
            ],
                                    check=False,
                                    capture_output=True,
                                    text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Output directory already exists", result.stderr)
            self.assertEqual(list(root.iterdir()), [sentinel])
            self.assertEqual(sentinel.read_text(encoding="utf-8"),
                             "historical execution\n")


if __name__ == "__main__":
    unittest.main()
