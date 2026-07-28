# Milestone 8 Dependency Audit

Status: complete; accepted at Publication Checkpoint B

Date: 2026-07-28

## Product graph

Milestone 8 adds no product target or direct product dependency. The exact
consumer surface remains:

| Target | Direct ASC dependencies | Direct external implementation edge |
| --- | --- | --- |
| `ASC::core` | none | none |
| `ASC::utilities` | `ASC::core` | none |
| `ASC::expression` | `ASC::core` | none |
| `ASC::dense` | `ASC::core`, `ASC::expression` | none |
| `ASC::sparse` | `ASC::core`, `ASC::expression` | none |
| `ASC::random` | `ASC::core` | none |
| `ASC::random_dense` | `ASC::random`, `ASC::dense` | none |
| `ASC::random_sparse` | `ASC::random`, `ASC::sparse` | none |
| `ASC::cpp` | all provider-free targets | none |
| `ASC::core_cuda` | `ASC::core` | private `CUDA::cudart` |
| `ASC::dense_cuda` | `ASC::dense`, `ASC::core_cuda` | private `CUDA::cublas` |
| `ASC::sparse_cuda` | `ASC::sparse`, `ASC::core_cuda` | private `CUDA::cusparse` |
| `ASC::random_cuda` | `ASC::random`, `ASC::core_cuda` | none beyond Core CUDA closure |
| `ASC::random_dense_cuda` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | none beyond Core CUDA closure |
| `ASC::random_sparse_cuda` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | none beyond Core CUDA closure |

Provider-free lookup remains free of CUDA language, package discovery, include
directories, imported targets, and link edges. `ASC::cpp` remains
provider-free.

## Build dependency

The build continues to require exact released ASCCMake 0.1.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7` and calls only its verified public
helpers. Standard CMake continues to implement the approved multi-component
package behavior absent from that ASCCMake release. No helper is imitated or
added to asc-cmake.

## M8 development-only edges

Hardening scripts, fixtures, and benchmarks are repository development
artifacts. They are neither installed nor linked by a product or downstream
consumer. Local symbol inspection may invoke host tools such as `nm`,
`c++filt`, or platform equivalents when available; those tools are validation
inputs, not build, product, package, or installed dependencies. An unavailable
inspection tool produces a truthful skip.

The asc-xde-shaped trial requests only `ASC::dense`, whose exact closure is
`ASC::core`, `ASC::expression`, and `ASC::dense`. It disables CUDAToolkit
lookup, uses only public installed headers, and neither imports nor modifies
the real asc-xde repository.

## Prohibited-edge checks

Terminal validation must prove:

- exact source and installed target inventories;
- exact public header ownership and installed-header equivalence;
- every isolated component closure and repeated component lookup;
- no provider SDK discovery from a provider-free request;
- no source/build absolute path in relocated installed metadata;
- no M8 product file or later-milestone target;
- no OpenMP, Eigen, BLAS/LAPACK, oneMKL, TBB, SYCL, cuSOLVER, cuRAND,
  Thrust/CUB, CUDA Driver API, MdeCpp, or deleted compatibility edge.

Terminal target, header, package, relocation, provider-isolation, and
installed-metadata checks passed. Milestone 8 adds no product or installed
dependency. Exact commands and results are recorded at Publication
Checkpoint B.
