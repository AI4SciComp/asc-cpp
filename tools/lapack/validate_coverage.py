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
STATES = frozenset(
    {
        "not_started",
        "in_progress",
        "implemented_unverified",
        "verified",
        "blocked",
    }
)
REQUIRED_CLASSES = frozenset(
    {
        "public_driver",
        "public_computational",
        "expert_auxiliary",
        "deprecated_compatibility",
        "optional_extra_precision",
    }
)
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
    """Identify a complete upstream record without duplicating it in mappings."""
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
        file_hash(path) == digest, f"Artifact hash mismatch: {artifact['path']}"
    )


def check_counts(counts: dict[str, Any]) -> None:
    """Reject absent, inconsistent, empty, failed or skipped test selections."""
    keys = ("selected", "executed", "passed", "failed", "skipped")
    for key in keys:
        require(
            isinstance(counts.get(key), int)
            and not isinstance(counts[key], bool)
            and counts[key] >= 0,
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


def check_evidence(
    record: dict[str, Any], expected: dict[str, Any], root: pathlib.Path
) -> None:
    """Validate exact implementation/configuration identity and actual outputs.

    Evidence is a local record of execution, not an authenticity certificate.
    The original logs and test artifacts remain necessary for human review.
    """
    require(
        isinstance(record.get("exit_code"), int)
        and not isinstance(record["exit_code"], bool)
        and record["exit_code"] == 0,
        "Evidence command failed",
    )
    require(
        re.fullmatch(
            r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?(?:Z|[+-]\d\d:\d\d)",
            record.get("timestamp", ""),
        )
        is not None,
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
        isinstance(record.get("command"), list)
        and bool(record["command"])
        and all(isinstance(arg, str) for arg in record["command"]),
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
            case.get("test_id") in tests, "Coverage case has no executed test"
        )
    if any(
        case.get("route") == "reference_cpu" for case in record["covered_cases"]
    ):
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
    """Require reviewable typed callable contracts before implementation claims."""
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
    """Check immutable requirements; return whether the full profile needs it."""
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
            bool(source.get("classification_reason"))
            and bool(source.get("source_instances")),
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


def _verify_route(row: dict, route: str, context: dict) -> None:
    """Require every reviewed mode/test class to have real matching execution."""
    identifier = row["id"]
    evidence_ids = row["implementations"][route].get("evidence_ids", [])
    require(bool(evidence_ids), f"{identifier}/{route}: false verified claim")
    covered = set()
    for evidence_id in evidence_ids:
        require(
            evidence_id in context["records"],
            f"{identifier}/{route}: missing evidence {evidence_id}",
        )
        record = context["records"][evidence_id]
        if evidence_id not in context["checked"]:
            check_evidence(
                record, context["identity"], context["evidence_root"]
            )
            context["checked"].add(evidence_id)
        for case in record["covered_cases"]:
            if (
                case.get("routine_id") == identifier
                and case.get("route") == route
            ):
                require(
                    case.get("execution_kind") == "real",
                    f"{identifier}/{route}: injected evidence cannot close a mode",
                )
                covered.add((case.get("mode_id"), case.get("test_class")))
    for mode in row["contract"]["mode_cases"]:
        for test_class in mode["required_test_classes"]:
            require(
                (mode["id"], test_class) in covered,
                f"{identifier}/{route}: missing {mode['id']}/{test_class}",
            )


def _check_route(row: dict, route: str, context: dict) -> str:
    """Validate one independently selected native or reference implementation."""
    identifier = row["id"]
    implementation = row.get("implementations", {}).get(route, {})
    state = implementation.get("state")
    require(state in STATES, f"{identifier}/{route}: invalid state")
    old = (
        context["old_rows"]
        .get(identifier, {})
        .get("implementations", {})
        .get(route, {})
    )
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


def _count_route(
    totals: dict, counter: str, state: str, required: bool
) -> None:
    """Accumulate implementation and evidence counts without conflating them."""
    totals["callable_" + counter] += state in (
        "implemented_unverified",
        "verified",
    )
    totals["verified_" + counter] += state == "verified"
    if counter == "reference" and required:
        totals["blocked_reference"] += state == "blocked"
        totals["unimplemented_reference"] += state in (
            "not_started",
            "in_progress",
            "blocked",
        )
        totals["untested_reference"] += state == "implemented_unverified"


@dataclasses.dataclass
class ValidationOptions:
    """Explicit paths and closure policy for one offline validation."""

    source_root: pathlib.Path
    evidence_root: pathlib.Path
    require_full: bool = False
    previous: dict | None = None


def validate(
    inventory: dict, mapping: dict, evidence: dict, options: ValidationOptions
) -> dict:
    """Check honest incremental status, optionally requiring reference closure.

    Args:
      inventory: Immutable source-derived requirements.
      mapping: One ASC contract and independent route states per source row.
      evidence: Executed records bound to one expected implementation identity.
      options: Explicit artifact roots, closure requirement and previous mapping.

    Returns:
      Counts separating pending contracts, callable routes and verified routes.

    Raises:
      ValidationError: A missing or contradictory claim invalidates the report.
    """
    require(inventory.get("schema_version") == 1, "inventory: schema version")
    require(mapping.get("schema_version") == 1, "mapping: schema version")
    require(evidence.get("schema_version") == 1, "evidence: schema version")
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
    require(
        source_rows.keys() == rows.keys(), "Missing or extra inventory mappings"
    )
    context = {
        "old_rows": (
            indexed(options.previous["routines"], "previous")
            if options.previous
            else {}
        ),
        "records": (
            indexed(evidence["records"], "evidence")
            if evidence.get("records")
            else {}
        ),
        "identity": evidence.get("expected_identity", {}),
        "checked": set(),
        "source_root": options.source_root,
        "evidence_root": options.evidence_root,
    }
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
            "unimplemented_reference",
            "untested_reference",
        ),
        0,
    )
    totals["discovered"] = len(rows)
    for identifier, source in source_rows.items():
        row = rows[identifier]
        required = _check_source_mapping(source, row)
        totals["required" if required else "excluded"] += 1
        totals["mapped_contracts"] += (
            row.get("contract", {}).get("state") == "reviewed"
        )
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
        rows[identifier]["implementations"]["reference_cpu"]["state"]
        == "verified"
        for identifier, source in source_rows.items()
        if PROFILE in source.get("required_profiles", [])
    )
    return totals


def main() -> int:
    """Validate local input files and print a concise machine-readable summary."""
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
        require(
            lock.get("specification") == inventory.get("specification"),
            "Source lock and inventory identities differ",
        )
        require(
            mapping.get("provider_lock_sha256")
            == file_hash(args.provider_lock),
            "Mapping provider lock SHA-256 mismatch",
        )
        require(
            mapping.get("inventory_sha256") == file_hash(args.inventory),
            "Mapping inventory SHA-256 mismatch",
        )
        identity = evidence.get("expected_identity", {})
        if evidence.get("records"):
            require(
                identity.get("provider_lock_sha256")
                == file_hash(args.provider_lock),
                "Evidence provider lock hash mismatch",
            )
            require(
                identity.get("inventory_sha256") == file_hash(args.inventory),
                "Evidence inventory hash mismatch",
            )
            require(
                identity.get("mapping_sha256") == file_hash(args.mapping),
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
