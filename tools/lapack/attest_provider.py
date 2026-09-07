#!/usr/bin/env python3
# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Attest a locally prepared reference provider without awarding ASC coverage.

The record binds exact pristine source, CMake options, compilers, installed
headers/libraries and actual upstream CTest results. It is a local reproducibility
record, not a signature, license approval or proof of an ASC ABI/binding. Outputs
must be outside the ASC source tree and are created exclusively.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import pathlib
import subprocess
import sys
import xml.etree.ElementTree as element_tree

import validate_coverage as coverage

_REQUIRED_ON = (
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
)
_REQUIRED_OFF = (
    "USE_OPTIMIZED_BLAS",
    "USE_OPTIMIZED_LAPACK",
    "BUILD_INDEX64_EXT_API",
)


def command(argv: list[str], cwd: pathlib.Path) -> str:
    """Run one read-only probe, preserving a failed exit as an error."""
    return subprocess.run(
        argv,
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ).stdout.strip()


def cache_values(path: pathlib.Path) -> dict[str, str]:
    """Parse CMake's line-oriented cache without executing any CMake input."""
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("#", "//")):
            continue
        declaration, separator, value = line.partition("=")
        key, type_separator, _ = declaration.partition(":")
        coverage.require(separator and type_separator, "Malformed CMake cache")
        coverage.require(key not in values, f"Duplicate cache key: {key}")
        values[key] = value
    return values


def validate_options(cache: dict[str, str], integer_bits: int) -> None:
    """Require an explicit all-precision reference build and actual ABI route."""
    coverage.require(integer_bits in (32, 64), "Unsupported integer width")
    for key in _REQUIRED_ON:
        coverage.require(cache.get(key) == "ON", f"Required option {key}=ON")
    for key in _REQUIRED_OFF:
        coverage.require(cache.get(key) == "OFF", f"Required option {key}=OFF")
    expected_index64 = "ON" if integer_bits == 64 else "OFF"
    coverage.require(
        cache.get("BUILD_INDEX64") == expected_index64,
        "Requested ABI differs from BUILD_INDEX64",
    )
    coverage.require(
        cache.get("USE_XBLAS") in ("ON", "OFF"), "Unknown XBLAS mode"
    )
    coverage.require(
        cache.get("BUILD_SHARED_LIBS") in ("ON", "OFF"), "Unknown linkage"
    )
    coverage.require(
        bool(cache.get("CMAKE_BUILD_TYPE")), "Missing build configuration"
    )


def test_counts(path: pathlib.Path) -> dict[str, int]:
    """Count actual JUnit cases; reject zero cases, failures, errors and skips."""
    root = element_tree.parse(path).getroot()
    tests = list(root.iter("testcase"))
    counts = {
        "selected": len(tests),
        "executed": len(tests),
        "passed": 0,
        "failed": 0,
        "skipped": 0,
    }
    for test in tests:
        if test.find("skipped") is not None or test.get("status") in (
            "notrun",
            "disabled",
            "skipped",
        ):
            counts["skipped"] += 1
        elif test.find("failure") is not None or test.find("error") is not None:
            counts["failed"] += 1
        else:
            coverage.require(
                test.get("status", "run") in ("run", "passed"),
                "Unknown JUnit case status",
            )
            counts["passed"] += 1
    counts["executed"] -= counts["skipped"]
    coverage.require(bool(tests), "Zero upstream tests")
    coverage.require(
        not counts["failed"] and not counts["skipped"],
        "Upstream tests failed or skipped",
    )
    return counts


def source_identity(source: pathlib.Path, inventory: dict) -> dict:
    """Check Git identity and every exact inventory source input byte sequence."""
    specification = inventory["specification"]
    coverage.require(
        specification["commit"] == coverage.PINNED_COMMIT,
        "Unapproved source commit",
    )
    coverage.require(
        command(["git", "rev-parse", "HEAD"], source)
        == specification["commit"],
        "Source HEAD mismatch",
    )
    coverage.require(
        command(["git", "rev-parse", "HEAD^{tree}"], source)
        == specification["tree"],
        "Source tree mismatch",
    )
    coverage.require(
        not command(
            ["git", "status", "--porcelain", "--untracked-files=all"], source
        ),
        "Source is not pristine",
    )
    for item in inventory["source_inputs"]:
        artifact = coverage.bounded_path(source, item["path"])
        coverage.require(
            coverage.file_hash(artifact) == item["sha256"],
            f"Changed source input: {item['path']}",
        )
    return specification


def installed_files(
    prefix: pathlib.Path, integer_bits: int = 32
) -> list[dict[str, str]]:
    """Hash installed regular files; retain relative names for relocation."""
    records = []
    for path in sorted(prefix.rglob("*")):
        if path.is_file():
            coverage.bounded_path(prefix, path.relative_to(prefix).as_posix())
            records.append(
                {
                    "path": path.relative_to(prefix).as_posix(),
                    "sha256": coverage.file_hash(path),
                }
            )
    names = {record["path"] for record in records}
    coverage.require(
        "include/lapacke.h" in names and "include/lapack.h" in names,
        "Incomplete LAPACKE header installation",
    )
    for stem in ("blas", "lapack", "lapacke"):
        if integer_bits == 64:
            stem += "64"
        coverage.require(
            any(
                pathlib.PurePosixPath(name).name
                in (
                    f"lib{stem}.a",
                    f"lib{stem}.so",
                    f"lib{stem}.dylib",
                    f"{stem}.lib",
                )
                for name in names
            ),
            f"Missing installed {stem} library",
        )
    return records


def compiler_identity(executable: str, cwd: pathlib.Path) -> dict[str, str]:
    """Record the actual compiler executable and version, not a family guess."""
    compiler = pathlib.Path(executable).resolve(strict=True)
    return {
        "path": str(compiler),
        "sha256": coverage.file_hash(compiler),
        "version": command([str(compiler), "--version"], cwd),
    }


def attest(args: argparse.Namespace) -> dict:
    """Validate source/build/install relationship and assemble an exact record."""
    inventory = coverage.read_json(args.inventory)
    lock = coverage.read_json(args.provider_lock)
    coverage.require(
        lock["specification"] == inventory["specification"],
        "Lock and inventory differ",
    )
    source = args.source.resolve(strict=True)
    build = args.build.resolve(strict=True)
    prefix = args.prefix.resolve(strict=True)
    coverage.require(
        len({source, build, prefix}) == 3, "Directories must differ"
    )
    cache_path = build / "CMakeCache.txt"
    cache = cache_values(cache_path)
    validate_options(cache, args.integer_bits)
    coverage.require(
        pathlib.Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() == source,
        "Build uses another source directory",
    )
    coverage.require(
        pathlib.Path(cache["CMAKE_INSTALL_PREFIX"]).resolve() == prefix,
        "Build uses another install prefix",
    )
    payload = {
        "schema_version": 1,
        "specification": source_identity(source, inventory),
        "inventory_sha256": coverage.file_hash(args.inventory),
        "provider_lock_sha256": coverage.file_hash(args.provider_lock),
        "integer_bits": args.integer_bits,
        "integer_route": "lp64" if args.integer_bits == 32 else "global_ilp64",
        "cache_sha256": coverage.file_hash(cache_path),
        "options": {
            key: value
            for key, value in cache.items()
            if key.startswith(
                (
                    "BUILD_",
                    "USE_",
                    "LAPACKE",
                    "CMAKE_C_FLAGS",
                    "CMAKE_Fortran_FLAGS",
                )
            )
            or key == "CMAKE_BUILD_TYPE"
        },
        "compilers": {
            language: compiler_identity(
                cache[f"CMAKE_{language}_COMPILER"], build
            )
            for language in ("C", "Fortran")
        },
        "installed_files": installed_files(prefix, args.integer_bits),
        "upstream_tests": {
            "counts": test_counts(args.junit),
            "junit_sha256": coverage.file_hash(args.junit),
            "log_sha256": coverage.file_hash(args.test_log),
        },
        "asc_abi_verified": False,
        "asc_routines_verified": 0,
        "full_profile_verified": False,
        "limitations": [
            "No ASC wrapper/ABI execution is credited by upstream tests",
            "License/redistribution approval is separate",
            "Foreign runtime/link-map and allocation audits remain required",
        ],
    }
    canonical = json.dumps(
        payload, sort_keys=True, separators=(",", ":")
    ).encode()
    return {
        "identity_sha256": hashlib.sha256(canonical).hexdigest(),
        "created_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "payload": payload,
    }


def main() -> int:
    """Create an external immutable attestation, refusing to overwrite a record."""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "source",
        "build",
        "prefix",
        "inventory",
        "provider-lock",
        "junit",
        "test-log",
        "output",
    ):
        parser.add_argument(f"--{name}", type=pathlib.Path, required=True)
    parser.add_argument(
        "--integer-bits", type=int, choices=(32, 64), required=True
    )
    args = parser.parse_args()
    try:
        source_root = pathlib.Path(__file__).resolve().parents[2]
        try:
            args.output.resolve().relative_to(source_root)
        except ValueError:
            pass
        else:
            raise coverage.ValidationError(
                "Attestations must be outside ASC source"
            )
        document = attest(args)
        with args.output.open("x", encoding="utf-8") as output:
            json.dump(document, output, indent=2, sort_keys=True)
            output.write("\n")
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        subprocess.SubprocessError,
        element_tree.ParseError,
    ) as error:
        print(f"provider attestation failed: {error}", file=sys.stderr)
        return 1
    print(document["identity_sha256"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
