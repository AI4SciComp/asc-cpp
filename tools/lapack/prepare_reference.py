#!/usr/bin/env python3
# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Prepare the exact locked provider in a new external directory.

This development tool builds and tests the dependency; it never copies it into
ASC or approves redistribution. The existing attestor validates pristine source,
build options, real Fortran integer width and every executed upstream test.
"""

from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import subprocess
import sys
import time
import xml.etree.ElementTree as element_tree

import attest_provider as attestation
import validate_coverage as coverage


def run(work: pathlib.Path, name: str, argv: list[str]) -> None:
    """Run a command with exclusive logs, retaining its actual exit status."""
    started = time.monotonic()
    started_at = datetime.datetime.now(datetime.timezone.utc).isoformat()
    with (work / f"{name}.log").open("x", encoding="utf-8") as log:
        result = subprocess.run(argv,
                                check=False,
                                stdout=log,
                                stderr=subprocess.STDOUT)
    record = {
        "argv": argv,
        "exit_code": result.returncode,
        "elapsed_seconds": time.monotonic() - started,
        "started": started_at,
        "completed": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    }
    with (work / f"{name}.json").open("x", encoding="utf-8") as output:
        json.dump(record, output, indent=2)
        output.write("\n")
    print(f"{name}: {result.returncode}", flush=True)
    if result.returncode:
        raise RuntimeError(f"{name} failed; see {work / (name + '.log')}")


def check_selection(listing: dict, junit: pathlib.Path) -> None:
    """Require every configured provider test to execute exactly once."""
    tests = listing.get("tests", [])
    coverage.require(bool(tests), "The pinned provider selected no tests")
    names = [test["name"] for test in tests]
    coverage.require(len(set(names)) == len(names), "Duplicate configured test")
    coverage.require(all(test.get("command") for test in tests),
                     "Configured provider test has no executable")
    actual = [
        case.get("name")
        for case in element_tree.parse(junit).getroot().iter("testcase")
    ]
    coverage.require(
        len(actual) == len(set(actual)) and set(actual) == set(names),
        "Provider selection and execution differ")
    attestation.test_counts(junit)


def read_listing(text: str) -> tuple[dict, str]:
    """Read CTest JSON followed by the pinned provider's optional summary.

    The upstream post-test callback can also run during --show-only. Preserve
    its footer separately; it is not executed-test or passing-test evidence.
    Unexpected extra output remains an error.
    """
    listing, end = json.JSONDecoder().raw_decode(text.lstrip())
    coverage.require(
        isinstance(listing, dict) and listing.get("kind") == "ctestInfo",
        "Unexpected CTest listing document")
    footer = text.lstrip()[end:]
    coverage.require(
        not footer.strip() or
        footer.lstrip().startswith("-->   LAPACK TESTING SUMMARY  <--"),
        "Unexpected output after CTest listing")
    return listing, footer


def main() -> int:
    """Build one actual ABI without reusing or overwriting prior evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=pathlib.Path, required=True)
    parser.add_argument("--integer-bits",
                        type=int,
                        choices=(32, 64),
                        required=True)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--cc", default="gcc-11")
    parser.add_argument("--cxx", default="g++-11")
    parser.add_argument("--fc", default="gfortran-11")
    parser.add_argument("--fortran-flags", default="")
    parser.add_argument(
        "--linkage",
        choices=("static", "shared"),
        default="static",
        help="External provider linkage; does not admit an ASC profile")
    parser.add_argument(
        "--source-dir",
        type=pathlib.Path,
        help="Reuse a pristine exact-pin external source without modifying it")
    args = parser.parse_args()
    source_root = pathlib.Path(__file__).resolve().parents[2]
    work = args.work_dir.resolve()
    if work == source_root or source_root in work.parents or args.jobs < 1:
        parser.error("Use a fresh external directory and a positive job count")
    lock_path = source_root / "docs/contracts/lapack-provider-lock.json"
    lock = json.loads(lock_path.read_text(encoding="utf-8"))["specification"]
    source = args.source_dir.resolve(
        strict=True) if args.source_dir else work / "source"
    if args.source_dir:
        if (source == source_root or source_root in source.parents or
                work == source or source in work.parents):
            parser.error(
                "Reused source and build must remain separate and external to ASC"
            )
        inventory = coverage.read_json(
            source_root / "docs/contracts/lapack-upstream-inventory.json")
        coverage.require(inventory["specification"] == lock,
                         "Source lock and inventory differ")
        attestation.source_identity(source, inventory)
    work.mkdir(parents=True, exist_ok=False)
    build = work / "build"
    prefix = work / "prefix"
    if args.source_dir:
        with (work / "source-reuse.json").open("x", encoding="utf-8") as record:
            json.dump(
                {
                    "path":
                        str(source),
                    "specification":
                        lock,
                    "action":
                        "Pristine source identity checked; no checkout or source write"
                },
                record,
                indent=2)
            record.write("\n")
    else:
        run(work, "clone", [
            "git", "clone", "--filter=blob:none", "--no-checkout", "--branch",
            lock["tag"], lock["upstream_url"],
            str(source)
        ])
        run(work, "checkout",
            ["git", "-C",
             str(source), "checkout", "--detach", lock["commit"]])
    configure = [
        "cmake", "-S",
        str(source), "-B",
        str(build), "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={prefix}", f"-DCMAKE_C_COMPILER={args.cc}",
        f"-DCMAKE_CXX_COMPILER={args.cxx}",
        f"-DCMAKE_Fortran_COMPILER={args.fc}",
        f"-DCMAKE_Fortran_FLAGS={args.fortran_flags}",
        "-DBUILD_INDEX64=" + ("ON" if args.integer_bits == 64 else "OFF"),
        "-DBUILD_SHARED_LIBS=" + ("ON" if args.linkage == "shared" else "OFF")
    ]
    for option in ("BUILD_SINGLE", "BUILD_DOUBLE", "BUILD_COMPLEX",
                   "BUILD_COMPLEX16", "BUILD_DEPRECATED", "BUILD_TESTING",
                   "LAPACKE", "LAPACKE_BUILD_SINGLE", "LAPACKE_BUILD_DOUBLE",
                   "LAPACKE_BUILD_COMPLEX", "LAPACKE_BUILD_COMPLEX16"):
        configure.append(f"-D{option}=ON")
    for option in ("USE_OPTIMIZED_BLAS", "USE_OPTIMIZED_LAPACK",
                   "BUILD_INDEX64_EXT_API", "USE_XBLAS", "LAPACKE_WITH_TMG"):
        configure.append(f"-D{option}=OFF")
    run(work, "configure", configure)
    run(work, "build",
        ["cmake", "--build",
         str(build), "--parallel",
         str(args.jobs)])
    run(work, "listing",
        ["ctest", "--test-dir",
         str(build), "--show-only=json-v1"])
    listing, footer = read_listing(
        (work / "listing.log").read_text(encoding="utf-8"))
    with (work / "listing-post-test.log").open("x", encoding="utf-8") as output:
        output.write(footer)
    if not listing["tests"]:
        raise RuntimeError("The pinned provider selected no tests")
    run(work, "tests", [
        "ctest", "--test-dir",
        str(build), "--no-tests=error", "--verbose", "--parallel",
        str(args.jobs), "--output-junit",
        str(work / "tests.xml")
    ])
    check_selection(listing, work / "tests.xml")
    run(work, "install", ["cmake", "--install", str(build)])
    run(work, "attest", [
        sys.executable, "-B",
        str(source_root / "tools/lapack/attest_provider.py"), "--source",
        str(source), "--build",
        str(build), "--prefix",
        str(prefix), "--inventory",
        str(source_root / "docs/contracts/lapack-upstream-inventory.json"),
        "--provider-lock",
        str(lock_path), "--junit",
        str(work / "tests.xml"), "--test-log",
        str(work / "tests.log"), "--integer-bits",
        str(args.integer_bits), "--output",
        str(work / "attestation.json")
    ])
    return 0


if __name__ == "__main__":
    sys.exit(main())
