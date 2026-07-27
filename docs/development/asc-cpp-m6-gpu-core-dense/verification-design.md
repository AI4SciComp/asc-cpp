# Milestone 6 Independent Verification Design

Status: Frozen before production inspection

Date: 2026-07-26

Role: independent verification

Branch: `feature/asc-cpp-m6-gpu-core-dense`

## Independence and sources

This design was frozen before inspecting Milestone 6 production source or any
other specialist report. It derives only from:

- the complete team runbook;
- the frozen Milestone 6 contract, ownership ledger, preflight, dependency
  audit, and provenance record;
- accepted ADRs 0001--0004, 0007--0011, 0013, 0017, and 0018;
- the predecessor public core and dense contracts; and
- official CMake `FindCUDAToolkit`, CUDA Runtime 12.9, and cuBLAS 12.9
  documentation.

MdeCpp and the deleted asc-cpp CUDA implementation and tests are outside this
review and were not inspected. No NVIDIA sample or third-party test source,
literal output, or benchmark code is used.

The official contracts relevant to the oracles are:

- CMake supplies `CUDA::cudart` and `CUDA::cublas`;
- a nonblocking CUDA stream avoids implicit synchronization with the legacy
  default stream;
- `cudaMemcpyAsync` requires nonoverlapping memory areas and may return before
  completion;
- event query distinguishes not-ready from completed work, while event
  synchronization waits for that event;
- CUDA allocation families have matching deallocation operations;
- cuBLAS typed GEMV/GEMM use column-major matrices, explicit leading
  dimensions, and 32-bit `int` dimensions;
- cuBLAS host pointer mode permits `alpha` and `beta` to cease lifetime after
  the provider call returns;
- `beta == 0` means GEMV/GEMM need not read the old output;
- disallowing atomics and selecting pedantic math are the deterministic
  provider settings required by the frozen contract; and
- host-result Dot/Nrm2 would synchronize and are therefore correctly absent.

## Evidence labels

The report records the following independently. One label never implies
another:

| Label | Required evidence |
| --- | --- |
| configure-tested | CUDA 12.9 discovery, CUDA language enablement, architecture 86, and requested-unavailable failure |
| compile-tested | Provider sources, public headers, tests, consumers, and multi-TU objects compiled for architecture 86 |
| runtime-tested | Public APIs executed on the identified RTX 3060 Laptop GPU with toolkit/runtime/driver recorded |
| parity-tested | Runtime float/double results compared with the independent numerical or exact-state oracles below |
| skipped | Exact unavailable tool, operation, or platform and reason |

Inventory alone satisfies none of these labels.

## State and lifetime oracles

1. `ExecutionContext::Serial()` remains serial and host-only.
2. Provider-free `ExecutionContext::Create(Backend::kCuda, ...)` validates
   vocabulary but does not create CUDA state.
3. `CreateCudaExecutionContext` rejects non-CUDA devices and invalid ordinals,
   creates exactly one CUDA context stream, and returns a copyable immutable
   `ExecutionContext`.
4. Copies of a CUDA execution context identify the same backend and device and
   remain usable after the source object is destroyed.
5. Separate CUDA contexts have independent stream ordering. Work enqueued in
   one is not made complete merely by waiting on an event from the other.
   Because hardware may finish quickly, absence of an observed not-ready state
   is not itself a failure; ordering is proved by per-stream data dependencies.
6. `CompletionEvent` is move-only. A moved-from event is invalid; query and
   wait fail without affecting the moved-to event.
7. `Query()` accepts either `false` or `true` immediately after enqueue,
   eventually returns `true` after `Wait()`, and never converts failure into
   success.
8. `Wait()` waits for the represented event, not all device work. No
   wall-clock threshold is a correctness gate.
9. Event state keeps provider execution/stream state alive after the
   originating public context value is destroyed. User storage remains alive
   until event completion.
10. Destroying a completed or moved-from event is safe. Tests do not rely on
    event destruction to synchronize.
11. Separate provider contexts may be used from separate host threads.
    Concurrent mutation of one `DenseCudaContext` is not attempted.

## Core CUDA memory and copy oracles

1. `CudaDeviceCount()` returns at least one on the identified runtime and
   preserves provider/native codes on provider failure.
2. `CudaMemoryResource::Create` accepts exactly pinned host, device, and
   managed spaces; it rejects host and invalid enum values.
3. Each resource is noncopyable and nonmovable so its address remains stable
   for every `Buffer`.
4. Zero-byte allocation returns a valid zero-sized buffer with a null pointer.
5. Nonzero allocations are non-null and satisfy requested valid alignment.
   Unsupported alignment fails before allocation.
6. Move construction, move assignment, `Reset`, and destruction release
   exactly once through the matching CUDA allocation family.
7. An intentionally impossible allocation returns allocation/provider failure
   with provider and native code; no owner is published.
8. Host-to-device, pinned-to-device, managed-to-device, device-to-host,
   device-to-pinned, device-to-managed, and device-to-device copies reproduce
   exact byte patterns after waiting.
9. Copy dispatch uses the explicit context and permits zero bytes without
   pointer access.
10. Exact same-address, same-space, same-size copy is a no-op completion event
    after pointer-space/device validation.
11. Partially overlapping CUDA ranges are rejected before enqueueing.
12. Destination-too-small, null nonempty views, invalid spaces, wrong backend,
    wrong device, and inaccessible combinations fail before pointer access and
    preserve destination bytes.
13. Sentinel pointers are used only where context/space validation must
    precede dereference; no invalid pointer is submitted to CUDA.
14. Provider calls restore any CUDA current-device state they temporarily
    change. A one-device host can check stability but not multi-device
    restoration.

Pageable `MemorySpace::kHost` copies are explicitly distinguished from the
stronger host-asynchronous behavior of pinned/device/managed memory: CUDA may
stage or block the host for pageable copies. Stream ordering and completion
remain represented by the returned event.

## Dense owner and clone oracles

For `float` and `double`, rank zero and representative ranks one through eight:

1. Existing `DenseArray::Create` remains host-only and value-initializing.
2. `CreateUninitialized` accepts valid unique exhaustive left/right mappings
   with host, pinned, device, and managed resources and allocates the exact
   owner span.
3. No test reads an uninitialized element. Each owner is fully written before
   transfer or arithmetic; Compute Sanitizer `initcheck` audits accidental
   reads.
4. A device owner yields a descriptor whose `At` rejects host dereference.
   Managed storage also does not silently migrate through `At`.
5. Zero-extent owners allocate no storage. Rank zero has logical size one.
6. Host/device and applicable device/device, pinned, and managed clones
   reproduce exact values and preserve layout.
7. Clone is synchronous and publishes a destination only after allocation,
   copy, and wait succeed.
8. Allocation, copy, wrong-context, wrong-device, and moved-owner failures
   publish no owner and leave the source unchanged.
9. Move construction/assignment leave one owner and prevent view creation from
   the moved-from owner.

## CUDA pointwise evaluator oracles

The accepted surface is tested for `float` and `double`, compile-time ranks
zero through eight, zero extents, and unique non-negative-stride device views:

1. Terminal-to-distinct-destination copies exact logical values.
2. Rank-zero scalar fill expands to every destination element.
3. One-level negate accepts only a dense terminal and produces `-x`.
4. One-level add, subtract, and multiply accept terminal/terminal,
   terminal/scalar, scalar/terminal, and scalar/scalar combinations.
5. Expected values are computed independently on the host in
   dimension-zero-fastest order and compared after explicit copy/wait.
6. Logical results agree across left, right, and padded-stride mappings; holes
   retain canaries where observable.
7. Exact terminal self-evaluation returns a no-op event and preserves bytes.
8. Any other possible destination overlap is rejected before enqueue and
   preserves the destination span.
9. Wrong rank/shape, placement/device/span, nested nodes, rank above eight,
   external adapters, unsupported scalars, and nonunique/negative mappings are
   rejected at the documented compile/runtime boundary. There is no fallback.
10. Rank zero executes once. Any zero extent executes no element operation.
11. Successful evaluation performs no ASC allocation, transfer, packing,
    workspace allocation, fallback, or hidden synchronization.
12. Separate contexts and buffers produce independent results concurrently.

## CUDA dense algebra oracles

All overloads are exercised for `float` and `double`.

### Copy, Scal, and Axpy

- rank-one and rank-two left, right, and unique padded-stride views;
- exact copy bit patterns including signed zero, quiet NaN, and infinities;
- exact power-of-two scaling and finite Axpy;
- zero length, degenerate rank-two shapes, allowed exact in-place forms, and
  prohibited partial/output-input overlap;
- wrong shape, placement, device, span, and scalar/rank constraints; and
- independent streams and failure-before-enqueue destination canaries.

### Gemv

- column-major matrices, padded leading dimensions, and positive vector
  increments;
- none/transpose on square and rectangular matrices;
- zero rows/columns, zero alpha, zero beta, and nonzero beta;
- row-major/arbitrary matrix stride rejected as unsupported;
- output overlap with matrix/input rejected;
- checked provider-width narrowing before pointer access; and
- NaN old output with `beta == 0`, requiring finite output for finite product.

### Gemm

- all four transpose pairs for square and rectangular operands;
- column-major valid padded leading dimensions;
- zero `m`, `n`, or `k`, zero alpha, zero beta, and nonzero beta;
- row-major/arbitrary layouts rejected;
- output overlap with either input rejected;
- checked `m/n/k/lda/ldb/ldc` narrowing before pointer access; and
- NaN old output with `beta == 0`.

Dot, Nrm2, reductions, solvers, complex/mixed precision, batching, fast math,
Tensor Core modes, native handles, and external streams receive negative
capability/absence checks, not implementation claims.

## Numerical and parity oracle

Copy uses byte equality where bit preservation is promised. Pointwise
integer-exact floating cases use exact equality. General Axpy/Gemv/Gemm values
are computed with explicit verifier-owned long-double loops, independent of
ASC reference kernels and cuBLAS.

For result `r`, reference `q`, input-product absolute sum `s`, reduction length
`k`, and scalar type `T`, accept:

```text
abs(r - q) <= 8 * epsilon(T) * max(1, k) *
              max(1, abs(q), abs(alpha) * s + abs(beta) * abs(old))
```

Failures report scalar, operation, dimensions, transposes, layout, errors, and
tolerance. NaN is accepted only where the independent operation produces NaN;
infinities require matching sign. No universal epsilon is used.

## Compile, isolation, and consumer oracles

Verifier-owned compile sources cover:

- each provider public header alone;
- common `asc/core.h` and `asc/dense.h` without CUDA headers or `__CUDACC__`;
- host compilation with `-fno-exceptions`;
- resource/context/event/owner copy and move traits;
- multi-TU declarations/linkage; and
- useful negative compilation for forbidden ownership operations and element
  types.

Installed consumers link only:

```text
ASC::core_cuda  -> ASC::core
ASC::dense_cuda -> ASC::dense;ASC::core_cuda
```

Each consumer performs a real runtime operation. Provider-free consumers
against a CUDA-enabled install remain CUDA-SDK-free. The lead owns package
closure and target-graph inspection.

## Sanitizer and failure matrix

- ASan/UBSan cover applicable host/provider-management paths.
- Compute Sanitizer `memcheck`, `racecheck`, `initcheck`, and `synccheck` run
  and report separately. Unsupported outcomes are skips with the exact
  diagnostic, never passes.
- CUDA asynchronous errors are forced to query/wait boundaries where possible
  and retain provider/native codes.
- Destination/state canaries prove invalid-input transactionality.
- Race tests use separate contexts/handles/buffers; one mutable provider
  context is not shared concurrently.

## Benchmark design

The project-owned harness records compiler, configuration, CUDA environment,
device/architecture, provider, scalar, dimensions, layout, warmups,
repetitions, and checksum. Host-to-device/device-to-host transfers are
separate from pointwise add, Axpy, Gemv, and Gemm timings. Operations warm up,
enqueue repeatedly on one context, and wait on a final event. A checksum
prevents dead-code elimination. There is no speed gate or superiority claim.

## Verifier-owned files

```text
tests/core_cuda/test_support.h
tests/core_cuda/core_cuda_test.cc
tests/dense_cuda/test_support.h
tests/dense_cuda/dense_cuda_storage_evaluate_test.cc
tests/dense_cuda/dense_cuda_linalg_test.cc
tests/dense_cuda/dense_cuda_concurrency_test.cc
tests/compile/m6_provider_contract.cc
tests/compile/m6_provider_multi_tu.h
tests/compile/m6_provider_multi_tu_a.cc
tests/compile/m6_provider_multi_tu_b.cc
tests/compile/m6_provider_multi_tu_main.cc
tests/compile/m6_negative_*.cc
tests/consumer/core_cuda/main.cc
tests/consumer/dense_cuda/main.cc
benchmarks/dense_cuda/dense_cuda_benchmark.cc
```

The lead owns every CMake list and test registration. If a planned case is
impossible without a private/native handle, the report records that limitation
instead of reaching through an internal header.
