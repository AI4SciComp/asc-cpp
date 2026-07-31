# Backend capability matrix

Status: Issue 9 Dense BLAS Level 3 Feature Gate B candidate

Evidence date: 2026-08-01

Evidence labels are independent: discovery, configure, compiler smoke,
ASCCpp-provider compile, real-hardware runtime, and parity. Detection alone is
not support.

| Provider | Owner/facet | Approved operations through Issue 9 | Scalars | Indices | Layouts/formats | Platforms | Configure evidence | Compile evidence | Runtime/parity evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| serial reference | provider-free modules plus `random_dense`, `random_sparse`, and `cpp` | prior Milestone 8 operations plus complete classic Dense BLAS Levels 1, 2, and 3 | BLAS S/D/C/Z where mathematically defined; Random facets exactly unqualified float/double | checked signed 64-bit logical metadata and checked unsigned random word offsets | host/pinned-host BLAS descriptors; row/column-major full, band, and packed storage; signed vector strides; canonical coordinate Sparse output | portable C++20 CPU | configure-tested: local GCC Debug static/shared Issue 9 builds | compile-tested: strict warnings, installed Dense consumer, and all 30 Level 3 rows instantiated | runtime-tested: independent CPU conformance, invalid-input, edge-value, layout/flag, degenerate-shape, alias, and allocation checks for all 30 Level 3 rows |
| GCC OpenMP | deferred CPU facet candidate | none | not approved | not approved | not approved | host | skipped | skipped | skipped |
| Eigen | deferred adapter candidate | none | provider-defined | provider-defined | provider-defined | host | skipped | skipped | skipped |
| BLAS/LAPACK | deferred dense candidate | none | provider-defined | provider-defined | provider-defined | host | skipped | skipped | skipped |
| CUDA Runtime | `core_cuda` | device discovery; pinned/device/managed allocation; explicit H2D/D2H/D2D copy; owned nonblocking stream; query/wait completion event | bytes | checked byte sizes and signed 32-bit device ordinal after validation | explicit Core memory spaces; no implicit transfer | NVIDIA GPU; CUDA 12+ | configure-tested: CMake 4.1.2, CUDA toolkit/compiler 12.9.86, GCC 11.4 host, architecture 86 | compile-tested: static Debug and shared Release provider targets, header isolation, no-exceptions, multi-TU, negative ownership contracts | runtime-tested: RTX 3060 Laptop, driver 576.83, runtime 12.9, compute 8.6; allocation/copy/event/current-device/lifetime/concurrency tests |
| CUDA Runtime and cuBLAS | `dense_cuda` | bounded pointwise evaluation; complete classic Dense BLAS Levels 1, 2, and 3 | BLAS S/D/C/Z where mathematically defined; evaluation exactly float/double | ranks 0--8 for evaluation; checked signed 64-bit BLAS extents and strides; cuBLAS `_64` calls | unique device views; row/column-major full, band, and packed storage; signed vector strides; approved project kernels for unrepresentable row-major complex cases; no hidden packing | NVIDIA GPU; CUDA 12+ | configure-tested: CUDA toolkit/compiler 12.9.86, GCC 11.4 host, architecture 86 | compile-tested: local static and shared provider plus isolated consumer targets; all 30 Level 3 rows instantiated | runtime-tested and parity-tested: all 30 Level 3 rows plus layouts, side/triangle/unit/transpose/conjugation flags, invalid input, edge values, degenerate shapes, event completion, and allocation contract on RTX 3060 Laptop, driver 576.83, compute 8.6 |
| CUDA Runtime and cuSPARSE | `sparse_cuda` | canonical host CSR clone; deterministic CSR SpMV; bounded same-structure coordinate/CSR evaluation | exactly unqualified float/double | checked signed 64-bit extents, NNZ, offsets, and indices | canonical trusted coordinate and CSR; unit-stride CSR ALG2 with explicit workspace; positive nonunit vector strides use a zero-workspace project kernel; trusted CSC success skipped | NVIDIA GPU; CUDA 12+ | configure-tested: CMake 4.1.2, CUDA toolkit/compiler 12.9.86, GCC 11.4 host, architecture 86 | compile-tested: clean static Release and shared Release provider/consumer targets; 22 M7 header/no-exception/ODR/positive/negative contracts | runtime-tested and parity-tested: independent float/double staging, SpMV, evaluator, failure, overlap, allocation, lifetime, and independent-context oracles on RTX 3060 Laptop; Compute Sanitizer memcheck clean |
| CUDA Runtime and project Philox kernel | `random_cuda` | asynchronous raw Philox4x32-10 words into a capacity-carrying device view | `uint32_t` words | checked byte count and unsigned word offsets | caller-owned aligned device `MutableMemoryView`; no transfer or workspace | NVIDIA GPU; CUDA 12+ | configure-tested: same M7 CUDA configuration | compile-tested: static/shared provider and isolated consumer targets; SDK-free headers | runtime-tested and parity-tested: exact independent Philox equations for lanes, blocks, tails, partitions, offsets, capacity failures, and independent contexts on RTX 3060 Laptop; Compute Sanitizer memcheck clean |
| CUDA Runtime and project Uniform01 kernel | `random_dense_cuda` | asynchronous deterministic Dense Uniform01 fill | exactly unqualified float/double | ranks 0--8; checked extents, strides, spans, and word offsets | unique left/right/padded nonnegative device views; logical dimension zero fastest | NVIDIA GPU; CUDA 12+ | configure-tested: same M7 CUDA configuration | compile-tested: static/shared provider and isolated consumer targets; exact destination constraints | runtime-tested and parity-tested: independent bit oracles for float/double, scalar/empty/left/right/padded/partition cases and independent contexts on RTX 3060 Laptop; Compute Sanitizer memcheck clean |
| CUDA Runtime and project exact-count kernels | `random_sparse_cuda` | asynchronous exact-count canonical coordinate/value Uniform01 generation | exactly unqualified float/double | arbitrary compile-time rank tested through rank 9; checked logical size, count, coordinates, and two word domains | canonical coordinate output; exactly two visible owner allocation attempts; no computational workspace | NVIDIA GPU; CUDA 12+ | configure-tested: same M7 CUDA configuration | compile-tested: static/shared provider and isolated consumer targets; ownership/type negative contracts | runtime-tested and parity-tested: independent priority/tie/coordinate/value oracle plus provider-free parity, rank 0/1/2/3/9, empty/partial/full, rollback, offsets, and independent contexts on RTX 3060 Laptop; Compute Sanitizer memcheck clean |
| HIP/ROCm | no approved facet | none | not approved | not approved | not approved | AMD GPU | skipped | skipped | skipped |

## Capability reporting contract

Every implemented provider row records:

- exact component and provider version;
- operation, input/compute/output scalar, index width, rank, layout/format,
  transpose/conjugation, memory space, device, algorithm, and determinism;
- workspace size/alignment and whether packing/conversion is permitted;
- configure, compile, runtime, and parity evidence separately;
- hardware, driver, toolkit, compiler, flags, and last verified date;
- unsupported cases and skips.

No single `Supports(Gemm)`-style Boolean is sufficient.

## Milestone 8 hardening revalidation

Milestone 8 changes no provider operation or edge. It revalidated the
unchanged surface on Linux with GCC 11.4, Clang 19, CMake 4.1.2, GNU Make
4.3, CUDA 12.9.86, driver 576.83, and the compute-8.6 RTX 3060 Laptop GPU.

- Corrected clean CPU rows passed GCC Debug/static 193/193, GCC Debug/shared
  195/195, and Clang Debug/shared 194/194. These include source/build/install/
  relocation package checks and isolated consumers; they do not claim a fresh
  rerun of every pre-correction CPU Cartesian row.
- The corrected GCC/NVCC Release/shared CUDA row passed with 260 registered
  tests: 250 passed, ten deterministic no-device cases were `skipped`, and
  zero failed. It includes all six CUDA facets, every CUDA component's
  build-tree and relocated consumer, exact all-15 package closures, full
  49-header installation, symbols, ABI observation, and provider-free
  isolation.
- Compute Sanitizer memcheck passed 13/13 applicable CUDA runtime executables
  with zero errors and zero leaks. The intentional impossible-allocation
  native-state oracle is `skipped` under memcheck because its expected CUDA
  API failure is itself reported as a sanitizer API error.
- ASan+UBSan passed the complete selected CPU sanitizer row, 139/139.
  Standalone LSan and TSan each passed a sanitizer-safe 12/12 runtime subset;
  their whole-test-tree builds are `skipped` because deliberate global
  allocation-interposition fixtures conflict with those sanitizer runtimes.

These are exact local combinations, not a claim that CUDA was exercised over
the complete CPU Cartesian matrix.

## Milestone 7 provider decision

Milestone 7 retains `core_cuda` and `dense_cuda` and adds only `sparse_cuda`,
`random_cuda`, `random_dense_cuda`, and `random_sparse_cuda`. All six are
opt-in package components with exact direct edges; provider-free modules,
provider-free Random storage facets, and `ASC::cpp` acquire no CUDA
dependency. CUDA language/toolkit discovery occurs only for
`ASC_CPP_ENABLE_CUDA=ON`, and installed consumers discover CUDAToolkit only
when a requested component closure contains a CUDA facet.

All four M7 facets are independently configure-tested, compile-tested,
runtime-tested, and parity-tested on the recorded local RTX 3060 Laptop
environment. Trusted device CSC success, multi-GPU, cross-toolkit,
cross-platform GPU runtime, HIP, SYCL, and other providers are **skipped**.

No provider may silently transfer, pack, allocate, synchronize, change
precision, downcast, or fall back. An operation must fail before destination
mutation when its complete capability key is unsupported.
