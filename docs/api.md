# API map

All public APIs are in namespace `asc`. The header comments are the detailed
reference; this page explains where to start.

## Core

Core Milestone 1 provides a canonical serial foundation through
`<asc/core.h>`. The umbrella contains the canonical headers below plus core
configuration, and links with `ASC::core`.

- `<asc/core/types.h>`: signed 64-bit `index_t`, `extent_t`, `stride_t`, and
  `nnz_t` metadata aliases.
- `<asc/core/status.h>`: stable `Status` codes and `Result<T>` values.
- `<asc/core/contracts.h>`: release-active public contracts and debug-only
  internal checks.
- `<asc/core/memory_space.h>` and `<asc/core/memory_resource.h>`: memory-space
  properties, the resource interface, and the M1 host resource.
- `<asc/core/buffer.h>`: move-only, single-space RAII ownership.
- `<asc/core/execution_context.h>` and `<asc/core/event.h>`: an explicit
  immutable context and completed-event interface. The canonical M1 provider
  is synchronous serial only.
- `<asc/core/config.h>`: version macros, `real_t`, backend feature macros, and
  the `_r` literal.

See the [Core module guide](modules/core.md) for ownership, lifetime, status,
memory-space, and execution contracts.

### Core compatibility APIs

The following MdeCpp-derived APIs remain available and unchanged because the
legacy array, linalg, and random implementations still consume them:

- `<asc/core/error.h>`: `ASC_ASSERT`, `ASC_VERIFY`, `ASC_ABORT`, mutable error
  action, and legacy exception translation;
- `<asc/core/device.h>` and `<asc/core/forall.h>`: the process-wide device
  policy and legacy serial/OpenMP/CUDA loop dispatch;
- `<asc/core/memory.h>`: `MemoryType`, `MemoryClass`, manual-lifetime
  `Memory<T>`, lazy mirroring, and global `MemoryManager mm`;
- `<asc/core/cuda.h>`: legacy CUDA runtime wrappers;
- `<asc/core/globals.h>` and `<asc/core/operators.h>`: global diagnostic streams
  and device-coupled operation functors.

Existing conversion, numeric, and string helpers remain available through the
narrow `<asc/core/casts.h>`, `<asc/core/math.h>`, and `<asc/core/string.h>`
headers. They are not included by `<asc/core.h>` in M1 because their current
implementations transitively use legacy error, global-stream, or device
facilities.

These interfaces are compatibility surfaces, not the model for new code. M1
does not deprecate or internally replace them, and it provides no implicit
conversion between `Buffer<T>` and `Memory<T>`.

## Arrays

Array M1 provides the canonical dense descriptor, mapping, view, and owner
path through `ASC::array`.

- `<asc/array.h>`: the canonical umbrella; it contains only the M1 dense
  descriptor, mapping, accessor, view, owner, and structural concept surface.
- `<asc/array/extents.h>`: fixed-rank `Extents`, including mixed static/dynamic
  extents and the `DynamicTensorExtents` spelling.
- `<asc/array/layout.h>`: checked left, right, and non-negative-stride mapping
  types using signed 64-bit metadata.
- `<asc/array/accessor.h>` and `<asc/array/tensor_view.h>`: explicit pointer
  access and non-owning views whose element type carries mutability.
- `<asc/array/tensor.h>`: move-only contiguous ownership over Core `Buffer`,
  plus explicit synchronous host `Clone` through an `ExecutionContext`.
- `<asc/array/tensor_concepts.h>`: structural readable, writable, contiguous,
  vector, and matrix concepts.

Canonical Array M1 is host-accessible and synchronous serial only. It does not
provide transforms, expression evaluation, broadcasting, sparse ownership, or
device transfer. See the [Array module guide](modules/array.md) and
[Array migration](migration/array.md) for the implemented contract and
boundary.

### Array compatibility APIs

The MdeCpp-derived Array APIs remain available while downstream modules
migrate:

- `<asc/array/mshape.h>`: `MShape<Extents...>`, `DShape<Rank>`, static/dynamic
  extents, and shape operations.
- `<asc/array/mlayout.h>`: dense left/right/strided maps.
- `<asc/array/mindex.h>` and `<asc/array/miterator.h>`: multidimensional index
  and traversal types.
- `<asc/array/uarray.h>`: resizable one-dimensional storage over `Memory<T>`.
- `<asc/array/dsmarray.h>`: `DenseMArray<T, Shape, Layout>`, references, device
  selection, indexing, serialization, and expression assignment.
- `<asc/array/spmarray.h>`: `SparseMArray<T, Shape, Layout>`, COO insertion,
  finalization, layout conversion, sparse operations, and reductions.
- `<asc/array/marray.h>`: the `DVector`, `DMatrix`, `DTensor`, `SVector`,
  `SMatrix`, sparse, and view aliases used by most applications.
- `<asc/array/carray.h>`: compile-time nested C arrays and allocation helpers for
  dynamically indexed C pointer trees.

Dense copies own independent data. `MakeRef` and `*View` types are explicitly
non-owning, so the referenced allocation must outlive the view. `HostRead`,
`HostWrite`, `Read`, and `Write` communicate access intent to the memory system.
These statements describe compatibility behavior; those types do not implement
canonical M1 ownership, const-view, metadata, dependency, or execution
contracts.

## Linear algebra

Canonical Linalg M1 uses the compiled `ASC::linalg` component:

- `<asc/linalg.h>`: canonical umbrella for narrow concepts, modes,
  capabilities, and the three BLAS-level headers; it does not re-export Array
  ownership or descriptor construction;
- `<asc/linalg/concepts.h>`: exact-rank readable/writable vector and matrix
  concepts over canonical Array descriptors;
- `<asc/linalg/capabilities.h>`: the seven-operation capability query and the
  `serial-reference` provider identity;
- `<asc/linalg/blas1.h>`: context-first `Copy`, `Scal`, `Axpy`, `Dot`, and
  `Nrm2` for identical `float` or `double` rank-one views;
- `<asc/linalg/blas2.h>`: context-first `Gemv`;
- `<asc/linalg/blas3.h>`: context-first `Gemm`;
- `<asc/linalg/types.h>`: common mode values; only `TransposeMode` is used by
  canonical M1.

All seven operations require an explicit `ExecutionContext`, preallocated
canonical views, host-accessible storage, checked shapes and aliasing, and
explicit modes/scalars. Mutations return `Status`; `Dot` and `Nrm2` return
`Result<T>`. M1 allocates, resizes, transfers, and synchronizes nothing. The
only canonical provider is the compiled deterministic synchronous
`serial-reference` implementation for `float` and `double`.

See the [Linalg module guide](modules/linalg.md) and
[Linalg migration guide](migration/linalg.md) for exact shape, layout, empty,
alias, numerical-order, status, and compatibility rules.

### Linalg compatibility APIs

The MdeCpp-derived headers remain installed for existing callers and are not
included by the canonical umbrella:

- `<asc/linalg/blas.h>` contains context-free pointwise, reduction, BLAS,
  matrix-product, Kronecker, and tensor-contraction algorithms over legacy
  arrays;
- `<asc/linalg/decomp.h>` contains LUP, Cholesky, QR, and optional SVD/eigen
  decomposition/solve routines;
- `<asc/linalg/lapack.h>` supplies LAPACK-style factor/solve wrappers,
  determinant/inverse helpers, automatic solver policy, and `LinearSolver`;
- `<asc/linalg/eigen.h>` is available with the compatibility Eigen option and
  exposes dense/sparse conversions, maps, and solver adapters.

Pointwise/general reductions belong to a future canonical Array evaluation
layer. Factors and solvers begin in Linalg M2. Eigen, MKL, BLAS/LAPACK, and
CUDA canonical providers require later private-provider milestones. Enabling a
compatibility option does not change the common canonical API or select an M1
provider.

## Random

Canonical Random M1 uses the compiled `ASC::random` component:

- `<asc/random.h>`: canonical umbrella for M1 types, counter engine, unit
  transform, and bulk fill; it does not include Array construction or any
  inherited sampler;
- `<asc/random/types.h>`: value-comparable `RandomKey` and `RandomCounter`;
- `<asc/random/counter_engine.h>`: compiled, versioned
  `Philox4x32_10::Generate` and `Generate64`;
- `<asc/random/distribution.h>`: stateless `Uniform01<float>` and
  `Uniform01<double>` mapping the high 24 or 53 input bits to `[0,1)`; and
- `<asc/random/fill.h>`: context-first `FillRandom` over writable structural
  canonical views.

The raw Philox words are bitwise stable for the named version. The unit
transform is bitwise stable under its documented IEC 60559 binary32/binary64
assumptions. Serial bulk fill uses rightmost-axis-fastest logical order and is
independent of supported physical layout and valid logical partition order.
See the [Random module guide](modules/random.md) and
[Random migration guide](migration/random.md) for the exact counter,
partition, status, and compatibility contracts.

### Random compatibility APIs

The inherited MdeCpp-derived headers remain installed through the aggregate
`ASC::cpp` compatibility surface and are not included by `<asc/random.h>`:

- `<asc/random/generator.h>`: `Splitmix64`, `Pcg32`, xoroshiro engines, and
  configurable standard-library uniform/normal generators;
- `<asc/random/permutation.h>`: mutable prime/permutation caches and radical
  inverse functions;
- `<asc/random/sampler.h>` and individual sampler headers: `PseudoSampler`,
  `LatinSampler`, `HaltonSampler`, `HammersleySampler`, `SobolSampler`,
  `NormalSampler`, and `SphericalSampler`.

These families retain legacy arrays, reseeding, layout, singleton, and Linalg
behavior. They gain no canonical cross-library bitwise promise. Sobol direction
numbers remain compiled into the compatibility implementation; an installed
program does not read a source-tree data file at runtime.

## Utilities

Utilities Milestone 1 is available through `<asc/utilities.h>` and links with
`ASC::utilities`.

- `<asc/utilities/config.h>`: standard-container `ConfigValue` alternatives;
  status-oriented definition, lookup, parsing, transactional loading, and
  canonical serialization through `ConfigParser`.
- `<asc/utilities/cli.h>`: owned typed options, transactional argv parsing,
  the migration-era option-file parser, and help/usage text with explicit
  output sinks.
- `<asc/utilities/timer.h>`: steady-clock state, empty-safe queries, lossless
  measurement statistics, reset, and explicit-stream printing.
- `<asc/utilities/optparser.h>`: a forwarding compatibility spelling for
  `<asc/utilities/cli.h>`; it is not a second parser implementation.

Recoverable configuration and CLI input failures return `Status` or
`Result<T>`. Existing void/value wrappers retain the historical throw-or-abort
translation for migration. See the [Utilities module guide](modules/utilities.md)
and [Utilities migration](migration/utilities.md) for the exact grammar,
transaction rules, timer state model, and compatibility boundary.

## Aggregate interface

`<asc/cpp.h>` includes the five canonical module umbrellas, retains the legacy
Linalg and Random compatibility headers required during migration, and
conditionally includes the Eigen adapter when the package was built with
Eigen. `<asc/asc.h>` is an equivalent top-level compatibility include. Prefer
`<asc/core.h>` or another narrow owning header in reusable libraries. The
aggregate continues to expose legacy core APIs needed by the compatibility
array stack.
