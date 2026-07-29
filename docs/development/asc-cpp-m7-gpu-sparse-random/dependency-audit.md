# Milestone 7 Dependency Audit

Status: accepted at Publication Checkpoint B

Date: 2026-07-28

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

`ASC::core_cuda` retains its private CUDA Runtime dependency. Random project
kernels use that established closure and do not add cuRAND. No M7 facet links
`ASC::cpp`, `ASC::dense_cuda`, a dense/sparse sibling, or an unapproved
provider.

## Build policy

Only released ASCCMake 0.1.0 APIs are allowed:

```text
asc_target_enable_cxx20
asc_target_enable_warnings
asc_target_enable_sanitizers
asc_register_test
```

Standard CMake continues to own CUDA language enablement, CUDAToolkit imported
targets, per-component exports, package configuration/version files, and
conditional installation because ASCCMake 0.1.0 has no conditional component
export abstraction.

## External dependency decision

The only new external target is the architecture-approved standard CMake
target `CUDA::cusparse`. CUDA Runtime was approved in M6. No source, binary,
table, header-only algorithm package, or runtime library is vendored.

Provider-free configure/build/install/no-component lookup must not discover
CUDAToolkit. A requested `sparse_cuda` closure discovers CUDA Runtime and
cuSPARSE; random CUDA closures discover CUDA Runtime without importing Sparse
or Dense CUDA.

Final configured, build, installed-target, manifest, include, static/shared,
package-relocation, isolated-consumer, CUDA-disabled, and
CUDA-requested-unavailable audits pass. The independent static export
inspection and clean shared 217-test aggregate confirm the exact graph above.
