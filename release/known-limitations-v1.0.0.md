# ASCCpp 1.0.0 known limitations

- CPU Dense and Sparse BLAS are serial correctness-reference implementations;
  no optimized-provider performance claim is made.
- CUDA components remain experimental. Local NVIDIA results do not establish
  supported CUDA status without the complete trusted exact-commit GPU gate.
- Shared-library SONAME is 1.0. Upgrading from 0.9.x requires a complete rebuild.
  No cross-toolchain or cross-minor binary compatibility promise is made.
- CMake package matching remains SameMinorVersion, with 1.0.x patch contracts.
- Numerical, Random-bit, provider, serialization, source, and binary
  compatibility remain separate documented dimensions.
- Platform and artifact claims require successful exact-tag workflow evidence.
