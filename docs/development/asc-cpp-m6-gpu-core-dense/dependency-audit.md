# Milestone 6 Dependency Audit

Status: Frozen pre-implementation contract

Date: 2026-07-26

## Approved additions

```text
asc_core_cuda / ASC::core_cuda
  direct ASC dependency: ASC::core
  private provider dependency: CUDA::cudart

asc_dense_cuda / ASC::dense_cuda
  direct ASC dependencies: ASC::dense;ASC::core_cuda
  private provider dependency: CUDA::cublas
```

No other direct dependency changes are approved.

## Required isolation

- `ASC::core` and all common core headers remain CUDA-SDK-free.
- `ASC::dense` and all common dense headers remain CUDA-SDK-free.
- `ASC::cpp` contains no provider target.
- CPU-only configuration performs no `CUDAToolkit` discovery.
- `core_cuda` includes/links no dense, expression, sparse, utilities, or
  random target.
- `dense_cuda` includes/links no utilities, sparse, random, cuSPARSE, cuRAND,
  or cuSOLVER target.
- Provider SDK discovery in the installed config occurs only for a requested
  `core_cuda` or `dense_cuda` closure.
- No provider target exists when `ASC_CPP_ENABLE_CUDA=OFF`.

## Exact package closures

```text
core_cuda:
  core
  core_cuda

dense_cuda:
  core
  expression
  dense
  core_cuda
  dense_cuda
```

All predecessor component closures remain unchanged.

## Mechanical evidence

The lead will require:

- build-target `LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES` inspection;
- forbidden include/provider token scans;
- provider-neutral header compilation without CUDA include paths;
- provider headers alone with their narrow imported targets;
- installed imported-target graph inspection;
- CPU-only package consumers against a CUDA-enabled and CUDA-disabled build;
- provider-only build-tree/install/relocation consumers; and
- requested-provider failure without an available toolkit/compiler.
