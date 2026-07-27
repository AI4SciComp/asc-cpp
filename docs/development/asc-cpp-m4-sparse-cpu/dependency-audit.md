# Milestone 4 Dependency Audit

Status: Complete; accepted

Date: 2026-07-26

## Approved graph

```text
asc_core        / ASC::core        -> []
asc_utilities   / ASC::utilities   -> [ASC::core]
asc_expression  / ASC::expression  -> [ASC::core]
asc_random      / ASC::random      -> [ASC::core]
asc_dense       / ASC::dense       -> [ASC::core, ASC::expression]
asc_sparse      / ASC::sparse      -> [ASC::core, ASC::expression]
```

## Forbidden Milestone 4 edges

Sparse production must not include or link:

```text
ASC::utilities
ASC::dense
ASC::random
ASC::random_dense
ASC::random_sparse
ASC::cpp
any retired array/linalg target
any provider target or SDK
```

The expression writable protocol remains storage-neutral. The dense
specialization must not include sparse. Every predecessor target retains its
approved ceiling and cannot acquire a sparse edge.

## Completed evidence

- Architecture inventory tests require exactly the approved sparse umbrella,
  six topic headers, one compiled source, and no unapproved production file.
- Source scanning covers all sparse production and the expression compatibility
  bridge. It rejects utilities, dense, random, retired array/linalg, aggregate,
  provider, CUDA, cuSPARSE, BLAS/LAPACK, Eigen, MKL, OpenMP, TBB, HIP, and SYCL
  includes or dispatch.
- CMake target-property checks require both the build and interface link lists
  of `asc_sparse` to equal `ASC::core;ASC::expression`.
- Every new or modified public header compiles alone with GCC 11 and Clang 19,
  with warnings as errors and with exceptions both enabled and disabled.
- Static GCC and shared Clang clean builds each passed 138/138 tests, including
  the sparse build-tree, installed/relocated, path-with-spaces, and subproject
  consumers plus component and package-registry checks.
- The isolated sparse consumer imports exactly core, expression, and sparse,
  rejects all sibling/facet/provider targets, and runs CSR SpMV using a
  project-external vector adapter.
- Package configuration exports `ASCCppSparseTargets.cmake` only when sparse is
  in the requested component closure. No-provider configuration performs no
  optional SDK discovery.
- Sparse evaluation visits only destination stored coordinates. Allocation
  instrumentation and structure/value oracles prove no dense allocation,
  hidden densification, workspace, packing, conversion, transfer,
  synchronization, provider dispatch, or fallback on successful evaluation or
  SpMV.

No forbidden edge or unapproved dependency remains.
