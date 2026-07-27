# Milestone 4 Ownership Ledger

Status: Frozen

Date: 2026-07-26

Branch: `feature/asc-cpp-m4-sparse-cpu`

All scopes are disjoint. Every role preserves cumulative Milestones 0--3 and
the user's prior deletions. No role may commit, push, merge, tag, release,
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
docs/development/asc-cpp-m4-sparse-cpu/milestone-contract.md
docs/development/asc-cpp-m4-sparse-cpu/ownership.md
docs/development/asc-cpp-m4-sparse-cpu/dependency-audit.md
docs/development/asc-cpp-m4-sparse-cpu/provenance-record.md
docs/development/asc-cpp-m4-sparse-cpu/publication-checkpoint-b.md
```

Responsibilities: actual ASCCMake usage, root/module/test integration,
component exports, package/consumer registration, architecture manifests,
complete diff review, reconciliation, clean validation, and checkpoint
reporting.

## Production implementation agent

Exclusive write scope:

```text
include/asc/sparse.h
include/asc/sparse/**
src/sparse/*.cc
include/asc/expression/writable.h
include/asc/expression.h
include/asc/expression/expression.h
include/asc/dense/view.h
docs/development/asc-cpp-m4-sparse-cpu/production-self-review.md
```

The expression/dense files are limited to the frozen storage-neutral
placement/writable protocol and dense-view specialization. The agent may not
edit another predecessor behavior, CMake, tests, general docs, packages,
manifests, or providers.

`include/asc/expression/expression.h` was lead-reassigned after independent
portability finding M4-PORT-01 demonstrated that identity-only alias tokens
could not conservatively reject partially overlapping ASC views. The
reassignment is limited to storage-neutral, span-aware alias metadata that
preserves existing identity-token source behavior; it does not authorize
storage ownership, evaluation, or a sibling dependency in expression.

## Independent verification agent

Exclusive write scope:

```text
tests/sparse/**
tests/compile/m4_*.cc
tests/consumer/sparse/**
benchmarks/sparse/**
docs/development/asc-cpp-m4-sparse-cpu/verification-design.md
docs/development/asc-cpp-m4-sparse-cpu/verification-review.md
```

The verifier derives expectations independently from the frozen contract and
accepted ADRs. It must not inspect MdeCpp tests or copy deleted/upstream test
material. It does not modify production or CMake files.

## Documentation and API review agent

Exclusive write scope:

```text
docs/modules/sparse.md
docs/modules/expression.md
docs/development/asc-cpp-m4-sparse-cpu/documentation-api-review.md
```

The reviewer tests public usability and documents formats, policies,
ownership, lifetime, aliasing, failure transactions, complexity, allocation,
no-densification, execution, numerical behavior, packaging, and examples.
API defects are reported to the lead rather than documented around.

`docs/modules/expression.md` was lead-reassigned after M4-PORT-01 required
span-aware neutral alias metadata in expression. The documentation agent's
change is limited to reconciling that public alias and writable/placement
protocol; unrelated Milestone 2 expression behavior remains unchanged.

## Portability/GPU/performance review agent

Exclusive write scope:

```text
docs/development/asc-cpp-m4-sparse-cpu/portability-review.md
```

This is an independent review-only role. It audits C++20 portability,
warnings, exceptions-disabled compilation, static/shared/DLL concerns,
checked arithmetic, undefined behavior, sanitizers, provider isolation,
allocation, conversion, densification, benchmark methodology, and GPU evidence.
For M4, GPU evidence is exactly `skipped`.

## Conflict rule

If work outside an assigned scope is required, the role reports the exact
file/symbol and reason. Only the lead may reassign ownership or perform a
shared integration change.
