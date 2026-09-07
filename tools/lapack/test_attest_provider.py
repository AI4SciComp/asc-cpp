# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Independent malformed attestation inputs; these are not provider tests."""

# The explicit fixture option inventory intentionally does not import the
# validator's own required-option table; changing it cannot weaken both sides.
# pylint: disable=duplicate-code

import pathlib
import tempfile
import unittest

import attest_provider as attestation
import validate_coverage as coverage


def reference_options():
    """Construct an explicit synthetic reference configuration."""
    result = dict.fromkeys(
        (
            "BUILD_SINGLE",
            "BUILD_DOUBLE",
            "BUILD_COMPLEX",
            "BUILD_COMPLEX16",
            "BUILD_DEPRECATED",
            "BUILD_TESTING",
            "LAPACKE",
            "LAPACKE_BUILD_SINGLE",
            "LAPACKE_BUILD_DOUBLE",
            "LAPACKE_BUILD_COMPLEX",
            "LAPACKE_BUILD_COMPLEX16",
        ),
        "ON",
    )
    result.update(
        dict.fromkeys(
            (
                "USE_OPTIMIZED_BLAS",
                "USE_OPTIMIZED_LAPACK",
                "BUILD_INDEX64_EXT_API",
            ),
            "OFF",
        )
    )
    result.update(
        BUILD_INDEX64="OFF",
        USE_XBLAS="OFF",
        BUILD_SHARED_LIBS="OFF",
        CMAKE_BUILD_TYPE="Release",
    )
    return result


class AttestationTest(unittest.TestCase):
    """Check explicit ABI/build and evidence boundaries with tiny fixtures."""

    def test_explicit_lp64_and_ilp64(self):
        """Global index64 differs from both LP64 and a suffixed interface."""
        options = reference_options()
        attestation.validate_options(options, 32)
        with self.assertRaisesRegex(coverage.ValidationError, "BUILD_INDEX64"):
            attestation.validate_options(options, 64)
        options["BUILD_INDEX64"] = "ON"
        attestation.validate_options(options, 64)
        options["BUILD_INDEX64_EXT_API"] = "ON"
        with self.assertRaisesRegex(coverage.ValidationError, "EXT_API"):
            attestation.validate_options(options, 64)

    def test_missing_precision_or_reference_blas(self):
        """An optimized or partial library cannot inherit reference identity."""
        for key, value in (
            ("BUILD_SINGLE", "OFF"),
            ("USE_OPTIMIZED_BLAS", "ON"),
            ("LAPACKE_BUILD_COMPLEX16", "OFF"),
            ("BUILD_DEPRECATED", "OFF"),
        ):
            with self.subTest(key=key):
                options = reference_options()
                options[key] = value
                with self.assertRaises(coverage.ValidationError):
                    attestation.validate_options(options, 32)

    def test_unknown_options(self):
        """No absent configuration is silently defaulted by the attestor."""
        for key in ("USE_XBLAS", "BUILD_SHARED_LIBS", "CMAKE_BUILD_TYPE"):
            with self.subTest(key=key):
                options = reference_options()
                del options[key]
                with self.assertRaises(coverage.ValidationError):
                    attestation.validate_options(options, 32)

    def test_cache_parsing(self):
        """Cache values may contain equals signs but keys must be unique."""
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "CMakeCache.txt"
            path.write_text(
                "# comment\n// doc\nFLAGS:STRING=-DFOO=x=y\n", encoding="utf-8"
            )
            self.assertEqual(
                attestation.cache_values(path), {"FLAGS": "-DFOO=x=y"}
            )
            for text in ("BROKEN\n", "FLAGS:STRING=a\nFLAGS:STRING=b\n"):
                path.write_text(text, encoding="utf-8")
                with self.assertRaises(coverage.ValidationError):
                    attestation.cache_values(path)

    def test_passed_actual_cases(self):
        """Actual testcase elements, not an untrusted root count, are counted."""
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "tests.xml"
            path.write_text(
                '<testsuite tests="999"><testcase name="a" status="run"/> '
                '<testcase name="b"/></testsuite>',
                encoding="utf-8",
            )
            self.assertEqual(
                attestation.test_counts(path),
                {
                    "selected": 2,
                    "executed": 2,
                    "passed": 2,
                    "failed": 0,
                    "skipped": 0,
                },
            )

    def test_failed_skipped_empty_cases(self):
        """Empty, skipped, failed, errored or unknown outcomes never attest."""
        fragments = (
            "",
            "<testcase><failure/></testcase>",
            "<testcase><error/></testcase>",
            "<testcase><skipped/></testcase>",
            '<testcase status="notrun"/>',
            '<testcase status="unknown"/>',
        )
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "tests.xml"
            for fragment in fragments:
                with self.subTest(fragment=fragment):
                    path.write_text(
                        f"<testsuite>{fragment}</testsuite>", encoding="utf-8"
                    )
                    with self.assertRaises(coverage.ValidationError):
                        attestation.test_counts(path)

    def test_partial_prefix(self):
        """A header alone cannot attest a usable provider installation."""
        with tempfile.TemporaryDirectory() as directory:
            prefix = pathlib.Path(directory)
            (prefix / "include").mkdir()
            (prefix / "include/lapacke.h").write_text(
                "synthetic", encoding="utf-8"
            )
            with self.assertRaisesRegex(coverage.ValidationError, "Incomplete"):
                attestation.installed_files(prefix)
            (prefix / "include/lapack.h").write_text(
                "synthetic", encoding="utf-8"
            )
            with self.assertRaisesRegex(
                coverage.ValidationError, "Missing installed"
            ):
                attestation.installed_files(prefix)

    def test_symbolic_escape(self):
        """A library symlink escaping the selected prefix is rejected."""
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prefix = root / "prefix"
            prefix.mkdir()
            external = root / "other-library"
            external.write_text("synthetic", encoding="utf-8")
            (prefix / "library").symlink_to(external)
            with self.assertRaisesRegex(coverage.ValidationError, "escapes"):
                attestation.installed_files(prefix)


if __name__ == "__main__":
    unittest.main()
