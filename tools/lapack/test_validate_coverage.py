# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Independent adversarial tests for coverage claims, not numerical evidence."""

import copy
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

import migrate_execution_identities as migration
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
        self.source.write_text("// Synthetic identity only.\n",
                               encoding="utf-8")
        self.results = self.root / "results.json"
        self.results.write_text(
            '{"tests": [{"id": "residual", "status": "passed"}]}\n',
            encoding="utf-8",
        )
        self.upstream = {
            "id":
                "lapack.dgetrf",
            "routine":
                "dgetrf",
            "classification":
                "public_computational",
            "required_profiles": [coverage.PROFILE],
            "source_instances": [{
                "path": "SRC/dgetrf.f",
                "sha256": "1" * 64,
                "declaration": "SUBROUTINE DGETRF(...)",
            }],
        }
        self.inventory = {
            "schema_version": 1,
            "specification": {
                "commit": coverage.PINNED_COMMIT
            },
            "routines": [copy.deepcopy(self.upstream)],
            "unresolved_classifications": [],
        }
        row = copy.deepcopy(self.upstream)
        row["upstream_record_sha256"] = coverage.record_hash(self.upstream)
        row["contract"] = {"state": "pending"}
        row["implementations"] = {
            "native": {
                "state": "not_started",
                "evidence_ids": []
            },
            "reference_cpu": {
                "state": "not_started",
                "evidence_ids": []
            },
        }
        self.mapping = {
            "schema_version": 1,
            "specification": {
                "commit": coverage.PINNED_COMMIT
            },
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
        """Construct a consistent synthetic claim for adversarial edits."""
        row = self.mapping["routines"][0]
        row["contract"] = {
            "state":
                "reviewed",
            "asc_operation":
                "Getrf",
            "scalar_signature": {
                "matrix": "f64"
            },
            "descriptors": ["full"],
            "workspace":
                "caller pivots",
            "mutation":
                "partial on singularity",
            "aliasing":
                "disjoint",
            "pivots":
                "one based swaps",
            "numerical_outcomes": ["success", "singular"],
            "documentation":
                "SRC/dgetrf.f",
            "mode_cases": [{
                "id": "ordinary",
                "no_options": True,
                "source_evidence": "SRC/dgetrf.f",
                "required_test_classes": ["reconstruction"],
            }],
        }
        row["implementation_artifacts"] = [{
            "path": self.source.name,
            "sha256": coverage.file_hash(self.source),
        }]
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
        self.evidence["records"] = [{
            "id":
                "synthetic-test-record",
            "timestamp":
                "2026-09-07T00:00:00Z",
            "identity":
                copy.deepcopy(identity),
            "exit_code":
                0,
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
            "working_directory":
                ".",
            "test_counts": {
                "selected": 1,
                "executed": 1,
                "passed": 1,
                "failed": 0,
                "skipped": 0,
            },
            "artifacts": [{
                "path": self.results.name,
                "sha256": coverage.file_hash(self.results),
                "kind": "test_results",
            }],
            "provider_build": {
                "source_commit": coverage.PINNED_COMMIT,
                "identity_sha256": "7" * 64,
                "integer_bits": 32,
            },
            "covered_cases": [{
                "routine_id": "lapack.dgetrf",
                "route": "reference_cpu",
                "mode_id": "ordinary",
                "test_class": "reconstruction",
                "execution_kind": "real",
                "test_id": "residual",
            }],
        }]


class MappingTest(CoverageFixture):
    """Source mapping and basic evidence consistency."""

    def test_incremental_is_not_full(self):
        """An incremental ledger cannot imply full-profile closure."""
        result = self.validate()
        self.assertEqual(result["required"], 1)
        self.assertEqual(result["not_started_reference"], 1)
        self.assertEqual(result["incomplete_reference"], 1)
        self.assertEqual(result["in_progress_reference"], 0)
        self.assertFalse(result["profile_complete"])
        with self.assertRaisesRegex(coverage.ValidationError, "incomplete"):
            self.validate(require_full=True)

    def test_missing_mapping(self):
        """Every inventoried ID needs a corresponding mapping row."""
        self.mapping["routines"] = []
        with self.assertRaises(coverage.ValidationError):
            self.validate()

    def test_unverified_does_not_mean_no_tests_ran(self):
        """A passing artifact without route closure remains unverified."""
        self.claim_verified()
        route = self.mapping["routines"][0]["implementations"]["reference_cpu"]
        route["state"] = "implemented_unverified"
        route["evidence_ids"] = []
        result = self.validate()
        self.assertEqual(result["implemented_unverified_reference"], 1)
        self.assertEqual(result["callable_reference"], 1)
        self.assertEqual(result["verified_reference"], 0)
        self.assertEqual(result["incomplete_reference"], 0)
        self.assertFalse(result["profile_complete"])

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
            "state"] = "verified"
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
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "missing evidence"):
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
            ("selected", "executed", "passed", "failed", "skipped"), 0)
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
            "execution_kind"] = "injected"
        with self.assertRaisesRegex(coverage.ValidationError, "injected"):
            self.validate()

    def test_missing_mode_class(self):
        """Each required legal mode and test class needs execution."""
        self.claim_verified()
        self.mapping["routines"][0]["contract"]["mode_cases"][0][
            "required_test_classes"].append("workspace")
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
        self.inventory["routines"][0]["source_instances"][0]["sha256"] = ("e" *
                                                                          64)
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "source signature"):
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
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "no executed test"):
            self.validate()

    def test_artifact_contradicts_test_count(self):
        """Claimed test totals cannot contradict the actual result records."""
        self.claim_verified()
        self.results.write_text('{"tests": []}', encoding="utf-8")
        self.evidence["records"][0]["artifacts"][0]["sha256"] = (
            coverage.file_hash(self.results))
        with self.assertRaisesRegex(coverage.ValidationError, "no records"):
            self.validate()

    def test_wrong_provider_source(self):
        """Another provider source commit cannot inherit this profile."""
        self.claim_verified()
        self.evidence["records"][0]["provider_build"]["source_commit"] = ("0" *
                                                                          40)
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "provider source"):
            self.validate()


class MultipleIdentityTest(CoverageFixture):
    """Separate source expectations cannot transfer route verification credit."""

    def split_identities(self):
        """Keep an old native identity beside a newer Reference execution."""
        self.claim_verified()
        native_identity = copy.deepcopy(self.evidence.pop("expected_identity"))
        reference_identity = copy.deepcopy(native_identity)
        reference_identity["implementation_commit"] = "8" * 40
        reference_identity["implementation_tree"] = "9" * 40
        reference = self.evidence["records"][0]
        native = copy.deepcopy(reference)
        native["id"] = "native-record"
        native["identity_id"] = "native-v1"
        native["covered_cases"][0]["route"] = "native"
        reference["identity_id"] = "reference-v2"
        reference["identity"] = copy.deepcopy(reference_identity)
        self.evidence["schema_version"] = 2
        self.evidence["expected_identities"] = {
            "native-v1": native_identity,
            "reference-v2": reference_identity,
        }
        self.evidence["records"].append(native)
        routes = self.mapping["routines"][0]["implementations"]
        routes["native"] = {
            "state": "verified",
            "evidence_ids": ["native-record"],
            "execution_identity_ids": ["native-v1"],
        }
        routes["reference_cpu"]["execution_identity_ids"] = ["reference-v2"]

    def test_distinct_sources_preserve_native_identity(self):
        """Both reviewed sources validate without rewriting either expectation."""
        self.split_identities()
        before = copy.deepcopy(self.evidence)
        result = self.validate(require_full=True)
        self.assertEqual(result["verified_native"], 1)
        self.assertEqual(result["verified_reference"], 1)
        self.assertEqual(self.evidence, before)

    def test_record_cannot_supply_its_own_expectation(self):
        """Changing a claimed tree does not change its named expected tree."""
        self.split_identities()
        self.evidence["records"][0]["identity"][
            "implementation_tree"] = "a" * 40
        with self.assertRaisesRegex(coverage.ValidationError, "Stale"):
            self.validate()

    def test_valid_catalog_entry_cannot_replace_route_source(self):
        """A record matching another source still fails the route expectation."""
        self.split_identities()
        record = self.evidence["records"][0]
        record["identity_id"] = "native-v1"
        record["identity"] = copy.deepcopy(
            self.evidence["expected_identities"]["native-v1"])
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "unexpected source"):
            self.validate()

    def test_missing_record_selector_has_no_default(self):
        """Schema 2 cannot accidentally reuse the first catalog identity."""
        self.split_identities()
        del self.evidence["records"][0]["identity_id"]
        with self.assertRaisesRegex(coverage.ValidationError, "identity_id"):
            self.validate()

    def test_unknown_record_selector_is_rejected(self):
        """A typo cannot silently select another execution identity."""
        self.split_identities()
        self.evidence["records"][0]["identity_id"] = "absent"
        with self.assertRaisesRegex(coverage.ValidationError, "identity_id"):
            self.validate()

    def test_missing_route_expectation_is_rejected(self):
        """Named records alone cannot approve a route's current source."""
        self.split_identities()
        del self.mapping["routines"][0]["implementations"]["native"][
            "execution_identity_ids"]
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "identity expectations"):
            self.validate()

    def test_unknown_or_duplicate_route_expectation_is_rejected(self):
        """Reviewed source selectors must be explicit, unique catalog names."""
        self.split_identities()
        route = self.mapping["routines"][0]["implementations"]["native"]
        for names in (["absent"], ["native-v1", "native-v1"]):
            with self.subTest(names=names):
                route["execution_identity_ids"] = names
                with self.assertRaisesRegex(coverage.ValidationError,
                                            "execution identity expectation"):
                    self.validate()

    def test_legacy_and_named_expectations_cannot_mix(self):
        """Ambiguous versioned expectations are rejected explicitly."""
        self.split_identities()
        self.evidence["expected_identity"] = self.evidence[
            "expected_identities"]["native-v1"]
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "default expected"):
            self.validate()

    def test_legacy_records_cannot_opt_into_named_selection(self):
        """Schema 1 behavior stays explicit rather than partially migrating."""
        self.claim_verified()
        self.evidence["records"][0]["identity_id"] = "pretend"
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "Schema 1 records"):
            self.validate()

    def test_unused_record_still_needs_an_expected_identity(self):
        """Unused or partial rows cannot conceal an unknown source binding."""
        self.split_identities()
        extra = copy.deepcopy(self.evidence["records"][0])
        extra.update(id="unused", identity_id="absent")
        self.evidence["records"].append(extra)
        with self.assertRaisesRegex(coverage.ValidationError, "identity_id"):
            self.validate()

    def test_second_source_failure_is_not_hidden_by_first_source(self):
        """Passing Reference execution cannot mask a failed native command."""
        self.split_identities()
        self.evidence["records"][1]["exit_code"] = 1
        with self.assertRaisesRegex(coverage.ValidationError, "command failed"):
            self.validate()

    def test_provider_abi_checks_still_apply(self):
        """Named source selection does not bypass the actual provider ABI."""
        self.split_identities()
        self.evidence["records"][0]["configuration"]["integer_bits"] = 64
        with self.assertRaisesRegex(coverage.ValidationError, "ABI"):
            self.validate()

    def test_partial_observation_cannot_close_a_class(self):
        """A passing fixture does not prove an entire mathematical class."""
        self.split_identities()
        self.evidence["records"][0]["covered_cases"][0][
            "class_complete"] = False
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "missing ordinary"):
            self.validate()

    def test_partial_route_remains_unverified(self):
        """Incomplete coverage is checked without awarding verified credit."""
        self.split_identities()
        route = self.mapping["routines"][0]["implementations"]["reference_cpu"]
        route["partial_evidence_ids"] = route.pop("evidence_ids")
        route.update(state="implemented_unverified", evidence_ids=[])
        self.evidence["records"][0]["covered_cases"][0][
            "class_complete"] = False
        self.assertEqual(self.validate()["verified_reference"], 0)
        self.evidence["records"][0]["exit_code"] = 1
        with self.assertRaisesRegex(coverage.ValidationError, "command failed"):
            self.validate()

    def test_partial_record_still_checks_source_selector(self):
        """A passing partial record cannot attach to the wrong expected source."""
        self.split_identities()
        route = self.mapping["routines"][0]["implementations"]["reference_cpu"]
        route["partial_evidence_ids"] = route.pop("evidence_ids")
        route.update(state="implemented_unverified",
                     evidence_ids=[],
                     execution_identity_ids=["native-v1"])
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "unexpected source"):
            self.validate()

    def test_unreferenced_failed_record_is_rejected(self):
        """Removing a route reference cannot hide a failed schema2 record."""
        self.split_identities()
        extra = copy.deepcopy(self.evidence["records"][0])
        extra.update(id="unused", exit_code=1)
        self.evidence["records"].append(extra)
        with self.assertRaisesRegex(coverage.ValidationError, "command failed"):
            self.validate()

    def test_unknown_mode_class_or_route_is_rejected(self):
        """Typographic fixture assignments cannot become silent coverage."""
        self.split_identities()
        case = self.evidence["records"][0]["covered_cases"][0]
        for field in ("routine_id", "mode_id", "test_class", "route"):
            with self.subTest(field=field):
                before = case[field]
                case[field] = "absent"
                with self.assertRaisesRegex(coverage.ValidationError,
                                            "unknown"):
                    self.validate()
                case[field] = before

    def test_nonboolean_completeness_is_rejected(self):
        """The string 'false' cannot be mistaken for completed evidence."""
        self.split_identities()
        self.evidence["records"][0]["covered_cases"][0][
            "class_complete"] = "false"
        with self.assertRaisesRegex(coverage.ValidationError, "boolean"):
            self.validate()


class IdentityCommandTest(CoverageFixture):
    """Actual CLI execution binds every catalog entry to supplied input files."""

    def write_fixture(self, multiple=True):
        """Write independently hashed synthetic CLI inputs in owned scratch."""
        if multiple:
            MultipleIdentityTest.split_identities(self)
        else:
            self.claim_verified()
        inventory = self.root / "inventory.json"
        mapping = self.root / "mapping.json"
        lock = self.root / "lock.json"
        self.evidence_path = self.root / "evidence.json"
        inventory.write_text(json.dumps(self.inventory), encoding="utf-8")
        lock.write_text(json.dumps(
            {"specification": self.inventory["specification"]}),
                        encoding="utf-8")
        self.mapping["inventory_sha256"] = coverage.file_hash(inventory)
        self.mapping["provider_lock_sha256"] = coverage.file_hash(lock)
        mapping.write_text(json.dumps(self.mapping), encoding="utf-8")
        hashes = {
            "inventory_sha256": coverage.file_hash(inventory),
            "provider_lock_sha256": coverage.file_hash(lock),
            "mapping_sha256": coverage.file_hash(mapping)
        }
        identities = (self.evidence["expected_identities"].values()
                      if multiple else [self.evidence["expected_identity"]])
        for identity in identities:
            identity.update(hashes)
        for record in self.evidence["records"]:
            record["identity"].update(hashes)
        self.arguments = [
            sys.executable, "-B",
            str(pathlib.Path(coverage.__file__).resolve()), "--inventory",
            str(inventory), "--mapping",
            str(mapping), "--provider-lock",
            str(lock), "--evidence",
            str(self.evidence_path), "--source-root",
            str(self.root), "--evidence-root",
            str(self.root), "--require-full"
        ]

    def execute_fixture(self):
        """Run the real parser/validator, retaining diagnostics for assertions."""
        self.evidence_path.write_text(json.dumps(self.evidence),
                                      encoding="utf-8")
        return subprocess.run(self.arguments,
                              check=False,
                              capture_output=True,
                              text=True)

    def test_named_cli_sources_are_bound(self):
        """A versioned two-source document passes the actual CLI checks."""
        self.write_fixture()
        result = self.execute_fixture()
        self.assertEqual(result.returncode, 0, result.stderr)
        counts = json.loads(result.stdout)
        self.assertEqual(counts["verified_native"], 1)
        self.assertEqual(counts["verified_reference"], 1)

    def test_legacy_cli_record_is_unchanged(self):
        """The existing single-source schema still passes the actual CLI."""
        self.write_fixture(multiple=False)
        result = self.execute_fixture()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["verified_reference"], 1)

    def test_every_catalog_input_hash_is_checked(self):
        """Even an unused catalog entry cannot name different current inputs."""
        self.write_fixture()
        for field in ("inventory_sha256", "mapping_sha256",
                      "provider_lock_sha256"):
            with self.subTest(field=field):
                extra = copy.deepcopy(
                    self.evidence["expected_identities"]["native-v1"])
                extra[field] = "0" * 64
                self.evidence["expected_identities"]["unused"] = extra
                result = self.execute_fixture()
                self.assertEqual(result.returncode, 1, result.stdout)
                self.assertIn("Evidence ", result.stderr)
                self.assertIn("hash mismatch", result.stderr)

    def test_fresh_metadata_does_not_hide_stale_record(self):
        """Correct manifest hashes cannot bless a different executed tree."""
        self.write_fixture()
        self.evidence["records"][0]["identity"][
            "implementation_tree"] = "b" * 40
        result = self.execute_fixture()
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("Stale evidence identity", result.stderr)


class MigrationCommandTest(CoverageFixture):
    """Migration CLI must bind original inputs before extending their index."""

    def prepare_command(self):
        """Prepare the established synthetic source and exact baseline files."""
        IdentityCommandTest.write_fixture(self, multiple=False)
        self.arguments[2] = str(pathlib.Path(migration.__file__).resolve())
        self.arguments.remove("--require-full")
        self.output = self.root / "migration"
        self.arguments.extend([
            "--legacy-name", "original", "--output-directory",
            str(self.output)
        ])

    def test_generated_cli_outputs_pass_coverage_validation(self):
        """The real generator writes an atomic, still-valid named ledger."""
        self.prepare_command()
        result = IdentityCommandTest.execute_fixture(self)
        self.assertEqual(result.returncode, 0, result.stderr)
        arguments = self.arguments[:]
        arguments[2] = str(pathlib.Path(coverage.__file__).resolve())
        arguments[arguments.index("--mapping") + 1] = str(self.output /
                                                          "mapping.json")
        arguments[arguments.index("--evidence") + 1] = str(self.output /
                                                           "evidence.json")
        arguments = arguments[:arguments.index("--legacy-name")]
        check = subprocess.run(arguments,
                               check=False,
                               capture_output=True,
                               text=True)
        self.assertEqual(check.returncode, 0, check.stderr)
        # Validate exact emitted bytes as well as the parsed manifest. This is
        # a file identity contract, independent of the host's newline spelling.
        generated = self.output / "mapping.json"
        self.assertNotIn(b"\r", generated.read_bytes())
        ledger = coverage.read_json(self.output / "evidence.json")
        expected = ledger["expected_identities"]["original"]["mapping_sha256"]
        self.assertEqual(coverage.file_hash(generated), expected)
        # Exclusive output is a preservation rule, not an implicit overwrite.
        repeated = IdentityCommandTest.execute_fixture(self)
        self.assertEqual(repeated.returncode, 1)
        self.assertIn("Output directory already exists", repeated.stderr)

    def test_stale_expectation_cannot_be_replaced_by_new_index_hash(self):
        """A migration rejects wrong old hashes instead of laundering them."""
        self.prepare_command()
        for field in ("mapping_sha256", "inventory_sha256",
                      "provider_lock_sha256"):
            with self.subTest(field=field):
                original = self.evidence["expected_identity"][field]
                self.evidence["expected_identity"][field] = "0" * 64
                result = IdentityCommandTest.execute_fixture(self)
                self.assertEqual(result.returncode, 1, result.stdout)
                self.assertIn("stale " + field, result.stderr)
                self.assertFalse(self.output.exists())
                self.evidence["expected_identity"][field] = original


if __name__ == "__main__":
    unittest.main()
