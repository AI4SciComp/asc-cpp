# Milestone 6 Documentation and API Review

Status: Independent review complete; resolved production findings and remaining
integration conditions reported to the lead

Date: 2026-07-27

Role: documentation and API reviewer

Writable scope:

```text
docs/modules/core.md
docs/modules/dense.md
docs/development/asc-cpp-m6-gpu-core-dense/documentation-api-review.md
```

## Review boundary

This review is distinct from production implementation, independent
verification, lead integration, and portability/GPU/performance review. It
uses the frozen Milestone 6 contract, ownership ledger, accepted ADRs, actual
public headers, declared target/package behavior, primary provider
documentation, and independently syntax-checked examples.

Production, CMake, package, tests, architecture manifests, root documentation,
and every other specialist report are read-only to this role. Public API
defects are reported to the lead rather than documented around.

The review does not inspect or copy MdeCpp, the user-deleted asc-cpp CUDA
implementation or tests, third-party sample source, or historical CUDA
wrappers.

## Authoritative material read

- the tracked baseline `AGENTS.md` engineering directive;
- the complete frozen M6 milestone contract and ownership ledger;
- the architecture blueprint, dependency/capability manifests, backend matrix,
  testing strategy, and release roadmap;
- approved ADRs 0001--0004, 0007--0011, 0013, 0017, and 0018, which govern the
  module graph, package names, C++ policy, errors, metadata, memory/execution,
  ownership, expressions, dense semantics/providers, provenance, and release
  evidence;
- the current provider-neutral core and dense public headers and module guides;
- the current ASCCpp component/package definitions;
- the current official
  [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html);
- CMake 3.25 and current
  [`FindCUDAToolkit`](https://cmake.org/cmake/help/latest/module/FindCUDAToolkit.html)
  documentation;
- CUDA Runtime 12.9
  [memory, stream, event, and synchronization documentation](https://docs.nvidia.com/cuda/archive/12.9.0/cuda-runtime-api/index.html);
  and
- cuBLAS 12.9
  [handle, stream, pointer-mode, atomics, math-mode, thread-safety,
  reproducibility, GEMV, and GEMM documentation](https://docs.nvidia.com/cuda/archive/12.9.0/cublas/index.html).

The working tree retains the owner's deletion of `AGENTS.md`; the review read
the baseline-tracked directive without restoring the file.

The primary CMake documentation confirms that `CUDA::cudart` and
`CUDA::cublas` are the imported target names. The primary CUDA documentation
confirms nonblocking-stream semantics, event query/wait behavior, the matching
allocation/free families, and the fact that an `Async` copy does not promise
host-asynchronous behavior for every memory combination. The primary cuBLAS
documentation confirms per-handle stream binding, host scalar pointer-mode
lifetime, atomics and math-mode controls, the discouraged sharing of one
mutable handle between threads, and typed float/double GEMV/GEMM contracts.
It also documents that `cublasDestroy` implicitly performs a device-wide
synchronization.

## Findings

### M6-DOC-01: unchecked CUDA byte-span end calculation

Initial severity: release-blocking validation and memory-safety defect

Initial evidence: `src/core/cuda/cuda.cc` computed each overlap end as an
unchecked `std::uintptr_t` base plus the public `byte_count`. `MemoryView`
validity checks nullability and length but does not establish that the
half-open address span is representable. Unsigned wrap could therefore bypass
the overlap rejection and pass an invalid range to `cudaMemcpyAsync`, contrary
to the frozen pre-enqueue span-validation contract.

Required resolution: validate both address-end additions before overlap
comparison and provider pointer access, return a stable ASC overflow or memory
error, and retain a boundary regression.

Resolution: production added checked `std::uintptr_t` end validation for both
source and destination before overlap comparison and enqueue. The overlap
calculation now runs only after those additions are proven representable.
Verifier boundary coverage remains required.

Status: resolved in production; independent regression pending.

### M6-DOC-02: live CUDA copy can lose its completion handle

Initial severity: release-blocking asynchronous-lifetime defect

Initial evidence: the initial `CopyBytes` provider path called
`cudaMemcpyAsync` before `RecordState` created and recorded its CUDA event. If
event creation or recording failed, the API returned a failed `Result` even
though work could remain live on the stream, with no returned event through
which the caller could establish safe storage/resource lifetime.

Required resolution: create completion state before enqueue where possible and
provide a defined safe path for any post-enqueue record failure, so the public
API never loses track of work whose arguments must remain alive.

Resolution: production now creates the CUDA completion event before enqueue.
If the subsequent event record is the operation that fails, the implementation
performs a failure-only stream-scoped wait before returning the provider
failure, so no live work escapes without a completion handle. This exceptional
wait is documented as failure behavior and is never device-wide.

Status: resolved in production; independent failure-path review pending.

### M6-DOC-03: guide examples and host-storage wording

Initial severity: documentation correctness and style

Initial evidence: the minimal core example was not accepted by the repository's
ClangFormat 19 configuration, described an explicit copy as performing no
transfer, and the dense guide described initialized creation as accepting
“host-accessible” storage even though `DenseArray::Create` accepts exactly
`MemorySpace::kHost`.

Resolution: the core example is formatted, its copy is described as an
explicit byte transfer with no allocation, packing, or hidden synchronization,
and the dense guide names ordinary `kHost` storage. The guide also prints both
exact `CreateUninitialized` overloads.

Status: resolved in documentation; strict GCC/Clang syntax and ClangFormat
checks pass.

## Public surface under review

The frozen surface is:

```text
include/asc/core/execution.h
include/asc/core/memory.h
include/asc/core/providers/cuda.h
include/asc/core/providers/cuda_export.h
include/asc/dense/array.h
include/asc/dense/providers/cuda.h
include/asc/dense/providers/cuda_export.h

asc_core_cuda  / ASC::core_cuda  -> ASC::core
asc_dense_cuda / ASC::dense_cuda -> ASC::dense;ASC::core_cuda
```

The integrated provider declarations are:

```text
CudaDeviceCount() -> Result<std::int32_t>

CudaMemoryResource::Create(Device, MemorySpace)
  -> Result<std::unique_ptr<CudaMemoryResource>>

CreateCudaExecutionContext(
    Device, Determinism = Determinism::kDeterministic)
  -> Result<ExecutionContext>

RecordCudaEvent(const ExecutionContext&)
  -> Result<CompletionEvent>

CompletionEvent::Query() -> Result<bool>

DenseArray<Element, ExtentsType>::CreateUninitialized(
    const ExtentsType&, MemoryResource&, LayoutLeft = {})
DenseArray<Element, ExtentsType>::CreateUninitialized(
    const ExtentsType&, MemoryResource&, LayoutRight)
  -> Result<DenseArray>

DenseCudaContext::Create(ExecutionContext)
  -> Result<DenseCudaContext>

DenseCudaContext::execution_context()
  -> const ExecutionContext& noexcept

CudaEvaluate(context, destination, expression)
CudaCopy(context, source, destination)
CudaScal(context, alpha, destination)
CudaAxpy(context, alpha, source, destination)
CudaGemv(context, operation, alpha, matrix, input, beta, output)
CudaGemm(context, left_operation, right_operation,
         alpha, left, right, beta, output)
  -> Result<CompletionEvent>
```

All supported declarations remain directly in flat `namespace asc`. Provider
headers expose no CUDA/cuBLAS SDK type. `CudaMemoryResource` is neither
copyable nor movable; `ExecutionContext` is a copyable immutable owner of
shared provider execution state; `CompletionEvent` and `DenseCudaContext` are
move-only.

Strict GCC 11 and Clang 19 host compilation with C++20, warnings as errors,
`-pedantic-errors`, and `-fno-exceptions` independently parsed each provider
header without any CUDA include path. Compile-time traits confirmed the
ownership properties above.

## API and contract conclusions

### Provider isolation and package closure

The public headers are SDK-neutral. `ASC::core_cuda` has one direct ASC edge to
`ASC::core` and a private implementation edge to `CUDA::cudart`.
`ASC::dense_cuda` has direct ASC edges only to `ASC::dense` and
`ASC::core_cuda`, with a private implementation edge to `CUDA::cublas`.
Neither CUDA facet changes the provider-free `ASC::cpp` closure.

The component contract must discover CUDAToolkit only when `core_cuda` or
`dense_cuda` is requested. A provider-free component request must remain usable
from the same installation without a CUDA toolkit. Package, relocation, and
isolated-consumer execution remain lead integration evidence rather than a
claim of this review.

### Ownership, lifetime, and synchronization

CUDA memory resources create and release provider allocations; each `Buffer`
owns one allocation while borrowing its resource, and each view borrows
storage. An execution context is a copyable immutable handle sharing provider
stream state. A completion event is move-only and retains
completion/execution state, but not user buffers, views, dense expressions, or
a `DenseCudaContext`.

Consequently, every operation argument and resource remains alive until its
event completes. Event destruction does not synchronize. `Query()` is the
nonwaiting completion poll and `Wait()` is event-local. The successful CUDA
path has no device-wide synchronization. A documented failure-only
stream-scoped wait is acceptable when an enqueue succeeded but publication of
its completion event failed.

One `DenseCudaContext` owns provider algebra state bound to its explicit core
context and is not concurrently usable. Independent contexts may run
concurrently when mutable storage does not overlap. The integrated
implementation stores the execution context independently of its provider
state: after a move, `execution_context()` remains defined, while operations
on the moved-from provider state return `kInvalidState`.

Destroying or replacing a live `DenseCudaContext` destroys its cuBLAS handle
and therefore device-wide synchronizes under the cuBLAS 12.9 contract. This
teardown cost is distinct from the successful operation path, which remains
asynchronous and event-tracked.

### Memory and copy behavior

`CudaMemoryResource::Create` accepts only pinned-host, device, or managed
spaces for one explicit CUDA device. It is noncopyable and nonmovable so a
buffer's borrowed resource address remains stable. Allocation and deallocation
use matching provider families. Ordinary device/managed free may synchronize;
it is not stream-ordered.

`CopyBytes` validates nullability, byte ranges, device access, and overlap
before enqueue. Exact self-copy is a no-op; partial CUDA overlap is rejected.
The completion event proves operation completion, but the word `Async` in the
CUDA provider does not guarantee a nonblocking host submission for every
memory combination. Pinned host storage is the documented choice when
host-asynchronous transfer behavior matters.

### Dense storage, expressions, and algebra

`DenseArray::CreateUninitialized` is the explicit uninitialized allocation
path for every valid core space, including non-host storage. `Clone` is the
explicit cross-space copy and waits its copy event before publishing the
result. Host element access remains restricted to ordinary host storage;
device data is not implicitly mirrored or migrated.

`CudaEvaluate` accepts only rank 0--8, `float` or `double`, device-resident
dense views, and the frozen shallow storage-neutral expression set. It
preserves logical-coordinate semantics for left, right, and padded unique
non-negative-stride mappings, allocates no ASC computational storage, and does
not materialize or pack. Unsupported nesting fails rather than falling back.

CUDA level-1, GEMV, and GEMM operations use explicit context-first entry
points. Exact `CudaCopy` self-assignment is a no-op, and `CudaAxpy` accepts its
mathematically valid exact same-index in-place form; unsafe partial overlap is
rejected. GEMV/GEMM output may not overlap any input. Their accepted scalar,
rank, layout, transpose, shape, alias, and overlap rules otherwise match the
frozen contract. No implicit host/device transfer, provider fallback, packing,
workspace, default device, or default stream is documented. Pointwise,
basic-kernel, and cuBLAS paths create completion state before enqueue;
event-record failure performs a failure-only stream drain so live work cannot
escape without a completion handle.

### Error and numerical policy

Recoverable validation, allocation, provider, and numerical failures use
`Status` or `Result<T>` with a stable ASC error category. Provider name and
native provider code are preserved for diagnostics; message text is not stable
API. Public production interfaces do not expose exceptions.

Determinism is explicit in the execution context. The CUDA dense provider owns
cuBLAS handle state, uses host scalar pointer mode for the synchronous
submission lifetime, and must apply the frozen deterministic versus
performance-mode controls. Provider parity is tolerance-based and must name
operation, layout, scalar, and tolerance coverage. This review does not infer
runtime or parity evidence from header compilation.

## Documentation changes

- `docs/modules/core.md` now describes the optional core CUDA target/component,
  package isolation, resource/context/event/copy APIs, explicit ownership and
  synchronization, the pageable-host caveat, provider failure behavior, and a
  CUDA copy example. The host example distinguishes its explicit byte transfer
  from hidden work and follows repository formatting.
- `docs/modules/dense.md` now describes uninitialized device allocation,
  explicit clone behavior, the dense CUDA context and supported evaluator and
  algebra subset, layout/alias/lifetime rules, provider evidence boundaries,
  the exact uninitialized-creation overloads, and a CUDA dense example.
- Both guides identify the cumulative M6 scope, deferred work, and clean-room
  provenance without representing third-party provider material as project
  code.

## Independent validation

The following checks were run from the repository root:

```text
g++ -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion \
  -Wsign-conversion -Werror -fno-exceptions -Iinclude \
  -x c++ -fsyntax-only <core CUDA provider-header probe>

clang++-19 -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion \
  -Wsign-conversion -Werror -fno-exceptions -Iinclude \
  -x c++ -fsyntax-only <core CUDA provider-header probe>

g++ -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion \
  -Wsign-conversion -Werror -fno-exceptions -Iinclude \
  -x c++ -fsyntax-only <dense CUDA provider-header probe>

clang++-19 -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion \
  -Wsign-conversion -Werror -fno-exceptions -Iinclude \
  -x c++ -fsyntax-only <dense CUDA provider-header probe>

g++|clang++-19 <same strict flags> \
  -x c++ -fsyntax-only <each of four extracted C++ guide examples>

clang-format-19 --dry-run --Werror --style=file \
  --assume-filename=example.cc <each extracted C++ guide example>

rg <CUDA/cuBLAS SDK include and public-type patterns> \
  include/asc/core include/asc/dense
```

Result: pass for both compilers and both headers, with no CUDA SDK include
path. Compile-time ownership traits passed in the same probes. The SDK scan
found no provider header/type in a provider-neutral header and no CUDA/cuBLAS
SDK type in either public provider header.

All four C++ examples in the two module guides were extracted and passed
ClangFormat 19 plus the same strict GCC 11.4 and Clang 19.0 syntax modes. These
are public-header syntax checks, not installed-link or GPU runtime evidence.

## Required lead integration follow-ups

- retain boundary regressions for checked CUDA source/destination span ends;
- verify core and dense post-enqueue event-record failure paths cannot release
  live work without a returned completion handle;
- build and run the documented examples as installed component consumers;
- validate provider-free and CUDA component closures, relocation, and
  separately supplied CUDAToolkit discovery; and
- publish GPU evidence only with the exact
  **configure-tested**, **compile-tested**, **runtime-tested**,
  **parity-tested**, or **skipped** classification.

At review close, no unresolved defect is present in the inspected provider
header signatures. The source-dependent and installed-consumer checks above
are mandatory integration conditions, not documentation assumptions. A
moved-from dense provider context retains an inspectable execution context but
rejects provider operations, and cuBLAS handle destruction remains an explicit
device-wide synchronization/performance boundary.
