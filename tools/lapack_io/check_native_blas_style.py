#!/usr/bin/env python3
"""Run the bounded native BLAS test/benchmark clang-tidy acceptance scope.

Configure an out-of-source build with testing, benchmarks and compile-command
export enabled first. This explicit development check does not change the
production-only tidy gate or download tools. Capture its output outside source.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import subprocess
import sys


_SOURCES = (
    "tests/dense/blas_test.cc",
    "tests/dense/blas_level1_test.cc",
    "tests/dense/blas_level2_test.cc",
    "tests/dense/blas_level3_test.cc",
    "benchmarks/dense/dense_benchmark.cc",
)


def check_scope(build: pathlib.Path, source: pathlib.Path) -> list[pathlib.Path]:
    """Require all five exact translation units in the compilation database."""
    with (build / "compile_commands.json").open(encoding="utf-8") as stream:
        entries = json.load(stream)
    compiled = set()
    for entry in entries:
        path = pathlib.Path(entry["file"])
        if not path.is_absolute():
            path = pathlib.Path(entry["directory"]) / path
        compiled.add(path.resolve())
    required = [(source / name).resolve() for name in _SOURCES]
    missing = [str(path) for path in required if path not in compiled]
    if missing:
        raise ValueError(
            "Missing required compile commands: " + ", ".join(missing)
        )
    return required


def main() -> int:
    """Check every recorded translation unit and fail if any audit fails."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=pathlib.Path, required=True)
    parser.add_argument("--clang-tidy", default="clang-tidy-18")
    args = parser.parse_args()
    executable = shutil.which(args.clang_tidy)
    if executable is None:
        parser.error("clang-tidy executable is unavailable: " + args.clang_tidy)
    source = pathlib.Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    try:
        paths = check_scope(build, source)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Native BLAS style scope rejected: " + str(error), file=sys.stderr)
        return 1
    failures = 0
    for path in paths:
        print("Checking " + str(path.relative_to(source)), flush=True)
        result = subprocess.run(
            [executable, str(path), "-p", str(build)], check=False
        )
        failures += result.returncode != 0
    print(f"Native BLAS style: {len(paths)} checked, {failures} failed", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
