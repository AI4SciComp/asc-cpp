#!/usr/bin/env python3
# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Normalize completed CTest execution without inferring numerical coverage.

Reviewed metadata supplies source/configuration identities and exact fixture
assignments. Selection, status and command fields come only from raw execution.
Failed and skipped results remain explicit; normalization awards no verification.
The output directory is exclusive and no recorded command is executed.
"""

from __future__ import annotations

import argparse
import copy
import json
import pathlib
import sys
import xml.etree.ElementTree as element_tree

import validate_coverage as coverage


def normalize(command: dict, selection: list[dict], xml: str,
              metadata: dict) -> tuple[dict, dict]:
    """Bind reviewed fixture assignments to a completed, nonempty selection.

    Raises:
      coverage.ValidationError: Raw inputs disagree or metadata replaces a
        field owned by execution. Ordinary failed tests are preserved.
    """
    coverage.require(
        isinstance(selection, list) and bool(selection),
        "Empty configured selection")
    selected = {}
    numbers = set()
    for test in selection:
        name = test.get("name")
        number = test.get("configured_id")
        coverage.require(
            isinstance(name, str) and bool(name) and name not in selected,
            "Duplicate or missing selected test")
        coverage.require(
            type(number) is int and number > 0 and number not in numbers,
            "Invalid configured test ID")
        coverage.require(
            isinstance(test.get("command"), list) and bool(test["command"]) and
            all(isinstance(arg, str) for arg in test["command"]),
            "Selected test has no executable command")
        selected[name] = test
        numbers.add(number)
    coverage.require("<!DOCTYPE" not in xml and "<!ENTITY" not in xml,
                     "JUnit declarations are not supported")
    tests = []
    names = set()
    for case in element_tree.fromstring(xml).iter("testcase"):
        name = case.get("name")
        coverage.require(name in selected and name not in names,
                         "Unexpected or duplicate executed test")
        names.add(name)
        failed = case.find("failure") is not None or case.find(
            "error") is not None
        skipped = case.find("skipped") is not None or case.get("status") in (
            "notrun", "disabled")
        coverage.require(not (failed and skipped), "Contradictory JUnit status")
        status = "failed" if failed else "skipped" if skipped else "passed"
        coverage.require(status != "passed" or case.get("status") == "run",
                         "JUnit does not confirm test execution")
        tests.append({
            "id": name,
            "configured_id": selected[name]["configured_id"],
            "command": selected[name]["command"],
            "status": status,
            "seconds": case.get("time"),
        })
    coverage.require(names == selected.keys(), "Selected/executed tests differ")
    exit_code = command.get("exit_code")
    coverage.require(
        type(exit_code) is int and bool(command.get("completed")),
        "Command has no completed exit record")
    argv = command.get("argv")
    coverage.require(
        isinstance(argv, list) and bool(argv) and
        all(isinstance(arg, str) for arg in argv),
        "Missing executed argument list")
    counts = {"selected": len(selected), "executed": len(tests)}
    for status in ("passed", "failed", "skipped"):
        counts[status] = sum(test["status"] == status for test in tests)
    counts["executed"] -= counts["skipped"]
    coverage.require(exit_code != 0 or counts["failed"] == 0,
                     "Successful command contradicts failed tests")
    owned = {
        "timestamp", "exit_code", "command", "working_directory", "test_counts",
        "artifacts"
    }
    coverage.require(not owned.intersection(metadata),
                     "Metadata cannot replace execution fields")
    record = copy.deepcopy(metadata)
    for field in ("id", "identity_id", "identity", "configuration",
                  "covered_cases"):
        coverage.require(bool(record.get(field)), f"Missing reviewed {field}")
    for case in record["covered_cases"]:
        coverage.require(
            case.get("test_id") in names,
            "Fixture assignment has no executed test")
        coverage.require(isinstance(case.get("class_complete"), bool),
                         "New fixture assignments need explicit completeness")
    record.update(timestamp=command["completed"],
                  exit_code=exit_code,
                  command=argv,
                  working_directory=command.get("cwd"),
                  test_counts=counts)
    return {"schema_version": 1, "tests": tests}, record


def main() -> int:
    """Write a reproducible compact record and hashes of its raw inputs."""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("command-record", "selection", "junit", "metadata",
                 "output-directory"):
        parser.add_argument("--" + name, type=pathlib.Path, required=True)
    args = parser.parse_args()
    try:
        with args.selection.open(encoding="utf-8") as stream:
            selection = json.load(stream)
        results, record = normalize(coverage.read_json(args.command_record),
                                    selection,
                                    args.junit.read_text(encoding="utf-8"),
                                    coverage.read_json(args.metadata))
        results["raw_inputs"] = {
            name: {
                "name": path.name,
                "sha256": coverage.file_hash(path)
            } for name, path in (("command", args.command_record),
                                ("selection", args.selection),
                                ("junit", args.junit), ("metadata",
                                                        args.metadata))
        }
        args.output_directory.mkdir()
        result_path = args.output_directory / "results.json"
        result_path.write_text(json.dumps(results, indent=2) + "\n",
                               encoding="utf-8")
        record["artifacts"] = [{
            "path": "results.json",
            "kind": "test_results",
            "sha256": coverage.file_hash(result_path)
        }]
        (args.output_directory / "record.json").write_text(
            json.dumps(record, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(record["test_counts"], sort_keys=True))
        return 0
    except (OSError, ValueError, element_tree.ParseError) as error:
        print(f"Execution normalization failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
