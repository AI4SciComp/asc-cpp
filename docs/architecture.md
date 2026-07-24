# Architecture and design

## Transitional architecture through Random M1

Core Milestone 1 adds the first canonical serial foundation beside the
MdeCpp-derived runtime, and Utilities M1 adds status-oriented configuration and
CLI paths plus a hardened timer. Array M1 adds the canonical dense descriptor,
view, and owner path. Linalg M1 adds seven canonical operations through a
compiled serial-reference provider. Random M1 adds a counter-addressed engine,
unit-uniform transform, and serial bulk fill over canonical views. Broader
Linalg and inherited Random samplers continue to use the legacy
array/memory/device path through the aggregate compatibility surface.

```text
Canonical path                    Legacy compatibility path
--------------                    -------------------------
Core types + Status/Result        globals + legacy error policy
contracts                         Memory<T> + MemoryManager mm
MemorySpace/MemoryResource        Device + forall + CUDA wrappers
Buffer + ExecutionContext         |
        |                         v
        v                         DenseMArray/expressions/sparse
Array M1 Tensor/View              legacy linalg + random samplers
        |       |                 (ASC::cpp compatibility)
        v       v
Linalg M1      Random M1 Philox + Uniform01 + FillRandom
seven BLAS     (compiled/header serial-reference path)
```

Canonical headers do not depend on legacy runtime headers. A canonical
`Buffer<T>` owns one allocation through RAII and is never registered with the
global memory manager. An explicit `ExecutionContext` is independent of the
legacy process-wide `Device`.

The compatibility higher-module path still retains these useful MdeCpp design
ideas:

- `MShape` owns extents, independent of storage.
- `LayoutLeft`, `LayoutRight`, and `LayoutStride` map logical coordinates to
  offsets and make dense views use the same indexing machinery as owners.
- `SparseLayoutStride`, `SparseLayoutRight`, and `SparseLayoutLeft` express COO,
  CSR, and CSC storage through parallel map types.
- `MIndex` and iterators traverse logical space without embedding traversal in
  every algorithm.
- `MObject<Derived>` uses CRTP to share shape, assignment, reduction, and
  expression behavior without virtual dispatch.
- Compatibility `Memory<T>` tracks host/device pointers, ownership, validity,
  aliasing, and the preferred execution space. `UArray` and `DenseMArray` still
  build container behavior over that mechanism in this milestone.
- Legacy BLAS-like operations, decompositions, and random samplers consume
  compatibility array concepts. Static and dynamic legacy arrays therefore
  share those algorithms.

## Modules and dependency direction

```text
ASC::utilities -> ASC::core
ASC::array     -> ASC::core
ASC::linalg    -> ASC::array
ASC::linalg    -> ASC::core
ASC::random    -> ASC::array
ASC::random    -> ASC::core

ASC::cpp = all components + inherited compatibility headers
```

Here `A -> B` means that A depends on B. Canonical Linalg and Random name Array
views and Core context/status types directly, so those direct edges are
explicit.
`utilities` depends only on `core`. Structured configuration values use
`std::vector`-based `ConfigVector` and `ConfigMatrix` storage and therefore do
not pull numerical arrays into command-line or configuration consumers.
The minimal `ASC::random` component has no Linalg or direct optional-SDK
dependency.
The inherited covariance-taking normal sampler still includes legacy Linalg,
so it is installed and consumed through `ASC::cpp`, which already links all
components. Compatibility ownership does not add a canonical Random-to-Linalg
edge.

An optional-backend build may still expose OpenMP/CUDA usage requirements from
the current public Core target and shared compiler policy. That transitive
package limitation waits for cross-module provider hardening; it does not
create an OpenMP/CUDA Random provider.

At the repository-family level:

```text
asc-cmake -> asc-cpp -> asc-xde -> asc-kinetic -> asc-lab
```

`asc-cpp` deliberately stops before geometry, mesh, FEM, integration, ODE,
kinetic, analysis, and visualization concerns.

## Legacy dense array model

`DenseMArray<T, Shape, Layout>` owns a layout map and `Memory<T>`. Common aliases
in `<asc/array/marray.h>` are:

- `DVector<T>`, `DMatrix<T>`, and `DTensor<T, Rank>` for runtime extents;
- `SVector<T, N>`, `SMatrix<T, M, N>`, and `STensor` for compile-time extents;
- corresponding `*View` aliases using `LayoutStride`.

The default layout is `LayoutLeft` (column-major) unless
`ASC_USE_ROW_MAJOR` is defined. Expressions carry callable element access and
are evaluated through assignment into a concrete array. Host/device execution
is selected through the destination's memory policy and the `ForallWrap`
dispatcher.

## Legacy sparse array model

Sparse construction normally starts in `SparseLayoutStride` (COO), where
insertion is cheap. `Finalize()` sorts entries and merges duplicates;
`ToLayout<SparseLayoutLeft>()` or `ToLayout<SparseLayoutRight>()` creates CSC or
CSR storage. Compressed formats keep a canonical increasing inner-index order.

An empty compressed array still stores an outer-pointer table of
`outer_extent + 1` zeros. This invariant makes empty CSR/CSC objects valid for
iteration and zero-copy Eigen maps.

## Canonical Core M1 memory and execution

Canonical storage separates allocation ownership from execution choice:

```text
MemoryResource -> Buffer<T>
ExecutionContext -> provider capabilities and resources
Event -> provider completion state
```

`Buffer<T>` is move-only, owns one allocation in one explicit memory space,
retains the resource required for no-throw destruction, and exposes checked
host access only for host-accessible spaces. It has no implicit raw-pointer
conversion, mirroring, execution preference, manual deletion, or deep-copy
operation in M1.

`ExecutionContext` is a cheap handle to immutable provider state. Serial is the
only canonical M1 provider and is always available. OpenMP and CUDA backend
enumerators exist for capability reporting, but creating those canonical
contexts returns an unavailable status in M1. Context fallback never performs
an implicit transfer. The M1 `Event` represents completed synchronous serial
work; real asynchronous providers are planned for a later milestone.

The canonical dependency order inside core is:

```text
types/status/contracts -> memory spaces/resources -> Buffer
                                              \-> ExecutionContext -> Event
```

Common canonical headers contain no optional-provider SDK types.

## Canonical Array M1 design

Array M1 is an additive dense host path with this internal dependency order:

```text
Extents -> contiguous/stride mappings -> DefaultAccessor -> TensorView
   \-------------------------------------------------------> Tensor
Core Buffer + ExecutionContext ----------------------------/
```

`Extents` has compile-time rank, mixed static/dynamic dimensions, and checked
signed 64-bit metadata. Mappings own the coordinate-to-offset policy and report
logical size, required span, contiguity, and conservatively proven uniqueness.
The fixed canonical default is left/column-major.

`TensorView` is a lightweight non-owning descriptor whose element type carries
mutability. Mutable-element views require unique addressing; const-element
views may describe non-unique broadcast strides. Conversion is one-way from
mutable elements to const elements, and a const owner returns only a
const-element view. The caller owns allocation lifetime.

`Tensor` combines a unique, exhaustive, contiguous mapping with Core
`Buffer<T>`. It is move-only and host-accessible in M1. Deep copy is an explicit
synchronous `Clone` with an `ExecutionContext` and destination resource or
space. No Array operation performs implicit synchronization, transfer, device
selection, or mirroring.

The canonical umbrella `<asc/array.h>` is designed to expose only this M1
surface and to depend only on canonical Core. Transform views, expression
evaluation, broadcasting, canonical sparse storage, and provider-specific
array access are later milestones. See the
[Array module guide](modules/array.md) and
[Array migration](migration/array.md).

## Canonical Linalg M1

Linalg M1 introduces a deliberately small compiled-plus-template boundary:

```text
canonical concepts + context-first operation facade
                         |
                         v
       validation, capability query, one dispatch
                         |
                         v
 compiled serial-reference float/double kernels
```

The exact operation set is `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and
`Gemm`. Public constrained templates accept canonical rank-one or rank-two
views, validate 64-bit shapes, real mappings, accessible spans, output
uniqueness, and conservative storage overlap, then dispatch once to compiled
`float` or `double` kernels. Outputs are preallocated. All validation completes
before pointer access or the first write, so a recoverable failure is
transactional.

The synchronous `serial-reference` provider is deterministic in operation
order, supports canonical left, right, and proven-unique non-negative stride
mappings, and allocates or transfers nothing. `Nrm2` uses scaled sum of squares;
the other reduction/inner loops accumulate in ascending logical-coordinate
order in the operand type. The capability query reports this actual provider
and the seven operations. Optional build switches do not change that canonical
capability.

The canonical umbrella `<asc/linalg.h>` excludes `blas.h`, `decomp.h`,
`lapack.h`, `eigen.h`, and every provider SDK. Those headers remain separately
installed through the compatibility path. It also does not re-export Array
owner or descriptor construction; callers include `<asc/array.h>` explicitly
when building operands. Pointwise/general reduction
algorithms in legacy `blas.h` are assigned to future Array evaluation work;
factors, solvers, sparse/matrix-free operations, and optional numerical
providers are deferred Linalg milestones.

See the [Linalg module guide](modules/linalg.md) for the operation
contracts and [Linalg migration](migration/linalg.md) for the complete
legacy-family classification.

## Canonical Random M1

Random M1 introduces one reproducible generation chain:

```text
RandomKey + RandomCounter
          |
          v
compiled Philox4x32-10 integer engine
          |
          v
Uniform01<float/double> -> FillRandom(context, writable TensorView)
```

The key identifies a logical stream. The counter contains a high 64-bit
subsequence and low 64-bit output offset, so selecting a value is a pure
function rather than a mutation of hidden engine state. Entropy seeding is not
part of M1.

`Uniform01<float>` uses the high 24 bits of one generated 64-bit word;
`Uniform01<double>` uses the high 53 bits. The transform is owned by ASC rather
than delegated to a standard-library distribution.

Bulk fill enumerates logical coordinates with the rightmost axis varying
fastest and writes through the view's actual non-negative strides. Supported
left, right, and unique padded-stride mappings receive identical values at the
same logical coordinate, and padding holes are preserved. Counter offsets make
one complete fill bit-identical to correctly addressed logical partitions,
independent of their scheduling order.

The only M1 execution path is synchronous `serial-reference` generation over
host-accessible canonical views. Validation is release-active, finishes before
the first write, and reports status without allocating, transferring, or
synchronizing the destination.

The canonical umbrella `<asc/random.h>` includes no inherited sampler,
compatibility Array, Linalg, aggregate, or optional-provider header. Mutable
engines, normal/transcendental distributions, canonical quasi-random sequences,
Sobol table redesign, multivariate transforms, and parallel providers remain
future reviewed milestones. See the [Random module guide](modules/random.md)
and [Random migration](migration/random.md).

## Canonical Utilities M1

Utilities M1 adds a status-oriented path beside its MdeCpp-derived class names:

```text
ConfigValue/ConfigParser -> Core Status/Result + contracts
OptionParser             -> Core Status/Result + build configuration
Timer                    -> Core build configuration + contracts
compatibility.cc         -> legacy error translation and global output only
```

Canonical configuration storage uses only `std::vector`-based values. CLI
options own their names and candidate values, and configuration/CLI parsing
commits only after the complete input validates. `Timer` has per-instance
idle/running state and lossless statistics; it does not share mutable timing
state across instances.

The canonical umbrella `<asc/utilities.h>` includes configuration, CLI, and
timer only. Their canonical headers and implementation files contain no Array,
Linalg, Random, aggregate, optional-provider SDK, or legacy Core runtime
include. Historical error/global-output integration is isolated in
`compatibility.cc`; `<asc/utilities/optparser.h>` only forwards to the canonical
CLI header. Diagnostics and tracing remain deferred because a sink consumed by
Core cannot be owned above Core without reversing the dependency graph.

See the [Utilities module guide](modules/utilities.md) for the public behavior
and [Utilities migration](migration/utilities.md) for the compatibility mapping.

## Legacy memory and execution compatibility

`Memory<T>` can allocate normal, aligned, managed, or device memory and can wrap
or alias external storage. It tracks which host/device copy is current and
moves data lazily on `Read`, `Write`, or `ReadWrite`. Ownership flags determine
which allocation is released; aliases are registered with the memory manager so
synchronization applies to the base allocation.

`Device` holds the runtime backend policy. `ForallWrap` chooses CUDA when the
array requests device execution and the translation unit was compiled as CUDA,
then OpenMP when enabled and allowed, otherwise the serial CPU loop.

This path remains unchanged for compatibility in M1. It is not implemented on
top of `Buffer<T>` or `ExecutionContext`, and new foundational code should not
add dependencies on it. Compatibility forwarding and provider isolation are
later milestones described in [Core migration](migration/core.md).

## Build and package boundaries

The installed CMake package exports exactly six targets: `ASC::core`,
`ASC::array`, `ASC::utilities`, `ASC::linalg`, `ASC::random`, and `ASC::cpp`.
Following MdeCpp's module-owned build organization, each directory under `src`
declares its own target, source list, public header file set, direct
dependencies, and optional backend requirements. The root build only establishes
project-wide policy and orchestrates configuration, dependency discovery,
packaging, examples, and tests. This keeps source ownership local without
reintroducing MdeCpp's monolithic library or directory-global compiler flags.

Core M1 adds canonical headers and compiled serial runtime sources to the
existing `ASC::core` target. It does not add a component, external dependency,
or public provider target.

Array M1 likewise adds no component or provider target. Its descriptor,
mapping, accessor, view, owner, and concept templates are header-visible and
belong to the existing `ASC::array` public file set. The target continues to
link only `ASC::core`; legacy Array translation units remain installed as a
separately characterized compatibility surface during migration.

Linalg M1 converts the existing `ASC::linalg` interface target
to a normal compiled target. Its direct public ASC dependencies are exactly
`ASC::array` and `ASC::core`. Public constrained facades and internal installed
declarations dispatch to compiled serial-reference `float`/`double` kernels;
no separate public provider target or optional SDK type is introduced. Legacy
Eigen/MKL usage requirements remain conditional compatibility requirements
until a later adapter/provider split.

Random M1 keeps `ASC::random` compiled and changes its public contract to the
canonical Random umbrella plus types, engine, distribution, fill, and required
template-detail headers. Its direct public ASC dependencies are exactly
`ASC::array` and `ASC::core`. The target also compiles the inherited
permutation and Sobol definitions so the aggregate compatibility headers keep
linking, but those headers are installed through the `ASC::cpp` file set and
are not minimal-component promises. A request for package component `random`
therefore does not request Linalg or an optional Linalg SDK.
OpenMP/CUDA requirements may still propagate from a correspondingly configured
legacy Core target; Random M1 does not describe them as Random providers.

Optional dependencies are recorded in `ASCCppConfig.cmake` and rediscovered for
installed consumers. The stable `<asc/core/config.h>` facade includes the
generated `<asc/config/_config.h>`, which carries precision and backend ABI
choices to every dependent target. The exact CMake responsibilities and the
procedure for adding a component are described in
[build-system architecture](build-system.md).
