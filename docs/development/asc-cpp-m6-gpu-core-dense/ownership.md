# Milestone 6 Ownership Ledger

Status: Frozen

Date: 2026-07-28

Branch: `feature/asc-cpp-m6-gpu-core-dense`

Every role preserves cumulative Milestones 0--5 and the owner's prior
deletions. No role may commit, push, merge, tag, release, delete a branch, add
an unapproved dependency, or implement Milestone 7 or 8.

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
tests/compile/*_dependency_check.cmake
tests/consumer/run_*.cmake
tests/package/**
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/modules/expression.md
docs/modules/random.md
docs/modules/sparse.md
docs/modules/utilities.md
docs/development/asc-cpp-architecture/**
docs/development/asc-cpp-m6-gpu-core-dense/milestone-contract.md
docs/development/asc-cpp-m6-gpu-core-dense/ownership.md
docs/development/asc-cpp-m6-gpu-core-dense/preflight.md
docs/development/asc-cpp-m6-gpu-core-dense/dependency-audit.md
docs/development/asc-cpp-m6-gpu-core-dense/provenance-record.md
docs/development/asc-cpp-m6-gpu-core-dense/publication-checkpoint-b.md
```

The lead alone owns CUDA option/language/discovery, target definitions,
conditional package exports/closures, test registration, shared manifests,
integration, reconciliation, clean matrices, and checkpoint reporting.
The live provider-free module guides were added to lead reconciliation scope
only for the mechanical package-version update from 0.5 to 0.6; their APIs and
Milestone 2--5 behavior remain unchanged.
The predecessor dependency audits remain frozen to their original layers and
were added only so the lead can exclude the separately audited M6 provider
subdirectories from their inventory globbing.

## Production implementation agent

Exclusive write scope:

```text
include/asc/core/execution.h
include/asc/core/memory.h
include/asc/core/providers/cuda.h
include/asc/core/providers/cuda_export.h
include/asc/dense/array.h
include/asc/dense/view.h
include/asc/dense/providers/cuda.h
include/asc/dense/providers/cuda_export.h
src/core/execution.cc
src/core/execution_internal.h
src/core/cuda/**
src/dense/cuda/**
docs/development/asc-cpp-m6-gpu-core-dense/production-self-review.md
```

Only the frozen opaque execution/event evolution, Core CUDA runtime facet,
uninitialized Dense owner, bounded evaluator, and selected CUDA Dense algebra
may be implemented. CMake, tests, manifests, general docs, Sparse, Random, and
later-milestone files are forbidden.

## Independent verification agent

Exclusive write scope:

```text
tests/core_cuda/**
tests/dense_cuda/**
tests/compile/m6_*.cc
tests/compile/m6_*.h
tests/consumer/core_cuda/**
tests/consumer/dense_cuda/**
benchmarks/dense_cuda/**
docs/development/asc-cpp-m6-gpu-core-dense/verification-design.md
docs/development/asc-cpp-m6-gpu-core-dense/verification-review.md
```

The verifier freezes independent state, lifetime, copy, event, numerical,
allocation, synchronization, and parity oracles before inspecting production.
It does not edit production/CMake or inspect/copy prohibited historical code.

## Documentation and API review agent

Exclusive write scope:

```text
docs/modules/core.md
docs/modules/dense.md
docs/development/asc-cpp-m6-gpu-core-dense/documentation-api-review.md
```

The reviewer documents/tests configuration, component use, ownership,
asynchronous lifetimes, memory spaces, supported operations, errors, provider
limits, numerical claims, and examples. API defects are reported to the lead.

## Portability/GPU/performance review agent

Exclusive write scope:

```text
docs/development/asc-cpp-m6-gpu-core-dense/portability-review.md
```

This independent review-only role audits host/CUDA portability, static/shared
packaging, provider isolation, narrowing, asynchronous lifetimes, sanitizers,
error translation, cuBLAS determinism/capabilities, transfers, synchronization,
benchmark methodology, and every GPU evidence label.

## Conflict rule

Scopes are disjoint. If another file is required, the role reports the exact
file/symbol and reason. Only the lead may reassign ownership or edit a shared
integration path.
