# Milestone 3 documentation and API review

Status: complete on 2026-07-28

## Scope and authority

This independent review covers only the frozen Milestone 3 Dense CPU contract.
Its authority is `main:AGENTS.md`; the M3 contract, ledger, and provenance
record; ADRs 0003, 0004, 0007--0011, and 0013; the M2 API and Core/Expression
guides; and the seven stable M3 public headers.

The review did not use MdeCpp, the deleted asc-cpp implementation, or any M3
implementation, test, report, or documentation from a later branch. It does
not treat Sparse, Random storage integration, an optimized provider, a GPU
facet, or a compatibility aggregate as implemented.

## Inventory and dependency result

The public inventory is exact:

```text
include/asc/dense.h
include/asc/dense/array.h
include/asc/dense/evaluate.h
include/asc/dense/export.h
include/asc/dense/layout.h
include/asc/dense/linalg.h
include/asc/dense/view.h
```

The sole compiled production source is `src/dense/linalg.cc`. Template
mapping, view, owner, evaluation, and reduction definitions remain in their
owning headers.

| Build target | Consumer target | Kind | Direct ASC dependencies |
| --- | --- | --- | --- |
| `asc_dense` | `ASC::dense` | Compiled | `ASC::core`, `ASC::expression` |

No public header or compiled source introduces a Utilities, Random, Sparse,
retired Array/Linalg, provider SDK, or external dependency.

## API review result

No unresolved API or documentation defect remains.

### Layouts and metadata

- `DenseLayout<Rank>::Create` has exact `LayoutLeft`, `LayoutRight`, and
  `LayoutStride<Rank>` overloads.
- Rank is a checked `std::size_t` template argument; extents, coordinates, and
  strides use the signed Core metadata types.
- Left layout is column-major, right layout is row-major, and explicit
  strides are non-negative.
- Construction validates negative metadata and logical-size, stride,
  maximum-offset, required-span, and conversion overflow before publication.
- Rank zero has logical and required span size one. Any zero extent gives
  both sizes zero.
- `Offset` checks every coordinate. `is_unique()` and `is_exhaustive()` expose
  the conservative mapping proof; a mathematically unique mapping may be
  rejected when the proof is unavailable.
- Logical traversal increments dimension zero fastest regardless of layout.

### Views and expressions

- `DenseView<Element, Rank>::Create` rejects a null nonempty pointer,
  byte-span overflow, and every non-unique mapping.
- A view is trivially copyable and non-owning. It retains a pointer, mapping,
  and `MemorySpace`, but no owner, resource, deleter, or synchronization.
- `At` returns `Result<Element*>`, checks coordinates, and permits dereference
  only for `MemorySpace::kHost`.
- Mutable-to-const conversion is one-way. A const owner exposes only a
  const-element view.
- `Subview` is rank preserving, allocation free, stride and space preserving,
  region checked, and transactional.
- `ExpressionAdapter` reports exact shape and rank,
  `ExpressionOperation::kTerminal`, structure-preserving sparsity, value
  reads, and conservative address-range aliasing.

An externally constructed view is only a descriptor. The caller guarantees
that the allocation covers the mapping's span and remains alive; a raw pointer
does not expose its allocation capacity to the factory.

### Ownership and lifetime

- `DenseArray<Element, ExtentsType>` is move-only and accepts Core
  mixed-static/dynamic `Extents`.
- Owner elements are non-cv arithmetic types other than `bool`; linear
  algebra is narrower at `float` and `double`.
- `Create` takes an explicit host `MemoryResource`, defaults to `LayoutLeft`,
  also accepts `LayoutRight`, checks bytes, and value-initializes storage.
- Owners publish only unique, exhaustive mappings. Explicit-stride and padded
  mappings remain view-only.
- `Clone` names the deep copy and requires an explicit destination resource
  and serial context.
- `DiscardResize` preserves the current layout unless another supported
  contiguous layout is requested. Failure leaves the owner and old views
  unchanged; success discards values and invalidates all old views.
- The resource is non-owning state and must outlive final deallocation. The
  type cannot detect a dangling view after destruction, move, or resize.

### Evaluation and reductions

- `Evaluate` takes a `ReadableExpression`, explicit serial context, and
  caller-owned mutable host destination with exactly matching value type.
- Ranked input requires exact rank and extents. A rank-zero input is the sole
  expansion.
- Context, memory, shape, and alias validation completes before mutation.
  Exact direct-view self-assignment is a no-op; every other possible
  destination overlap is rejected.
- Alias validation queries every logical destination address before writing.
  Padded holes may cause conservative rejection but cannot hide real overlap.
- Evaluation and `ReduceSum`, `ReduceMin`, and `ReduceMax` use deterministic
  dimension-zero-fastest order and add no allocation, packing, transfer,
  synchronization, or fallback.
- Empty sum returns zero. Empty minimum and maximum return
  `kInvalidArgument`. Integral sum overflow returns a status.

### Serial dense linear algebra

- The compiled overloads are exactly `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`,
  `Gemv`, and `Gemm` for `float` and `double`.
- Copy, Scal, and Axpy accept ranks one and two. Dot and Nrm2 are rank one;
  Gemv and Gemm have their conventional rank-one/rank-two operands.
- `DenseTranspose` has only `kNone` and `kTranspose`; unknown values fail
  before mutation and conjugation is unavailable.
- Each operation checks serial execution, host memory, complete shape, and
  forbidden output span overlap before mutation. Padding makes overlap
  decisions conservative.
- Copy exact identity is a no-op. Axpy exact same-index identity and in-place
  Scal are supported.
- Dot uses logical order. Nrm2 uses scaled finite accumulation and explicitly
  propagates NaN and infinity.
- Gemv and Gemm branch on `beta == 0` before accessing the old output.

The reference loops make no optimized-provider or stable speed claim and do
not allocate, pack, transfer, change precision, synchronize, or fall back.

## Findings and resolutions

### A1: Dense terminal operation category was absent

Initial severity: release blocking; outside the production writer's scope.

M2 `ExpressionOperation` ended at `kMultiply`, while M3 requires terminal
metadata. The lead appended `kTerminal` without renumbering the six existing
values, and the Expression guide records the compatibility addition. Closed.

### A2: header and element constraints were incomplete

Initial severity: medium.

An early `view.h` used `std::same_as` and `std::numeric_limits` without their
direct headers. The first owner constraint also admitted cv elements and
`bool`, causing invalid mutation or a deep `ReduceSum` constraint failure.

The header now includes what it uses. `DenseElement` rejects cv types and
`bool` at the public constraint, leaving constness to `DenseView`. Closed.

### A3: const views admitted repeated addresses

Initial severity: release blocking.

The first `DenseView<const Element, Rank>::Create` accepted a non-unique
mapping even though repeated-address views are deferred. Every M3 view now
requires proven uniqueness; mutable and const non-unique construction fail
before publication. Closed.

### N1: scaled Nrm2 mishandled non-finite values

Initial severity: release blocking.

The first scaled loop could return zero for a leading NaN and produce NaN
from repeated infinities. `Nrm2` now returns quiet NaN for NaN input, records
infinity without `inf / inf`, returns infinity when applicable, and retains
scaled sum-of-squares for finite values. Closed.

## Documentation result

`docs/modules/dense.md` now:

- gives the exact package target and two direct dependency edges;
- documents rank-zero/zero-extent mapping, offsets, uniqueness,
  exhaustiveness, padding, and overflow;
- explains view constness, fallible access, subviews, external-storage
  lifetime, and Expression participation;
- explains resource lifetime, clone, resize rollback, and invalidation;
- documents exact evaluation, reduction, alias, allocation, packing,
  transfer, and traversal behavior;
- lists the exact reference operations, ranks, types, transpose modes,
  `beta == 0`, and non-finite behavior;
- supplies checked mapping, owner, access, evaluation, and reduction examples;
  and
- identifies every deferred provider, GPU, solver, and compatibility boundary.

## Independent validation

```sh
for header in include/asc/dense.h include/asc/dense/array.h \
  include/asc/dense/evaluate.h include/asc/dense/export.h \
  include/asc/dense/layout.h include/asc/dense/linalg.h \
  include/asc/dense/view.h; do
  printf '#include <%s>\nint main() { return 0; }\n' \
    "${header#include/}" |
    g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Iinclude \
      -x c++ -fsyntax-only -
  printf '#include <%s>\nint main() { return 0; }\n' \
    "${header#include/}" |
    g++ -std=c++20 -Wall -Wextra -Werror -pedantic -fno-exceptions \
      -Iinclude -x c++ -fsyntax-only -
done
```

Result: pass, 14/14 strict standalone header configurations.

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Iinclude \
  -fsyntax-only src/dense/linalg.cc
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -fno-exceptions \
  -Iinclude -fsyntax-only src/dense/linalg.cc
clang++-19 -std=c++20 -Wall -Wextra -Werror -pedantic -Iinclude \
  -fsyntax-only src/dense/linalg.cc
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Iinclude \
  -fsyntax-only /tmp/asc-cpp-m3-dense-doc-example.cc
clang++-19 -std=c++20 -Wall -Wextra -Werror -pedantic -Iinclude \
  -fsyntax-only /tmp/asc-cpp-m3-dense-doc-example.cc
git diff --check -- docs/modules/dense.md \
  docs/development/asc-cpp-m3-dense-cpu/documentation-api-review.md
```

Result: pass. The temporary example combined the guide's mapping,
mixed-dynamic owner, mutable access, scalar evaluation, and reduction
snippets; it was removed after validation. Full link, runtime, package,
relocation, sanitizer, and isolated-consumer results remain lead-owned
integration evidence, not claims of this API review.

## Remaining risks

- The 0.3 API is pre-1.0 and may change at a later minor release.
- Raw-pointer views cannot validate allocation capacity or lifetime. Resource,
  owner, view, expression-capture, and concurrency lifetimes remain caller
  responsibilities.
- Successful resize invalidation and dangling view use cannot be diagnosed by
  the descriptor type.
- Conservative uniqueness and overlap analysis may reject a mathematically
  safe stride mapping or disjoint padded span.
- Deterministic reference accumulation is not a correctly rounded or
  overflow-free promise; ordinary IEEE results remain order sensitive.
- Hosted MSVC/AppleClang, runtime tests, sanitizers, relocation, isolated
  consumers, and the allocation-free benchmark remain integration evidence.
- No GPU facet or provider exists. GPU evidence is exactly **skipped**.
