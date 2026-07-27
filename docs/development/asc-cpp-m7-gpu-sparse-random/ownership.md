# Milestone 7 Ownership Ledger

Status: Frozen

Date: 2026-07-27

Branch: `feature/asc-cpp-m7-gpu-sparse-random`

All scopes are disjoint. Every role preserves cumulative Milestones 0--6 and
the owner's prior deletions. No role may commit, push, merge, tag, release,
delete a branch, add an unapproved dependency, or implement Milestone 8.

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
docs/development/asc-cpp-m7-gpu-sparse-random/milestone-contract.md
docs/development/asc-cpp-m7-gpu-sparse-random/ownership.md
docs/development/asc-cpp-m7-gpu-sparse-random/preflight.md
docs/development/asc-cpp-m7-gpu-sparse-random/dependency-audit.md
docs/development/asc-cpp-m7-gpu-sparse-random/provenance-record.md
docs/development/asc-cpp-m7-gpu-sparse-random/publication-checkpoint-b.md
```

Responsibilities: actual ASCCMake and standard-CMake use, target definitions,
conditional exports, package closures, test registration, architecture
manifests, integration, complete diff review, clean CPU/CUDA validation,
evidence classification, and checkpoint reporting. The lead alone edits any
root, package, or shared CMake file.

## Production implementation agent

Exclusive write scope:

```text
include/asc/sparse/**
include/asc/random/providers/**
src/sparse/cuda/**
src/random/cuda/**
docs/development/asc-cpp-m7-gpu-sparse-random/production-self-review.md
```

The production agent implements only the frozen sparse CUDA context/staging/
bounded evaluator/CSR SpMV and the raw/dense/sparse random CUDA facets. It may
make the minimum provider-enabling evolution inside existing sparse public
owners, but may not edit base random algorithms, core, dense, expression,
CMake, tests, package files, architecture manifests, general docs, or later
milestone paths. Any needed out-of-scope change is reported to the lead.

## Independent verification agent

Exclusive write scope:

```text
tests/sparse_cuda/** except tests/sparse_cuda/CMakeLists.txt
tests/random_cuda/** except tests/random_cuda/CMakeLists.txt
tests/random_dense_cuda/** except tests/random_dense_cuda/CMakeLists.txt
tests/random_sparse_cuda/** except tests/random_sparse_cuda/CMakeLists.txt
tests/compile/m7_*.cc
tests/compile/m7_*.h
tests/consumer/sparse_cuda/** except tests/consumer/sparse_cuda/CMakeLists.txt
tests/consumer/random_cuda/** except tests/consumer/random_cuda/CMakeLists.txt
tests/consumer/random_dense_cuda/** except tests/consumer/random_dense_cuda/CMakeLists.txt
tests/consumer/random_sparse_cuda/** except tests/consumer/random_sparse_cuda/CMakeLists.txt
benchmarks/sparse_cuda/** except benchmarks/sparse_cuda/CMakeLists.txt
benchmarks/random_cuda/** except benchmarks/random_cuda/CMakeLists.txt
docs/development/asc-cpp-m7-gpu-sparse-random/verification-design.md
docs/development/asc-cpp-m7-gpu-sparse-random/verification-review.md
```

The verifier freezes independent structural, numerical, bit, lifetime,
failure, package-consumer, and benchmark oracles before inspecting production.
It does not inspect or copy MdeCpp/deleted asc-cpp implementation or tests and
does not modify production, CMake, package, architecture, or general docs.

## Documentation and API review agent

Exclusive write scope:

```text
docs/modules/sparse.md
docs/modules/random.md
docs/development/asc-cpp-m7-gpu-sparse-random/documentation-api-review.md
```

The reviewer documents and audits the M7 public API, exact component use,
ownership/lifetime, device storage, supported operations, failures, costs,
reproducibility, evidence labels, and installed-consumer examples. API defects
are reported to the lead and production owner rather than documented around.

## Portability/GPU/performance review agent

Exclusive write scope:

```text
docs/development/asc-cpp-m7-gpu-sparse-random/portability-review.md
```

This independent review-only role audits C++20/CUDA portability, host compiler
compatibility, checked narrowing, device address safety, CUDA error
translation, cuSPARSE index/algorithm/workspace behavior, asynchronous
lifetimes, static/shared packaging, CPU-only isolation, sanitizers,
determinism, benchmark methodology, and exact evidence labels. Findings are
reported; production or integration changes remain with their owners.

## Conflict rule

If work outside an assigned scope is required, the role reports the exact
file/symbol and reason. Only the lead may reassign ownership or perform a
shared integration change.
