# ASCCpp 0.9.0 release notes

ASCCpp 0.9.0 is the first release of the portable C++20 scientific-computing
foundation. It provides six explicitly layered modules: Core, Utilities,
Expression, Dense, Sparse, and Random.

## Highlights

- Provider-free CPU arrays, views, expressions, sparse formats, deterministic
  random generation, storage adapters, and QMC/Sobol support.
- Correctness-reference Dense BLAS Levels 1--3 and Sparse BLAS operations.
- Isolated CMake package components with build-tree, installed, and relocated
  consumers.
- Self-contained public headers, negative compile contracts, provenance drift
  checks, strict API documentation, and compiled installed-package examples.
- Experimental CUDA provider components with explicit context, memory,
  completion, and package boundaries.

## Migration

The former `linalg` headers and names were replaced by the `blas` API. See
[`docs/migration/linalg-to-blas.md`](../docs/migration/linalg-to-blas.md).

## Support boundary

CPU BLAS implementations prioritize defined numerical behavior and testing;
they are not optimized vendor-provider replacements. CUDA is experimental for
this release unless the complete real-GPU release gate is later attached to
the exact release commit. See the
[`support matrix`](../docs/support-matrix.md) and
[`known limitations`](known-limitations-v0.9.0.md).
