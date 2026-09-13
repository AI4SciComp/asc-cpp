#!/usr/bin/env python3
# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Validate separate LAPACK source requirements, ASC mapping and run evidence.

Manifests use JSON syntax (also valid YAML 1.2); no YAML constructors execute.
Incremental validation checks honesty and identity, not profile completion.
--require-full additionally requires every required reference row/mode to have
real passing execution evidence. This tool never invokes commands from records.
"""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import pathlib
import re
import sys
from typing import Any

PINNED_COMMIT = "6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca"
PROFILE = "reference_cpu_full"
STATES = frozenset({
    "not_started",
    "in_progress",
    "implemented_unverified",
    "verified",
    "blocked",
})
REQUIRED_CLASSES = frozenset({
    "public_driver",
    "public_computational",
    "expert_auxiliary",
    "deprecated_compatibility",
    "optional_extra_precision",
})
SHA256 = re.compile(r"[0-9a-f]{64}\Z")
GIT_SHA = re.compile(r"[0-9a-f]{40}\Z")


class ValidationError(ValueError):
    """A contract, source identity or evidence record is inconsistent."""


def read_json(path: pathlib.Path) -> dict[str, Any]:
    """Read an object, rejecting duplicate keys and non-finite JSON numbers."""

    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValidationError(f"Duplicate JSON key: {key}")
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValidationError(f"Non-finite JSON constant: {value}")

    with path.open(encoding="utf-8") as stream:
        result = json.load(
            stream,
            object_pairs_hook=unique_pairs,
            parse_constant=invalid_constant,
        )
    if not isinstance(result, dict):
        raise ValidationError(f"{path.name}: expected JSON object")
    return result


def file_hash(path: pathlib.Path) -> str:
    """Return a streamed file SHA-256 without executing its contents."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def record_hash(record: dict[str, Any]) -> str:
    """Identify an upstream record without duplicating it in mappings."""
    serialized = json.dumps(
        record,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=True,
        allow_nan=False,
    )
    return hashlib.sha256(serialized.encode("ascii")).hexdigest()


def require(condition: bool, message: str) -> None:
    """Reject an inconsistent requirement with a bounded diagnostic."""
    if not condition:
        raise ValidationError(message)


def indexed(rows: list[dict[str, Any]], label: str) -> dict[str, Any]:
    """Index records by unique nonempty IDs; reject empty inventories."""
    require(isinstance(rows, list) and bool(rows), f"{label}: no records")
    result = {}
    for row in rows:
        require(isinstance(row, dict), f"{label}: record must be an object")
        identifier = row.get("id")
        require(
            isinstance(identifier, str) and bool(identifier),
            f"{label}: missing ID",
        )
        require(identifier not in result, f"{label}: duplicate ID {identifier}")
        result[identifier] = row
    return result


def bounded_path(root: pathlib.Path, value: str) -> pathlib.Path:
    """Resolve a relative artifact path inside the explicitly supplied root."""
    require(isinstance(value, str) and bool(value), "Artifact path is missing")
    path = pathlib.Path(value)
    require(not path.is_absolute(), "Artifact path must be relative")
    resolved = (root / path).resolve()
    try:
        resolved.relative_to(root.resolve())
    except ValueError as error:
        raise ValidationError("Artifact escapes root") from error
    require(resolved.is_file(), f"Artifact does not exist: {value}")
    return resolved


def check_artifact(root: pathlib.Path, artifact: dict[str, Any]) -> None:
    """Check an existing artifact's exact byte identity within a chosen root."""
    digest = artifact.get("sha256", "")
    require(
        isinstance(digest, str) and SHA256.fullmatch(digest) is not None,
        "Artifact needs SHA-256",
    )
    path = bounded_path(root, artifact.get("path", ""))
    require(
        file_hash(path) == digest,
        f"Artifact hash mismatch: {artifact['path']}")


def check_counts(counts: dict[str, Any]) -> None:
    """Reject absent, inconsistent, empty, failed or skipped test selections."""
    keys = ("selected", "executed", "passed", "failed", "skipped")
    for key in keys:
        require(
            isinstance(counts.get(key), int) and
            not isinstance(counts[key], bool) and counts[key] >= 0,
            f"Evidence needs nonnegative integer {key} count",
        )
    require(counts["selected"] > 0, "Zero-test evidence is not verification")
    require(
        counts["selected"] == counts["executed"] == counts["passed"],
        "Not every selected test executed and passed",
    )
    require(
        counts["failed"] == counts["skipped"] == 0,
        "Failed or skipped tests are not verification",
    )


def identity_catalog(evidence: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """Read explicit expected identities without trusting a record's claim.

    Schema 1 retains its single expectation. Schema 2 names separate source
    expectations; records must select one explicitly, without a default.
    """
    schema = evidence.get("schema_version")
    require(schema in (1, 2), "evidence: schema version")
    if schema == 1:
        require("expected_identities" not in evidence,
                "Schema 1 cannot contain multiple expected identities")
        identity = evidence.get("expected_identity", {})
        require(isinstance(identity, dict),
                "Expected identity must be an object")
        return {"": identity}
    require("expected_identity" not in evidence,
            "Schema 2 cannot contain a default expected identity")
    identities = evidence.get("expected_identities")
    require(
        isinstance(identities, dict) and bool(identities),
        "Schema 2 needs named expected identities")
    for name, identity in identities.items():
        require(
            isinstance(name, str) and bool(name), "Identity name is missing")
        require(isinstance(identity, dict),
                "Expected identity must be an object")
    return identities


def bind_record_identities(records: dict[str, Any], catalog: dict[str, Any],
                           schema: int) -> dict[str, Any]:
    """Bind every record to an independent expectation, including unused rows."""
    bindings = {}
    for identifier, record in records.items():
        if schema == 1:
            require("identity_id" not in record,
                    "Schema 1 records cannot select an identity")
            name = ""
        else:
            name = record.get("identity_id")
            require(
                isinstance(name, str) and name in catalog,
                f"{identifier}: unknown or missing identity_id")
        expected = catalog[name]
        require(record.get("identity") == expected, "Stale evidence identity")
        bindings[identifier] = expected
    return bindings


def check_evidence(record: dict[str, Any], expected: dict[str, Any],
                   root: pathlib.Path) -> None:
    """Validate exact implementation/configuration identity and actual outputs.

    Evidence is a local record of execution, not an authenticity certificate.
    The original logs and test artifacts remain necessary for human review.
    """
    require(
        isinstance(record.get("exit_code"), int) and
        not isinstance(record["exit_code"], bool) and record["exit_code"] == 0,
        "Evidence command failed",
    )
    require(
        re.fullmatch(
            r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?(?:Z|[+-]\d\d:\d\d)",
            record.get("timestamp", ""),
        ) is not None,
        "Evidence timestamp must include timezone",
    )
    require(record.get("identity") == expected, "Stale evidence identity")
    identity = record["identity"]
    require(
        GIT_SHA.fullmatch(identity.get("implementation_commit", ""))
        is not None,
        "Evidence needs implementation commit",
    )
    require(
        GIT_SHA.fullmatch(identity.get("implementation_tree", "")) is not None,
        "Evidence needs implementation tree",
    )
    for field in (
            "dirty_diff_sha256",
            "inventory_sha256",
            "mapping_sha256",
            "provider_lock_sha256",
    ):
        require(
            SHA256.fullmatch(identity.get(field, "")) is not None,
            f"Evidence needs {field}",
        )
    config = record.get("configuration", {})
    for field in (
            "compiler",
            "standard_library",
            "os",
            "architecture",
            "build_type",
            "linkage",
    ):
        require(
            isinstance(config.get(field), str) and bool(config[field]),
            f"Evidence missing configuration {field}",
        )
    require(config.get("integer_bits") in (32, 64), "Unknown integer ABI")
    require(
        isinstance(record.get("command"), list) and bool(record["command"]) and
        all(isinstance(arg, str) for arg in record["command"]),
        "Evidence command must be an argument list",
    )
    require(bool(record.get("working_directory")), "Evidence missing cwd")
    check_counts(record.get("test_counts", {}))
    artifacts = record.get("artifacts", [])
    require(
        isinstance(artifacts, list) and bool(artifacts),
        "Evidence has no execution artifacts",
    )
    require(
        any(item.get("kind") == "test_results" for item in artifacts),
        "Evidence needs machine-readable test results",
    )
    for artifact in artifacts:
        check_artifact(root, artifact)
    results = []
    for artifact in artifacts:
        if artifact.get("kind") == "test_results":
            document = read_json(bounded_path(root, artifact["path"]))
            results.extend(document.get("tests", []))
    tests = indexed(results, "executed tests")
    require(
        len(tests) == record["test_counts"]["executed"],
        "Executed results disagree with claimed test count",
    )
    require(
        all(test.get("status") == "passed" for test in tests.values()),
        "Execution artifact contains a nonpassing test",
    )
    require(bool(record.get("covered_cases")), "Evidence covers no cases")
    for case in record["covered_cases"]:
        require(
            case.get("test_id") in tests, "Coverage case has no executed test")
        require(isinstance(case.get("class_complete", True), bool),
                "Class completeness must be boolean")
    if any(
            case.get("route") == "reference_cpu"
            for case in record["covered_cases"]):
        provider = record.get("provider_build", {})
        require(
            provider.get("source_commit") == PINNED_COMMIT,
            "Reference evidence needs exact provider source",
        )
        require(
            SHA256.fullmatch(provider.get("identity_sha256", "")) is not None,
            "Reference evidence needs exact provider build",
        )
        require(
            provider.get("integer_bits") == config["integer_bits"],
            "Provider ABI and execution ABI disagree",
        )


def check_contract(row: dict[str, Any], source_root: pathlib.Path) -> None:
    """Require typed callable contracts before implementation claims."""
    contract = row.get("contract", {})
    for field in (
            "asc_operation",
            "scalar_signature",
            "descriptors",
            "workspace",
            "mutation",
            "aliasing",
            "pivots",
            "numerical_outcomes",
            "documentation",
    ):
        require(bool(contract.get(field)), f"{row['id']}: missing {field}")
    require(
        contract.get("state") == "reviewed",
        f"{row['id']}: contract is not reviewed",
    )
    for artifact in row.get("implementation_artifacts", []):
        check_artifact(source_root, artifact)
    require(
        bool(row.get("implementation_artifacts")),
        f"{row['id']}: implementation artifacts absent",
    )
    cases = indexed(contract.get("mode_cases", []), row["id"] + " modes")
    for case in cases.values():
        require(
            bool(case.get("options")) or case.get("no_options") is True,
            f"{row['id']}: missing legal option contract",
        )
        require(
            bool(case.get("source_evidence")),
            f"{row['id']}: mode lacks upstream evidence",
        )
        require(
            bool(case.get("required_test_classes")),
            f"{row['id']}: mode lacks required tests",
        )


def _check_source_mapping(source: dict, row: dict) -> bool:
    """Check immutable requirements and whether the full profile needs them."""
    identifier = row["id"]
    required = PROFILE in source.get("required_profiles", [])
    require(
        source.get("classification") == row.get("classification"),
        f"{identifier}: classification drift",
    )
    require(
        source.get("required_profiles") == row.get("required_profiles"),
        f"{identifier}: denominator drift",
    )
    if source.get("classification") in REQUIRED_CLASSES:
        require(required, f"{identifier}: required source operation excluded")
    if not required:
        require(
            bool(source.get("classification_reason")) and
            bool(source.get("source_instances")),
            f"{identifier}: unsupported exclusion",
        )
    require(
        bool(source.get("source_instances")),
        f"{identifier}: missing exact source instances",
    )
    require(
        row.get("upstream_record_sha256") == record_hash(source),
        f"{identifier}: source signature/hash drift",
    )
    return required


def _route_records(row: dict, route: str, context: dict,
                   evidence_ids: list[str]) -> list[dict]:
    """Check the explicit source selection for passing or partial evidence."""
    identifier = row["id"]
    require(
        isinstance(evidence_ids, list) and bool(evidence_ids),
        f"{identifier}/{route}: false verified claim")
    require(
        all(isinstance(value, str) for value in evidence_ids) and
        len(set(evidence_ids)) == len(evidence_ids),
        f"{identifier}/{route}: invalid or duplicate evidence IDs")
    expected_ids = row["implementations"][route].get("execution_identity_ids",
                                                     [])
    if context["evidence_schema"] == 2:
        require(
            isinstance(expected_ids, list) and bool(expected_ids),
            f"{identifier}/{route}: missing execution identity expectations")
        require(
            all(
                isinstance(name, str) and name in context["catalog"]
                for name in expected_ids),
            f"{identifier}/{route}: unknown execution identity expectation")
        require(
            len(set(expected_ids)) == len(expected_ids),
            f"{identifier}/{route}: duplicate execution identity expectation")
    records = []
    for evidence_id in evidence_ids:
        require(
            evidence_id in context["records"],
            f"{identifier}/{route}: missing evidence {evidence_id}",
        )
        record = context["records"][evidence_id]
        if context["evidence_schema"] == 2:
            require(
                record["identity_id"] in expected_ids,
                f"{identifier}/{route}: evidence uses an unexpected source identity"
            )
        if evidence_id not in context["checked"]:
            check_evidence(record, context["identities"][evidence_id],
                           context["evidence_root"])
            context["checked"].add(evidence_id)
        records.append(record)
    return records


def _verify_route(row: dict, route: str, context: dict) -> None:
    """Require matching real execution for every reviewed mode/test class."""
    identifier = row["id"]
    records = _route_records(
        row, route, context,
        row["implementations"][route].get("evidence_ids", []))
    covered = set()
    for record in records:
        for case in record["covered_cases"]:
            if (case.get("routine_id") == identifier and
                    case.get("route") == route):
                require(
                    case.get("execution_kind") == "real",
                    f"{identifier}/{route}: "
                    "injected evidence cannot close a mode",
                )
                if case.get("class_complete", True):
                    covered.add((case.get("mode_id"), case.get("test_class")))
    for mode in row["contract"]["mode_cases"]:
        for test_class in mode["required_test_classes"]:
            require(
                (mode["id"], test_class) in covered,
                f"{identifier}/{route}: missing {mode['id']}/{test_class}",
            )


def _check_route(row: dict, route: str, context: dict) -> str:
    """Validate an independently selected native or reference implementation."""
    identifier = row["id"]
    implementation = row.get("implementations", {}).get(route, {})
    state = implementation.get("state")
    require(state in STATES, f"{identifier}/{route}: invalid state")
    if context["evidence_schema"] == 1:
        require("execution_identity_ids" not in implementation,
                "Schema 1 routes cannot select execution identities")
        require("partial_evidence_ids" not in implementation,
                "Schema 1 routes cannot select partial evidence")
    partial_ids = implementation.get("partial_evidence_ids", [])
    if partial_ids:
        require(
            state in ("implemented_unverified", "in_progress"),
            f"{identifier}/{route}: partial evidence needs an incomplete route")
        for record in _route_records(row, route, context, partial_ids):
            require(
                any(
                    case.get("routine_id") == identifier and
                    case.get("route") == route
                    for case in record["covered_cases"]),
                f"{identifier}/{route}: partial record has no matching case")
    old = (context["old_rows"].get(identifier, {}).get("implementations",
                                                       {}).get(route, {}))
    if old.get("state") == "verified" and state != "verified":
        require(
            bool(implementation.get("invalidation_reason")),
            f"{identifier}/{route}: unexplained evidence invalidation",
        )
    if state == "blocked":
        require(
            bool(implementation.get("blocker_id")),
            f"{identifier}/{route}: missing blocker",
        )
    if state in ("implemented_unverified", "verified"):
        check_contract(row, context["source_root"])
    if state == "verified":
        _verify_route(row, route, context)
    else:
        require(
            not implementation.get("evidence_ids"),
            f"{identifier}/{route}: nonverified route claims evidence",
        )
    return state


def _count_route(totals: dict, counter: str, state: str,
                 required: bool) -> None:
    """Accumulate implementation and evidence counts without conflating them."""
    totals["callable_" + counter] += state in (
        "implemented_unverified",
        "verified",
    )
    totals["verified_" + counter] += state == "verified"
    totals["implemented_unverified_" +
           counter] += (state == "implemented_unverified")
    if counter == "reference" and required:
        totals["blocked_reference"] += state == "blocked"
        totals["not_started_reference"] += state == "not_started"
        totals["in_progress_reference"] += state == "in_progress"
        totals["incomplete_reference"] += state in (
            "not_started",
            "in_progress",
            "blocked",
        )


@dataclasses.dataclass
class ValidationOptions:
    """Explicit paths and closure policy for one offline validation."""

    source_root: pathlib.Path
    evidence_root: pathlib.Path
    require_full: bool = False
    previous: dict | None = None


def validate(inventory: dict, mapping: dict, evidence: dict,
             options: ValidationOptions) -> dict:
    """Check honest incremental status, optionally requiring reference closure.

    Args:
      inventory: Immutable source-derived requirements.
      mapping: One ASC contract and independent route states per source row.
      evidence: Executed records bound to explicit expected source identities.
      options: Artifact roots, closure requirement and previous mapping.

    Returns:
      Counts separating pending contracts, callable routes and verified routes.

    Raises:
      ValidationError: A missing or contradictory claim invalidates the report.
    """
    require(inventory.get("schema_version") == 1, "inventory: schema version")
    require(mapping.get("schema_version") == 1, "mapping: schema version")
    catalog = identity_catalog(evidence)
    require(
        inventory.get("specification", {}).get("commit") == PINNED_COMMIT,
        "Inventory source commit does not match the approved pin",
    )
    require(
        mapping.get("specification", {}).get("commit") == PINNED_COMMIT,
        "Mapping source commit mismatch",
    )
    require(
        not inventory.get("unresolved_classifications"),
        "Unresolved source classifications prevent a valid denominator",
    )
    source_rows = indexed(inventory.get("routines", []), "inventory")
    rows = indexed(mapping.get("routines", []), "mapping")
    require(source_rows.keys() == rows.keys(),
            "Missing or extra inventory mappings")
    records = indexed(evidence["records"],
                      "evidence") if evidence.get("records") else {}
    context = {
        "old_rows": (indexed(options.previous["routines"], "previous")
                     if options.previous else {}),
        "records":
            records,
        "catalog":
            catalog,
        "evidence_schema":
            evidence["schema_version"],
        "identities":
            bind_record_identities(records, catalog,
                                   evidence["schema_version"]),
        "checked":
            set(),
        "source_root":
            options.source_root,
        "evidence_root":
            options.evidence_root,
    }
    if evidence["schema_version"] == 2:
        for identifier, record in records.items():
            check_evidence(record, context["identities"][identifier],
                           options.evidence_root)
            context["checked"].add(identifier)
            for case in record["covered_cases"]:
                require(
                    case.get("routine_id") in rows,
                    "Execution case names an unknown routine")
                row = rows[case["routine_id"]]
                modes = indexed(row["contract"].get("mode_cases", []),
                                row["id"] + " modes")
                require(
                    case.get("mode_id") in modes,
                    "Execution case names an unknown mode")
                require(
                    case.get("test_class")
                    in modes[case["mode_id"]]["required_test_classes"],
                    "Execution case names an unknown test class")
                require(
                    case.get("route") in ("native", "reference_cpu"),
                    "Execution case names an unknown route")
    totals = dict.fromkeys(
        (
            "required",
            "excluded",
            "mapped_contracts",
            "callable_reference",
            "verified_reference",
            "callable_native",
            "verified_native",
            "blocked_reference",
            "not_started_reference",
            "in_progress_reference",
            "incomplete_reference",
            "implemented_unverified_reference",
            "implemented_unverified_native",
        ),
        0,
    )
    totals["discovered"] = len(rows)
    for identifier, source in source_rows.items():
        row = rows[identifier]
        required = _check_source_mapping(source, row)
        totals["required" if required else "excluded"] += 1
        totals["mapped_contracts"] += (row.get("contract",
                                               {}).get("state") == "reviewed")
        for route, counter in (
            ("native", "native"),
            ("reference_cpu", "reference"),
        ):
            state = _check_route(row, route, context)
            _count_route(totals, counter, state, required)
            if required and route == "reference_cpu" and options.require_full:
                require(
                    state == "verified",
                    f"{identifier}: full profile incomplete",
                )
    require(totals["required"] > 0, "Empty required profile")
    totals["profile_complete"] = all(
        rows[identifier]["implementations"]["reference_cpu"]["state"] ==
        "verified"
        for identifier, source in source_rows.items()
        if PROFILE in source.get("required_profiles", []))
    return totals


def main() -> int:
    """Validate local inputs and print a machine-readable summary."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=pathlib.Path, required=True)
    parser.add_argument("--mapping", type=pathlib.Path, required=True)
    parser.add_argument("--provider-lock", type=pathlib.Path, required=True)
    parser.add_argument("--evidence", type=pathlib.Path, required=True)
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--evidence-root", type=pathlib.Path, required=True)
    parser.add_argument("--previous", type=pathlib.Path)
    parser.add_argument("--require-full", action="store_true")
    args = parser.parse_args()
    try:
        inventory = read_json(args.inventory)
        mapping = read_json(args.mapping)
        lock = read_json(args.provider_lock)
        evidence = read_json(args.evidence)
        lock_hash = file_hash(args.provider_lock)
        inventory_hash = file_hash(args.inventory)
        mapping_hash = file_hash(args.mapping)
        require(
            lock.get("specification") == inventory.get("specification"),
            "Source lock and inventory identities differ",
        )
        require(
            mapping.get("provider_lock_sha256") == lock_hash,
            "Mapping provider lock SHA-256 mismatch",
        )
        require(
            mapping.get("inventory_sha256") == inventory_hash,
            "Mapping inventory SHA-256 mismatch",
        )
        catalog = identity_catalog(evidence)
        for identity in catalog.values() if evidence.get("records") else ():
            require(
                identity.get("provider_lock_sha256") == lock_hash,
                "Evidence provider lock hash mismatch",
            )
            require(
                identity.get("inventory_sha256") == inventory_hash,
                "Evidence inventory hash mismatch",
            )
            require(
                identity.get("mapping_sha256") == mapping_hash,
                "Evidence mapping hash mismatch",
            )
        result = validate(
            inventory,
            mapping,
            evidence,
            ValidationOptions(
                args.source_root,
                args.evidence_root,
                args.require_full,
                read_json(args.previous) if args.previous else None,
            ),
        )
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(f"coverage validation failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
