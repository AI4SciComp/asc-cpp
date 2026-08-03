#!/usr/bin/env python3
"""Generate the reviewed v0.9.0 disposition for every baseline path."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


BASELINE = "snapshot/v0.9.0-candidate-20260802^{}"

RELOCATIONS = {
    "docs/development/asc-cpp-architecture/architecture-blueprint.md":
        "docs/architecture/overview.md",
    "docs/development/asc-cpp-architecture/dependency-manifest.yaml":
        "docs/contracts/dependency-manifest.yaml",
    "docs/development/asc-cpp-architecture/capability-manifest.yaml":
        "docs/contracts/capability-manifest.yaml",
    "docs/development/asc-cpp-architecture/blas-coverage.yaml":
        "docs/contracts/blas-coverage.yaml",
    "docs/development/asc-cpp-architecture/random-crosswalk.yaml":
        "docs/contracts/random-crosswalk.yaml",
    "docs/development/asc-cpp-architecture/mdecpp-disposition.yaml":
        "docs/provenance/mdecpp-disposition.yaml",
    "docs/development/asc-cpp-architecture/provenance-review.md":
        "docs/provenance/mdecpp-review.md",
}
for number, name in (
    ("0001", "six-module-graph"),
    ("0002", "package-target-naming"),
    ("0003", "namespace-and-source-policy"),
    ("0004", "error-and-exception-policy"),
    ("0005", "configuration-boundary"),
    ("0006", "io-and-serialization-boundary"),
    ("0007", "index-rank-shape-types"),
    ("0008", "memory-and-execution"),
    ("0009", "ownership-buffers-views"),
    ("0010", "expression-protocol"),
    ("0011", "dense-semantics"),
    ("0012", "sparse-semantics"),
    ("0013", "dense-linalg-providers"),
    ("0014", "sparse-linalg-providers"),
    ("0015", "random-reproducibility"),
    ("0016", "mixed-dense-sparse"),
    ("0017", "third-party-provenance"),
    ("0018", "versioning-release-boundaries"),
    ("0019", "blas-contract"),
    ("0020", "random-contract"),
):
    old = f"docs/development/asc-cpp-architecture/decisions/{number}-{name}.md"
    RELOCATIONS[old] = f"docs/architecture/decisions/{number}-{name}.md"

CONSOLIDATIONS = {
    "docs/api.md": "docs/api/mainpage.md and docs/api/conventions.md",
    "docs/development/asc-cpp-architecture/asc-cmake-consumption.md":
        "docs/installation.md and docs/release-process.md",
    "docs/development/asc-cpp-architecture/backend-capability-matrix.md":
        "docs/support-matrix.md",
    "docs/development/asc-cpp-architecture/ci-strategy.md":
        "docs/release-process.md and CONTRIBUTING.md",
    "docs/development/asc-cpp-architecture/testing-strategy.md":
        "docs/release-process.md, CONTRIBUTING.md, and module pages",
}

ARCHIVE_EXACT = {
    "docs/blas-completion-audit.md",
    "docs/random-completion-audit.md",
    "docs/development/asc-cpp-architecture/disagreement-matrix.md",
    "docs/development/asc-cpp-architecture/implementation-plan.md",
    "docs/development/asc-cpp-architecture/independent-analysis.md",
    "docs/development/asc-cpp-architecture/internal-analysis.md",
    "docs/development/asc-cpp-architecture/mdecpp-analysis.md",
    "docs/development/asc-cpp-architecture/release-roadmap.md",
    "docs/development/asc-cpp-architecture/repository-audit.md",
}
ARCHIVE_PREFIXES = tuple(
    f"docs/development/asc-cpp-m{number}-" for number in range(9)
)


def tracked_paths() -> list[str]:
    output = subprocess.check_output(
        ["git", "ls-tree", "-r", "--name-only", BASELINE], text=True
    )
    return output.splitlines()


def keep_reason(path: str) -> tuple[str, str]:
    prefix_reasons = (
        ("include/", "Public API", "CMake file sets, consumers, Doxygen"),
        ("src/", "Product implementation", "Library targets and tests"),
        ("tests/", "Verification evidence", "CTest and release matrix"),
        ("benchmarks/", "Reference performance evidence", "Benchmark targets"),
        ("abi/", "ABI and public-surface contract", "Hardening tests"),
        ("cmake/", "Build/package infrastructure", "Top-level CMake and generators"),
        ("data/", "Runtime scientific data and license", "Random package and tests"),
        ("tools/", "Regeneration/validation tooling", "Contract and hardening tests"),
        ("docs/", "Durable release documentation", "Readers, Doxygen, and link checks"),
        (".github/", "Repository automation/community policy", "GitHub Actions and contributors"),
    )
    for prefix, reason, consumers in prefix_reasons:
        if path.startswith(prefix):
            return reason, consumers
    return "Release root metadata or policy", "Build, packaging, release, or contributors"


def classify(path: str) -> tuple[str, str, str]:
    if path in RELOCATIONS:
        return "relocate", RELOCATIONS[path], "Links, tests, generators, and Doxygen"
    if path in CONSOLIDATIONS:
        return "consolidate", CONSOLIDATIONS[path], "Release documentation"
    if path in ARCHIVE_EXACT or path.startswith(ARCHIVE_PREFIXES):
        return (
            "archive-only",
            "snapshot tag and Git history",
            "Durable decisions/provenance extracted before removal",
        )
    reason, consumers = keep_reason(path)
    return "keep", reason, consumers


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    paths = tracked_paths()
    rows: list[str] = []
    counts = {name: 0 for name in ("keep", "relocate", "consolidate", "archive-only", "remove")}
    for path in paths:
        disposition, destination, consumers = classify(path)
        counts[disposition] += 1
        rows.append(
            f"| `{path}` | {disposition} | {destination} | {consumers} |"
        )

    expected = {
        "keep": 442,
        "relocate": 27,
        "consolidate": 5,
        "archive-only": 104,
        "remove": 0,
    }
    if counts != expected or len(paths) != 578:
        raise SystemExit(f"unexpected baseline classification: {len(paths)=}, {counts=}")

    header = """# v0.9.0 release-tree manifest

This manifest records the Gate A-approved disposition of every path tracked at
`402cbac35334bb2a20e7e6afa7214efb8fad1c8f`, preserved by the annotated tag
`snapshot/v0.9.0-candidate-20260802`. It is generated by
`tools/release/generate_release_tree_manifest.py`; changes require release
approver review.

Summary: 442 keep, 27 relocate, 5 consolidate, 104 archive-only, 0 remove.
Archive-only material remains immutable in the snapshot tag and Git history.

| Baseline path | Disposition | Release destination or reason | Consumers/evidence |
| --- | --- | --- | --- |
"""
    args.output.write_text(header + "\n".join(rows) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
