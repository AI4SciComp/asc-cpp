# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Independent malformed attestation inputs; these are not provider tests."""

# The explicit fixture option inventory intentionally does not import the
# validator's own required-option table; changing it cannot weaken both sides.
# pylint: disable=duplicate-code

import argparse
import contextlib
import hashlib
import json
import pathlib
import shutil
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

    def test_distinct_ilp64_library_names(self):
        """The pinned global index64 build has distinct, unsuffixed-API libraries."""
        with tempfile.TemporaryDirectory() as directory:
            prefix = pathlib.Path(directory)
            (prefix / "include").mkdir()
            (prefix / "lib").mkdir()
            for name in ("lapack.h", "lapacke.h"):
                (prefix / "include" / name).write_text(
                    "synthetic", encoding="utf-8"
                )
            for name in ("blas64", "lapack64", "lapacke64"):
                (prefix / "lib" / f"lib{name}.a").write_text(
                    "synthetic", encoding="utf-8"
                )
            self.assertEqual(len(attestation.installed_files(prefix, 64)), 5)
            with self.assertRaisesRegex(
                coverage.ValidationError, "Missing installed"
            ):
                attestation.installed_files(prefix, 32)


@contextlib.contextmanager
def synthetic_attestation():
    """Yield a tiny synthetic identity record, never an actual ABI claim."""
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        prefix = root / "prefix"
        (prefix / "include").mkdir(parents=True)
        (prefix / "lib").mkdir()
        for name in (
            "include/lapack.h",
            "include/lapacke.h",
            "lib/libblas.a",
            "lib/liblapack.a",
            "lib/liblapacke.a",
        ):
            (prefix / name).write_text(
                "Synthetic identity fixture.", encoding="utf-8"
            )
        specification = {"commit": coverage.PINNED_COMMIT}
        inventory = root / "inventory.json"
        lock = root / "lock.json"
        for path in (inventory, lock):
            path.write_text(
                json.dumps({"specification": specification}), encoding="utf-8"
            )
        payload = {
            "schema_version": 1,
            "specification": specification,
            "inventory_sha256": coverage.file_hash(inventory),
            "provider_lock_sha256": coverage.file_hash(lock),
            "integer_bits": 32,
            "options": reference_options(),
            "upstream_tests": {
                "counts": {
                    "selected": 1,
                    "executed": 1,
                    "passed": 1,
                    "failed": 0,
                    "skipped": 0,
                }
            },
            "installed_files": attestation.installed_files(prefix),
        }
        digest = hashlib.sha256(
            json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        record = root / "record.json"
        record.write_text(
            json.dumps({"payload": payload, "identity_sha256": digest}),
            encoding="utf-8",
        )
        yield argparse.Namespace(
            attestation=record,
            prefix=prefix,
            inventory=inventory,
            provider_lock=lock,
            integer_bits=32,
        )


class VerificationTest(unittest.TestCase):
    """Reproducibility validation never trusts labels or a different prefix."""

    def test_valid_and_relocated_record(self):
        """Byte-identical relocation preserves the dependency identity."""
        with synthetic_attestation() as args:
            original = attestation.verify_attestation(args)
            destination = args.prefix.parent / "relocated prefix"
            shutil.copytree(args.prefix, destination)
            args.prefix = destination
            self.assertEqual(attestation.verify_attestation(args), original)

    def test_canonical_payload_hash(self):
        """An unchanged label cannot hide modified payload fields."""
        with synthetic_attestation() as args:
            document = coverage.read_json(args.attestation)
            document["payload"]["integer_bits"] = 64
            args.attestation.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(
                coverage.ValidationError, "payload identity"
            ):
                attestation.verify_attestation(args)

    def test_changed_provider_binary(self):
        """Replacing a library invalidates a previously attested prefix."""
        with synthetic_attestation() as args:
            (args.prefix / "lib/liblapack.a").write_text(
                "changed", encoding="utf-8"
            )
            with self.assertRaisesRegex(
                coverage.ValidationError, "file inventory"
            ):
                attestation.verify_attestation(args)

    def test_changed_lock_or_inventory(self):
        """A source-contract change requires new dependency attestation."""
        for field in ("inventory", "provider_lock"):
            with self.subTest(field=field), synthetic_attestation() as args:
                with getattr(args, field).open("a", encoding="utf-8") as stream:
                    stream.write("\n")
                with self.assertRaisesRegex(
                    coverage.ValidationError, "identity mismatch"
                ):
                    attestation.verify_attestation(args)

    def test_wrong_requested_width(self):
        """A real LP64 record cannot be selected through an ILP64 request."""
        with synthetic_attestation() as args:
            args.integer_bits = 64
            with self.assertRaisesRegex(
                coverage.ValidationError, "integer ABI"
            ):
                attestation.verify_attestation(args)


if __name__ == "__main__":
    unittest.main()
