# Linalg-to-BLAS migration

Issue 6 applies the approved breaking pre-1.0 owner-scoped rename. It changes
names only; it does not add BLAS operations, backends, targets, components, or
dependencies.

## Mechanical changes

| Before | After |
| --- | --- |
| `<asc/dense/linalg.h>` | `<asc/dense/blas.h>` |
| `<asc/sparse/linalg.h>` | `<asc/sparse/blas.h>` |
| `asc::DenseTranspose` | `asc::DenseBlasTranspose` |
| `src/dense/linalg.cc` | `src/dense/blas.cc` |
| `src/sparse/reference_linalg.cc` | `src/sparse/reference_blas.cc` |

Operation names remain in the flat `asc` namespace. Calls to `asc::Copy`,
`asc::Scal`, `asc::Axpy`, `asc::Dot`, `asc::Nrm2`, `asc::Gemv`, `asc::Gemm`,
and `asc::Spmv` do not change.

Consumer targets also remain unchanged:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense sparse)
target_link_libraries(my_target PRIVATE ASC::dense ASC::sparse)
```

The old owner-scoped headers and `DenseTranspose` name are absent. There are no
forwarding headers, aliases, deprecated targets or components, duplicate
implementations, or compatibility window. Update includes and the transpose
type in the same downstream change.

The CUDA provider targets remain `ASC::dense_cuda` and `ASC::sparse_cuda`.
Provider selection stays explicit; the rename adds no transfer,
synchronization, allocation, backend dispatch, or fallback behavior.
