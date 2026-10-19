# ASCCpp 1.0.0 release notes

ASCCpp 1.0.0 retains the six-module C++20 foundation: Core, Utilities,
Expression, Dense, Sparse, and Random. It includes deterministic
random generation, storage adapters, QMC/Sobol, and correctness-reference
Dense and Sparse BLAS implementations.

## Changes since 0.9.0

- Fixed GCC 15 warnings-as-errors compatibility for intentional CUDA context
  self-move testing and fixed-rank Sparse shape copying.
- Fixed CUDA BLAS Level 1 test readbacks of unwritten scalar-buffer and
  scratch-workspace regions; all numerical assertions remain enabled.
- Updated package consumers, version checks, documentation contracts,
  generated reports, and release automation together for version 1.0.0.
- Advanced shared-library VERSION to 1.0.0 and SONAME to 1.0. Rebuild all
  applications and shared-library boundaries when upgrading from 0.9.x.

Public signatures, six-module layering, numerical algorithms, Random sequence
versions, and the explicit CPU/CUDA boundary are unchanged.

## Compatibility and support

CMake package compatibility remains SameMinorVersion: 1.0.x patches preserve
this release's documented public contract; another minor or major requires
explicit downstream review. This release makes no cross-toolchain ABI promise.
CPU Dense and Sparse BLAS remain correctness references without performance
claims. CUDA remains experimental, including with CUDA 13.3 and GCC 15.

Hosted release validation and independently consumed artifacts are bound to
the exact version tag. Local migration evidence does not substitute for the
hosted release matrix. See the [support matrix](../docs/support-matrix.md),
[compatibility policy](../docs/api-compatibility.md), and
[known limitations](known-limitations-v1.0.0.md).
