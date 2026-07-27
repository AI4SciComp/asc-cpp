# Milestone 6 Ownership Ledger

Status: Frozen

Date: 2026-07-26

Branch: `feature/asc-cpp-m6-gpu-core-dense`

All scopes are disjoint. Every role preserves cumulative Milestones 0--5 and
the owner's prior deletions. No role may commit, push, merge, tag, release,
delete a branch, add an unapproved dependency, or implement Milestone 7 or 8.

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
docs/development/asc-cpp-m6-gpu-core-dense/milestone-contract.md
docs/development/asc-cpp-m6-gpu-core-dense/ownership.md
docs/development/asc-cpp-m6-gpu-core-dense/preflight.md
docs/development/asc-cpp-m6-gpu-core-dense/dependency-audit.md
docs/development/asc-cpp-m6-gpu-core-dense/provenance-record.md
docs/development/asc-cpp-m6-gpu-core-dense/publication-checkpoint-b.md
```

Responsibilities: exact ASCCMake and standard-CMake use, CUDA option/language/
discovery, target definitions, conditional exports, package closure, test
registration, architecture manifests, complete diff review, reconciliation,
clean CPU/CUDA validation, evidence classification, and checkpoint reporting.

## Production implementation agent

Exclusive write scope:

```text
include/asc/core/execution.h
include/asc/core/memory.h
include/asc/core/providers/cuda.h
include/asc/core/providers/cuda_export.h
include/asc/dense/array.h
include/asc/dense/providers/cuda.h
include/asc/dense/providers/cuda_export.h
src/core/execution.cc
src/core/cuda/**
src/core/execution_internal.h
src/dense/cuda/**
docs/development/asc-cpp-m6-gpu-core-dense/production-self-review.md
```

The agent implements only the frozen opaque execution/event evolution, core
CUDA runtime facet, explicit uninitialized dense storage, bounded pointwise
CUDA evaluator, and selected asynchronous cuBLAS operations. It may not edit
CMake, tests, package files, general docs, manifests, another module, random,
sparse, or later-milestone paths.

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

The verifier freezes independent state, lifetime, copy, event, numerical, and
parity oracles before inspecting production. It does not inspect or copy
MdeCpp/deleted asc-cpp CUDA code or tests and does not modify production or
CMake files.

## Documentation and API review agent

Exclusive write scope:

```text
docs/modules/core.md
docs/modules/dense.md
docs/development/asc-cpp-m6-gpu-core-dense/documentation-api-review.md
```

The reviewer documents and tests public configuration, component use, memory,
context/stream/event ownership, asynchronous lifetime, device storage,
supported evaluator/algebra capabilities, failures, costs, provider limits,
numerical parity, GPU evidence, and installed examples. API defects are
reported to the lead rather than documented around.

## Portability/GPU/performance review agent

Exclusive write scope:

```text
docs/development/asc-cpp-m6-gpu-core-dense/portability-review.md
```

This independent review-only role audits C++20/CUDA portability, host compiler
compatibility, static/shared packaging, CPU-only/provider-enabled isolation,
checked provider narrowing, undefined behavior, asynchronous lifetimes,
sanitizers, CUDA error translation, cuBLAS capabilities/determinism,
allocation/transfer/synchronization, benchmark methodology, and all four GPU
evidence labels.

## Conflict rule

If work outside an assigned scope is required, the role reports the exact
file/symbol and reason. Only the lead may reassign ownership or perform a
shared integration change.
