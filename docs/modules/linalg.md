# Linalg module

> **Superseded historical document.** This file describes the deleted implementation at historical HEAD `33b261ea33616a6395c4ad3b20646093103344f7`; it is retained only for auditability and is not current API, build, package, or implementation guidance. See the [approved Stage A six-module blueprint](../development/asc-cpp-architecture/architecture-blueprint.md).

`ASC::linalg` owns numerical linear-algebra operations over canonical Array
views. Linalg Milestone 1 (M1) adds one small, deterministic, synchronous BLAS
path beside the broad MdeCpp-derived Linalg headers retained for compatibility.

| Surface | M1 status | Intended use |
| --- | --- | --- |
| `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2` | Canonical M1 | Rank-one `float`/`double` views |
| `Gemv` | Canonical M1 | Rank-two matrix/vector products |
| `Gemm` | Canonical M1 | Rank-two matrix products |
| `LinalgCapabilities` and `GetLinalgCapabilities` | Canonical M1 | Query the selected compiled provider |
| `blas.h`, `decomp.h`, `lapack.h`, and `eigen.h` | Compatibility, unchanged by M1 | Existing legacy-array callers |
| Factors, solvers, sparse/matrix-free operations | Deferred to Linalg M2--M3 | Do not rely on them yet |
| Eigen, MKL, BLAS/LAPACK, and CUDA canonical providers | Deferred provider milestones | Compatibility options are not canonical providers |

The canonical path is:

```text
ExecutionContext + TensorView descriptors
                  |
                  v
      validation and dispatch facade
                  |
                  v
   compiled serial-reference float/double kernels
```

No canonical Linalg operation consults the legacy process-wide `Device`,
selects a global default layout, resizes an output, transfers storage, or
implicitly chooses an optional third-party implementation.

## Include and component

New code uses the canonical umbrella and the narrow installed component:

```cpp
#include <asc/linalg.h>
```

```cmake
find_package(ASCCpp REQUIRED COMPONENTS linalg)
target_link_libraries(my_target PRIVATE ASC::linalg)
```

`ASC::linalg` is a compiled target with direct public dependencies on
`ASC::array` and `ASC::core`. The umbrella contains canonical concepts, modes,
capabilities, and BLAS1/2/3 declarations. It does not include any compatibility
monolith, optional-provider header, or Array owner/descriptor construction
surface. Code that constructs canonical operands explicitly includes
`<asc/array.h>` as well as `<asc/linalg.h>`.

During migration, `<asc/cpp.h>` includes `<asc/linalg.h>` and continues to
include the legacy Linalg headers separately. Reusable libraries should avoid
the aggregate and link only the components they use.

The installed Linalg-only consumer includes `<asc/array.h>` for canonical
owners/views and `<asc/linalg.h>` for operations. It verifies one BLAS1
operation, one matrix operation, status handling, and linkage through
`ASC::linalg` alone.

## Exact M1 operation set

M1 implements exactly seven operations in the flat `asc` namespace. A
mandatory `ExecutionContext` first argument distinguishes them from historical
context-free overloads.

| Header | Operation | Mathematical result |
| --- | --- | --- |
| `<asc/linalg/blas1.h>` | `Copy(context, x, y)` | `y[i] = x[i]` |
| `<asc/linalg/blas1.h>` | `Scal(context, alpha, x)` | `x[i] = alpha * x[i]` |
| `<asc/linalg/blas1.h>` | `Axpy(context, alpha, x, y)` | `y[i] = alpha * x[i] + y[i]` |
| `<asc/linalg/blas1.h>` | `Dot(context, x, y)` | ascending-order sum of `x[i] * y[i]` |
| `<asc/linalg/blas1.h>` | `Nrm2(context, x)` | scaled sum-of-squares Euclidean norm |
| `<asc/linalg/blas2.h>` | `Gemv(context, transpose, alpha, a, x, beta, y)` | `y = alpha * op(A) * x + beta * y` |
| `<asc/linalg/blas3.h>` | `Gemm(context, transpose_a, transpose_b, alpha, a, b, beta, c)` | `C = alpha * op(A) * op(B) + beta * C` |

Mutating operations return `Status`. `Dot` and `Nrm2` return `Result<T>`. M1
provides no default context, transpose mode, `alpha`, or `beta`, and never
allocates or resizes a result.

The following similarly named families are not added to the canonical surface
in M1: `Swap`, `Axpby`, `Asum`, `Iamax`, `Ger`, symmetric/triangular/banded
BLAS, `MatMul`, `Outer`, `TensorDot`, `Kron`, and `Cross`.

## Operand and scalar contract

Canonical operands are structural readable or writable Linalg vectors and
matrices built on Array's tensor concepts:

- vectors have exact compile-time rank one;
- matrices have exact compile-time rank two;
- owners are not operation arguments; call `Tensor::View()` explicitly;
- writable views carry non-const element authority and must have a mapping
  whose uniqueness is proven;
- all operands and scalar parameters in one call have exactly the same
  `ValueType`;
- M1 accepts only `float` and `double`.

Integer, Boolean, long-double, mixed-precision, complex, automatic-
differentiation, and custom scalar calls fail template constraints rather than
silently narrowing or selecting a compatibility overload. `asc::real_t`
remains a build-selected convenience type; it does not restrict a caller from
using either supported M1 scalar type.

## Shape and transpose rules

BLAS1 source and destination vector extents must match. For the matrix
operations:

```text
Gemv: x.extent(0) == op(A).extent(1)
      y.extent(0) == op(A).extent(0)

Gemm: op(A).extent(1) == op(B).extent(0)
      C.extent(0) == op(A).extent(0)
      C.extent(1) == op(B).extent(1)
```

`TransposeMode::kNoTranspose` uses stored logical coordinates.
`TransposeMode::kTranspose` exchanges the two axes. Any other underlying enum
value returns `kInvalidArgument`. M1 has no conjugate-transpose mode.

All mode, shape, metadata, access, uniqueness, and alias validation completes
before a provider reads input data or writes output data.

## Empty inputs and alpha/beta

- Empty `Copy`, `Scal`, and `Axpy` calls succeed without accessing storage.
- `Dot` and `Nrm2` on valid empty vectors return positive `T{0}`.
- A zero output dimension makes `Gemv` or `Gemm` a successful no-op.
- A zero inner dimension still evaluates `y = beta*y` or `C = beta*C`.
- When `beta == T{0}`, `Gemv` and `Gemm` do not read the old destination.

The last rule permits a destination containing uninitialized or NaN values to
be mathematically overwritten without contaminating the result. M1 makes no
corresponding `alpha == 0` no-read promise. Other NaN and infinity values
propagate through ordinary `float` or `double` arithmetic.

## Layout, memory, and lifetime

The serial reference provider traverses logical coordinates using every
operand's real strides. Canonical left, right, and proven-unique non-negative
stride mappings are supported. Padded storage holes are not touched unless a
logical coordinate maps to them.

Before dispatch, every nonempty operand must provide:

- a non-null raw data handle;
- a sufficient available span;
- checked 64-bit extent, stride, span, and byte-range arithmetic;
- host-accessible memory;
- proven uniqueness for an output mapping.

The operation does not own or extend operand lifetime. All owners and external
allocations must remain alive for the synchronous call. M1 performs no
allocation, synchronization, host/device transfer, materialization, or
fallback through a different address space.

## Aliasing and failure transactions

Linalg compares conservative touched byte intervals. Interleaved layouts may
therefore be rejected even when their logical coordinates do not collide, but
an actual overlap must never be missed.

| Operation | Allowed storage relationship |
| --- | --- |
| `Copy` | Disjoint descriptors, or exactly the same descriptor as a no-op |
| `Scal` | Its documented in-place operand |
| `Axpy` | Disjoint descriptors, or exactly equal `x` and `y` descriptors |
| `Dot`, `Nrm2` | Arbitrary overlap among read-only inputs |
| `Gemv` | `y` must not overlap `A` or `x` |
| `Gemm` | `C` must not overlap `A` or `B`; `A` and `B` may overlap |

Exact descriptor equality includes the data handle, rank, every extent, and
every stride. Forbidden or conservatively detected partial overlap returns
`kFailedPrecondition`.

Every recoverable validation failure occurs before the first output write and
leaves all output bytes unchanged. Arithmetic itself has no recoverable
mid-kernel status in M1.

## Numerical order and determinism

`Dot`, `Gemv`, and `Gemm` accumulate in the operand type in ascending logical
inner-coordinate order. `Nrm2` uses a LAPACK-style scaled sum-of-squares
update, avoiding the avoidable overflow or underflow of directly evaluating
`sqrt(sum(x*x))` for finite representable inputs.

The provider introduces no extended accumulator, reassociation policy,
condition estimate, or provider-specific tolerance. Results follow the active
C++ floating-point environment and compiler contraction rules.

`LinalgCapabilities::IsDeterministic()` means that repeated execution by the
same built provider on the same inputs follows the same operation order. It is
not a bitwise guarantee across compilers, architectures, build flags, or
floating-point environments.

## Capability query

`<asc/linalg/capabilities.h>` provides:

```cpp
enum class LinalgOperation {
  kCopy, kScal, kAxpy, kDot, kNrm2, kGemv, kGemm
};
```

`GetLinalgCapabilities(context)` returns a `LinalgCapabilities` value for a
supported context. For the M1 serial context, the frozen contract reports:

- backend `BackendKind::kSerial`;
- provider name `serial-reference`;
- all seven operations supported;
- deterministic operation order.

The query reports actual provider capability, not merely an enum or build
option. M1 cannot construct another valid Core context. Future unavailable
providers with fallback disallowed return `kUnavailable`; any future
same-space reference fallback remains limited to host-accessible operands and
must not transfer them.

## Status contract

| Condition | Reported code |
| --- | --- |
| Successful mutation | `StatusCode::kOk` |
| Shape mismatch or invalid transpose value | `StatusCode::kInvalidArgument` |
| Insufficient or overflowed extent, stride, span, or byte-range arithmetic | `StatusCode::kOverflow` |
| Missing data handle for a nonempty operand | `StatusCode::kFailedPrecondition` |
| Non-unique output or forbidden overlap | `StatusCode::kFailedPrecondition` |
| Non-host storage or unsupported scalar/layout capability | `StatusCode::kUnsupported` |
| Requested execution provider absent with fallback disallowed | `StatusCode::kUnavailable` |
| Future native provider failure | `Status::FromProvider(...)` |

Reduction failures use the same codes through `Result<T>`. Non-finite
arithmetic remains a value result; the seven M1 operations do not return
`kNumericalFailure`. Rank and unsupported scalar errors are compile-time
constraint failures.

## Compatibility boundary

These installed headers remain available for existing code but do not define
canonical M1 behavior:

- `<asc/linalg/blas.h>`: context-free pointwise, reduction, BLAS, and
  contraction operations over legacy arrays;
- `<asc/linalg/decomp.h>`: portable LUP, Cholesky, QR, and optional
  Eigen-backed decomposition functions;
- `<asc/linalg/lapack.h>`: LAPACK-style wrappers, automatic solver policy, and
  `LinearSolver`;
- `<asc/linalg/eigen.h>`: optional Eigen maps, conversions, and another solver
  hierarchy.

M1 does not change those headers' integer sizes, output resizing, failure
translation, global execution behavior, optional feature gates, or legacy
array requirements. Optional Eigen and MKL usage requirements remain part of
this explicit compatibility path; they do not change the canonical umbrella
or turn either library into an M1 canonical provider.

Pointwise, transcendental, logical, comparison, and general reduction
operations currently found in legacy `blas.h` belong to a future canonical
Array evaluation layer. Decompositions, factorizations, and solvers return to
Linalg only after their own reviewed contracts exist.

## Random compatibility boundary

Canonical `ASC::random` no longer depends on `ASC::linalg`. Its M1 engine,
unit-uniform transform, and bulk fill use only canonical Core and Array.

The inherited covariance-taking `NormalSampler` still includes legacy
`blas.h`, computes Cholesky internally, and calls legacy `MatMul`. That header
is installed through `ASC::cpp` compatibility rather than the minimal Random
component. A future reviewed migration will make Linalg produce an explicit
validated factor and Random consume it without introducing a target edge.
Linalg M1 does not provide that factor API, and Random M1 does not duplicate
factorization.

## Deferred work

Linalg M2 is reserved for reusable LU and Cholesky factor values with explicit
workspace, pivots, repeated-solve, multiple-right-hand-side, finite-input, and
numerical-failure contracts. Linalg M3 covers QR/least-squares, SVD/eigen,
sparse and matrix-free operations, and removal of duplicate legacy solver
hierarchies.

BLAS/LAPACK, Eigen, MKL, and CUDA implementations require a private provider
SPI and the same conformance suite as `serial-reference`. Their SDK headers
must not enter common canonical headers. Tensor contractions and convenience
operations wait for reviewed Array shape algebra and evaluation.

See [Linalg migration](../migration/linalg.md) for old-to-new guidance and the
approved [`linalg_design.md`](../design/linalg_design.md) for the frozen M1
contract.
