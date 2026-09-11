#!/usr/bin/env python3
# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Prepare a reviewed schema1-to-schema2 evidence migration without promotion.

Source files remain untouched. The exclusive output contains the new mapping,
ledger and an exact baseline/change bridge. Additional records attach only as
partial evidence on already implemented or in-progress routes. Numerical
contracts, implementation states and previous execution outcomes are preserved.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import pathlib
import sys

import validate_coverage as coverage


def serialized(value: dict, *, sort_keys: bool = False) -> str:
    """Use the maintained manifests' stable JSON spelling."""
    return json.dumps(value, indent=2, sort_keys=sort_keys) + "\n"


def migrate(mapping: dict, evidence: dict, legacy_name: str,
            additions: list[dict]) -> tuple[dict, dict, dict]:
    """Return selector-only mapping changes and an unchanged-outcome ledger.

    The only rewritten identity field is the current coverage-index hash.
    The bridge retains complete prior expectations and canonical record hashes.
    """
    coverage.require(
        evidence.get("schema_version") == 1,
        "Migration requires the exact schema1 baseline")
    coverage.require(bool(legacy_name), "Missing legacy identity name")
    prior_identity = coverage.identity_catalog(evidence)[""]
    coverage.bind_record_identities(
        coverage.indexed(evidence.get("records", []), "baseline records"),
        {"": prior_identity}, 1)
    result = copy.deepcopy(evidence)
    result.pop("expected_identity")
    result.update(
        schema_version=2,
        expected_identities={legacy_name: copy.deepcopy(prior_identity)})
    for record in result["records"]:
        record["identity_id"] = legacy_name
    result["note"] = (
        "Schema2 preserves the native20 execution source and adds explicitly "
        "partial Reference observations without verification credit. Prior "
        "schema1 ledger context (historical): " + evidence.get("note", ""))
    next_mapping = copy.deepcopy(mapping)
    rows = coverage.indexed(next_mapping["routines"], "mapping")
    baseline_ids = {record["id"] for record in result["records"]}
    for row in rows.values():
        for route in row["implementations"].values():
            coverage.require(
                "execution_identity_ids" not in route and
                "partial_evidence_ids" not in route,
                "Baseline mapping already has schema2 selectors")
            if route.get("evidence_ids"):
                coverage.require(
                    set(route["evidence_ids"]) <= baseline_ids,
                    "Baseline route references absent evidence")
                route["execution_identity_ids"] = [legacy_name]
    for addition in additions:
        record = copy.deepcopy(addition)
        name = record.get("identity_id")
        coverage.require(
            isinstance(name, str) and bool(name) and name != legacy_name,
            "Invalid additional source name")
        catalog = result["expected_identities"]
        coverage.require(
            name not in catalog or catalog[name] == record["identity"],
            "Additional source name has conflicting identities")
        catalog[name] = copy.deepcopy(record["identity"])
        coverage.require(record["id"] not in baseline_ids,
                         "Duplicate additional record")
        baseline_ids.add(record["id"])
        result["records"].append(record)
        pairs = {(case["routine_id"], case["route"])
                 for case in record["covered_cases"]}
        for identifier, route_name in sorted(pairs):
            coverage.require(
                identifier in rows and
                route_name in rows[identifier]["implementations"],
                "Additional evidence references an unknown route")
            route = rows[identifier]["implementations"][route_name]
            coverage.require(
                route["state"] in ("implemented_unverified", "in_progress"),
                "Additional evidence requires an incomplete implementation")
            route.setdefault("partial_evidence_ids", []).append(record["id"])
            names = route.setdefault("execution_identity_ids", [])
            if name not in names:
                names.append(name)
    mapping_hash = hashlib.sha256(
        serialized(next_mapping, sort_keys=True).encode()).hexdigest()
    for identity in result["expected_identities"].values():
        identity["mapping_sha256"] = mapping_hash
    for record in result["records"]:
        record["identity"]["mapping_sha256"] = mapping_hash
    bridge = {
        "schema_version": 1,
        "prior_expected_identity": prior_identity,
        "prior_record_hashes": {
            record["id"]: coverage.record_hash(record)
            for record in evidence["records"]
        },
        "new_mapping_sha256": mapping_hash,
        "changed_routes": [],
        "scope":
            "Selector migration and passing partial evidence only. No state, "
            "numerical contract, old source identity or execution outcome changes. "
            "The current mapping hash identifies the extended index; it is not "
            "a claim that this index was present during old execution.",
    }
    previous = coverage.indexed(mapping["routines"], "previous mapping")
    for identifier, row in rows.items():
        for name, route in row["implementations"].items():
            if route != previous[identifier]["implementations"][name]:
                bridge["changed_routes"].append({
                    "routine_id": identifier,
                    "route": name,
                    "before": previous[identifier]["implementations"][name],
                    "after": route
                })
    return next_mapping, result, bridge


def main() -> int:
    """Validate all generated records before writing a candidate migration."""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("mapping", "evidence", "inventory", "provider-lock",
                 "source-root", "evidence-root", "output-directory"):
        parser.add_argument("--" + name, type=pathlib.Path, required=True)
    parser.add_argument("--legacy-name", required=True)
    parser.add_argument("--addition",
                        type=pathlib.Path,
                        action="append",
                        default=[])
    args = parser.parse_args()
    try:
        baseline = coverage.read_json(args.mapping)
        original = coverage.read_json(args.evidence)
        additions = [coverage.read_json(path) for path in args.addition]
        inventory = coverage.read_json(args.inventory)
        lock = coverage.read_json(args.provider_lock)
        coverage.require(
            lock.get("specification") == inventory.get("specification"),
            "Provider lock and inventory differ")
        input_hashes = {
            "inventory_sha256": coverage.file_hash(args.inventory),
            "provider_lock_sha256": coverage.file_hash(args.provider_lock),
            "mapping_sha256": coverage.file_hash(args.mapping)
        }
        for field in ("inventory_sha256", "provider_lock_sha256"):
            coverage.require(
                baseline.get(field) == input_hashes[field],
                "Baseline mapping has stale input hashes")
        identities = list(coverage.identity_catalog(original).values())
        identities.extend(record["identity"] for record in additions)
        for identity in identities:
            for field, digest in input_hashes.items():
                coverage.require(
                    identity.get(field) == digest,
                    f"Migration input has stale {field}")
        mapping, evidence, bridge = migrate(baseline, original,
                                            args.legacy_name, additions)
        counts = coverage.validate(
            inventory, mapping, evidence,
            coverage.ValidationOptions(args.source_root,
                                       args.evidence_root,
                                       previous=baseline))
        bridge["baseline_files"] = {
            "mapping_sha256": coverage.file_hash(args.mapping),
            "evidence_sha256": coverage.file_hash(args.evidence),
        }
        try:
            args.output_directory.mkdir()
        except FileExistsError as error:
            raise coverage.ValidationError(
                "Output directory already exists; select a new path") from error
        for name, value in (("mapping.json", mapping),
                            ("evidence.json", evidence), ("migration.json",
                                                          bridge)):
            # The mapping identity hashes these exact UTF-8/LF bytes. Text-mode
            # newline translation would invalidate that identity on Windows.
            with (args.output_directory / name).open("xb") as output:
                output.write(
                    serialized(
                        value,
                        sort_keys=name == "mapping.json").encode("utf-8"))
        print(json.dumps(counts, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError) as error:
        print(f"Evidence migration failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
