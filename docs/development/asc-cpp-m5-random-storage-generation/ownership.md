# Milestone 5 Ownership Ledger

Status: Frozen

Date: 2026-07-28

Branch: `feature/asc-cpp-m5-random-storage-generation`

All scopes are disjoint. Every role preserves cumulative Milestones 0--4 and
the owner's prior deletions. No role may commit, push, merge, tag, release,
delete a branch, add a dependency, or implement a later milestone.

## Lead integrator

Exclusive write scope:

```text
CMakeLists.txt
CMakePresets.json
.github/workflows/ci.yml
cmake/**
src/**/CMakeLists.txt
tests/**/CMakeLists.txt
tests/architecture/**
tests/compile/dependency_check.cmake
tests/consumer/run_*.cmake
tests/package/**
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/development/asc-cpp-architecture/**
docs/development/asc-cpp-m5-random-storage-generation/milestone-contract.md
docs/development/asc-cpp-m5-random-storage-generation/ownership.md
docs/development/asc-cpp-m5-random-storage-generation/preflight.md
docs/development/asc-cpp-m5-random-storage-generation/dependency-audit.md
docs/development/asc-cpp-m5-random-storage-generation/provenance-record.md
docs/development/asc-cpp-m5-random-storage-generation/publication-checkpoint-b.md
```

Responsibilities: exact ASCCMake use, root/facet/test integration,
component exports, aggregate target, package/consumer registration,
architecture manifests, complete diff review, reconciliation, clean
validation, and checkpoint reporting.

## Production implementation agent

Exclusive write scope:

```text
include/asc/random/dense.h
include/asc/random/sparse.h
docs/development/asc-cpp-m5-random-storage-generation/production-self-review.md
```

The agent implements only the frozen CPU template facets. It may not edit the
random base, another module, CMake, tests, package files, general docs,
manifests, providers, or later-milestone paths.

## Independent verification agent

Exclusive write scope:

```text
tests/random_dense/**
tests/random_sparse/**
tests/compile/m5_*.cc
tests/compile/m5_*.h
tests/consumer/random_dense/**
tests/consumer/random_sparse/**
tests/consumer/cpp/**
benchmarks/random_storage/**
docs/development/asc-cpp-m5-random-storage-generation/verification-design.md
docs/development/asc-cpp-m5-random-storage-generation/verification-review.md
```

The verifier freezes independent structural, sequence, state-advance, and
allocation oracles before inspecting production source. It must not inspect or
copy MdeCpp/deleted asc-cpp random tests or implementation. It does not modify
production or CMake files.

## Documentation and API review agent

Exclusive write scope:

```text
docs/modules/random.md
docs/development/asc-cpp-m5-random-storage-generation/documentation-api-review.md
```

The reviewer tests public usability and documents state, sequence mapping,
layout/canonical traversal, ownership, failure transactions, allocation,
complexity, concurrency, packaging, provider limits, provenance, and examples.
API defects are reported to the lead rather than documented around.

## Portability/GPU/performance review agent

Exclusive write scope:

```text
docs/development/asc-cpp-m5-random-storage-generation/portability-review.md
```

This is an independent review-only role. It audits C++20 portability,
warnings, exceptions-disabled compilation, static/shared/DLL concerns,
checked arithmetic, undefined behavior, concurrency, sanitizers, provider
isolation, allocation, deterministic sequence behavior, benchmark
methodology, and GPU evidence. For M5, GPU evidence is exactly `skipped`.

## Conflict rule

If work outside an assigned scope is required, the role reports the exact
file/symbol and reason. Only the lead may reassign ownership or perform a
shared integration change.
