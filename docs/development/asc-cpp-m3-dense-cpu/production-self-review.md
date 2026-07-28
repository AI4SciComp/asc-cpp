# Milestone 3 Dense CPU production self-review

Status: production implementation complete; independent verification pending

## Exact production inventory

Public headers:

```text
include/asc/dense.h
include/asc/dense/array.h
include/asc/dense/evaluate.h
include/asc/dense/export.h
include/asc/dense/layout.h
include/asc/dense/linalg.h
include/asc/dense/view.h
```

Compiled sources:

```text
src/dense/linalg.cc
```

There is exactly one compiled source. The lead-owned Dense target lists only
that source and the seven frozen headers. No production file outside the
assigned Dense paths was changed by the production engineer.

## Delivered contract

### Layout, views, and ownership

- `DenseLayout<Rank>` validates left, right, and caller-stride layouts with
  signed Core metadata. It checks negative inputs and every logical-size,
  offset, span, and conversion operation before publishing a mapping.
- Rank zero has logical/span size one. A zero extent has logical/span size
  zero. `LayoutLeft` increments dimension zero fastest; `LayoutRight`
  increments the last dimension fastest.
- Arbitrary non-negative strides use a conservative sorted-stride uniqueness
  proof and separately report exhaustiveness. Dense owners require both
  properties. All Milestone 3 views reject non-unique mappings because
  repeated-address views are deferred; padded unique mappings remain
  view-only.
- `DenseView<Element, Rank>` is trivially copyable and non-owning. It validates
  pointer, mapping, byte-span, and element mutability requirements.
  Mutable-to-const conversion is one-way. `At` is bounds checked and rejects
  non-host dereference. Rank-preserving `Subview` validates the whole region
  before publishing a zero-allocation descriptor.
- `DenseArray<Element, ExtentsType>` is move-only and owns a Core `Buffer` from
  an explicit host `MemoryResource`. Supported elements are non-cv arithmetic
  scalars other than `bool`, and are trivially copyable/destructible.
  Allocation is value initialized. `Clone` names the deep copy and requires an
  explicit resource and serial context. `DiscardResize` constructs the
  replacement completely before move assignment, so failure preserves the
  original owner and success invalidates prior views.

### Expressions and reductions

- `DenseView` specializes the storage-neutral adapter without introducing a
  reverse Expression dependency. It reports the lead-added terminal operation
  category, exact rank/shape, structure-preserving sparsity, checked host
  scalar reads, and a conservative address-span alias query.
- `Evaluate` validates context, host memory, exact rank/shape, and every
  destination element address for possible expression overlap before the
  first write. Exact descriptor self-assignment is a no-op; other possible
  overlap is rejected. Rank-zero scalar expansion is supported. Evaluation is
  allocation-free and traverses dimension zero fastest.
- `ReduceSum`, `ReduceMin`, and `ReduceMax` use the same deterministic logical
  traversal. Integral sum overflow is reported through Core checked
  arithmetic. Empty sum is zero; empty min/max return `kInvalidArgument`.

### Serial linear algebra

- The compiled float/double surface implements `Copy`, `Scal`, `Axpy`, `Dot`,
  scaled `Nrm2`, `Gemv`, and `Gemm` for their frozen rank-one/rank-two
  operands.
- Every operation validates serial backend, host storage, shapes, transpose
  values, and forbidden output overlap before mutation.
- `Copy` exact identity is a no-op. Exact identity is safe for `Axpy`; partial
  overlap is rejected. `Scal` mutates its sole operand in place.
- `Dot` and all matrix products use deterministic logical index order. `Nrm2`
  uses scaled sum-of-squares for finite values, propagates NaN, and returns
  infinity for one or repeated infinite magnitudes when no NaN occurs.
- `Gemv` and `Gemm` support only none/transpose. Their `beta == 0` branches do
  not evaluate the old destination value.
- No operation allocates, packs, transfers, synchronizes, dispatches to a
  provider, changes precision, or falls back.

## Review findings resolved

1. The frozen Dense adapter required a terminal operation category that did
   not exist in the Milestone 2 Expression enum. The lead appended
   `ExpressionOperation::kTerminal`, preserving every prior enumerator and
   numeric value. Dense uses that lead-owned compatibility correction.
2. Early review found missing direct `<concepts>` and `<limits>` includes in
   `view.h`; both are now included.
3. Cv-qualified and Boolean owner elements could otherwise produce deep
   initialization/reduction template failures. `DenseElement` now rejects
   them at constraint selection; constness remains an element property of
   `DenseView`.
4. Early const-view publication permitted non-unique mappings. Both mutable
   and const view factories now reject them, matching the milestone's explicit
   repeated-address deferral.
5. The first scaled-norm implementation could return zero for a leading NaN
   and NaN for repeated infinities. `Nrm2` now handles NaN and infinity
   explicitly before finite scaled accumulation.
6. The adapter alias query is directional by design, while `Evaluate`
   preflights every logical destination address. This closes both relative
   overlap directions without adding range state to the trivially copyable
   view descriptor.
7. Independent portability review found that a device-space `DenseView`
   embedded directly or in a nested expression could reach its fatal scalar
   read because the storage-neutral protocol had no access preflight. The lead
   added the optional, recursively propagated
   `ValidateExpressionAccess(context, expression)` hook without changing
   existing adapter requirements. The Dense adapter now returns
   `kUnsupported` for non-serial execution and `kMemoryAccess` for non-host or
   inaccessible storage. `Evaluate` invokes the recursive preflight before
   alias analysis or mutation.

## Production validation

All commands ran from the repository root.

```text
for each of the seven public headers:
  clang++-19 -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    -Iinclude -x c++ -fsyntax-only -
result: PASS, 7/7

for each of the seven public headers:
  clang++-19 -std=c++20 -fno-exceptions \
    -Wall -Wextra -Wpedantic -Werror \
    -Iinclude -x c++ -fsyntax-only -
result: PASS, 7/7

clang++-19 -std=c++20 -fno-exceptions \
  -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -fsyntax-only src/dense/linalg.cc
result: PASS

clang++-19 and g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  <representative rank-zero and mixed-extents owner/view/evaluate programs>
result: PASS

clang-format-19 --dry-run --Werror <seven headers and linalg.cc>
result: PASS

cmake -S . -B /tmp/asc-cpp-m3-production.mfoqqu \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug \
  -DBUILD_TESTING=OFF -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m3-production.mfoqqu \
  --target asc_dense -j2
result: PASS, GCC 11.4 Debug/static

CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-production-shared.QboKaC \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug \
  -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m3-production-shared.QboKaC \
  --target asc_dense -j2
result: PASS, Clang 19 Release/shared

g++ ... <owner/layout/view/evaluate/reduction smoke> &&
  /tmp/asc-cpp-m3-owner-smoke
result: PASS

g++ ... <all compiled linear-algebra operations, beta-zero,
  NaN/infinity and scaled norm smoke> &&
  /tmp/asc-cpp-m3-linalg-smoke
result: PASS

g++ ... <direct and nested device-source expression access smoke> &&
  /tmp/asc-cpp-m3-expression-access-smoke
result: PASS; both paths returned kMemoryAccess and left the destination
  unchanged

git diff --check -- include/asc/dense.h include/asc/dense \
  src/dense/linalg.cc
result: PASS
```

An earlier combined ad hoc smoke returned code 10 because its handwritten
`Dot` oracle expected `54.5` after the vector had been transformed to
`[7.5, 10, 0]`; the correct dot product with `[3, 4, 0]` is `62.5`.
Production code was unchanged. The corrected focused linear-algebra smoke
passed.

## Dependency, provenance, and non-claims

- Dense production includes only Core, Expression, and C++20 standard-library
  headers. It contains no Utilities, Random, Sparse, retired Array/Linalg,
  provider, SDK, or third-party dependency.
- The implementation was derived only from the frozen contract, approved
  ADRs, current Core/Expression APIs, and ordinary mathematical definitions.
  MdeCpp, deleted asc-cpp implementation/tests, the hardening branch, and
  external implementation/vector corpora were not inspected.
- Local GPU evidence is exactly **skipped**. This milestone has no provider or
  GPU implementation.
- Hosted MSVC and AppleClang compilation, package relocation, sanitizer
  execution, independent numerical oracles, and the allocation-free reference
  benchmark remain verification/integration evidence rather than production
  claims.
- Views remain non-owning. Clone/resize callers must honor owner/resource
  lifetimes, and successful resize invalidates all previous views.
