# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Adversarial execution normalization and identity migration checks."""

import copy
import unittest

import migrate_execution_identities as migration
import normalize_execution as normalization
import validate_coverage as coverage


class ExecutionTest(unittest.TestCase):
    """Synthetic raw records test bookkeeping, never numerical capability."""

    def setUp(self):
        self.command = {
            "argv": ["ctest", "--no-tests=error"],
            "cwd": "/synthetic",
            "exit_code": 0,
            "completed": "2026-09-11T00:00:00Z"
        }
        self.selection = [{
            "name": "ordinary",
            "configured_id": 7,
            "command": ["synthetic-test"]
        }]
        self.xml = '<testsuite><testcase name="ordinary" status="run" time="1"/></testsuite>'
        self.metadata = {
            "id":
                "new-run",
            "identity_id":
                "new-source",
            "identity": {
                "implementation_commit": "1" * 40,
                "implementation_tree": "2" * 40,
                "mapping_sha256": "3" * 64
            },
            "configuration": {
                "integer_bits": 32
            },
            "covered_cases": [{
                "routine_id": "lapack.dgetrf",
                "route": "reference_cpu",
                "test_id": "ordinary",
                "mode_id": "row_major",
                "test_class": "reconstruction",
                "class_complete": False
            }],
        }

    def normalize(self):
        """Normalize the owned synthetic inputs."""
        return normalization.normalize(self.command, self.selection, self.xml,
                                       self.metadata)

    def test_exact_selection_and_partial_scope(self):
        """Configured ID, command, outcome and partial scope survive."""
        results, record = self.normalize()
        self.assertEqual(results["tests"][0]["configured_id"], 7)
        self.assertEqual(record["test_counts"]["passed"], 1)
        self.assertFalse(record["covered_cases"][0]["class_complete"])

    def test_failures_and_skips_remain_explicit(self):
        """Normalization does not turn expected failures into passing tests."""
        for tag, status in (("failure", "failed"), ("error", "failed"),
                            ("skipped", "skipped")):
            with self.subTest(tag=tag):
                self.command["exit_code"] = 8
                self.xml = ('<testsuite><testcase name="ordinary" status="run">'
                            f'<{tag}/></testcase></testsuite>')
                results, record = self.normalize()
                self.assertEqual(results["tests"][0]["status"], status)
                self.assertEqual(record["exit_code"], 8)
                self.assertEqual(record["test_counts"][status], 1)
                self.assertEqual(record["test_counts"]["executed"],
                                 0 if status == "skipped" else 1)

    def test_empty_missing_duplicate_and_unexpected_selections(self):
        """A positive profile total cannot conceal incorrect selection."""
        for selection in ([], self.selection * 2,
                          [dict(self.selection[0], name="different")
                          ], [dict(self.selection[0], command=[])
                             ], [dict(self.selection[0], configured_id=True)]):
            with self.subTest(selection=selection):
                with self.assertRaises(coverage.ValidationError):
                    normalization.normalize(self.command, selection, self.xml,
                                            self.metadata)

    def test_duplicate_or_missing_execution_is_rejected(self):
        """Selected binaries must actually appear once in executed results."""
        for xml in ('<testsuite/>',
                    '<testsuite><testcase name="ordinary" status="run"/>'
                    '<testcase name="ordinary" status="run"/></testsuite>',
                    '<testsuite><testcase name="ordinary"/></testsuite>'):
            with self.subTest(xml=xml):
                with self.assertRaises(coverage.ValidationError):
                    normalization.normalize(self.command, self.selection, xml,
                                            self.metadata)

    def test_uncompleted_command_and_metadata_override_are_rejected(self):
        """A plan or a self-reported success cannot replace execution."""
        del self.command["completed"]
        with self.assertRaisesRegex(coverage.ValidationError, "completed"):
            self.normalize()
        self.command["completed"] = "2026-09-11T00:00:00Z"
        self.metadata["exit_code"] = 0
        with self.assertRaisesRegex(coverage.ValidationError, "replace"):
            self.normalize()

    def test_fixture_must_name_an_executed_test_and_completeness(self):
        """Missing fixture bindings or implicit new completeness are rejected."""
        case = self.metadata["covered_cases"][0]
        case["test_id"] = "absent"
        with self.assertRaisesRegex(coverage.ValidationError, "executed"):
            self.normalize()
        case["test_id"] = "ordinary"
        del case["class_complete"]
        with self.assertRaisesRegex(coverage.ValidationError, "completeness"):
            self.normalize()

    def test_migration_preserves_original_and_does_not_promote(self):
        """Only selectors and the coverage-index hash change on old evidence."""
        _, addition = self.normalize()
        old_record = copy.deepcopy(addition)
        old_record.update(id="old-run", identity_id="old-source")
        del old_record["identity_id"]
        old_record["identity"]["implementation_tree"] = "8" * 40
        evidence = {
            "schema_version": 1,
            "expected_identity": copy.deepcopy(old_record["identity"]),
            "records": [old_record]
        }
        mapping = {
            "routines": [{
                "id": "lapack.dgetrf",
                "contract": {
                    "unchanged": True
                },
                "implementations": {
                    "native": {
                        "state": "verified",
                        "evidence_ids": ["old-run"]
                    },
                    "reference_cpu": {
                        "state": "implemented_unverified",
                        "evidence_ids": []
                    }
                }
            }]
        }
        original = copy.deepcopy((mapping, evidence))
        changed, result, bridge = migration.migrate(mapping, evidence,
                                                    "old-source", [addition])
        self.assertEqual((mapping, evidence), original)
        old_after = copy.deepcopy(result["records"][0])
        del old_after["identity_id"]
        old_after["identity"]["mapping_sha256"] = old_record["identity"][
            "mapping_sha256"]
        self.assertEqual(old_after, old_record)
        self.assertEqual(
            changed["routines"][0]["implementations"]["reference_cpu"]["state"],
            "implemented_unverified")
        self.assertEqual(bridge["prior_record_hashes"]["old-run"],
                         coverage.record_hash(old_record))
        with self.assertRaisesRegex(coverage.ValidationError,
                                    "Duplicate additional"):
            migration.migrate(mapping, evidence, "old-source",
                              [addition, addition])


if __name__ == "__main__":
    unittest.main()
