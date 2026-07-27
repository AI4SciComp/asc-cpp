# API map

The unreleased ASCCpp `0.9.0` candidate has nine provider-free component
targets and six optional CUDA facets. Supported public declarations are
directly in `namespace asc`.

| Component | Umbrella | Direct ASC dependency | Current surface |
| --- | --- | --- | --- |
| `ASC::core` | `<asc/core.h>` | none | metadata, status/result, configuration values, I/O, host memory, serial execution |
| `ASC::utilities` | `<asc/utilities.h>` | `ASC::core` | command-line configuration and monotonic timing |
| `ASC::expression` | `<asc/expression.h>` | `ASC::core` | storage-neutral expression protocol and pointwise nodes |
| `ASC::random` | `<asc/random.h>` | `ASC::core` | Philox4x32-10 raw bits and scalar unit-uniform transforms |
| `ASC::dense` | `<asc/dense.h>` | `ASC::core`, `ASC::expression` | checked host storage/views, expression evaluation, reductions, serial reference linalg |
| `ASC::sparse` | `<asc/sparse.h>` | `ASC::core`, `ASC::expression` | canonical coordinate/CSR/CSC storage, conversion, evaluation, serial CSR SpMV |
| `ASC::random_dense` | `<asc/random/dense.h>` | `ASC::random`, `ASC::dense` | deterministic explicit-state dense `float`/`double` fill |
| `ASC::random_sparse` | `<asc/random/sparse.h>` | `ASC::random`, `ASC::sparse` | deterministic explicit-state exact-count coordinate generation |
| `ASC::cpp` | none | all six modules and both random facets | provider-free convenience aggregate |
| `ASC::core_cuda` | `<asc/core/providers/cuda.h>` | `ASC::core` | CUDA resources, execution contexts, copies, and events |
| `ASC::dense_cuda` | `<asc/dense/providers/cuda.h>` | `ASC::dense`, `ASC::core_cuda` | CUDA pointwise evaluation and selected dense algebra |
| `ASC::sparse_cuda` | `<asc/sparse/providers/cuda.h>` | `ASC::sparse`, `ASC::core_cuda` | canonical CSR clone, bounded sparse evaluation, and CSR SpMV |
| `ASC::random_cuda` | `<asc/random/providers/cuda.h>` | `ASC::random`, `ASC::core_cuda` | asynchronous Philox4x32 raw-word fill |
| `ASC::random_dense_cuda` | `<asc/random/providers/dense_cuda.h>` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | asynchronous dense Uniform01 fill |
| `ASC::random_sparse_cuda` | `<asc/random/providers/sparse_cuda.h>` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | asynchronous exact-count coordinate generation |

Components are requested explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_sparse)
target_link_libraries(my_target PRIVATE ASC::random_sparse)
```

CUDA facets are available only in a CUDA-enabled build. No-component lookup
requests the provider-free `cpp` aggregate.

## Core

`ASC::core` is the provider-free CPU foundation:

- `<asc/core/types.h>`: `index_t`, `extent_t`, `stride_t`, `nnz_t`, `rank_t`,
  `kDynamicExtent`, and checked integral operations;
- `<asc/core/extents.h>`: compile-time-rank mixed static/dynamic `Extents`;
- `<asc/core/status.h>`: stable `ErrorCode`, `Status`, and error names;
- `<asc/core/result.h>`: `Result<T>` value-or-error transport;
- `<asc/core/contracts.h>`: release-active and debug-only fatal contracts;
- `<asc/core/configuration.h>`: recursive `ConfigurationValue`,
  `ConfigurationSchema`, validated `Configuration`, origin metadata,
  `ConfigurationOrigins`, JSON Pointer lookup, origin-aware validation, and
  redaction;
- `<asc/core/io.h>`: byte source/sink contracts, exact I/O, move-only local
  files, bounded text, and portable little-endian scalar encoding;
- `<asc/core/memory.h>`: memory-space vocabulary, host resource, move-only
  `Buffer`, and byte views;
- `<asc/core/execution.h>`: backend/device/context/event vocabulary, explicit
  byte-copy dispatch, and queryable completion events; and
- `<asc/core/export.h>`: shared-library symbol visibility.

The [core module guide](modules/core.md) documents exact ownership, lifetime,
failure, cost, and execution contracts.

## Utilities

`ASC::utilities` provides two independent facilities:

- `<asc/utilities/command_line.h>` declares `CommandLineOption`,
  `CommandLineParser`, `CommandLineParseResult`, and deterministic help
  rendering. Parsing accepts schema-typed bool, signed/unsigned 64-bit,
  double, and UTF-8 string leaves and returns a completely validated core
  `Configuration` plus positional tokens. Parser creation requires an object
  root schema.
- `<asc/utilities/timer.h>` declares `TimerState` and `Timer`. The timer uses
  `std::chrono::steady_clock`, rejects invalid state transitions with status,
  and exposes elapsed, last, total, average, and sample-count queries without
  retaining a sample buffer.
- `<asc/utilities/export.h>` provides utilities shared-library visibility.

Parsing is transactional. Non-default parsed leaves carry
`ConfigurationOriginKind::kCommandLine` and token-index location. There is no
local-file, environment, response-file, storage, expression, random, provider,
or GPU API. Narrow text is UTF-8; Windows callers convert native wide argv
text explicitly before parsing.

Milestone 2 adds `ConfigurationOrigin::CommandLine`,
`ConfigurationOrigins`, and `ValidateConfigurationWithOrigins` to core so the
parser can preserve leaf origins without creating a reverse core-to-utilities
dependency. Exact JSON Pointer entries override inherited origin for the named
value and its descendants, schema defaults remain `kDefault`, and unknown
origin paths fail validation.

See the [utilities module guide](modules/utilities.md).

## Expression

`ASC::expression` is a header-only interface. Its readable and pointwise
surface is in `<asc/expression/expression.h>`:

- `ExpressionAdapter<T>` is the single non-intrusive customization point;
- `ReadableExpression` recognizes adapter-participating external and ASC
  expression types;
- opaque `AliasToken::FromIdentity` and checked
  `AliasToken::FromAddressSpan` values, `AliasTokensMayOverlap`, and
  `MayAlias` express conservative identity or byte-span overlap without
  exposing a stored pointer;
- `SparsityEffect` and operation-category metadata describe evaluation
  consequences without naming storage;
- arithmetic scalar terminals are rank zero and captured by value; and
- `MakeNegate`, `MakeAdd`, `MakeSubtract`, and `MakeMultiply` construct safe
  pointwise nodes.

`<asc/expression/writable.h>` adds storage-neutral placement and destination
customization through `ExpressionPlacementAdapter`,
`PlacedReadableExpression`, `WritableExpressionAdapter`,
`WritableExpression`, `ExpressionSpace`, `WritableExpressionShape`,
`WritableExpressionAlias`, and `WriteExpression`. It owns no storage or
evaluation and is specialized independently by dense, sparse, and external
view types.

Ranked compatibility is exact shape. Rank-zero scalar expansion is the only
broadcasting rule. Lvalues are held by non-owning reference; rvalues and nested
nodes are held by value. The module defines no array, destination, evaluator,
materialization, allocation, provider, transfer, or synchronization.

Node constructors are factory-only. Built-in scalar operations use ordinary
C++ operator conversions and `decltype` promotion. Signed integral results
must be representable; the module does not turn signed overflow into a status.
A borrowed lvalue must retain a shape/index contract compatible with the shape
snapshotted by the node.

See the [expression module guide](modules/expression.md).

## Random

`ASC::random` separates exact raw bits from exact scalar transforms:

- `<asc/random/engine.h>` declares `Philox4x32Counter`,
  `Philox4x32Key`, `Philox4x32Result`, `RandomStream`,
  `RandomSubsequence`, `RandomOffset`, the pure `Philox4x32_10` operation,
  direct block and position-to-word generation, and checked
  `AdvanceRandomOffset`;
- `<asc/random/distribution.h>` declares exact `Uniform01<float>` and
  `Uniform01<double>` transforms; and
- `<asc/random/export.h>` provides random shared-library visibility.

All state is caller-owned and explicit. There is no entropy source, mutable or
default engine, normal/rejection distribution, serialized state, provider, or
GPU implementation. The base umbrella remains storage-neutral and does not
include either storage facet.
The scalar transforms are available only when compile-time guards confirm IEC
60559 radix-two `float` with 24 significand bits and `double` with 53.

The [random module guide](modules/random.md) gives the exact lane/address and
bit-transform contracts. The
[Milestone 2 provenance record](development/asc-cpp-m2-independent-foundations/provenance-record.md)
identifies the primary Philox paper and clean-room boundary.

### Random storage facets

`<asc/random/dense.h>` declares `FillDenseUniform01`. It fills a mutable host
`DenseView<float, Rank>` or `DenseView<double, Rank>` in logical
dimension-zero-fastest order and returns the checked first unused word offset.
The operation is serial, layout/stride invariant, and allocation-free.

`<asc/random/sparse.h>` declares
`SparseUniform01Generation<Element, ExtentsType>` and
`GenerateSparseUniform01`. It uses separate explicit structure and value
address domains to produce a move-only canonical coordinate array with an
exact requested count and checked next offsets. The deterministic serial
reference algorithm selects the smallest project-defined `(priority, ordinal)`
pairs; it makes no mathematically uniform-subset claim.

Both facets support exactly unqualified `float` and `double`, require host
memory and an explicit serial context, validate offsets before mutation or
allocation, and perform no transfer, synchronization, provider dispatch, or
fallback.

## Dense

`ASC::dense` owns the serial CPU dense-storage boundary:

- `<asc/dense/layout.h>` declares checked rank-static `LayoutLeft`,
  `LayoutRight`, `LayoutStride`, and `DenseLayoutMapping`;
- `<asc/dense/view.h>` declares explicit-memory-space, trivially copyable
  `DenseView` descriptors, checked access, subviews, const propagation, and
  expression adaptation;
- `<asc/dense/array.h>` declares move-only `DenseArray` ownership over core
  `Extents` and `MemoryResource`, explicit uninitialized allocation in valid
  spaces, synchronous named clone, and transactional host discard-resize;
- `<asc/dense/evaluate.h>` declares caller-destination pointwise evaluation
  and deterministic sum/minimum/maximum reductions; and
- `<asc/dense/linalg.h>` declares serial `Copy`, `Scal`, `Axpy`, `Dot`,
  scaled `Nrm2`, `Gemv`, and `Gemm` for exact `float` or `double` views.

Evaluation and linalg require an explicit serial context and host-accessible
operands. They validate metadata, shape, memory, and forbidden overlap before
mutation. Successful computational paths allocate no storage/workspace and
hide no packing, transfer, synchronization, precision change, or fallback.
Diagnostic `Status` construction is not a no-heap guarantee. The
[dense module guide](modules/dense.md) documents the complete ownership,
alias, numerical, error, and cost contracts.

## CUDA provider facets

`<asc/core/providers/cuda.h>` adds:

- `CudaDeviceCount`;
- noncopyable/nonmovable `CudaMemoryResource` for pinned-host, device, and
  managed allocation;
- `CreateCudaExecutionContext` for one owned nonblocking stream; and
- `RecordCudaEvent`.

Common `CopyBytes` dispatches through the explicit context and returns a
move-only `CompletionEvent`; `Query` observes stream progress and `Wait` waits
for only that event.

`<asc/dense/providers/cuda.h>` adds move-only `DenseCudaContext`,
`CudaEvaluate`, `CudaCopy`, `CudaScal`, `CudaAxpy`, `CudaGemv`, and `CudaGemm`.
The pointwise subset accepts exact `float`/`double`, ranks zero through eight,
dense terminals, rank-zero scalars, one terminal negate, and one-level
add/subtract/multiply. Gemv/Gemm accept only checked cuBLAS-compatible
column-major mappings and no-transpose/transpose modes.

Provider signatures expose no CUDA SDK type. Successful operations return
completion events and perform no hidden ASC fallback or device-wide
synchronization. Destroying or replacing a live `DenseCudaContext` destroys
its cuBLAS handle; cuBLAS 12.9 documents that teardown as an implicit
device-wide synchronization.
Pageable-host transfers may stage or block inside the CUDA Runtime call; the
event is the completion contract, while pinned memory provides the stronger
host-asynchronous transfer expectation. `DenseArray::Clone` is explicitly
synchronous.

`<asc/sparse/providers/cuda.h>` adds move-only `SparseCudaContext`,
`CudaCloneCsr`, explicit `CudaCsrSpmvWorkspaceSize`, asynchronous
`CudaCsrSpmv`, and bounded `CudaEvaluate`. Unit-stride SpMV uses
`CUSPARSE_SPMV_CSR_ALG2` and explicit caller-owned workspace; positive
nonunit strides use the original deterministic project kernel and require
zero workspace.

The three random CUDA headers add `CudaFillPhilox4x32`,
`CudaFillDenseUniform01`, and `CudaGenerateSparseUniform01`. They preserve the
provider-free Philox word mapping, Uniform01 transforms, dense logical order,
and sparse priority/tie-breaking contract. All state, offsets, memory,
execution, and completion are explicit; no cuRAND state or fallback exists.

## Sparse

`ASC::sparse` owns the serial CPU sparse-storage boundary:

- `<asc/sparse/coordinate.h>` declares `SparseElement`, explicit duplicate and
  zero policies, fixed-capacity move-only `CoordinateBuilder`, move-only
  `CoordinateArray`, and trivially copyable mutable/const `CoordinateView`;
- `<asc/sparse/compressed.h>` declares canonical rank-two
  `CompressedSparseArray` and `CompressedSparseView`, CSR/CSC aliases, and the
  named coordinate-to-CSR/CSC, CSR/CSC-to-coordinate, and CSR/CSC cross-format
  conversions;
- `<asc/sparse/evaluate.h>` evaluates structure-preserving expressions only at
  an existing caller-provided canonical sparse structure; and
- `<asc/sparse/linalg.h>` declares deterministic serial `Spmv` for canonical
  CSR `float` and `double` matrices with storage-neutral rank-one operands.

Construction, conversion, evaluation, and SpMV validate the explicit serial
context, host memory, checked metadata, shapes, structure, and conservative
overlap before publication or mutation. Successful evaluation and SpMV
allocate no computational storage and perform no hidden packing,
densification, conversion, transfer, synchronization, provider dispatch, or
fallback. The [sparse module guide](modules/sparse.md) documents exact
ownership, lifetime, canonical-format, writable-adapter, error, numerical, and
cost contracts.

## Error and evidence boundary

Recoverable failures return `Status` or `Result<T>`; the public production
surface has no exception API. Headers are self-contained C++20 and contain no
optional SDK type or include.

Local CUDA evidence is classified separately as configure-tested,
compile-tested, runtime-tested, parity-tested, or skipped in Publication
Checkpoint B. Hosted compiler/GPU/package results are not claimed until they
actually run; the API surface itself does not imply provider, sanitizer,
portability, or performance evidence.
