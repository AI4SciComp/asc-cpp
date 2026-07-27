# Milestone 3 Ownership Ledger

Status: Frozen

Date: 2026-07-26

Branch: `feature/asc-cpp-m3-dense-cpu`

All scopes are disjoint. Agents preserve cumulative Milestones 0--2 and the
user's prior deletions. No agent may commit, push, merge, tag, release, delete
a branch, add a dependency, or implement a later milestone.

## Lead integrator

Exclusive write scope:

```text
CMakeLists.txt
CMakePresets.json
.github/workflows/ci.yml
cmake/**
src/**/CMakeLists.txt
tests/**/CMakeLists.txt
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/development/asc-cpp-architecture/**
docs/development/asc-cpp-m3-dense-cpu/milestone-contract.md
docs/development/asc-cpp-m3-dense-cpu/ownership.md
docs/development/asc-cpp-m3-dense-cpu/dependency-audit.md
docs/development/asc-cpp-m3-dense-cpu/provenance-record.md
docs/development/asc-cpp-m3-dense-cpu/publication-checkpoint-b.md
```

Responsibilities: actual ASCCMake usage, root/module/test integration,
component exports, package/consumer registration, manifests, complete diff
review, reconciliation, clean validation, and checkpoint reporting.

## Production implementation agent

Exclusive write scope:

```text
include/asc/dense.h
include/asc/dense/**
src/dense/*.cc
docs/development/asc-cpp-m3-dense-cpu/production-self-review.md
```

The agent implements only the frozen dense CPU production contract. It may run
ad hoc builds but may not edit CMake, tests, other modules, general docs, or
package files.

## Independent verification agent

Exclusive write scope:

```text
tests/dense/**
tests/compile/m3_*.cc
tests/consumer/dense/**
benchmarks/dense/**
docs/development/asc-cpp-m3-dense-cpu/verification-design.md
docs/development/asc-cpp-m3-dense-cpu/verification-review.md
```

The agent derives tests independently from the frozen contract and accepted
ADRs. It must not inspect MdeCpp tests or copy deleted/upstream test material.
It does not modify production or CMake files.

## Documentation and API review agent

Exclusive write scope:

```text
docs/modules/dense.md
docs/development/asc-cpp-m3-dense-cpu/documentation-api-review.md
```

The agent reviews public usability, costs, lifetime, alias, error, numerical,
component, and example contracts. It may write the dense module guide and its
review report only; proposed changes elsewhere are findings for the lead.

## Portability/GPU/performance review agent

Exclusive write scope:

```text
docs/development/asc-cpp-m3-dense-cpu/portability-review.md
```

This is an independent review-only role. It audits C++20 portability,
warnings, exceptions-disabled compilation, static/shared/DLL concerns,
overflow/undefined behavior, sanitizer suitability, provider isolation,
allocation/packing/transfer behavior, and benchmark methodology. GPU evidence
must be classified exactly; for M3 it is expected to be `skipped`.

## Conflict rule

If work outside an assigned scope is needed, the agent reports the exact
file/symbol and rationale. Only the lead may reassign ownership or perform the
integration change.
