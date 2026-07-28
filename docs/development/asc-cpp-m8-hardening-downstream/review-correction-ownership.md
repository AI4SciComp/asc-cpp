# Milestone 8 Post-Checkpoint Review Correction Ownership

Status: Frozen

Date: 2026-07-28

All write scopes are disjoint. Unrelated work and the cumulative approved
Milestones 0--8 candidate must be preserved. No role may commit, push, merge,
tag, release, mutate a pull request, delete a branch, modify another
repository, add a dependency, or implement later work.

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
tests/compile/CMakeLists.txt
tests/compile/m2_dependency_check.cmake
tests/consumer/CMakeLists.txt
tests/package/**
tests/cmake/**
tests/consumer/*.cmake
tests/downstream/*.cmake
tests/hardening/*.cmake
benchmarks/dense/dense_benchmark.cc
benchmarks/dense_cuda/benchmark.cc
benchmarks/random_storage/random_storage_benchmark.cc
benchmarks/sparse/sparse_benchmark.cc
abi/public-headers.sha256
abi/linux-x86_64-clang19-cpu-shared.txt
abi/linux-x86_64-gcc11-cpu-shared.txt
abi/linux-x86_64-gcc11-cuda12-shared.txt
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/modules/**
docs/development/asc-cpp-architecture/**
docs/development/asc-cpp-m8-hardening-downstream/{review-correction-contract,review-correction-ownership,review-correction-integration,publication-checkpoint-b}.md
```

Responsibilities: package optional-component behavior, safe test workspace
integration, downstream opt-in design, exact ABI baseline selection,
test/fixture registration, no-device skip registration, manifests, clean
integration, and final checkpoint evidence.

Final-review amendment: the lead also owns the CPU benchmark sources named
above and any new `tests/cmake`/`tests/hardening` scripts needed to correct
recursive cleanup, actual CUDA-host selector integration, exact exit
classification, and operation-specific benchmark oracles. This amendment was
recorded before those writes and does not overlap another role's scope.

Compatibility re-review amendment: the lead also owns the CMake-version gate
and older-CMake non-enforcing CUDA-host selector regression required by
finding 19. This amendment was recorded before those writes.

Default-host re-review amendment: the lead also owns the optional-host test
driver correction and default-NVCC-host regression required by finding 20.
This amendment was recorded before those writes.

## Production correction agent

Exclusive write scope:

```text
src/core/cuda/runtime.cc
src/core/cuda/runtime_test_internal.h
src/dense/cuda/operations.cc
src/random/cuda/sparse_kernels.cu
src/random/cuda/sparse_kernels_internal.h
include/asc/sparse/coordinate.h
include/asc/sparse/compressed.h
include/asc/sparse/linalg.h
include/asc/dense/evaluate.h
include/asc/utilities/timer.h
src/utilities/timer.cc
src/utilities/timer_internal.h
```

Responsibilities: post-enqueue CUDA failure draining, complete sparse readable
alias metadata, checked integral reduction, checked Timer arithmetic, and one
shared host/device sparse-random priority/ordinal comparator for deterministic
collision verification.
Report any required file outside this scope to the lead before editing it.

## Independent verification agent

Exclusive write scope:

```text
tests/core_cuda/*.cc
tests/dense_cuda/*.cc
tests/dense/*.cc
tests/sparse/*.cc
tests/utilities/*.cc
tests/random/*.cc
tests/random_cuda/*.cc
tests/random_dense/*.cc
tests/random_dense_cuda/*.cc
tests/random_sparse/*.cc
tests/random_sparse_cuda/*.cc
tests/random_sparse_cuda/*.cu
tests/**/test_support.h
docs/development/asc-cpp-m8-hardening-downstream/review-correction-verification.md
```

Responsibilities: independently add focused failure, alias, numerical,
partition, collision, Timer, and no-device regressions without changing
production or CMake/package integration. Tests must use public behavior where
possible and explicitly identify any required fault-injection seam.

## Documentation and API review agent

Exclusive write scope:

```text
SECURITY.md
docs/architecture.md
docs/build-system.md
docs/design/**
docs/migration/**
docs/optional-backends.md
docs/testing.md
docs/modules/{array,linalg}.md
docs/support-matrix.md
docs/api-compatibility.md
docs/package-capabilities.md
docs/extension-guide.md
docs/downstream-integration.md
docs/performance.md
docs/development/asc-cpp-m8-hardening-downstream/review-correction-documentation.md
```

Responsibilities: correct the security surface and all retained-history
banners, then independently audit public/package/downstream claims. Product,
package, architecture, and test defects are reported to the lead.

## Final portability/GPU/performance reviewer

Read-only over the complete candidate, except:

```text
docs/development/asc-cpp-m8-hardening-downstream/review-correction-portability.md
```

This role runs only after integration and independently audits portability,
GPU isolation/lifetime/evidence, performance methodology, security,
provenance, package relocation, and exact skips.

## Conflict rule

If a role needs an out-of-scope edit, it reports the exact file and reason.
Only the lead may revise this ledger, and the revision is recorded before the
write.
