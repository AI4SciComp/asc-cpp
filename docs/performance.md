# Performance methodology and envelope

Status: unreleased `0.9.0` Milestone 8 candidate

Date: 2026-07-27

ASCCpp performance evidence is correctness-checked observational data, not a
cross-machine timing guarantee. The serial implementations are reference
paths. CUDA facets are bounded explicit providers. No M8 optimization or
speedup claim is approved.

## Required measurement record

Every reported measurement records:

- ASCCpp version/worktree identity;
- operating system, CPU, memory, compiler, standard library, CMake, and build
  mode;
- static or shared linkage and relevant compiler/sanitizer options;
- provider, toolkit/compiler, driver, GPU, and architecture when applicable;
- exact operation, scalar, shape/layout/format, stored-entry count, and
  workspace;
- warmup count and measured repetition count;
- synchronization boundary and whether allocation/transfer is inside the
  timed region;
- elapsed unit and whether the value is a total or per-operation statistic;
- checksum or independent correctness oracle;
- pass, fail, or skip result; and
- known noise such as frequency scaling, thermal state, contention, first-use
  provider initialization, and timer resolution.

A number without that record is not ASCCpp performance evidence.

## Timing boundaries

CPU probes use `std::chrono::steady_clock`. Setup, shape validation, fixture
construction, and result verification are outside the timed region unless the
measurement explicitly names them.

CUDA probes warm up the operation, enqueue the declared repetitions on the
explicit context, and wait for the final completion before stopping the host
timer. That measures host scheduling plus device completion over the batch.
It is not a kernel-only hardware-event measurement. Transfers and allocation
are separated unless the probe explicitly measures them.

CUDA context creation/destruction and first provider use can allocate or
synchronize. In particular, dense context destruction follows the documented
cuBLAS handle boundary. Those costs must not be silently mixed into an
operation-only timing.

Checksums are computed after synchronization and outside the timed interval.
A finite checksum is only a smoke guard; parity-sensitive probes also compare
an independent numerical, structural, or bit oracle.

## Existing runtime probes

| Probe | Representative workload | Correctness guard |
| --- | --- | --- |
| dense CPU | pointwise evaluation and GEMM on fixed dense matrices | zero computational allocations and exact checksum |
| sparse CPU | canonical CSR SpMV | zero computational allocations and exact checksum |
| random storage CPU | dense fill and exact-count sparse generation | allocation accounting plus deterministic checksums |
| dense CUDA | vector pointwise/level-1, GEMV, and GEMM | completed events and finite/expected result checks |
| sparse CUDA | float/double CSR SpMV, unit stride, explicit workspace | independent expected checksum |
| random CUDA | raw words, dense Uniform01, sparse Uniform01 | raw/dense checksums and independent sparse coordinate/value oracle |

Milestone 8 adds representative public-header compile-time and object-size
observations plus an aggregate report. Compile-time probes use a fresh output
file, one named compiler invocation, and a monotonic wall-time measurement.
Object-size probes record the exact compiler/options and use the resulting
object file byte count. They are observations of that translation unit and
toolchain, not ABI or application-build predictions.

## Complexity and allocation envelope

| Surface | Documented envelope |
| --- | --- |
| checked metadata/layout/view creation | linear in inspected rank or metadata; no numerical result allocation |
| dense serial evaluation/reduction | one logical traversal; no computational temporary/workspace |
| dense serial GEMV/GEMM | conventional quadratic/cubic reference work; no packing/workspace |
| coordinate finalization | worst-case `O(rank * nnz^2)` with declared owner buffers |
| sparse conversion | deterministic scan bounds documented by the sparse module; destination buffers only |
| serial CSR SpMV | `O(rows + nnz)` plus adapter-defined access cost; no workspace |
| dense serial random fill | `O(logical_size * rank)`; no workspace |
| sparse serial random | `O(exact_count * logical_size + exact_count^2 * rank)`; coordinate/value result buffers only |
| dense CUDA pointwise/level-1 | linear device work plus event overhead; no ASC workspace |
| CUDA GEMV/GEMM | provider algebra cost; no ASC packing/workspace |
| CUDA CSR SpMV | cuSPARSE ALG2 with queried caller workspace for unit stride; project kernel and zero workspace for positive nonunit stride |
| CUDA sparse evaluation | one project-kernel operation per stored value; no workspace |
| CUDA raw/dense random | linear in generated words/logical values; no computational allocation |
| CUDA sparse random | deliberately single-threaded `O(exact_count * logical_size + exact_count^2 * rank)`; two result buffers and no workspace |

No-allocation statements cover ASCCpp computational storage on successful
paths. Provider libraries may own context/internal state, completion-event
creation has provider cost, and a failed `Status` diagnostic may allocate.

## Regression use

Absolute timing thresholds are prohibited across machines or unlike builds.
A local regression gate is meaningful only when:

1. candidate and baseline use the same source probe, compiler, options,
   linkage, provider/toolkit/driver, hardware, power/thermal policy, and
   workload;
2. both pass the same correctness oracle;
3. both use repeated samples with an explicitly recorded statistic;
4. the tolerance is stated before examining the candidate; and
5. noise or an environment change invalidates the comparison instead of
   being called a product regression.

The gate reports the raw measurements and chosen tolerance. It does not claim
that a faster smoke probe is representative of another application.

## Evidence interpretation

Provider timing requires real-device execution and is therefore at least
`runtime-tested`; configuration or compilation alone cannot produce a runtime
measurement. Numerical/bit agreement with an independent oracle may also be
`parity-tested`. Missing runtime hardware is `skipped`.

Sanitizer builds are correctness evidence, not representative performance
baselines. Debug, ASan, UBSan, TSan, LSan, Compute Sanitizer, and profiler
instrumentation can materially change timing and memory behavior.

The exact Milestone 8 commands and observed compile-time, object-size, CPU,
CUDA, and local repeat-regression results belong in Publication Checkpoint B.
This document defines how those values are collected and the limited claims
they support.
