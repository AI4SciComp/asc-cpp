# Milestone 6 Dependency Audit

Status: Final; approved dependency graph verified

Date: 2026-07-28

## Approved additions

```text
asc_core_cuda / ASC::core_cuda
  direct ASC dependency: ASC::core
  private provider dependency: CUDA::cudart

asc_dense_cuda / ASC::dense_cuda
  direct ASC dependencies: ASC::dense;ASC::core_cuda
  private provider dependency: CUDA::cublas
```

No other product dependency change is approved.

## Required isolation

- Common Core/Dense headers and base targets remain CUDA-SDK-free.
- `ASC::cpp` retains no provider target.
- CUDA-disabled configuration performs no CUDAToolkit discovery.
- `core_cuda` imports no Dense, Expression, Sparse, Utilities, or Random.
- `dense_cuda` imports no Utilities, Sparse, Random, cuSOLVER, cuSPARSE, or
  cuRAND.
- Installed provider discovery occurs only for a requested CUDA closure.
- No provider target exists when `ASC_CPP_ENABLE_CUDA=OFF`.

Exact package closures:

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

## Final audit

The configured, built, exported, installed, and relocated graphs match the
approved additions:

```text
ASC::core_cuda
  public ASC edge: ASC::core
  private provider edge: CUDA::cudart

ASC::dense_cuda
  public ASC edges: ASC::dense;ASC::core_cuda
  private provider edge: CUDA::cublas
```

`readelf` on the clean Release/shared artifacts confirms that Core CUDA needs
the Core library and shared CUDA Runtime, while Dense CUDA needs Dense, Core
CUDA, Core, cuBLAS, and the shared CUDA Runtime. `libcublasLt` is a transitive
implementation dependency of the system cuBLAS library, not a direct ASC
target edge. No toolkit binary is bundled.

Provider-neutral public headers parse without CUDA include paths. CUDA-disabled
configuration does not enable the CUDA language, discover CUDAToolkit, or
produce provider targets. Provider-free consumers set
`CMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE` and pass from both the build tree
and a relocated CUDA-enabled package.

The clean Release/shared package matrix passed all component build-tree and
install/relocation checks. Exact `core_cuda` and `dense_cuda` isolated
consumers configured, linked, and ran from build-tree and relocated packages.
CUDA-disabled isolation and a deliberately requested unavailable CUDA
component produced the required failures without mutating package registry
state.

The architecture dependency manifest, approved-target inventory, public-file
policy, and predecessor layer audits pass. No Utilities, Sparse, Random,
cuSOLVER, cuSPARSE, cuRAND, HIP, or later-milestone edge was introduced.
