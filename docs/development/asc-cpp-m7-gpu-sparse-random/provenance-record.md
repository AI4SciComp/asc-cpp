# Milestone 7 Provenance Record

Status: clean-room boundary accepted at Publication Checkpoint B

Date: 2026-07-28

Milestone 7 production kernels, provider adapters, tests, expected values, and
benchmarks are original clean-room work derived from the frozen asc-cpp
contracts, accepted ADRs, NVIDIA's installed toolkit API documentation, and
the approved Philox4x32-10 primary-paper contract.

No MdeCpp or deleted asc-cpp source, test, literal vector, table, generated
data, or prose is copied or mechanically translated. Verification freezes its
structural, numerical, and raw-bit oracles independently before production
inspection.

No third-party source/data is imported. The build links the system CUDA
Runtime and cuSPARSE through standard CMake CUDAToolkit targets. CUDA binaries,
cuRAND code/state, Sobol data, and provider tables are not redistributed.
`THIRD_PARTY_NOTICES` remains deleted and Apache-2.0 remains unchanged.

Required identity:

- raw GPU words equal
  `Philox4x32Word(stream, subsequence, offset)` exactly;
- float/double Uniform01 matches the frozen bit transforms and word
  consumption;
- sparse priorities, ties, stream domains, canonical order, values, and
  offsets match the provider-free M5 contract;
- cuSPARSE executes only bounded CSR SpMV and does not define ASC ownership,
  structure, index, error, or reproducibility semantics.

The lead and independent reviewers scanned the complete M7 scope and installed
artifacts for copied notices/tables/vectors, SDK leakage into common headers,
untracked generated sources, and dependencies outside the frozen graph. No
such material was found. Strict public-header isolation, source/dependency
search, installed-target inspection, and the architecture suite pass.
