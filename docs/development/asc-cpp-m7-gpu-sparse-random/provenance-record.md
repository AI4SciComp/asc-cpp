# Milestone 7 Provenance Record

Status: Clean-room boundary frozen; final diff scan pending

Date: 2026-07-27

## Production and test origin

Milestone 7 production kernels, provider adapters, tests, expected values, and
benchmarks are original clean-room work derived from the frozen asc-cpp API
contracts, the accepted ADRs, NVIDIA's installed toolkit API documentation,
and the already approved Philox4x32-10 primary-paper contract.

No MdeCpp or deleted asc-cpp production source, tests, literal vectors, tables,
or generated data are copied or mechanically translated. The independent
verification role froze its structural, numerical, and raw-bit oracle design
before inspecting M7 production.

## External code and data

No third-party source or data is imported into the repository. The build links
the locally supplied CUDA Runtime and cuSPARSE through standard CMake
CUDAToolkit imported targets. The project does not redistribute CUDA binaries,
cuRAND state/code, Sobol direction data, or provider-generated tables.

Consequently M7 does not create or restore `THIRD_PARTY_NOTICES`. The existing
Apache-2.0 project license remains unchanged. Provider discovery is not a
redistribution or legal-compatibility claim.

## Algorithm identity

- Raw GPU words must equal the frozen clean-room
  `Philox4x32Word(stream, subsequence, offset)` contract.
- Float/double GPU Uniform01 values must equal the frozen project transforms,
  including exact word consumption.
- GPU sparse candidate priority, tie breaking, independent structure/value
  address domains, canonical order, and offset advancement must equal the
  provider-free M5 contract.
- cuSPARSE supplies execution of the bounded CSR SpMV operation but does not
  define asc-cpp structure, index, error, ownership, or reproducibility
  contracts.

## Final gate

Before Publication Checkpoint B, scan the complete M7 diff and installed
artifacts for copied notices, suspicious tables/vectors, CUDA SDK leakage into
common headers, untracked generated sources, and dependencies outside the
frozen graph. Record any finding and resolution in the checkpoint report.
