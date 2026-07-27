# Backend capability matrix

Status: Milestone 8 hardens the unchanged Milestone 7 provider and capability
surface; local Publication Checkpoint B evidence accepted

Evidence date: 2026-07-27

Evidence labels are independent: discovery, configure, compiler smoke,
asc-provider compile, real-hardware runtime, and parity. “Detected” is not
support.

| Provider | Owner/facet | Proposed operations | Scalars | Indices | Layouts/formats | Platforms | Configure evidence | Compile evidence | Runtime evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| serial reference | core base | host allocation/copy, synchronous context/event | raw bytes / typed owners above | checked 64-bit logical | host | portable C++20 | CMake 3.25.0 and 4.1.2, static and shared | GCC 11.4 and Clang 19.0, C++20, warnings-as-errors; GCC whole-library `-fno-exceptions` | local core runtime, contract, package, relocation, and isolated-consumer tests passed |
| serial reference | dense base | host owner/view, pointwise evaluation, sum/min/max, Copy/Scal/Axpy/Dot/Nrm2/Gemv/Gemm | arithmetic storage/evaluation; float/double linalg | signed 64-bit ASC metadata; compile-time rank | left/right owners; unique non-negative-stride views | portable C++20 CPU | CMake 3.25.0 and 4.1.2, static and shared | GCC 11.4 and Clang 19.0, C++20, warnings-as-errors; exceptions-disabled public headers | local mapping/owner/evaluation/numerical/allocation tests, package relocation, subproject, and isolated dense consumers passed |
| serial reference | sparse base | coordinate/CSR/CSC validation and six named conversions, structure-preserving evaluation, CSR SpMV | arithmetic storage/evaluation; float/double SpMV | signed 64-bit ASC metadata | canonical coordinate, CSR, CSC | portable C++20 CPU | CMake 3.25.0 and 4.1.2, static and shared | GCC 11.4 and Clang 19.0, C++20, warnings-as-errors; exceptions-disabled public headers | local canonical-format/conversion/evaluation/numerical/alias/allocation tests, package relocation, subproject, and isolated sparse consumers passed |
| ASC Philox | random base | raw bits, Uniform01 transforms | uint32/uint64, float, double | 128-bit conceptual counter from fixed words | storage-free | portable CPU | CMake 3.25.0 and 4.1.2, static and shared | GCC 11.4 and Clang 19.0, C++20, warnings-as-errors and exceptions-disabled headers | independent fixed vectors, mapping/overflow cases, exact transforms, package relocation, and isolated-consumer tests passed |
| serial ASC Philox facet | `random_dense` | explicit-state logical dense Uniform01 fill | float, double | signed 64-bit ASC shape; checked uint64 word offsets | mutable host dense view; left/right/unique non-negative stride | portable C++20 CPU | CMake 3.25.0 and 4.1.2, static and shared | GCC 11.4 and Clang 19.0, C++20, warnings-as-errors and exceptions-disabled header | local layout/padding/partition/determinism/overflow/allocation tests, package relocation, and isolated facet consumer passed |
| serial ASC Philox facet | `random_sparse` | explicit-state deterministic exact-count coordinate generation | float, double | signed 64-bit ASC shape/count; checked uint64 word offsets | canonical coordinate owner; static/dynamic extents | portable C++20 CPU | CMake 3.25.0 and 4.1.2, static and shared | GCC 11.4 and Clang 19.0, C++20, warnings-as-errors and exceptions-disabled header | local independent-priority/canonical/stream-separation/overflow/rollback/allocation tests, package relocation, and isolated facet consumer passed |
| GCC OpenMP | deferred core/dense/sparse CPU facet | parallel reference candidates | not approved | not approved | not approved | Linux host | GCC reports OpenMP 4.5 | compiler flag probe only | no asc runtime |
| Eigen 3.4.0 | deferred dense/sparse adapter candidate | interoperability or selected CPU algorithms | provider-defined | provider-defined | provider-defined | header package detected | CMake `Eigen3` found | no asc provider | no asc runtime |
| BLAS/LAPACK via oneAPI MKL 2024.2 | deferred dense provider candidate | BLAS/factorization/solver subset | provider-defined | LP64 detected; ILP64 not approved | provider-defined | Linux x86_64 host | CMake BLAS/LAPACK and MKL found | no asc provider | no asc runtime |
| oneAPI TBB 2024.2 | deferred execution candidate | no approved operation | n/a | n/a | n/a | Linux x86_64 host | CMake TBB found | no asc provider | no asc runtime |
| oneAPI SYCL 2024.2 | unapproved future candidate | none | n/a | n/a | n/a | OpenCL CPU visible | `sycl-ls` sees CPU | compiler present | no asc runtime |
| CUDA Runtime 12.9.86 | `core_cuda` | pinned/device/managed allocation, explicit-stream copies, events | raw bytes | checked `size_t` byte spans and signed 64-bit ASC metadata above | pageable host, pinned host, device, managed; device 0 | Linux WSL2 x86_64, RTX 3060 Laptop 6 GiB, compute capability 8.6, driver 576.83 | **configure-tested**: CMake 4.1.2 found CUDA compiler 12.9.86 and `CUDA::cudart`; unavailable-compiler request failed as required | **compile-tested**: GCC 11.4/NVCC 12.9.86, C++20/CUDA 20, architecture 86, warnings-as-errors, provider headers/multi-TU/consumers | **runtime-tested**: real-device resource/copy/context/event/ordering/error/lifetime and isolated-consumer operations passed |
| cuBLAS 12.9 | `dense_cuda` | bounded pointwise evaluator; Copy/Scal/Axpy kernels; Gemv/Gemm | float, double | checked signed 64-bit ASC metadata with complete pre-access `int` narrowing for cuBLAS | unique non-negative-stride device views; column-major Gemv/Gemm with positive increments/padded leading dimensions; none/transpose | same CUDA host | **configure-tested**: `CUDA::cublas` discovered only with CUDA enabled or requested provider package closure | **compile-tested**: provider sources, CUDA kernels, headers, positive/negative contracts, multi-TU, provider consumers | **parity-tested**: real-device pointwise and algebra results passed independent host exact/long-double oracles; concurrency and beta-zero cases passed |
| cuSOLVER 12.9 | deferred, unapproved dense candidate | no Milestone 6 operation | n/a | n/a | n/a | same CUDA host | library/header inventory only | no ASCCpp target or link edge | **skipped**: explicitly outside Milestone 6 |
| cuSPARSE 12.9 + original kernels | `sparse_cuda` | canonical CSR clone, bounded sparse pointwise evaluation, CSR SpMV | float, double | signed 64-bit ASC offsets/indices; checked provider narrowing | trusted canonical coordinate/CSR; unit or positive nonunit vector strides | same CUDA host | **configure-tested**: `CUDA::cusparse` discovered only for CUDA-enabled/provider closures | **compile-tested**: C++20/CUDA 20 provider sources, headers, contracts, package consumers | **parity-tested**: real-device clone, evaluator, unit/nonunit-stride SpMV against independent CPU oracles; trusted device CSC staging is **skipped** |
| ASC CUDA Philox | `random_cuda`, `random_dense_cuda`, `random_sparse_cuda` | raw words, dense Uniform01, exact-count coordinate generation | uint32 words; float, double | frozen counter mapping and checked signed-64 metadata | raw storage; dense left/right/padded; canonical coordinate output | same CUDA host | **configure-tested**: CUDA Runtime closure discovered only when requested | **compile-tested**: all three facets, headers, contracts, package consumers | **parity-tested**: real-device bit/layout/structure/value results against independent CPU oracles |
| cuRAND 12.9 | optional deferred random path | provider-specific distributions | provider-defined | provider-defined | provider ordering-specific | same CUDA host | library/header detected | no asc provider | none |
| HIP/ROCm | no approved facet | none | n/a | n/a | n/a | unavailable on host | not found | none | none |

## Capability reporting contract

Every implemented matrix row records:

- exact component and provider version;
- operation, input/compute/output scalar, index width, rank, layout/format,
  transpose/conjugation, memory space, device, algorithm, and determinism;
- workspace size/alignment and whether packing/conversion is permitted;
- configure, compile, runtime, and parity evidence separately;
- hardware, driver, toolkit, compiler, flags, and last verified date;
- unsupported cases and skips.

No single `Supports(Gemm)`-style Boolean is sufficient.

## Initial provider decision

Only the serial reference paths were approved for the first CPU milestones.
Detected OpenMP, Eigen, oneMKL/BLAS/LAPACK, TBB, and SYCL remain candidates,
not dependencies or capabilities. Milestone 7 retains `core_cuda` and
`dense_cuda` and adds the four separately exported sparse/random CUDA facets.
They default off together and have only the capability-specific local evidence
recorded above and in Publication Checkpoint B.

No provider may silently transfer, pack, densify, allocate, synchronize,
change precision, downcast, or fall back. An operation fails before destination
mutation when its complete capability key is unsupported.

For Milestone 7, `core_cuda` is **configure-tested**, **compile-tested**, and
**runtime-tested**; the five numerical/random CUDA facets are additionally
**parity-tested** for the exact tested capability subsets. Those labels apply
only to the exact local host above. A hosted GPU runner, multi-device runtime,
non-Linux CUDA host, trusted device CSC staging, and every unimplemented
provider are **skipped** rather than inferred.

The Milestone 7 provider claims remain bounded by their original checkpoint.
Milestone 8 revalidates the same claims without adding an operation or
provider. Final M8 commands, counts, compiler/sanitizer/provider-tool results,
and skips are recorded in
`docs/development/asc-cpp-m8-hardening-downstream/publication-checkpoint-b.md`.
Package and consumer tests are intentionally non-instrumented in sanitizer
builds and are validated separately in the static/shared matrices.
