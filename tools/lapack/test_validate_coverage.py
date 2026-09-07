# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Independent adversarial tests for coverage claims, not numerical evidence."""

import copy
import hashlib
import pathlib
import tempfile
import unittest

import validate_coverage as coverage


class CoverageFixture(unittest.TestCase):
    """Synthetic records exercise validators without awarding ASC capability."""

    def setUp(self):
        # unittest owns cleanup across setUp/test failures; supports Python 3.8.
        # pylint: disable=consider-using-with
        temporary = tempfile.TemporaryDirectory()
        # pylint: enable=consider-using-with
        self.addCleanup(temporary.cleanup)
        self.root = pathlib.Path(temporary.name)
        self.source = self.root / "implementation.cc"
        self.source.write_text(
            "// Synthetic identity only.\n", encoding="utf-8"
        )
        self.results = self.root / "results.json"
        self.results.write_text(
            '{"tests": [{"id": "residual", "status": "passed"}]}\n',
            encoding="utf-8",
        )
        self.upstream = {
            "id": "lapack.dgetrf",
            "routine": "dgetrf",
            "classification": "public_computational",
            "required_profiles": [coverage.PROFILE],
            "source_instances": [
                {
                    "path": "SRC/dgetrf.f",
                    "sha256": "1" * 64,
                    "declaration": "SUBROUTINE DGETRF(...)",
                }
            ],
        }
        self.inventory = {
            "schema_version": 1,
            "specification": {"commit": coverage.PINNED_COMMIT},
            "routines": [copy.deepcopy(self.upstream)],
            "unresolved_classifications": [],
        }
        row = copy.deepcopy(self.upstream)
        row["upstream_record_sha256"] = coverage.record_hash(self.upstream)
        row["contract"] = {"state": "pending"}
        row["implementations"] = {
            "native": {"state": "not_started", "evidence_ids": []},
            "reference_cpu": {"state": "not_started", "evidence_ids": []},
        }
        self.mapping = {
            "schema_version": 1,
            "specification": {"commit": coverage.PINNED_COMMIT},
            "routines": [row],
        }
        self.evidence = {"schema_version": 1, "records": []}

    def validate(self, **kwargs):
        """Validate this synthetic fixture under an explicit profile policy."""
        return coverage.validate(
            self.inventory,
            self.mapping,
            self.evidence,
            coverage.ValidationOptions(self.root, self.root, **kwargs),
        )

    def claim_verified(self):
        """Construct an internally consistent synthetic claim for adversarial edits."""
        row = self.mapping["routines"][0]
        row["contract"] = {
            "state": "reviewed",
            "asc_operation": "Getrf",
            "scalar_signature": {"matrix": "f64"},
            "descriptors": ["full"],
            "workspace": "caller pivots",
            "mutation": "partial on singularity",
            "aliasing": "disjoint",
            "pivots": "one based swaps",
            "numerical_outcomes": ["success", "singular"],
            "documentation": "SRC/dgetrf.f",
            "mode_cases": [
                {
                    "id": "ordinary",
                    "no_options": True,
                    "source_evidence": "SRC/dgetrf.f",
                    "required_test_classes": ["reconstruction"],
                }
            ],
        }
        row["implementation_artifacts"] = [
            {
                "path": self.source.name,
                "sha256": coverage.file_hash(self.source),
            }
        ]
        row["implementations"]["reference_cpu"] = {
            "state": "verified",
            "evidence_ids": ["synthetic-test-record"],
        }
        identity = {
            "implementation_commit": "1" * 40,
            "implementation_tree": "2" * 40,
            "dirty_diff_sha256": "3" * 64,
            "inventory_sha256": "4" * 64,
            "mapping_sha256": "5" * 64,
            "provider_lock_sha256": "6" * 64,
        }
        self.evidence["expected_identity"] = identity
        self.evidence["records"] = [
            {
                "id": "synthetic-test-record",
                "timestamp": "2026-09-07T00:00:00Z",
                "identity": copy.deepcopy(identity),
                "exit_code": 0,
                "configuration": {
                    "compiler": "synthetic",
                    "standard_library": "synthetic",
                    "os": "synthetic",
                    "architecture": "synthetic",
                    "build_type": "Debug",
                    "linkage": "static",
                    "integer_bits": 32,
                },
                "command": ["synthetic-test"],
                "working_directory": ".",
                "test_counts": {
                    "selected": 1,
                    "executed": 1,
                    "passed": 1,
                    "failed": 0,
                    "skipped": 0,
                },
                "artifacts": [
                    {
                        "path": self.results.name,
                        "sha256": coverage.file_hash(self.results),
                        "kind": "test_results",
                    }
                ],
                "provider_build": {
                    "source_commit": coverage.PINNED_COMMIT,
                    "identity_sha256": "7" * 64,
                    "integer_bits": 32,
                },
                "covered_cases": [
                    {
                        "routine_id": "lapack.dgetrf",
                        "route": "reference_cpu",
                        "mode_id": "ordinary",
                        "test_class": "reconstruction",
                        "execution_kind": "real",
                        "test_id": "residual",
                    }
                ],
            }
        ]


class MappingTest(CoverageFixture):
    """Source mapping and basic evidence consistency."""

    def test_incremental_is_not_full(self):
        """An incremental ledger cannot imply full-profile closure."""
        result = self.validate()
        self.assertEqual(result["required"], 1)
        self.assertEqual(result["unimplemented_reference"], 1)
        self.assertFalse(result["profile_complete"])
        with self.assertRaisesRegex(coverage.ValidationError, "incomplete"):
            self.validate(require_full=True)

    def test_missing_mapping(self):
        """Every inventoried ID needs a corresponding mapping row."""
        self.mapping["routines"] = []
        with self.assertRaises(coverage.ValidationError):
            self.validate()

    def test_duplicate_identifier(self):
        """Duplicate source identities are rejected rather than overwritten."""
        self.inventory["routines"].append(copy.deepcopy(self.upstream))
        with self.assertRaisesRegex(coverage.ValidationError, "duplicate"):
            self.validate()

    def test_unresolved_classification(self):
        """Unreviewed classification gaps cannot pass the ledger check."""
        self.inventory["unresolved_classifications"] = ["review auxiliary"]
        with self.assertRaisesRegex(coverage.ValidationError, "Unresolved"):
            self.validate()

    def test_exclusion_cannot_shrink_public_denominator(self):
        """A required public routine cannot be relabeled out of scope."""
        self.inventory["routines"][0]["required_profiles"] = []
        self.mapping["routines"][0]["required_profiles"] = []
        with self.assertRaisesRegex(coverage.ValidationError, "excluded"):
            self.validate()

    def test_false_verified_no_contract(self):
        """A verified label alone is not a checked operation contract."""
        self.mapping["routines"][0]["implementations"]["native"][
            "state"
        ] = "verified"
        with self.assertRaises(coverage.ValidationError):
            self.validate()

    def test_valid_record_keeps_native_separate(self):
        """Reference evidence never awards native verification."""
        self.claim_verified()
        result = self.validate(require_full=True)
        self.assertTrue(result["profile_complete"])
        self.assertEqual(result["verified_native"], 0)
        self.assertEqual(result["verified_reference"], 1)

    def test_false_verified_no_evidence(self):
        """Claimed evidence IDs must resolve to actual records."""
        self.claim_verified()
        self.evidence["records"] = []
        with self.assertRaisesRegex(
            coverage.ValidationError, "missing evidence"
        ):
            self.validate()

    def test_stale_identity(self):
        """An implementation identity change invalidates prior evidence."""
        self.claim_verified()
        self.evidence["records"][0]["identity"]["dirty_diff_sha256"] = "a" * 64
        with self.assertRaisesRegex(coverage.ValidationError, "Stale"):
            self.validate()

    def test_zero_tests(self):
        """A successful command with zero cases is not verification."""
        self.claim_verified()
        self.evidence["records"][0]["test_counts"] = dict.fromkeys(
            ("selected", "executed", "passed", "failed", "skipped"), 0
        )
        with self.assertRaisesRegex(coverage.ValidationError, "Zero-test"):
            self.validate()

    def test_skipped_test(self):
        """Skipped cases are failures of the required evidence gate."""
        self.claim_verified()
        self.evidence["records"][0]["test_counts"]["skipped"] = 1
        with self.assertRaisesRegex(coverage.ValidationError, "skipped"):
            self.validate()

    def test_filtered_out_test(self):
        """Selected and actually executed case counts must agree."""
        self.claim_verified()
        self.evidence["records"][0]["test_counts"]["selected"] = 2
        with self.assertRaisesRegex(coverage.ValidationError, "selected"):
            self.validate()


class EvidenceTest(CoverageFixture):
    """Artifact, mode and implementation identity checks."""

    def test_missing_test_artifact(self):
        """Every referenced result artifact must exist."""
        self.claim_verified()
        self.evidence["records"][0]["artifacts"][0]["path"] = "nonexistent.json"
        with self.assertRaisesRegex(coverage.ValidationError, "does not exist"):
            self.validate()

    def test_modified_test_artifact(self):
        """Test artifact hashes cannot survive byte changes."""
        self.claim_verified()
        self.results.write_text("changed", encoding="utf-8")
        with self.assertRaisesRegex(coverage.ValidationError, "hash mismatch"):
            self.validate()

    def test_escaping_artifact(self):
        """Evidence paths must remain inside the selected evidence root."""
        self.claim_verified()
        self.evidence["records"][0]["artifacts"][0]["path"] = "../outside.json"
        with self.assertRaisesRegex(coverage.ValidationError, "escapes"):
            self.validate()

    def test_modified_implementation(self):
        """Implementation bytes are bound to the claimed evidence."""
        self.claim_verified()
        self.source.write_text("changed", encoding="utf-8")
        with self.assertRaisesRegex(coverage.ValidationError, "hash mismatch"):
            self.validate()

    def test_wrong_abi(self):
        """Unrecognized or mismatched integer widths are rejected."""
        self.claim_verified()
        self.evidence["records"][0]["configuration"]["integer_bits"] = 16
        with self.assertRaisesRegex(coverage.ValidationError, "ABI"):
            self.validate()

    def test_injected_evidence_cannot_verify_numerics(self):
        """Injected outcomes cannot substitute for real numerical execution."""
        self.claim_verified()
        self.evidence["records"][0]["covered_cases"][0][
            "execution_kind"
        ] = "injected"
        with self.assertRaisesRegex(coverage.ValidationError, "injected"):
            self.validate()

    def test_missing_mode_class(self):
        """Each required legal mode and test class needs execution."""
        self.claim_verified()
        self.mapping["routines"][0]["contract"]["mode_cases"][0][
            "required_test_classes"
        ].append("workspace")
        with self.assertRaisesRegex(coverage.ValidationError, "workspace"):
            self.validate()

    def test_failed_command(self):
        """A failed invocation cannot award verified state."""
        self.claim_verified()
        self.evidence["records"][0]["exit_code"] = 1
        with self.assertRaisesRegex(coverage.ValidationError, "command failed"):
            self.validate()

    def test_source_signature_drift(self):
        """Mapped source declarations retain their exact record identity."""
        self.inventory["routines"][0]["source_instances"][0]["sha256"] = (
            "e" * 64
        )
        with self.assertRaisesRegex(
            coverage.ValidationError, "source signature"
        ):
            self.validate()

    def test_unexplained_state_regression(self):
        """Downgrading prior verification requires an invalidation reason."""
        self.claim_verified()
        previous = copy.deepcopy(self.mapping)
        self.mapping["routines"][0]["implementations"]["reference_cpu"] = {
            "state": "not_started"
        }
        with self.assertRaisesRegex(coverage.ValidationError, "invalidation"):
            self.validate(previous=previous)

    def test_duplicate_json_key_rejected(self):
        """Duplicate JSON keys cannot conceal contradictory identities."""
        self.results.write_text('{"a": 1, "a": 2}', encoding="utf-8")
        with self.assertRaisesRegex(coverage.ValidationError, "Duplicate"):
            coverage.read_json(self.results)

    def test_nonfinite_json_rejected(self):
        """Nonstandard NaN and infinity JSON tokens are rejected."""
        self.results.write_text('{"a": NaN}', encoding="utf-8")
        with self.assertRaisesRegex(coverage.ValidationError, "Non-finite"):
            coverage.read_json(self.results)

    def test_streamed_hash_matches_independent(self):
        """The streaming file hash agrees with an independent digest."""
        expected = hashlib.sha256(self.source.read_bytes()).hexdigest()
        self.assertEqual(coverage.file_hash(self.source), expected)

    def test_case_not_executed(self):
        """A named case must actually occur in a passed result artifact."""
        self.claim_verified()
        self.evidence["records"][0]["covered_cases"][0]["test_id"] = "absent"
        with self.assertRaisesRegex(
            coverage.ValidationError, "no executed test"
        ):
            self.validate()

    def test_artifact_contradicts_test_count(self):
        """Claimed test totals cannot contradict the actual result records."""
        self.claim_verified()
        self.results.write_text('{"tests": []}', encoding="utf-8")
        self.evidence["records"][0]["artifacts"][0]["sha256"] = (
            coverage.file_hash(self.results)
        )
        with self.assertRaisesRegex(coverage.ValidationError, "no records"):
            self.validate()

    def test_wrong_provider_source(self):
        """Another provider source commit cannot inherit this profile."""
        self.claim_verified()
        self.evidence["records"][0]["provider_build"]["source_commit"] = (
            "0" * 40
        )
        with self.assertRaisesRegex(
            coverage.ValidationError, "provider source"
        ):
            self.validate()


if __name__ == "__main__":
    unittest.main()
