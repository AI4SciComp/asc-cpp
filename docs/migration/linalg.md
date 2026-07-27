# Linalg migration

> **Superseded historical document.** This file describes the deleted implementation at historical HEAD `33b261ea33616a6395c4ad3b20646093103344f7`; it is retained only for auditability and is not current API, build, package, or implementation guidance. See the [approved Stage A six-module blueprint](../development/asc-cpp-architecture/architecture-blueprint.md).

Linalg M1 adds seven explicit-context operations over canonical Array views
without rewriting the MdeCpp-derived algebra implementation. The old and new
paths intentionally coexist because the compatibility surface also contains
pointwise algorithms, contractions, factorizations, solvers, sparse adapters,
and provider-native types whose canonical contracts are deferred.

## Milestone status

| Milestone | Status | Scope |
| --- | --- | --- |
| Linalg M1 | Implemented | Compiled serial-reference capability and `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, `Gemm` over canonical views |
| Linalg M2 | Planned | Reusable LU and Cholesky factors, workspaces, pivots, repeated solve, and multiple right-hand sides |
| Linalg M3 | Planned | QR/least-squares, SVD/eigen, sparse/matrix-free operations, and duplicate solver removal |
| Array integration | Planned in Array M3 | Pointwise/general reductions, shape algebra, expression evaluation, and reviewed convenience operations |
| Random M1 | Implemented | Minimal Random target no longer depends on Linalg; covariance-taking normal remains aggregate compatibility |
| Provider milestones | Planned | Private SPI and conforming BLAS/LAPACK, Eigen, MKL, and CUDA providers |
| Pre-1.0 cleanup | Planned | Remove obsolete compatibility APIs only after downstream migration and a breaking-release table |

M1 is additive. It does not deprecate a legacy Linalg header or declare a
removal release.

## Family classification

| Inherited API family | Canonical direction | M1 disposition |
| --- | --- | --- |
| `TransposeMode`, `TriangleMode`, `DiagonalMode`, `SideMode` in `types.h` | narrow shared mode vocabulary | `TransposeMode` is canonical M1; the other modes remain source-compatible and are not M1 capability claims |
| `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2` in `blas.h` | context-first operations over canonical rank-one views | Seven-operation subset implemented; context-free overloads remain compatibility |
| `Gemv`, `Gemm` in `blas.h` | context-first operations over canonical rank-two/rank-one views | Seven-operation subset implemented; implicit resizing/global execution remain compatibility only |
| `Swap`, `Axpby`, `Asum`, `Iamax`, `Ger`, symmetric/triangular/banded BLAS | reviewed expansion of narrow BLAS headers | Deferred; legacy operations remain unchanged |
| pointwise transcendental, logical, comparison, classification, and general reductions in `blas.h` | Array shape algebra and context-driven evaluation | Future Array; legacy operations remain compatibility |
| `MatMul`, `Outer`, `TensorDot`, `Kron`, `Cross` | reviewed convenience/contraction APIs after Array shape algebra | Deferred; no canonical M1 replacement |
| `LUP`, `Cholesky`, and `QR` in `decomp.h` | reusable factor values with explicit workspace and solve contracts | Future factor/solver; legacy functions remain compatibility |
| SVD/eigen routines in `decomp.h` and `lapack.h` | provider-independent reusable decomposition results | Linalg M3; Eigen-gated compatibility remains unchanged |
| `Getrf`/`Getrs`/`Gesv`/`Getri`, `Potrf`/`Potrs`/`Posv`/`Potri`, `Geqrf`/`Gels` | canonical factor/solve APIs with stable status | Future factor/solver; integer-info wrappers remain compatibility |
| determinant, inverse, symmetric/general/generalized eigendecomposition helpers | operations built from reviewed reusable factors/decompositions | Future factor/solver or Linalg M3; current helpers remain compatibility |
| `LinearSolver`, automatic matrix-property inference, repeated/multiple solve | explicit provider-independent factor/solver values | Future factor/solver; current policy remains compatibility |
| `eigen.h` maps, conversions, `SpLinearSolver`, and Pardiso path | explicit optional adapter/provider layer | Future optional adapter/provider; current header remains compatibility |
| Eigen/MKL public target requirements | private provider dependencies that do not alter common API | Retained only for explicit compatibility headers during M1 |

No entry in a deferred row should be emulated by quietly routing a canonical
view through a legacy array, hidden copy, or optional adapter.

## Migrating the seven operations

Canonical overloads are distinguished by a required first
`ExecutionContext` argument:

```text
Copy(context, x, y)                         -> Status
Scal(context, alpha, x)                     -> Status
Axpy(context, alpha, x, y)                  -> Status
Dot(context, x, y)                          -> Result<T>
Nrm2(context, x)                            -> Result<T>
Gemv(context, mode, alpha, A, x, beta, y)   -> Status
Gemm(context, modeA, modeB,
     alpha, A, B, beta, C)                  -> Status
```

Migration is not just an added context argument. A caller must also:

1. include `<asc/array.h>` for operand construction and `<asc/linalg.h>` for
   operations; the Linalg umbrella does not re-export Array ownership;
2. represent storage with canonical `Tensor` or a checked canonical
   `TensorView`;
3. call `View()` explicitly instead of passing an owner;
4. preallocate the exact destination descriptor;
5. use one identical `float` or `double` value type for operands and scalars;
6. pass transpose, `alpha`, and `beta` explicitly;
7. handle `Status` or `Result<T>`;
8. obey the canonical alias and host-access rules.

The compatibility operation remains appropriate when a call still depends on
legacy `DenseMArray`, `Memory<T>`, implicit output management, expressions,
device mirroring, or a BLAS operation outside the seven-operation subset.

## Owner and view migration

Legacy BLAS accepts broad nominal array families and often resizes a dynamic
destination. Canonical Linalg accepts structural views only and never owns an
output allocation.

For a canonical call:

- construct and validate Array `Extents` and the chosen mapping;
- allocate a `Tensor` or create a checked external `TensorView` with the full
  available backing span and memory space;
- obtain mutable views only from mutable owners;
- retain the owner or external allocation for the complete synchronous call;
- allocate the result before calling Linalg and make its shape exact.

Do not wrap a legacy `Memory<T>` allocation in canonical `Buffer<T>` ownership.
There is no shared lifetime registry, mirror, or synchronization bridge. If a
zero-copy checked canonical view cannot be constructed without confusing
ownership or validity, keep that complete call chain on the compatibility
path.

## Execution and provider migration

Replace process-wide or destination-owned execution choice with an explicit
context:

```text
legacy Device / UseDevice / ASC_FORALL
                    -> ExecutionContext argument
```

The M1 canonical context is synchronous serial and the Linalg provider is
`serial-reference`. Enabling legacy OpenMP, CUDA, Eigen, or MKL options does not
change canonical dispatch. M1 never transfers operands, synchronizes a legacy
mirror, or falls back through another address space.

Call `GetLinalgCapabilities(context)` when provider support is a runtime
decision. Test `Supports(LinalgOperation::...)` and propagate an unavailable or
unsupported result. Do not infer capability from an enum value, preprocessor
macro, linked SDK, or the presence of a compatibility header.

## Shape, layout, and metadata migration

Legacy code often relies on integer extents, `DefaultLayout`, flattened storage
order, and destination resize. Canonical operations instead:

- use signed 64-bit extents and strides;
- validate matrix equations before pointer access;
- use each descriptor's actual logical-coordinate mapping;
- support canonical left, right, and proven-unique non-negative strides;
- preserve padded holes;
- reject overflow before allocation or dereference;
- leave a destination unchanged after any recoverable validation failure.

Code that depends on `ASC_USE_ROW_MAJOR` must select a canonical mapping type
explicitly. Do not flatten a stride view as if logical order necessarily
matches contiguous storage.

## Allocation and result migration

Legacy dynamic outputs may be resized by `Gemv`, `Gemm`, or convenience
wrappers. Canonical M1 deliberately has no allocation path:

```text
legacy: operation chooses or resizes result storage
new:    caller validates/allocates owner -> obtains view -> calls operation
```

This makes resource, lifetime, and failure ownership visible. Allocation
failure is handled while constructing the Array owner. A Linalg validation
failure then leaves that already-existing allocation byte-for-byte unchanged.

Empty shapes also have explicit semantics. A zero output dimension is a no-op;
a zero inner dimension still applies `beta`; `beta == 0` never reads the old
destination. Preserve these rules when removing special-case code from a
legacy caller.

## Aliasing migration

Historical routines do not provide one consistent overlap policy. Canonical
M1 permits only:

- exact descriptor self-copy as a no-op;
- in-place `Scal`;
- exact descriptor alias for `Axpy`;
- arbitrary read-only overlap for reductions;
- disjoint output/input storage for `Gemv` and `Gemm`.

Partial or conservatively detected overlap returns `kFailedPrecondition`
before the first write. If an algorithm intentionally uses overlapping
subviews, allocate an explicit separate result and perform a reviewed copy;
do not rely on incidental reference-loop traversal order.

## Numerical migration

Canonical `Dot`, `Gemv`, and `Gemm` accumulate in ascending logical inner
coordinates in the operand type. `Nrm2` uses a scaled sum-of-squares algorithm.
No cross-build bitwise promise, implicit extended accumulator, mixed precision,
condition estimate, or provider-specific tolerance is provided.

Existing code that compares results produced by Eigen, MKL, CUDA, or reordered
legacy loops should use a numerically justified tolerance. The deterministic
M1 claim is limited to operation order within the same built
`serial-reference` provider.

## Pointwise and general reduction migration

The name `blas.h` historically obscures ownership: much of the file implements
pointwise transcendental, logical, comparison, classification, and reduction
algorithms. Those operations belong to Array because their central concerns
are broadcasting, shape algebra, aliasing, and evaluation rather than linear
algebra.

Array M1 has no canonical replacement. Keep those calls on the compatibility
path until Array M3 defines the complete evaluation contract. Do not add them
to `<asc/linalg.h>` for convenience.

## Factorization and solver migration

M1 intentionally does not provide a canonical replacement for `LUP`,
`Cholesky`, `QR`, LAPACK-style wrappers, `LinearSolver`, or the Eigen sparse
solver hierarchy. Their current interfaces combine inconsistent ownership,
workspace, integer, failure, reuse, property-inference, and optional-provider
semantics.

Linalg M2 will freeze reusable LU and Cholesky values together with pivot,
workspace, repeated-solve, multiple-right-hand-side, finite-input, and
numerical-failure behavior. Linalg M3 will address QR/least-squares, SVD/eigen,
sparse/matrix-free operations, and duplicate solver removal. Until then, keep
each legacy factor/solve chain internally coherent and do not present it as a
canonical provider-independent API.

## Optional adapter and provider migration

`<asc/linalg/eigen.h>` and the current Eigen/MKL target requirements are
explicit compatibility mechanisms. They may expose provider-native types and
build-dependent declarations. The canonical umbrella includes none of them.

Future providers must implement the common canonical operation contract behind
a private SPI, keep SDK types out of public common headers, report capability
through status values, and pass the same shape, layout, alias, numerical,
dependency, package, and allocation tests as `serial-reference`.

## Random dependency

Random M1 removes Linalg from the minimal `ASC::random` target contract. Its
canonical Philox engine, unit transform, and fill link only Array and Core.

The inherited covariance-taking normal sampler still calls legacy `MatMul` and
is installed through the `ASC::cpp` aggregate, which already links Linalg and
Random. That compatibility include relationship is not a canonical
Random-to-Linalg component edge.

A later migration will make Linalg compute an explicit validated factor or
transform and Random consume it. Do not duplicate factorization inside Random
or promote the inherited covariance constructor merely because aggregate
compatibility keeps it available.

## Compatibility window

Legacy Linalg APIs remain available during the bounded pre-1.0 transition.
Removal requires:

1. a canonical replacement with frozen numerical, allocation, and failure
   contracts;
2. migration of Random and known downstream users;
3. canonical conformance and legacy regression coverage;
4. provider, package, dependency, sanitizer, and configuration gates;
5. a documented breaking release and removal table.

Linalg M1 itself neither deprecates a legacy header nor changes its source
behavior.

## Provenance

The compatibility headers were selected and adapted from MdeCpp as recorded in
[the migration inventory](inventory.md). Canonical M1 derives from the asc-cpp
architecture and the approved
[`linalg_design.md`](../design/linalg_design.md); it is not a rename of the
MdeCpp algebra hierarchy.
