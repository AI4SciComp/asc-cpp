# Independent scientific-computing architecture analysis

**Role:** Designer C, independent scientific-computing architect

**Date:** 2026-07-26

**Scope:** architecture only; no production implementation

**Repository state inspected:** `asc-cpp`
`33b261ea33616a6395c4ad3b20646093103344f7`

**Build infrastructure inspected:** `asc-cmake` `v0.1.0`,
`8a7dcbad3a97267cce59810aff24de800a3497a7`

## 1. Executive conclusion

The fixed six-module architecture is viable and should be approved as the
starting graph, subject to the decisions and falsification gates in this
report. Its most important property is not the six names; it is the set of
ceilings:

- `core` owns vocabulary, memory, execution, errors, configuration values, and
  storage-independent I/O, but no numerical storage or provider policy;
- `expression` describes computation without choosing storage, allocating,
  transferring, synchronizing, or selecting a provider;
- `dense` and `sparse` own different storage invariants, evaluators, and
  linear-algebra surfaces and never depend on each other;
- `random` owns a core-only reproducibility contract, while separately
  consumable random-owned facets integrate with dense and sparse storage;
- CPU reference behavior is always available, and every optimized or GPU
  provider is an optional compiled facet;
- memory location, execution context, random state, workspace, packing,
  conversion, transfer, and synchronization are explicit.

The graph will fail in practice if the installed package loads every optional
provider, if an expression node performs evaluation, if sparse operations use a
dense container for metadata, or if a storage target acquires a random
dependency. These boundaries need compile and package tests, not documentation
alone.

My recommended initial accelerator is CUDA, but only as a sequence of optional
facets and only after a real runner is available. Toolkit discovery or a
successful compile is not runtime evidence. No present `asc-cpp` worktree code
or test establishes a GPU capability.

## 2. Evidence discipline

This report uses three labels:

- **Sourced fact — local:** observed in the specified repository or released
  build-infrastructure commit.
- **Sourced fact — external:** stated in a standards paper or official project
  or provider document linked directly.
- **Recommendation/inference:** my architectural conclusion from the facts.

External systems were compared at the level of contracts and concepts. No
external source code was inspected or proposed for copying.

### 2.1 Local repository facts

**Sourced fact — local.** The `asc-cpp` worktree is on `main` at
`33b261ea33616a6395c4ad3b20646093103344f7` and contains exactly 208 tracked
deletions. `AGENTS.md`, `generator.md`, all production source, root/build CMake
files, examples, and tests are among those deletions. There is therefore no
live repository-local generator or agent instruction in the worktree.

**Sourced fact — local.** The files that remain live are the license, README,
and historical documentation for an earlier five-component architecture:
`core`, `utilities`, `array`, `linalg`, and `random`. They are useful as
historical requirements and test ideas, but they do not satisfy the owner's
six-module restart.

**Sourced fact — historical prior art only.** At the inspected `HEAD`, the
deleted build defined `ASC::core`, `ASC::utilities`, `ASC::array`,
`ASC::linalg`, `ASC::random`, and `ASC::cpp`. The deleted Random target linked
`asc_array` publicly. The deleted Core target publicly linked CUDA runtime,
cuBLAS, and cuSPARSE when CUDA was enabled. The deleted package config also
discovered CUDA and OpenMP based on package-wide build switches. These are
direct counterexamples to the new random-base and provider-isolation
requirements.

**Sourced fact — historical prior art only.** The deleted code also contains
useful concepts that may be independently re-specified: a provider-neutral
status/result pair, explicit execution context, memory resource, non-owning
strided view, provider capability query, and Philox4x32-10 raw-bit engine.
These files are not live implementation and must not be represented as current
capability.

**Sourced fact — local.** `asc-cmake` is clean at released tag `v0.1.0`. It
requires CMake 3.25 and provides these stable functions:

- `asc_target_enable_cxx20`;
- `asc_target_enable_warnings`;
- `asc_target_enable_sanitizers`;
- `asc_register_test`;
- `asc_install_package`;
- `asc_add_project_options`.

Its effects are target-local. `asc_install_package` generates one build-tree
and install-tree export/config pair per call, requires one package/export set
and a list of targets, and leaves dependency discovery and component semantics
to the consumer's config template. It does not provide a multi-export,
component-appendix abstraction.

**Recommendation/inference.** Use the released helpers for strict C++20,
warnings, sanitizers, and test registration. For the component-aware ASCCpp
package, either:

1. use standard CMake `install(EXPORT)`,
   `configure_package_config_file()`, and per-component export files; or
2. first add and release a bounded, generally useful multi-component feature
   in `asc-cmake`.

Do not imitate a missing helper privately. With the current helper, one export
can safely cover the provider-free base closure, but optional provider exports
must not be unconditionally included because that would make their imported
SDK targets mandatory.

### 2.2 External architectural facts

**Sourced fact — external.** The WG21 `mdspan` design separates extents,
layout mapping, accessor, and a multidimensional view. It explicitly argues
that views are more fundamental than containers, particularly because
allocation/deallocation are synchronization points in parallel programs.
[P0009R18](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p0009r18.html)
is a C++23 library design, so ASC cannot expose `std::mdspan` while targeting
C++20; its factorization is nevertheless relevant architectural evidence.

**Sourced fact — external.** Kokkos distinguishes execution spaces, memory
spaces, layouts, and memory traits. `Kokkos::View` carries multidimensional
layout and memory-space information, while accessibility is a relation between
execution and memory spaces. Kokkos also documents that dispatch may be
asynchronous and that a fence is required for completion-sensitive operations
such as timing.
[Kokkos programming model](https://kokkos.org/kokkos-core-wiki/ProgrammingGuide/ProgrammingModel.html),
[View](https://kokkos.org/kokkos-core-wiki/API/core/view/view.html),
[space accessibility](https://kokkos.org/kokkos-core-wiki/API/core/SpaceAccessibility.html),
[machine model](https://kokkos.org/kokkos-core-wiki/ProgrammingGuide/Machine-Model.html).

**Sourced fact — external.** Ginkgo executors identify data placement and the
place where operations execute, and expose allocation and explicit cross-
executor copies. This supports an explicit context/resource boundary rather
than process-global backend selection.
[Ginkgo executors](https://ginkgo-project.github.io/ginkgo-generated-documentation/doc/main/group__Executor.html).

**Sourced fact — external.** Eigen's lazy evaluation may insert temporaries
for aliasing or cost reasons, and exposes `eval()` and `noalias()` controls.
That is strong evidence that aliasing and temporary policy are inseparable
from expression evaluation.
[Eigen lazy evaluation and aliasing](https://eigen.tuxfamily.org/dox/TopicLazyEvaluation.html).

**Sourced fact — external.** cuBLAS uses explicit handles, streams, result
status, and host/device scalar-pointer modes. Its documented reproducibility
scope depends on toolkit version, GPU architecture, stream use, and workspace.
cuSOLVER associates work with a stream and many operations require queried
workspace. cuSPARSE exposes explicit formats, layouts, index types, algorithms,
and work buffers; its storage documentation warns that duplicate or unsorted
indices may invalidate correctness for some operations.
[cuBLAS](https://docs.nvidia.com/cuda/cublas/),
[cuSOLVER](https://docs.nvidia.com/cuda/cusolver/),
[cuSPARSE generic API](https://docs.nvidia.com/cuda/cusparse/generic-api/generic-api.html),
[cuSPARSE storage formats](https://docs.nvidia.com/cuda/cusparse/storage-formats.html).

**Sourced fact — external.** Counter-based generators make a random result a
function of a key and counter and were designed for parallel subdivision.
[Salmon et al., “Parallel Random Numbers: As Easy as 1, 2, 3”](https://www.thesalmons.org/john/random123/papers/random123sc11.pdf).
Kokkos documents that acquiring a CUDA random-pool state uses atomics and is
non-deterministic. cuRAND exposes explicit seed, subsequence, offset, and
ordering, and documents that some performant orderings may change across
releases.
[Kokkos Random](https://kokkos.org/kokkos-core-wiki/API/algorithms/Random-Number.html),
[cuRAND host API](https://docs.nvidia.com/cuda/curand/host-api-overview.html),
[cuRAND device API](https://docs.nvidia.com/cuda/curand/device-api-overview.html).

**Sourced fact — external.** CMake package components are package-defined;
unsatisfied required components make the package not found. Installed imported
targets carry usage requirements, and relocatable configs should be generated
with `configure_package_config_file()`.
[find_package](https://cmake.org/cmake/help/latest/command/find_package.html),
[cmake-packages(7)](https://cmake.org/cmake/help/latest/manual/cmake-packages.7.html),
[CMakePackageConfigHelpers](https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html).

**Sourced fact — external.** The current Google C++ Style Guide targets C++20,
requires self-contained guarded `.h` files and direct includes, advises against
C++ exceptions, prefers explicit ownership, and disallows C++20 modules.
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

## 3. Proposed module and target graph

### 3.1 Base modules and integration facets

**Recommendation/inference.** Freeze this production graph:

```text
ASC::core
├── ASC::utilities
├── ASC::expression
├── ASC::random
├── ASC::dense ───────> ASC::expression
└── ASC::sparse ──────> ASC::expression

ASC::random + ASC::dense
  └── ASC::random_dense       (owned by random)

ASC::random + ASC::sparse
  └── ASC::random_sparse      (owned by random)

ASC::cpp
  └── six base targets + random_dense + random_sparse
```

Exact direct dependencies:

| Target | Owner | Direct ASC dependencies |
| --- | --- | --- |
| `ASC::core` | core | none |
| `ASC::utilities` | utilities | `ASC::core` |
| `ASC::expression` | expression | `ASC::core` |
| `ASC::dense` | dense | `ASC::core`, `ASC::expression` |
| `ASC::sparse` | sparse | `ASC::core`, `ASC::expression` |
| `ASC::random` | random | `ASC::core` |
| `ASC::random_dense` | random | `ASC::random`, `ASC::dense` |
| `ASC::random_sparse` | random | `ASC::random`, `ASC::sparse` |
| `ASC::cpp` | package convenience | all six bases plus both random storage facets |

`ASC::cpp` is deliberately broad and never a prerequisite of a narrow target.
It includes both storage-generation facets because requesting the umbrella is
an explicit request for the complete provider-free CPU surface. It includes no
optional provider.

Forbidden edges include all of these:

```text
core       -> utilities | expression | dense | sparse | random
utilities  -> expression | dense | sparse | random
expression -> utilities | dense | sparse | random
dense      -> utilities | sparse | random
sparse     -> utilities | dense | random
random     -> utilities | expression | dense | sparse
```

### 3.2 Initial CUDA facets

**Recommendation/inference.** If CUDA is approved, add these facets without
creating a seventh module:

```text
ASC::core_cuda
  -> ASC::core
  -> CUDA runtime privately or link-only as required by static linkage

ASC::dense_cuda
  -> ASC::dense
  -> ASC::core_cuda
  -> cuBLAS/cuSOLVER in private implementation

ASC::sparse_cuda
  -> ASC::sparse
  -> ASC::core_cuda
  -> cuSPARSE in private implementation

ASC::random_cuda
  -> ASC::random
  -> ASC::core_cuda
  -> approved generator implementation/provider

ASC::random_dense_cuda
  -> ASC::random_dense
  -> ASC::random_cuda
  -> ASC::core_cuda

ASC::random_sparse_cuda
  -> ASC::random_sparse
  -> ASC::random_cuda
  -> ASC::core_cuda
```

The last two targets must not depend on `dense_cuda` or `sparse_cuda` merely
because those targets contain algebra providers. They need storage descriptors
and CUDA execution, not dense or sparse algebra. No CUDA facet is part of
`ASC::cpp`.

Future optimized CPU targets should use provider names, for example
`ASC::dense_onemkl`, rather than a misleading generic `ASC::dense_optimized`.
No optimized CPU provider is selected in this report because adding one
requires a dependency, license, ABI, supported-operation, and CI decision.

### 3.3 Namespace and headers

**Recommendation/inference.**

- All public names live directly in `namespace asc`.
- Module identity appears in headers and targets, not `asc::dense` or
  `asc::sparse`.
- Public headers live under `include/asc/<module>/`; an optional narrow umbrella
  such as `<asc/dense.h>` may re-export only that module's approved surface.
- Public provider-neutral headers never include CUDA, BLAS, LAPACK, MKL,
  Eigen, or another provider SDK header.
- Provider-specific public headers, if unavoidable, live below the owning
  module and are reachable only through the provider facet. Prefer opaque ASC
  options and compiled factories even there.
- Internal namespaces include `internal`, and internal types never appear in a
  public signature.
- Public headers end in `.h`, compiled implementation in `.cc`, and approved
  CUDA implementation in the toolchain-required extension.
- Every public header has a full-path include guard, includes what it uses, and
  compiles by itself as C++20 with extensions disabled.

## 4. Core: memory, execution, state, and capability

### 4.1 Vocabulary

**Recommendation/inference.** `core` should define:

- signed 64-bit logical `Index` and `Extent` types;
- `Rank` represented by `std::size_t`;
- checked conversions between logical sizes, provider integer widths, element
  counts, byte counts, and `std::size_t`;
- shape/extents descriptors with checked product and required-span operations;
- `Backend`, `Device`, `MemorySpace`, `MemoryAccess`, and capability vocabulary;
- a memory-resource handle and move-only allocation/buffer owner;
- explicit copy operations;
- an execution context and completion event;
- `Status` and `Result<T>`;
- configuration value/schema primitives;
- storage-independent byte/text source and sink contracts.

Logical indices and extents are signed so negative input can be diagnosed before
conversion. Valid extents are non-negative. Allocation sizes remain
`std::size_t`. Provider adapters must checked-cast to 32- or 64-bit provider
arguments. Zero rank, zero extent, and zero-byte allocation are specified
states, not incidental special cases.

### 4.2 Memory

**Recommendation/inference.**

- A memory resource serves exactly one explicit memory space and device.
- The owning buffer retains the resource required for deallocation and records
  byte size and alignment.
- The base buffer should own raw storage, not multidimensional semantics.
  Dense and sparse modules own construction, shape, layout, and index metadata.
- Host, pinned-host, device, and managed memory are distinct. “Managed” does
  not authorize implicit prefetch, migration, or host access claims.
- Allocation never chooses a device implicitly.
- Resize is not an operation on the core buffer; the storage owner performs an
  explicit allocate/copy/release transaction.
- No public buffer has an implicit raw-pointer conversion.
- A host dereference API must reject non-host-accessible storage.

The core resource interface may use type erasure internally, but common headers
must expose no provider handle. Provider resource factories belong to
`core_cuda` or another provider facet.

### 4.3 Execution

**Recommendation/inference.** An execution context is explicit on every
operation that may allocate workspace, dispatch asynchronously, use a provider,
or access non-host memory. It records:

- backend and device;
- queue/stream identity through provider-neutral state;
- determinism requirement;
- synchronization policy;
- compatible memory spaces;
- workspace policy;
- provider capability table.

The context is created through a named provider factory. There is no
process-global current device, mutable provider registry, or initialization-
order-dependent registration. The base library always supplies an explicit
synchronous serial context.

Provider selection rules:

1. The requested context is authoritative.
2. Unsupported operation/type/layout/memory combinations return
   `kUnsupported`.
3. An unavailable compiled provider returns `kUnavailable`.
4. No provider silently transfers, packs, changes precision, densifies, or
   falls back.
5. `AUTO` is omitted from v1. If later added, its ordered selection and chosen
   result must be queryable and deterministic.

### 4.4 Asynchrony and lifetime

**Recommendation/inference.**

- A synchronous operation returns `Status` or `Result<T>`.
- An asynchronous operation is named as such and returns a move-only event or
  operation handle.
- The event owns provider completion state and any provider-owned workspace it
  needs, but not user arrays.
- Input/output storage, execution context state, external stream, and
  caller-supplied workspace must remain valid until documented completion.
- `Wait()` and `IsReady()` are explicit. Destruction must not silently perform
  a device-wide synchronization.
- Timing an asynchronous operation requires an event or explicit context
  synchronization; a host wall clock around enqueue is not an execution-time
  measurement.

cuBLAS and cuSOLVER's handle/stream contracts and Kokkos's documented fencing
behavior are evidence for making these lifetimes visible rather than hiding
them behind a backend enum.

## 5. Dense multidimensional ownership and views

### 5.1 Descriptor family

**Recommendation/inference.** Use one template family factored into:

```text
Extents -> LayoutMapping -> Accessor -> DenseView
                                  \-> DenseArray owner
```

The first release should use compile-time rank with any extent optionally
dynamic at run time. That supports arbitrary rank while keeping descriptors
small and GPU-specializable. A fully runtime-rank owner is deferred until an
identified downstream use case justifies its complexity and dispatch cost.
ASC must implement or approve a C++20-compatible equivalent; it must not expose
C++23 `std::mdspan`.

Required mappings are:

- left/column-major contiguous;
- right/row-major contiguous;
- explicit stride.

Mappings report extents, strides, logical size, required span, uniqueness,
exhaustiveness, and contiguity. Every product and offset calculation is checked
on construction. A mutable view requires a mapping proven unique; a const view
may represent repeated/broadcast addresses only when the API explicitly
permits it.

### 5.2 Ownership and lifetime

**Recommendation/inference.**

- `DenseArray` owns one allocation and one unique/exhaustive owning mapping.
- It is move-only in the first release. Deep copy is a named operation with
  explicit destination resource and execution context.
- `DenseView` is trivially copyable where practical, never owns storage, and
  records element mutability, mapping, accessible span, and memory space.
- Constness of an owner yields a const-element view; constness of the view
  handle itself does not grant or remove mutation authority.
- Slices/subviews are views and never extend lifetime.
- Resize invalidates all views. Reshape is zero-allocation only when element
  count and mapping permit it; otherwise it fails rather than reallocating.
- Device-resident views are descriptors. Common host code cannot dereference
  them. Provider-specific execution consumes normalized descriptors.

This follows the separation supported by the `mdspan` design while deliberately
choosing exclusive rather than Kokkos-style reference-counted array ownership.
Shared ownership should be added only for a demonstrated domain need.

### 5.3 Evaluation and dense algebra

**Recommendation/inference.**

- `dense` evaluates storage-neutral pointwise expressions into an explicit
  dense destination.
- The base target includes a no-provider serial reference path for its claimed
  operations.
- Elementwise fusion ends at reductions, algebra descriptors, incompatible
  traversal, or an explicit materialization point.
- Exact same-index aliasing may be safe. Other overlap is conservatively
  rejected unless the caller supplies an approved workspace/temporary policy.
- No evaluator allocates an undisclosed temporary. An API that permits a
  temporary takes a workspace or an explicit allocation policy and reports the
  requirement before modifying the destination.
- Packing row-major, padded, or noncontiguous inputs for a provider is an
  explicit algorithm option with queryable workspace. Otherwise the provider
  rejects the layout.

Initial base dense algebra should be narrow: copy, scale, axpy, dot/norm,
matrix-vector, matrix-matrix, and reductions for `float` and `double`. Broad
factorization and solvers wait for an approved provider and residual/backward-
error test design. Generic elementwise types and provider-supported algebra
types are separate capability claims.

## 6. Sparse storage, evaluation, and algebra

### 6.1 Formats and invariants

**Recommendation/inference.**

- Coordinate storage with compile-time rank is the general-rank format.
- CSR and CSC are the first rank-two compressed formats.
- BSR, SELL, and other provider-oriented formats are later capabilities, not
  aliases for CSR.
- Internal ASC indices are zero-based.
- Sparse index type is explicitly 32- or 64-bit signed; 64-bit is the default.
- Provider downcasts and index-base conversion are checked and explicit.

Construction uses a builder/finalize model:

1. A builder may accept unsorted entries and duplicates.
2. Finalization requires explicit duplicate and explicit-zero policies.
3. A finalized object is sorted and unique in its canonical traversal order.
4. Structural arrays are immutable after finalization; values may be mutable.
5. Structural edits create a builder or a new sparse object and invalidate
   views only through an explicit owner operation.

Do not choose silent defaults for duplicate combination or explicit-zero
removal. `sum`, `reject`, `keep zero`, and `drop zero` have materially different
floating-point, NaN, reproducibility, and structural semantics. cuSPARSE's
documented assumptions reinforce the need to validate before provider calls.

Empty compressed objects still have a valid outer-offset array of
`outer_extent + 1` zeros. Conversion among COO, CSR, and CSC is a named
allocation-bearing operation with stated complexity, execution context, and
workspace.

### 6.2 Sparse expressions

**Recommendation/inference.** Every expression operation has one sparsity
effect:

- structure-preserving;
- structure-filtering;
- structure-union/intersection;
- value-dependent;
- densifying;
- destination-required.

The sparse evaluator accepts structure-preserving operations directly.
Structure-changing operations require an explicit sparse builder/destination
and workspace. Densifying expressions are rejected by sparse evaluation and
require a named conversion into a caller-provided dense destination. There is
no “helpful” dense fallback.

### 6.3 Sparse algebra and dense operands

**Recommendation/inference.** Sparse owns SpMV, SpMM, sparse reductions, and
approved sparse solvers. An operation such as SpMV accepts expression-level
readable/writable rank-one operands. A concrete dense view satisfies those
protocols when the user includes both modules, but `sparse` never includes or
links `dense`.

The caller provides the result destination. The operation validates:

- rank and dimensions;
- index base, width, sorted/unique invariants, and format;
- scalar and compute type;
- destination uniqueness and overlap;
- memory-space accessibility;
- provider support;
- workspace and synchronization.

Sparse base should initially provide a reference SpMV path over canonical CSR
and a small COO/CSR correctness path. SpMM, triangular solve, and iterative or
direct solvers should be added only with separate capability and numerical
contracts.

## 7. Storage-neutral expressions

### 7.1 Protocol

**Recommendation/inference.** `expression` should define a small set of
non-intrusive customization-point objects, backed by an internal
`tag_invoke`-style mechanism or an equivalently constrained project-owned
customization protocol. It must not require inheritance or CRTP. A type outside
ASC must be able to participate without deriving from an ASC class.

Justified public concepts are limited to contracts that materially improve
diagnostics:

- expression participation;
- readable scalar/ranked operand;
- writable destination;
- shape/rank query;
- element/value type;
- placement/accessibility;
- alias/overlap description.

Semantic requirements that C++ cannot prove—purity, lifetime, sparsity effect,
and side effects—belong in explicit metadata and tests, not misleading
concepts.

### 7.2 Nodes and lifetime

**Recommendation/inference.**

- Scalar operands are captured by value.
- Lvalue expression operands are captured by a documented non-owning holder.
- Rvalue operands are owned by value.
- The factory layer must make it impossible to store a direct reference to an
  rvalue expression node.
- A view remains non-owning even when captured by value; its storage owner must
  still outlive evaluation.
- Nodes carry shape, evaluation category, sparsity effect, and alias metadata,
  but no provider handle or destination.
- Construction performs no allocation, transfer, synchronization, or
  evaluation.

Static type/rank errors fail constraint checking. Dynamic shape errors are
detected before the destination is modified. The first release should support
exact-shape elementwise operations and rank-zero scalars only; implicit
broadcasting is deferred. This avoids freezing a complex broadcasting and
sparse-expansion policy prematurely.

### 7.3 Evaluation ownership

**Recommendation/inference.**

- `expression` never defines `Evaluate(dense)` or `Evaluate(sparse)`.
- `dense` selects dense traversal, fusion, alias handling, and providers.
- `sparse` selects sparse traversal, structure policy, and providers.
- Algebraic nodes such as matrix product are operation descriptors, not
  pointwise callable expressions.
- A node never selects its result container.

Eigen demonstrates the performance value and alias complexity of lazy
evaluation. ASC should adopt explicit evaluator ownership but not Eigen's
storage-coupled expression hierarchy or automatic, potentially hidden,
temporary policy.

### 7.4 GPU expressions

**Recommendation/inference.** Do not claim arbitrary external-type GPU
expression support in the first CUDA milestone. Common C++20 headers cannot
make an arbitrary user callable device-compilable without imposing a CUDA-
specific annotation or compilation contract. The first CUDA evaluator should
support a documented finite set of built-in nodes in compiled provider code.
External expressions remain a CPU capability until an ADR specifies:

- the CUDA compilation boundary;
- host/device annotation policy;
- allowed scalar functors;
- descriptor trivial-copy requirements;
- diagnostics for a non-device-callable node.

This limitation is preferable to leaking CUDA macros through every common
expression header or silently evaluating on the host.

## 8. Random reproducibility and storage facets

### 8.1 Base random target

**Recommendation/inference.** `ASC::random` depends only on `ASC::core` and
provides:

- an explicit counter-based Philox4x32-10 raw-bit engine;
- key, counter, stream, subsequence, and offset vocabulary;
- exact integer reference vectors;
- fixed-algorithm `Uniform01<float>` and `Uniform01<double>`;
- an optional explicit stateful sequence wrapper;
- versioned state serialization only after the core I/O encoding is frozen.

The raw engine is a pure function of key and counter. A stateful wrapper is
copyable only if copying is documented to duplicate the future sequence.
There is no default engine, thread-local hidden engine, or global mutable
engine.

Do not define reproducibility in terms of `std::uniform_*_distribution`. The
standard random design historically leaves exact distribution algorithms
implementation-defined; the original WG21 proposal says this explicitly.
[WG21 N1452](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2003/n1452.html).

### 8.2 Reproducibility levels

**Recommendation/inference.** Document independent guarantees:

1. **Raw-bit guarantee:** engine/key/counter yields frozen words.
2. **Distribution guarantee:** a named, versioned transform yields frozen bits.
3. **Logical-fill guarantee:** each logical coordinate maps to a defined
   counter independent of physical layout and parallel partition.
4. **Provider guarantee:** a particular provider matches the approved vectors.
5. **Release guarantee:** serialized state and sequence remain compatible for
   a stated version interval.

CPU/GPU bit identity is not implied by using an engine with the same name.
It is claimed only after cross-provider vectors pass. Floating reductions and
random generation have different reproducibility contracts.

### 8.3 Dense generation facet

**Recommendation/inference.** `random_dense` fills a caller-provided dense
destination. Its contract includes:

- explicit key/base counter or state object;
- logical coordinate order independent of row/column physical layout;
- distribution and exact counter consumption;
- explicit execution context;
- zero-size behavior;
- strided destination behavior;
- returned consumed range or next counter;
- no allocation, transfer, or synchronization not requested by the caller.

Fixed-consumption distributions are the first milestone. Rejection-based
distributions need a separate substream/attempt mapping so one coordinate's
draw count cannot shift all later coordinates.

### 8.4 Sparse generation facet

**Recommendation/inference.** `random_sparse` separates structure and values.
Its API chooses exactly one structural meaning:

- exact stored-entry count; or
- Bernoulli inclusion probability/density with documented count variability.

It also specifies:

- sampling with or without replacement;
- canonical output order;
- duplicate policy;
- whether diagonal or symmetry constraints apply;
- independent structure and value keys/substreams;
- explicit-zero policy when a value distribution yields zero;
- allocation/workspace and finalization behavior.

Exact-count generation should sample structure without replacement and produce
unique canonical coordinates. Using separate structure and value streams makes
value changes independent of the structure algorithm.

### 8.5 CUDA random facet

**Recommendation/inference.** Prefer an ASC-owned, provider-conformance
implementation of the frozen Philox mapping when CPU/GPU bit identity is
required. A cuRAND facet is acceptable for capabilities whose seed/offset/order
contract is documented, but its ordering choice and toolkit scope must appear
in the backend matrix. cuRAND's “best” ordering is not a release-stable ASC
sequence guarantee.

## 9. Provider architecture and backend capability matrix

### 9.1 Isolation and dispatch

**Recommendation/inference.**

- Each provider is a compiled facet owned by `core`, `dense`, `sparse`, or
  `random`.
- Common headers contain normalized ASC descriptors only.
- SDK headers, provider handles, error enums, and calls stay in provider `.cc`
  or CUDA translation units.
- A provider factory creates a context with an immutable function table or
  equivalent type-erased state. There is no mutable global plugin registry.
- Provider errors translate to ASC status while retaining provider name and
  native numeric code.
- Capability queries occur before destination mutation.
- Static-library exports may carry provider libraries as link-only
  requirements; the corresponding package component must find them before
  loading that export.

The public capability key should include at least:

```text
operation, scalar/input/compute type, index width, rank,
layout or sparse format, transpose/conjugate mode,
memory spaces, device, synchronization mode,
determinism, workspace size/alignment, packing/conversion
```

A single `Supports(Gemm)` boolean is insufficient.

### 9.2 Matrix guidance

The following is a planning matrix, not an implementation claim:

| Provider/facet | Owner | Candidate capability | Key restrictions to record | Required evidence before claim |
| --- | --- | --- | --- | --- |
| serial reference / base | core | host allocation, copy, synchronous execution | host only | runtime + sanitizer |
| serial reference / base | dense | elementwise, reductions, narrow BLAS set | initially `float`/`double`; documented strides | runtime + numerical + allocation |
| serial reference / base | sparse | canonical COO/CSR/CSC validation, SpMV | 32/64-bit ASC indices; host only | runtime + structural + numerical |
| ASC Philox / base | random | raw bits and scalar transforms | frozen key/counter/word order | cross-compiler reference vectors |
| `core_cuda` | core | device/pinned allocation, explicit copy, stream, event | exact toolkit/driver/platform | configure + compile + real runtime |
| `dense_cuda` / cuBLAS | dense | BLAS subset | layout, scalar/compute type, pointer mode, stream, workspace | runtime + CPU parity |
| `dense_cuda` / cuSOLVER | dense | selected factorizations/solvers | column-major/provider layout, workspace, info output | residual/backward error + runtime |
| `sparse_cuda` / cuSPARSE | sparse | selected SpMV/SpMM/conversions | format, sorted/unique, 32/64 indices, algorithm, workspace | structural + parity + runtime |
| `random_cuda` / ASC Philox | random | provider bit generation | exact logical mapping | CPU/GPU bit-vector runtime |
| optional cuRAND path | random | provider-specific bulk distributions | seed, offset, order, toolkit/version scope | documented vector/runtime evidence |

Each capability-manifest row should identify:

- configure status;
- compile status;
- runtime hardware, driver, toolkit, and provider version;
- parity oracle and tolerance;
- unsupported types/layouts/formats;
- determinism scope;
- allocation, workspace, transfer, and synchronization;
- last verified date.

The evidence labels remain distinct: configure-tested, compile-tested,
runtime-tested, parity-tested, or skipped.

### 9.3 No implicit fallback

**Recommendation/inference.** A provider may implement an explicit same-space
reference fallback only if the operation call requests it and the capability
report names the provider actually selected. It may never:

- copy device data to host for a CPU implementation;
- densify sparse input;
- downcast indices without a check;
- change compute precision;
- pack or allocate invisibly;
- synchronize merely to obtain a host scalar;
- substitute a deprecated provider API.

cuBLAS's documented stream/workspace reproducibility and cuSPARSE's algorithm-
specific determinism show why provider choice must be observable.

## 10. Errors, configuration, and I/O

### 10.1 Error model

**Recommendation/inference.** Use no public production exceptions. Define:

- `Status` for failure without a value;
- `Result<T>` for value-or-failure;
- stable ASC error domain/code;
- optional provider name and numeric provider code;
- actionable context without secrets;
- `[[nodiscard]]` on both transport types.

Factories return `Result<T>` when construction can fail. Assertions are only
for internal invariants; public invalid shape, index, parser, I/O, allocation,
provider, and numerical conditions return status. Accessing the value of a
failed `Result` is a fatal contract violation, not a recoverable exception.
`noexcept` is used only when accurate, especially on move/destruction paths.

Do not build an inheritance hierarchy of error objects. First catalogue errors
by domain:

```text
argument, shape/index/overflow, state/lifetime, allocation,
memory/access/transfer, provider/capability, numerical,
configuration/parser, I/O/encoding/version
```

### 10.2 Configuration boundary

**Recommendation/inference.** `core` owns a storage-independent recursive
configuration model:

- null, bool, signed/unsigned 64-bit integer, double, UTF-8 string;
- list of values;
- string-keyed object;
- schema type, required/default state, constraints, sensitivity, and
  deprecation;
- validation diagnostics and source provenance.

No dense or sparse object appears in a configuration value. `core` does not
parse `argv` or a concrete file format.

`utilities` owns parsers and merge precedence. Freeze this order:

```text
schema default < local files in command order < command line
               < explicit programmatic override
```

Unknown keys are errors by default. Duplicate scalar keys at one precedence
level are errors. Repeated/list append semantics require an explicit schema
flag. Every effective value can report its origin. Sensitive values are
redacted in diagnostics. Environment variables and response files are out of
the first release.

**Open dependency decision.** Do not write a broad configuration-language
parser without a security and conformance test burden. The first Utilities
milestone can deliver programmatic and command-line input. Before local-file
parsing, the owner must approve either:

- a narrow documented project format; or
- a reviewed third-party TOML/JSON parser and its license/dependency cost.

The architecture boundary is frozen even though the concrete format is not.

### 10.3 I/O boundary

**Recommendation/inference.** `core` owns:

- byte/text source and sink interfaces;
- RAII local file handle;
- `ReadSome`, `ReadExact`, `WriteSome`, and `WriteAll` semantics;
- short-read/write, EOF, permission, overflow, and path errors;
- approved fixed-width scalar and metadata encoding;
- magic/version/endianness helpers.

Dense and sparse own serialization of shapes, layouts, indices, formats, and
values. Random owns versioned engine state. Serialization never triggers a
device-to-host copy. A device object must first be copied explicitly into a
host destination or use a future provider-specific I/O facet.

The first release should omit general logging from core. A caller-supplied
diagnostic sink can be added only when an actual cross-module need is known.

## 11. Packaging and component consumption

### 11.1 Package identity

**Recommendation/inference.** Retain the established package identity
`ASCCpp` and imported namespace `ASC::`, while replacing the historical
component set with:

```text
core utilities expression dense sparse random
random_dense random_sparse cpp
```

Provider component names match their targets, beginning with:

```text
core_cuda dense_cuda sparse_cuda random_cuda
random_dense_cuda random_sparse_cuda
```

Canonical use:

```cmake
find_package(ASCCpp CONFIG REQUIRED COMPONENTS dense)
target_link_libraries(my_target PRIVATE ASC::dense)
```

No-component lookup loads the provider-free `cpp` closure. It never discovers
CUDA or another optional provider. An unsupported or unavailable required
component makes `ASCCpp_FOUND` false with a corrective diagnostic.

### 11.2 Export structure

**Recommendation/inference.** Generate one installed export file per component
or per minimal dependency closure. The package config:

1. validates requested component names;
2. expands required ASC component dependencies;
3. calls `find_dependency()` only for requested external-provider components;
4. includes exports in dependency order;
5. sets per-component found variables;
6. calls `check_required_components(ASCCpp)`.

This structure makes a CPU-only `core` or `random` consumer independent of
CUDA discovery. It also lets component tests inspect the exact imported graph.

The released `asc-cmake` helper remains useful for simple base packaging, but
strict conditional provider exports require standard CMake or an approved
`asc-cmake` enhancement, as described in Section 2.1.

### 11.3 Build options

**Recommendation/inference.**

- Top-level testing/examples/install defaults differ from subproject defaults.
- Provider build options default off.
- Explicitly enabled unavailable providers fail configuration.
- Multiple providers may be built; build-time options do not choose a hidden
  runtime default.
- `BUILD_SHARED_LIBS` behavior is tested if both static and shared are claimed.
- Warning and sanitizer carriers remain build-interface/private and never
  propagate to consumers.
- CPU-only configure does not enable CUDA language or call
  `find_package(CUDAToolkit)`.

Required package tests cover build tree, install tree, relocation, path with
spaces, every base target alone, both random storage facets independently,
provider-disabled CPU-only use, required-unavailable failure, umbrella use,
and supported static/shared modes.

## 12. Verification, CI, and release evidence

### 12.1 Mechanical dependency gates

**Recommendation/inference.** Every merge runs:

- a direct target-edge audit;
- a forbidden-include scan;
- each public header compiled as the only include;
- provider-neutral headers compiled without provider SDKs installed;
- each module enabled/consumed in isolation;
- installed imported-target graph inspection;
- a CPU-only package and consumer configure;
- negative component/provider requests.

Dependency tests must inspect both source and installed graphs. A broad
`ASC::cpp` consumer is not evidence for a module boundary.

### 12.2 Contract tests by area

`core`:

- checked arithmetic at signed/unsigned and provider-width boundaries;
- zero-size/alignment/allocation failure and exactly-once release;
- memory-space access rejection;
- copy/stream/event ordering and lifetimes;
- provider error translation;
- configuration validation and I/O short operations.

`expression`:

- third-party type outside ASC inheritance;
- lvalue, rvalue, nested temporary, moved operand, and view lifetime;
- static and dynamic shape failures;
- scalar promotion;
- alias and sparsity-effect metadata;
- useful negative compile diagnostics.

`dense`:

- owner move/clone/resize/reshape/invalidation;
- left, right, padded, and noncontiguous unique-stride mappings;
- zero-rank/zero-extent;
- const propagation and slices;
- safe exact alias and rejected unsafe overlap;
- allocation/workspace counters;
- reference numerical values and provider parity.

`sparse`:

- invalid offsets/indices and width boundaries;
- unsorted/duplicate/explicit-zero finalize policies;
- empty canonical offsets;
- COO/CSR/CSC round trips;
- structure mutation/view invalidation;
- sparsity-effect enforcement;
- no-densification allocation test;
- SpMV reference values and provider checks.

`random`:

- frozen raw-bit and distribution vectors;
- key/counter/stream/subsequence/copy/state advance;
- dense layout/stride/partition invariance;
- sparse exact-count/density/duplicate/order semantics;
- parallel partition equivalence;
- CPU/GPU vectors only for promised guarantees;
- statistical smoke tests with printed reproducing seed and robust bounds.

### 12.3 Numerical evidence

**Recommendation/inference.**

- Elementwise and BLAS-like operations use operation-appropriate absolute,
  relative, or ULP checks.
- Solvers use residual and backward-error checks scaled by norms and condition
  estimates where available.
- Provider parity compares to an independent reference, not merely another
  call through the same provider.
- NaN, infinity, signed zero, transpose/conjugate, `beta == 0`, degenerate
  shapes, and extreme finite values have explicit contracts.
- No universal epsilon is used.

### 12.4 Runtime safety and performance

**Recommendation/inference.**

- ASan and UBSan cover CPU debug tests.
- TSan covers same-instance and separate-instance contracts where supported.
- GPU memory/race checking is a separately reported runtime job.
- Allocation counters verify no allocation in prepared no-allocation calls.
- Workspace query and packing paths are tested separately.
- Benchmarks record compiler, flags, CPU/GPU, driver/toolkit/provider, sizes,
  layouts, repetitions, and robust summaries.
- Performance thresholds remain non-gating until variance and baselines are
  demonstrated.

### 12.5 CI and release

**Recommendation/inference.** The minimum CPU matrix is:

- Linux GCC Debug and Release;
- Linux Clang Debug plus sanitizers;
- Windows MSVC multi-config, install, and path-with-spaces;
- macOS AppleClang install/consumer.

GPU CI separates:

- toolkit configure;
- provider compile;
- real-hardware runtime;
- CPU/reference parity.

A skipped runtime is not a pass. Release notes list exact test counts, skips,
toolchains, enabled facets, providers, hardware, driver, and toolkit. Pre-1.0
releases may change ABI with documented migration; source and serialized
random-state compatibility require separate explicit policies.

## 13. Main risks and falsification tests

| Risk or assumption | Falsification test | Consequence if falsified |
| --- | --- | --- |
| Expression is truly storage-neutral | Implement a small external strided type with no ASC base and evaluate it through dense and sparse-readable protocols | Redesign protocol before storage APIs freeze |
| Expression capture cannot dangle | ASan tests for lvalue, rvalue, nested temporary, view of temporary owner, and stored expression | Change holder/factory rules; reject unsafe construction |
| Dense and sparse are independent | Configure/install/consume each with the other disabled and scan includes/targets | Remove shared storage types; move only neutral vocabulary to core/expression |
| Random base is storage-free | Compile every random base header and consume `ASC::random` with dense/sparse disabled | Move fill adapters into random-owned facets |
| Provider SDKs are isolated | Build and consume every base component on a host with no provider SDK; scan preprocessed dependencies and exports | Split provider target/export before release |
| Component package is genuinely conditional | Request `core`, `random`, and an unavailable CUDA facet separately from relocated installs | Replace monolithic export/config logic |
| No hidden allocation/packing | Global/resource allocation counters around prepared evaluation and algebra calls | Add workspace query or change the documented capability |
| No hidden transfer/synchronization | Instrument resource copy and stream/event APIs; run device calls with host-only operands | Fail operation instead of fallback |
| Sparse never silently densifies | Evaluate a huge, extremely sparse expression under a strict allocation resource and no dense component | Reclassify operation or require explicit dense destination |
| Sparse provider preconditions are enforced | Feed duplicate, unsorted, wrong-base, overflowing-width, and invalid-offset descriptors before provider invocation | Strengthen canonicalization and checked adapters |
| Logical random fill is partition invariant | Compare one fill with many partitions, thread counts, layouts, strides, and CPU/GPU providers bit for bit | Redesign coordinate-to-counter mapping |
| Serialized random state is stable | Golden files across endianness simulation and released schema versions | Version or withdraw compatibility claim |
| Async lifetimes are enforceable | Destroy/move context, event, workspace, and owners in every legal/illegal order under sanitizers/GPU checker | Redesign event/context ownership |
| Capability query predicts invocation | Property-test every matrix row: advertised cases succeed; unadvertised cases fail before mutation | Unify query and dispatch source of truth |
| Reference/provider numerical parity is meaningful | Independent high-precision or trusted-vector oracle plus residual checks | Narrow the capability or tolerance claim |
| Compile-time rank does not cause unacceptable code growth | Build-time/object-size benchmarks for representative ranks and expressions | Add bounded dynamic-rank or explicit instantiations |
| Result/status use is not silently ignored | `[[nodiscard]]` compile tests and fault injection for every error domain | Tighten API and review policy |
| Configuration parser is robust | corpus, fuzz, Unicode/path, duplicate, depth/size-limit, and secret-redaction tests | Narrow the format or adopt a reviewed parser |
| Static and shared packages preserve provider isolation | Independent build/install/relocate/consume matrices for both modes | Narrow supported linkage mode before release |

## 14. Decisions still requiring owner or ADR approval

These are not reasons to weaken the six-module graph. They are explicit gates:

1. Whether the concrete local configuration format is a narrow owned format or
   an approved third-party parser.
2. Whether fully runtime-rank dense/sparse types are needed in v1; this report
   recommends deferral.
3. Which optimized CPU dense and sparse providers, if any, are approved.
4. CUDA as the first GPU backend and access to a trusted runtime runner.
5. Exact scalar/complex/index capability in each algebra milestone.
6. Whether any provider-specific public header may expose a native stream for
   interoperability, or only opaque ASC construction options.
7. The exact ABI policy before 1.0 and version interval for serialized random
   state.
8. Whether an `asc-cmake` multi-component export enhancement is justified or
   ASCCpp should use standard CMake directly for conditional exports.

## 15. Recommended implementation ordering

The runbook's milestone order is technically sound:

1. architecture, ADRs, manifests, dependency enforcement, and real package
   fixtures;
2. CPU `core`;
3. independent `utilities`, `expression`, and random base waves;
4. CPU `dense`;
5. CPU `sparse`;
6. random dense/sparse facets;
7. `core_cuda` and `dense_cuda`;
8. `sparse_cuda` and random CUDA facets;
9. integration hardening and an `asc-xde` trial.

Before any storage implementation, freeze the checked shape/index vocabulary,
memory/execution contract, error transport, expression metadata, and installed
component strategy. Before any provider implementation, freeze the capability
key, workspace/packing policy, error translation, and evidence matrix.

## 16. Final independent assessment

**Recommendation/inference.** Proceed with the six modules and the exact base
and random-facet graph in Section 3. The design is strongest when ASC borrows
the conceptual separations demonstrated by `mdspan`, Kokkos, Ginkgo, and modern
vendor APIs, while declining their implicit defaults or storage coupling where
those would violate ASC requirements.

The architectural release blockers are:

- a monolithic package export that makes provider SDKs mandatory;
- unspecified expression capture/alias/temporary behavior;
- unspecified sparse canonicalization and densification policy;
- random generation without a logical coordinate-to-counter contract;
- backend capability claims without runtime evidence;
- any implicit allocation, conversion, transfer, fallback, or synchronization.

None requires a seventh module. All require ADRs and mechanical tests before
public APIs are frozen.
