# Milestone 6 Independent Verification Design

Status: Frozen before production-source inspection

Date: 2026-07-28

Scope: Milestone 6 — GPU core and dense

## Independence statement

This design was frozen from the approved Milestone 6 contract, ownership
ledger, public Milestones 0--5 APIs, and project test conventions before the
verification agent inspected any Milestone 6 production implementation. The
agent will not inspect or copy MdeCpp, the deleted asc-cpp CUDA implementation,
or any Milestone 8 implementation. Expected values, state transitions,
lifetime probes, and failure cases below are independently derived.

## Test environments and evidence labels

The lead registers and executes the fixtures in clean matrices. Evidence may
only use these exact labels:

```text
core_cuda:  configure-tested, compile-tested, runtime-tested
dense_cuda: configure-tested, compile-tested, runtime-tested, parity-tested
```

Every runtime/parity record must identify CUDA toolkit, CUDA compiler, driver,
CUDA runtime, cuBLAS, GPU name, compute capability, compiled architecture,
configuration, shared/static mode, operation, type, rank/layout/size,
tolerance, and any skipped case.

CPU-only fixtures independently assert that the disabled configuration does
not discover CUDA, enable the CUDA language, expose CUDA targets, or link a
CUDA runtime. Package fixtures must distinguish provider-free requests from
CUDA component closures.

## Core CUDA state and lifetime oracles

1. Device enumeration returns a nonnegative count. Device `0` is used only
   after proving the count is positive. Negative and `count` device indices
   fail without changing the caller's CUDA current device.
2. CUDA resources accept exactly pinned-host, device, and managed spaces.
   Host space, invalid devices, invalid alignments, and overflow-sized
   requests fail transactionally. Zero-byte allocation returns null.
3. A successful allocation reports the requested space/device/alignment and
   is deallocated exactly once by its retained stable resource. Buffers may
   outlive the caller's temporary resource handle only where the API's owner
   contract retains that resource; tests otherwise keep resources alive.
4. Each execution context owns one nonblocking stream. Independent contexts
   have independent streams and preserve the caller's current device around
   creation, enqueue, query, wait, and destruction.
5. Host-to-device, device-to-host, and device-to-device `CopyBytes` use an
   explicit compatible context, preserve byte patterns, and remain
   asynchronous until the returned event is waited. Exact self-copy succeeds
   as a no-op; partial overlap, null nonzero pointers, inaccessible spaces,
   device mismatch, and size overflow fail before enqueue.
6. A fresh completion event can be queried without an implicit wait.
   `Query()` eventually reports complete after `Wait()`. `Wait()` orders only
   work preceding that event on its stream. Moving an event preserves its
   completion state. Destroying an un-waited event must not call a
   device-wide synchronization and must not retain user arrays.
7. Provider failures contain a stable ASC status code, provider name, and
   signed native code. Tests compare stable fields and avoid depending on
   driver-local prose.
8. A two-stream delay/progress probe demonstrates that waiting or destroying
   one event does not synchronize unrelated work on another stream. Timing is
   reported diagnostically, while independent marker/event state is the
   correctness oracle.

## Dense owner and transfer oracles

1. `CreateUninitialized` computes the exact unique exhaustive mapping span for
   ranks zero through eight, including zero extents and padded strides, and
   requests that exact byte count without touching device elements.
2. Device and managed views reject host dereference through `At`. Pinned-host
   views remain host-accessible. Existing `Create` remains host-only and
   value-initializing.
3. Host-to-device-to-host clone round trips preserve independently generated
   element bit patterns. Clone waits for its explicit copy event before
   publishing the owner. Allocation, enqueue, and wait failures publish no
   owner and leak no allocation.
4. Creating owners, obtaining views, and failed evaluation/algebra operations
   perform no implicit transfer, provider selection, or synchronization.

## Pointwise evaluator oracles

Expected results are generated on the CPU from integer-derived values that are
exactly representable in both `float` and `double`.

1. Terminal copy, scalar fill, unary negate, and one-level add/subtract/
   multiply are checked for both types, ranks zero through eight, zero extent,
   LayoutLeft, LayoutRight, and representative unique padded positive-stride
   views.
2. Logical comparison uses dimension-zero-fastest coordinates, independent of
   physical layout and padding. Padding sentinels remain unchanged.
3. Exact terminal self-evaluation is a successful no-op. All other
   destination/input overlap fails before enqueue.
4. Shape/rank mismatch, inaccessible placement, device mismatch, nonunique or
   negative stride, nested expression nodes, unsupported external adapters,
   unsupported scalar types, and ranks above eight are rejected without
   allocation, transfer, launch, or destination mutation.
5. Independent contexts evaluate independent buffers concurrently and return
   independently waitable events.

## Dense algebra and CPU parity oracles

Reference answers are computed in verifier-owned scalar loops using
`long double` accumulation for Gemv/Gemm, then rounded once to the tested
element type. Inputs use bounded deterministic integer-derived values.

1. Copy, Scal, and Axpy cover `float`/`double`, rank one/two, empty and
   degenerate shapes, left/right layout where permitted, padded positive
   strides, and exact self/alias rules.
2. Gemv covers none/transpose, rectangular and degenerate matrices, positive
   vector increments, padded column-major leading dimensions, and beta zero
   with destination bytes prefilled to a quiet-NaN pattern.
3. Gemm covers all four transpose pairs, rectangular/degenerate dimensions,
   padded leading dimensions, and beta zero with uninitialized/NaN
   destination storage.
4. LayoutRight/arbitrary-stride matrices, bad leading dimensions, negative or
   zero increments, narrowing overflow, output/input overlap, device mismatch,
   and shape mismatch fail before enqueue and leave output unchanged.
5. Tolerances are operation-aware:
   Copy is bit-exact; Scal/Axpy use
   `8 * epsilon * max(1, |reference|)`; Gemv/Gemm use
   `32 * epsilon * max(1, reduction_length) * max(1, |reference|)`.
   No tolerance is used to excuse NaN, infinity, shape, or placement errors.
6. The same inputs are evaluated through provider-free CPU loops and CUDA.
   A test earns `parity-tested` only after all output coordinates satisfy the
   declared tolerance on real hardware.

## Allocation and synchronization probes

1. A counting verifier resource wraps public allocation operations. Each owner
   allocation/deallocation is counted; evaluator and algebra calls must add
   zero ASC workspace allocations.
2. CUDA memory-use snapshots are diagnostic only. Correctness rests on the
   counting resource and successful Compute Sanitizer checks.
3. Operation tests call `Query()` before `Wait()` and retain all contexts,
   provider contexts, resources, owners, views, and scalar storage until event
   completion.
4. No test calls device-wide synchronization as part of an operation oracle.
   A final fixture teardown synchronization is permitted only after all
   per-event assertions and is separately identified.
5. Compute Sanitizer memcheck, racecheck, initcheck, and synccheck are four
   separately recorded runs; success in one does not imply another.

## Compile and package oracles

1. Both provider headers are self-contained in C++20 and exceptions-disabled
   translation units, expose no CUDA SDK types, and are ODR-safe across
   multiple translation units.
2. Context/resource/provider ownership properties are asserted:
   immutable execution contexts are copyable, completion events and
   `DenseCudaContext` are move-only, and `CudaMemoryResource` is neither
   copyable nor movable.
3. Useful compile negatives cover integral element dispatch, unsupported rank,
   unsupported nested/external expression dispatch, and attempts to copy
   move-only/noncopyable provider objects.
4. Build-tree, copied-build-tree, installed, relocated path-with-spaces,
   static/shared, provider-only, and subproject consumers link the exact
   imported targets.
5. Required closure is independently checked:
   `core_cuda -> core;core_cuda`,
   `dense_cuda -> core;expression;dense;core_cuda;dense_cuda`.
   Provider-free/no-component consumption of a CUDA-enabled installation must
   not execute CUDAToolkit discovery or gain a provider edge.

## Benchmark design

The verifier-owned benchmark has no unstable pass/fail speed threshold. It:

- records full hardware/toolchain/build metadata;
- uses warmup iterations;
- reports host-to-device, device-to-host, device-to-device, terminal
  evaluation, Axpy, Gemv, and Gemm separately;
- retains and waits on one event per measured operation;
- reports bytes or FLOPs, elapsed time, throughput, checksum, and allocation
  counts;
- excludes allocation and transfers from operation-only timings; and
- validates a checksum after timing so work cannot be optimized away.

## Acceptance and defect policy

A fixture failure is reported as a defect with the exact command, expected
oracle, observed state, and smallest implicated public API. Verification does
not repair production or shared integration. Unsupported environments are
reported as skips and never promoted to a stronger evidence label.
