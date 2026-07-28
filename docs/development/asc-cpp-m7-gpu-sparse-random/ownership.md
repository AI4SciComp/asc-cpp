# Milestone 7 Ownership Ledger

Status: Frozen

Date: 2026-07-28

Branch: `feature/asc-cpp-m7-gpu-sparse-random`

Every role preserves cumulative Milestones 0--6 and the owner's prior
deletions. No role may commit, push, merge, tag, release, delete a branch, add
an unapproved dependency, or implement Milestone 8.

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
tests/compile/dependency_check.cmake
tests/consumer/run_*.cmake
tests/package/**
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/modules/core.md
docs/modules/dense.md
docs/modules/expression.md
docs/modules/utilities.md
docs/development/asc-cpp-architecture/**
docs/development/asc-cpp-m7-gpu-sparse-random/milestone-contract.md
docs/development/asc-cpp-m7-gpu-sparse-random/ownership.md
docs/development/asc-cpp-m7-gpu-sparse-random/preflight.md
docs/development/asc-cpp-m7-gpu-sparse-random/dependency-audit.md
docs/development/asc-cpp-m7-gpu-sparse-random/provenance-record.md
docs/development/asc-cpp-m7-gpu-sparse-random/publication-checkpoint-b.md
```

The lead alone owns versioning, CUDA option/language/discovery, target
definitions, conditional exports and package closures, shared manifests,
test registration, ASCCMake integration, dependency-audit layer filtering,
clean validation, reconciliation, and checkpoint reporting. The lead alone
edits any root, package, or shared CMake file.

## Production implementation agent

Exclusive write scope:

```text
include/asc/sparse/**
include/asc/random/providers/**
src/sparse/cuda/**
src/random/cuda/**
docs/development/asc-cpp-m7-gpu-sparse-random/production-self-review.md
```

Only the frozen sparse CUDA context/staging/evaluator/CSR SpMV and raw/dense/
sparse random CUDA facets may be implemented. Minimum provider-enabling
evolution of existing sparse owners is allowed. Core, Dense, Expression, base
Random algorithms, CMake, tests, manifests, general docs, and later-milestone
files are forbidden.

## Independent verification agent

Exclusive write scope:

```text
tests/sparse_cuda/** except tests/sparse_cuda/CMakeLists.txt
tests/random_cuda/** except tests/random_cuda/CMakeLists.txt
tests/random_dense_cuda/** except tests/random_dense_cuda/CMakeLists.txt
tests/random_sparse_cuda/** except tests/random_sparse_cuda/CMakeLists.txt
tests/compile/m7_*.cc
tests/compile/m7_*.h
tests/consumer/sparse_cuda/** except CMakeLists.txt
tests/consumer/random_cuda/** except CMakeLists.txt
tests/consumer/random_dense_cuda/** except CMakeLists.txt
tests/consumer/random_sparse_cuda/** except CMakeLists.txt
benchmarks/sparse_cuda/** except benchmarks/sparse_cuda/CMakeLists.txt
benchmarks/random_cuda/** except benchmarks/random_cuda/CMakeLists.txt
docs/development/asc-cpp-m7-gpu-sparse-random/verification-design.md
docs/development/asc-cpp-m7-gpu-sparse-random/verification-review.md
```

The verifier freezes independent structural, numerical, bit, lifetime,
failure, allocation/workspace, package-consumer, and benchmark oracles before
inspecting production. It does not inspect/copy prohibited historical code or
modify production, CMake, package, architecture, or general documentation.

## Documentation and API review agent

Exclusive write scope:

```text
docs/modules/sparse.md
docs/modules/random.md
docs/development/asc-cpp-m7-gpu-sparse-random/documentation-api-review.md
```

The reviewer documents and audits exact components/APIs, configuration,
ownership/lifetime, device storage, supported operations, failures, costs,
reproducibility, evidence labels, and consumer examples. API defects are
reported to the lead and production owner.

## Portability/GPU/performance review agent

Exclusive write scope:

```text
docs/development/asc-cpp-m7-gpu-sparse-random/portability-review.md
```

This review-only role audits C++20/CUDA portability, host compilers, checked
narrowing, device-address safety, provider error translation, cuSPARSE
index/algorithm/workspace behavior, async lifetimes, determinism,
static/shared packaging, CPU-only isolation, sanitizers, benchmarks, and exact
GPU evidence labels.

## Conflict rule

Scopes are disjoint. If another file is required, the role reports the exact
file/symbol and reason. Only the lead may reassign ownership or edit a shared
integration path.
