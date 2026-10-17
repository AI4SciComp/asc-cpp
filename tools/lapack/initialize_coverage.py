#!/usr/bin/env python3
# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Create an explicitly unimplemented mapping from the exact source inventory.

This is a one-time development bootstrap, not a declaration/binding generator.
It refuses to overwrite a reviewed mapping and never awards a capability.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

import validate_coverage


def initial_mapping(
    inventory: dict, inventory_sha256: str, lock_sha256: str
) -> dict:
    """Preserve every source requirement as a pending reviewed ASC contract."""
    validate_coverage.require(
        inventory.get("specification", {}).get("commit")
        == validate_coverage.PINNED_COMMIT,
        "Unexpected source commit",
    )
    validate_coverage.require(
        not inventory.get("unresolved_classifications"),
        "Resolve source classifications first",
    )
    source_rows = validate_coverage.indexed(
        inventory.get("routines", []), "inventory"
    )
    rows = []
    for identifier, source in sorted(source_rows.items()):
        rows.append(
            {
                "id": identifier,
                "upstream_routine": source["routine"],
                "upstream_record_sha256": validate_coverage.record_hash(source),
                "classification": source["classification"],
                "required_profiles": source["required_profiles"],
                "contract": {
                    "state": "pending",
                    "asc_operation": None,
                    "mode_cases": [],
                },
                "implementations": {
                    "native": {"state": "not_started", "evidence_ids": []},
                    "reference_cpu": {
                        "state": "not_started",
                        "evidence_ids": [],
                    },
                },
                "implementation_artifacts": [],
            }
        )
    return {
        "schema_version": 1,
        "program": "ASC-CPP-LAPACK-IO",
        "specification": inventory["specification"],
        "inventory_sha256": inventory_sha256,
        "provider_lock_sha256": lock_sha256,
        "bootstrap": {
            "generator": "tools/lapack/initialize_coverage.py",
            "generator_sha256": validate_coverage.file_hash(
                pathlib.Path(__file__)
            ),
            "command": "python3 tools/lapack/initialize_coverage.py "
            "--inventory docs/contracts/lapack-upstream-inventory.json "
            "--provider-lock docs/contracts/lapack-provider-lock.json "
            "--output <new-mapping-path>",
            "claim": "Requirements ledger only; typed ASC contracts need review.",
        },
        "routines": rows,
    }


def main() -> int:
    """Write a fresh pending mapping; fail without replacing existing work."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", required=True, type=pathlib.Path)
    parser.add_argument("--provider-lock", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    try:
        inventory = validate_coverage.read_json(args.inventory)
        lock = validate_coverage.read_json(args.provider_lock)
        validate_coverage.require(
            lock["specification"] == inventory["specification"],
            "Provider lock source identity differs",
        )
        document = initial_mapping(
            inventory,
            validate_coverage.file_hash(args.inventory),
            validate_coverage.file_hash(args.provider_lock),
        )
        serialized = json.dumps(document, indent=2, sort_keys=True) + "\n"
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(serialized)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"coverage bootstrap failed: {error}", file=sys.stderr)
        return 1
    print(f"Created {len(document['routines'])} unimplemented requirement rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
