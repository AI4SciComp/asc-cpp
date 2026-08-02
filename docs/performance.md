# Performance methodology and envelope

Status: unreleased `0.9.0` Issue 16 Feature Gate B candidate

Date: 2026-08-02

ASCCpp performance evidence is correctness-checked observational data, not a
cross-machine timing guarantee. Serial implementations are reference paths;
CUDA facets are bounded explicit providers. Issue 13 adds a threshold-free
CPU probe for all four versioned engines, uniform integer/real transforms, and
scalar Box-Muller normal generation. Every row uses fixed seed/state and
parameters, records repetitions and elapsed time, checks zero successful-path
allocation, and guards correctness with an independently derived checksum or
fixed-seed moment invariant.

Issue 14 extends that probe with indexed Sobol word evaluation. Issue 15 adds
correctness-guarded Dense Sobol mapping and Sparse structure-only selection to
the existing Random storage benchmark. Each row names algorithm/version,
explicit state, parameters, shape/layout/sparsity, repetition count,
allocation/workspace, and a deterministic checksum or invariant. Rows report
elapsed time without a speed pass gate. Issue 13 through Issue 15 work is
CPU-only; no new GPU benchmark or provider claim is approved.

Issue 16 adds no benchmark or performance threshold. It audits that both
Random benchmark executables remain registered, correctness-guarded,
fixed-state, and allocation-observed; local reruns are completion evidence,
not a new performance promise.

## Issue 16 local revalidation

The Release/static completion rerun used the same local platform described
below. The scalar/QMC benchmark preserved aggregate checksum
`14039962399095420760`; all eight operations reported zero successful-path
allocation calls. The storage benchmark preserved aggregate checksum
`1928359207440646895`. Its audited adapter rows observed:

| Operation | Shape/count | Repetitions | Elapsed (ns) | Operation allocations | Correctness guard |
| --- | --- | ---: | ---: | ---: | --- |
| Dense Sobol adapter v1 | 2048 samples x 8 dimensions | 10 | 19,588,235 | 0 | scalar `SobolCoordinate`; checksum `3528554356640777091` |
| Sparse structure-only adapter v1 | 64 x 64, exact count 256 | 10 | 2,070,699 | 0 | independent Philox priority sort; checksum `10016369130947636770`; next offset `8329` |

These single-process observations have no pass threshold and make no
cross-machine performance claim.

## Issue 15 local observation

The Issue 15 benchmark times only the synchronous adapter calls. Caller-owned
Dense output, point workspace, Sparse candidate workspace, and ordinal output
are allocated before timing. Correctness checks and independent priority
sorting run after timing. Both new rows require zero process-visible allocation
inside the operation and contribute to the aggregate checksum.

The threshold-free observation was collected on Linux/WSL2 6.18, an Intel
Core i7-11800H with 16 logical CPUs and 15 GiB RAM, GNU C++ 11.4, CMake 4.1.2,
Release/static, and ASCCMake 0.1.0.

| Operation | Shape/count | Repetitions | Elapsed (ns) | Allocation calls | Correctness guard |
| --- | --- | ---: | ---: | ---: | --- |
| Dense Sobol adapter v1 | 2048 samples x 8 dimensions | 10 | 21,425,096 | 0 | scalar `SobolCoordinate`; checksum `3528554356640777091` |
| Sparse structure-only adapter v1 | 64 x 64, exact count 256 | 10 | 2,291,138 | 0 | independent Philox priority sort; checksum `10016369130947636770`; next offset `8329` |

These are observations for regression comparison, not pass thresholds or
cross-machine promises. The aggregate benchmark checksum was
`1928359207440646895`. The Sparse operation uses an explicit
`logical_size`-element candidate workspace and average-linear selection; the
combined owner generator retains its earlier correctness-oriented reference
complexity.

## Issue 14 local observation

The Issue 14 threshold-free row was run on the same local platform with GNU
C++ 11.4, CMake 4.1.2, Release/static, and ASCCMake 0.1.0. The timed region
performed indexed Sobol-Joe-Kuo-D6-v1 word evaluation for zero-based dimension
3; fixture setup and correctness checking were excluded.

| Operation | Repetitions | Elapsed (ns) | Allocation calls | Correctness observation |
| --- | ---: | ---: | ---: | --- |
| indexed Sobol word, zero-based dimension 3 | 100,000 | 10,260,550 | 0 | published point plus independent checksum `0xb9c9b02511675303` |

This single-process total is an implementation observation, not a speed
threshold or cross-machine comparison. The aggregate benchmark checksum was
also verified.

## Issue 13 local observation

The threshold-free probe was run once on Linux/WSL2 6.18, an Intel
Core i7-11800H (16 logical CPUs) with 15 GiB RAM, GNU C++ 11.4, CMake 4.1.2,
Release/static, and ASCCMake 0.1.0. There is no warmup phase; construction and
correctness checking are outside the timed region. Every row observed zero
successful-path allocation calls.

| Operation | Repetitions | Elapsed (ns) | Correctness observation |
| --- | ---: | ---: | --- |
| SplitMix64 v1 raw engine | 200,000 | 363,542 | checksum `0x31094ac056ef6f0a` |
| PCG32 v1 raw engine | 200,000 | 294,972 | checksum `0xb74ea408b377181a` |
| xoroshiro64* v1 raw engine | 200,000 | 699,953 | checksum `0xb47e791cf8607c35` |
| xoroshiro128+ v1 raw engine | 200,000 | 684,874 | checksum `0x9b8ab4ac4d3e0e4c` |
| PCG32 uniform integer `[-1000,1000]` | 200,000 | 2,150,740 | checksum `0xc7b222fe03542292` |
| xoroshiro128+ double uniform real `[0,1)` | 200,000 | 1,147,067 | checksum `0x9eb2f1cfad9f6ca8` |
| SplitMix64 double standard normal | 100,000 | 3,768,467 | mean `0.00173651`; variance `1.00033` |

These single-process totals are an implementation observation, not a speed
threshold or cross-machine comparison. The engine and uniform checksums were
derived independently from the frozen mappings; the normal row uses a
fixed-seed moment invariant because libm transcendental bits are not a
cross-ABI promise.

## Required measurement record

Every reported observation records:

- ASCCpp version and exact worktree/commit identity;
- operating system, CPU, memory, compiler, standard library, CMake, build mode,
  linkage, and relevant flags;
- provider, CUDA toolkit/compiler, driver, GPU, and architecture where
  applicable;
- operation, scalar, shape/layout/format, stored-entry count, and workspace;
- warmup and measured repetition counts;
- synchronization boundary and whether setup, allocation, or transfer is timed;
- elapsed unit and whether the value is total or per operation;
- checksum or independent correctness oracle;
- pass, fail, or skip; and
- known noise such as scheduling, frequency scaling, thermal state,
  contention, first-use initialization, and timer resolution.

A number without that record is not ASCCpp performance evidence.

## Timing boundaries

CPU probes use `std::chrono::steady_clock`. Fixture construction and result
verification stay outside the timed region unless explicitly named.

The Utilities `Timer` is a checked steady-clock accumulator, not a
higher-precision benchmark statistics package. `Stop` publishes a sample only
after its interval, total, and sample-count arithmetic is representable;
overflow returns `ErrorCode::kOverflow` without changing prior completed
statistics. `Average` also reports unrepresentable count conversion.
`Elapsed`, whose existing signature has no failure channel, saturates at
`Timer::Duration::max()` rather than wrapping an unrepresentable running total
or reporting a regressed steady clock as a duration. Measurement code must
treat that sentinel as an invalid timing observation.

CUDA probes warm up, then enqueue and wait for completion of each declared
measured repetition on the explicit context before stopping the host timer.
This measures repeated host scheduling plus device completion; it is not a
kernel-only event measurement. Transfers and allocations remain separate
unless the probe explicitly includes them.

Context creation/destruction and first provider use may allocate or
synchronize. Those costs cannot be silently mixed into an operation-only
measurement. Checksums are computed after synchronization and outside the
timed interval; parity-sensitive probes also use an independent oracle.

## Runtime probes

| Probe | Representative workload | Correctness guard |
| --- | --- | --- |
| Dense CPU | pointwise evaluation, Level 1 Axpy/Dot, Level 2 GEMV latency/estimated bandwidth, and exact-descriptor Level 3 GEMM | exact/expected result and zero ASCCpp computational allocation |
| Sparse CPU | indexed dot, canonical CSR SpMV, and CSR SpMM | independent results and zero workspace/allocation |
| Random storage CPU | Dense fill and exact-count Sparse generation | deterministic checksums and declared result allocations |
| Dense CUDA | pointwise, Level 1 Axpy/Dot/Iamax, Level 2 GEMV latency/estimated bandwidth, and exact-descriptor Level 3 GEMM | completed events, numerical parity, caller-owned Iamax workspace, and zero operation allocation |
| Sparse CUDA | retained float/double cuSPARSE SpMV and standardized float/double project-kernel SpMV | explicit/zero workspace as declared, numerical parity, and zero operation allocation |
| Random CUDA | raw words, Dense `Uniform01`, Sparse `Uniform01` | independent bit/structure/value oracles |

The Issue 9 Level 3 GEMM probes record five repeated timing samples and their
sample variance in addition to total/per-operation timing, throughput,
correctness, and allocation evidence. No benchmark-framework dependency or
cross-machine threshold is introduced.

## Compile-time and object-size observations

Milestone 8 also compiles three representative translation units from an
installed header tree:

- [`compile_core.cc`](../benchmarks/hardening/compile_core.cc): Core types and
  `Result`;
- [`compile_dense.cc`](../benchmarks/hardening/compile_dense.cc): Dense
  storage plus expression evaluation; and
- [`compile_provider_free.cc`](../benchmarks/hardening/compile_provider_free.cc):
  every provider-free umbrella.

Each observation uses one named compiler invocation and a fresh object,
records compiler identity/options, wall-time, object byte count, and separate
source/object SHA-256 values. It is a translation-unit/toolchain observation,
not an application build-time, binary size, ABI, or cross-compiler prediction.
There is no threshold.

## Complexity and allocation envelope

Let `N` denote a logical element count, `R` rank, `Z` stored nonzeros, and `K`
an exact Sparse random count.

| Surface | Documented envelope |
| --- | --- |
| checked metadata/layout/view creation | linear in inspected rank/metadata; no numerical result allocation |
| Dense serial evaluation/reduction | one logical traversal with current built-in mapping work `O(N * R)`; no computational temporary/workspace |
| Dense serial BLAS Level 1 | `O(N)` vector work (`O(1)` for scalar rotations); no allocation, packing, or workspace |
| Dense serial BLAS Level 2 | conventional `O(mn)` matrix-vector/rank-update work, reduced for band storage; no allocation, packing, or workspace |
| Dense serial BLAS Level 3 | conventional matrix-matrix algebra cost (`O(mnk)` for GEMM); no packing/workspace |
| coordinate finalization | current worst case `O(R * Z^2)`; declared owner buffers and `O(R)` local work storage |
| Sparse conversion | deterministic scans documented by the Sparse module; destination owner buffers only |
| serial indexed Sparse BLAS Level 1 | `O(Z)`; no workspace |
| serial CSR/CSC SpMV and SpMM | `O(Z)` and `O(Z * nrhs)` respectively; no workspace |
| serial sparse triangular solve | allocation-free reference traversal; current general CSR/CSC lookup is `O(order * Z)` per right-hand side |
| stateful Random engines and fixed-width distributions | `O(1)` work and storage per accepted engine/real/normal value; uniform-integer rejection has expected `O(1)` work and no finite worst-case attempt bound; no allocation |
| Dense serial random | `O(N * R)`; no computational allocation |
| Sparse serial random | `O(K * N)` selection plus current `O(K^2 * R)` coordinate finalization; result buffers plus `O(R)` local storage |
| Dense CUDA pointwise/random | current logical coordinate work `O(N * R)`; no ASCCpp computational workspace |
| CUDA BLAS Level 1 | provider or bounded project-kernel `O(N)` work; no ASC-managed allocation, with only explicit caller-owned Iamax workspace |
| CUDA BLAS Level 2 | provider or bounded project-kernel algebra cost; no ASCCpp packing/workspace |
| CUDA BLAS Level 3 | provider algebra cost; no ASCCpp packing/workspace |
| CUDA CSR SpMV | cuSPARSE ALG2 with queried caller workspace for unit stride; project kernel with zero workspace for positive nonunit stride |
| standardized CUDA Sparse BLAS | indexed Level 1 and non-transposed CSR matrix operations use `O(Z)`/`O(Z * nrhs)` project kernels; transpose and triangular reference kernels may scan CSR repeatedly; zero operation workspace |
| CUDA Sparse evaluation | `O(Z)` project-kernel work; no workspace |
| CUDA raw Random | `O(word_count)`; no computational allocation/workspace |
| CUDA Sparse random | `O(N * K + K^2 + R * K)`; exactly two result-buffer allocation attempts and no computational workspace |

No-allocation statements cover ASCCpp computational storage on successful
operation paths. Provider libraries may own context/internal state, event
creation has provider cost, result owners still allocate their documented
storage, and diagnostic strings may allocate.

The CUDA exact-count Sparse generator is intentionally a low-workspace
single-threaded reference path and can be very slow for large domains/counts.
That limitation is part of its performance envelope, not a regression by
itself.

## Regression use

Absolute thresholds are prohibited across machines or unlike builds. A local
regression comparison is meaningful only when:

1. candidate and baseline use the same probe, compiler/options, linkage,
   provider/toolkit/driver, hardware policy, and workload;
2. both pass the same correctness oracle;
3. repeated samples use a statistic selected before seeing the candidate;
4. the tolerance is fixed in advance; and
5. environment/noise changes invalidate the comparison instead of being
   labeled a product regression.

Report raw measurements, statistic, and tolerance. Sanitizer, Debug, Compute
Sanitizer, and profiler measurements are correctness evidence, not
representative performance baselines.

The exact Milestone 8 compile/object/runtime observations and skips belong in
Publication Checkpoint B. Provider timing requires real-device execution and
is therefore `runtime-tested`; agreement with an independent oracle may also
be `parity-tested`. Missing hardware is `skipped`.
