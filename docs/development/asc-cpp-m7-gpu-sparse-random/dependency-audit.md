# Milestone 7 Dependency Audit

Status: Frozen target graph; implementation evidence pending

Date: 2026-07-27

## Approved direct graph

```text
ASC::sparse_cuda
  -> ASC::sparse
  -> ASC::core_cuda
  -> private CUDA::cusparse

ASC::random_cuda
  -> ASC::random
  -> ASC::core_cuda

ASC::random_dense_cuda
  -> ASC::random_dense
  -> ASC::random_cuda
  -> ASC::core_cuda

ASC::random_sparse_cuda
  -> ASC::random_sparse
  -> ASC::random_cuda
  -> ASC::core_cuda
```

`ASC::core_cuda` retains its private CUDA Runtime dependency. The random
project kernels use that established runtime closure; they do not add cuRAND.
No M7 facet links `ASC::cpp`, `ASC::dense_cuda`, a dense/sparse sibling, or a
provider outside its approved owner.

## Build-policy APIs

The implementation uses released ASCCMake 0.1.0 APIs:

```text
asc_target_enable_cxx20
asc_target_enable_warnings
asc_target_enable_sanitizers
asc_register_test
```

Standard CMake continues to own CUDA language enablement, CUDAToolkit imported
targets, per-component exports, package configuration, version files, and
conditional install rules because ASCCMake 0.1.0 has no conditional component
export abstraction.

## External dependency decision

The only new target referenced by M7 is the already architecture-approved
standard CMake imported target `CUDA::cusparse`. CUDA Runtime was approved and
implemented in M6. No source, binary, table, header-only algorithm package, or
runtime library is vendored.

Provider-free configure, build-tree lookup, installed lookup, and no-component
lookup must not discover CUDAToolkit. A requested `sparse_cuda` closure must
discover `CUDA::cudart` and `CUDA::cusparse`; the three random closures must
discover `CUDA::cudart` but not import sparse or dense CUDA facets.

## Verification gate

Final status requires target-property inspection, dependency-manifest checks,
public include audits, build/install/relocation consumers for each exact
component, static/shared validation, and a CUDA-disabled package-isolation
test.
