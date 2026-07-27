# Milestone 8 Dependency Audit

Status: Complete; clean local matrix confirmed

Date: 2026-07-27

## Product graph

Milestone 8 adds no product target or direct dependency. The mechanically
checked graph remains:

| Target | Exact direct ASC dependencies | Direct external implementation dependency |
| --- | --- | --- |
| `ASC::core` | none | none |
| `ASC::utilities` | `ASC::core` | none |
| `ASC::expression` | `ASC::core` | none |
| `ASC::dense` | `ASC::core`, `ASC::expression` | none |
| `ASC::sparse` | `ASC::core`, `ASC::expression` | none |
| `ASC::random` | `ASC::core` | none |
| `ASC::random_dense` | `ASC::random`, `ASC::dense` | none |
| `ASC::random_sparse` | `ASC::random`, `ASC::sparse` | none |
| `ASC::cpp` | all six modules and both random facets | none |
| `ASC::core_cuda` | `ASC::core` | private `CUDA::cudart` |
| `ASC::dense_cuda` | `ASC::dense`, `ASC::core_cuda` | private `CUDA::cublas` |
| `ASC::sparse_cuda` | `ASC::sparse`, `ASC::core_cuda` | private `CUDA::cusparse` |
| `ASC::random_cuda` | `ASC::random`, `ASC::core_cuda` | none added |
| `ASC::random_dense_cuda` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | none added |
| `ASC::random_sparse_cuda` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | none added |

The configured-target inventory checks these exact local and public link
interfaces. The component baseline independently records the matching package
closures. `ASC::cpp` remains provider-free, and a provider-free package
request neither finds CUDAToolkit nor creates a CUDA target.

## Build, test, and tooling dependencies

- CMake 3.25 or newer remains the build/package/test driver.
- Released ASCCMake 0.1.0 at
  `8a7dcbad3a97267cce59810aff24de800a3497a7` remains an exact required build
  dependency; no API is copied or imitated.
- The C++20 standard library remains the only provider-free product
  dependency.
- CUDA 12 or newer, CUDA Runtime, cuBLAS, and cuSPARSE remain conditional and
  are discovered only for CUDA-enabled production or requested installed
  provider closures.
- The ELF observation tool uses locally available `readelf`, `nm`, and
  `c++filt` as review tools only. They are not product, package, or consumer
  dependencies; unsupported object formats are reported as `skipped`.
- The independent compile/object probes invoke installed GCC and Clang
  compilers directly. They add no exported flags or dependency.
- The asc-xde-shaped test uses only CMake, Git read-only inspection, and
  installed/public ASCCpp APIs. It does not make asc-xde a build dependency.

## Disposition

No unapproved package, fetched content, compatibility layer, provider SDK,
runtime, test framework, or later-milestone dependency was added. Package
metadata remains relocatable and contains no required source-tree or
asc-cmake path.
