# Repository instructions

These instructions apply to the entire `asc-cpp` repository.

- Preserve C++20, the six-module architecture, public API behavior, numerical
  semantics, component isolation, and the explicit CPU/CUDA boundary.
- Follow the repository `.clang-format` and `.clang-tidy` files and the Google
  C++ Style Guide. Public headers must be self-contained and include what they
  use.
- Treat `docs/contracts/`, `docs/architecture/decisions/`, `abi/`, licenses,
  and provenance records as release contracts. Update their generators and
  consumers atomically.
- Document every public declaration with useful Doxygen semantics. Do not
  expose private implementation details as supported API.
- Never weaken or skip a failing test. Unexpected CTest skips are failures.
- Build and test out of the source tree. Keep raw logs and generated release
  artifacts outside the source tree.
- Obtain `ASCCMake 0.1.0` only from the exact commit and checksum recorded in
  `docs/installation.md`; do not invent helper interfaces.
- CUDA is experimental for `v0.9.0` unless the complete real-NVIDIA release
  gate passes on the exact release commit.
- Do not make performance claims for the CPU Dense or Sparse BLAS reference
  implementations.
- Release preparation occurs only on `release/0.9.0`; do not work directly on
  `main`.
