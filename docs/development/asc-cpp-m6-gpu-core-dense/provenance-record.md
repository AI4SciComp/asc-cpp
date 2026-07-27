# Milestone 6 Provenance Record

Status: Frozen clean-room boundary

Date: 2026-07-26

## Project implementation

All M6 ASC source, headers, tests, examples, and benchmark logic are
project-owned work derived from:

- the approved asc-cpp architecture and ADRs;
- the frozen Milestone 6 contract;
- current public core/dense contracts; and
- official CMake, CUDA Runtime, and cuBLAS documentation.

No third-party source, test, vector, benchmark harness, generated code, or
data is imported.

## Prohibited source use

The team does not inspect, copy, mechanically translate, or differentially
port:

- MdeCpp CUDA allocation/copy/device wrappers;
- MdeCpp dense/provider implementation or GPU tests;
- the user-deleted asc-cpp CUDA/core/dense implementation or tests; or
- third-party sample source.

MdeCpp remains behavior/provenance catalogue evidence at the pinned commit
`f6294e9079262682ce63ae7ff2d8a643e658bf5d`.

## External provider

CUDA Runtime and cuBLAS are optional system-supplied provider libraries.
ASCCpp includes no NVIDIA source or headers in provider-neutral public files
and redistributes no CUDA binary or sample. Installed provider targets require
the consumer's separately supplied CUDAToolkit.

The exact locally inspected provider is CUDA 12.9.86 with driver 576.83 on an
RTX 3060 Laptop GPU. Provider discovery does not itself approve redistribution
or create a broader platform/support claim.

## License

Original asc-cpp work remains Apache-2.0. No new third-party notice is added
unless the final diff contains an accepted imported artifact requiring one.
The final source/install/archive audit must verify this clean-room boundary.
