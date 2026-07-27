# Milestone 3 Production Self-Review

Status: Production implementation complete; independent integration review
continues

Date: 2026-07-26

Branch: `feature/asc-cpp-m3-dense-cpu`

## Scope and provenance

The production implementation is confined to the frozen ownership scope:

```text
include/asc/dense.h
include/asc/dense/array.h
include/asc/dense/evaluate.h
include/asc/dense/export.h
include/asc/dense/layout.h
include/asc/dense/linalg.h
include/asc/dense/view.h
src/dense/reference_linalg.cc
```

This report is the only production-agent documentation change. No CMake,
test, package, manifest, sibling-module, provider, or root file was edited by
the production agent.

The implementation was derived from the frozen Milestone 3 contract, accepted
ADRs, current core/expression APIs, and independently reported review
findings. No MdeCpp, deleted asc-cpp implementation, upstream implementation,
test corpus, or numerical table was inspected or copied. There is no new
dependency or third-party source.

## Public surface

### Layouts

`DenseLayoutMapping<Rank>` is a trivially copyable validated affine mapping
created with:

```text
Create(LayoutLeft, shape)
Create(LayoutRight, shape)
Create(LayoutStride, shape, strides)
```

It reports shape, strides, logical size, required span, uniqueness,
exhaustiveness, kind, and checked coordinate offsets.

`LayoutLeft` is column-major and `LayoutRight` is row-major. Empty named
mappings preserve canonical strides while representable. At a zero extent, or
when an otherwise overflowing product is irrelevant because a zero remains in
the propagation direction, propagation stops and remaining strides become
zero. Thus even huge zero-containing named shapes publish deterministic
zero-span mappings without unchecked arithmetic. Nonempty canonical-stride
overflow is rejected.

The `LayoutStride` uniqueness algorithm is a sufficient proof: active
dimensions are ordered by stride, and each next stride must be no smaller than
the already covered address span. It can conservatively reject a
mathematically unique exotic mapping, as approved. Unique mappings are
exhaustive exactly when required span equals logical size.

### Views and ownership

`DenseView<Element, Rank>` is a trivially copyable non-owning descriptor with
an element pointer, mapping, `MemorySpace`, and conservative alias identity.
It supplies checked `At`, rank-preserving `Subview`, exact-view comparison,
and physical byte-span `MayOverlap`. Mutable-to-const conversion is implicit
and one-way. Mutable views require proven uniqueness; const views may describe
repeated addresses.

Subviews allocate nothing, preserve strides, memory space, and alias identity,
and keep the parent pointer unchanged for empty end subviews to avoid invalid
pointer arithmetic. Host dereference rejects every non-host space.

`DenseArray<Element, ExtentsType>` is move-only and is deliberately bound to
the approved core `Extents<...>` family. Elements are unqualified arithmetic,
non-bool, trivially copyable, and trivially destructible values. `bool`,
`const`, and `volatile` owners are rejected at constraints.

Factories allocate through an explicit host `MemoryResource`, require a
unique/exhaustive left or right mapping, and value-initialize every element.
`Clone` names its destination resource and serial context and performs one
destination allocation plus an explicit core byte copy. `ResizeDiscard`
constructs a complete replacement before move assignment, so allocation or
metadata failure leaves the owner and prior views unchanged. Success
invalidates prior views.

### Expression evaluation and reductions

`DenseView` participates through `ExpressionAdapter` as a terminal,
structure-preserving readable expression.

`Evaluate` requires:

- an explicit serial context and host destination;
- exact destination rank/shape, except rank-zero scalar expansion;
- an exact result value type;
- validation before mutation;
- no allocation, packing, transfer, synchronization, or fallback.

Traversal increments dimension zero fastest. Exact direct-view assignment is
a no-op. Dense terminals are checked recursively through unary/binary nodes
using physical byte spans, including views independently created over
overlapping addresses. External adapters retain the protocol's conservative
`MayAlias` responsibility.

`ReduceSum`, `ReduceMin`, and `ReduceMax` use the same deterministic traversal.
Empty sum returns zero; empty minimum/maximum return `kInvalidArgument`.
Integral sum uses checked addition; floating sum follows ordinary IEEE
arithmetic. Bool-valued expressions are rejected cleanly at constraints.

### Serial linear algebra

The public float/double operations are:

```text
Copy, Scal, Axpy, Dot, Nrm2, Gemv, Gemm
```

`Copy`, `Scal`, and `Axpy` accept rank one or two. `Dot` and `Nrm2` accept
rank one. `Gemv` and `Gemm` accept `MatrixOperation::kNone` and
`kTranspose`. All readable operands independently accept mutable or
const-element views; outputs remain mutable.

Every operation validates serial execution, host memory, shape, operation
enumerators, and forbidden output overlap before mutation. `Copy` exact
self-identity is a no-op. `Axpy` exact same-index identity is supported;
partial overlap rejects. GEMV/GEMM outputs may not overlap an input.

`Dot` accumulates in logical index order. `Nrm2` uses scaled sum-of-squares and
handles zero, NaN, and infinity without a naive squaring overflow.
`beta == 0` branches before reading GEMV/GEMM output.

## Complexity and observable costs

| Operation | Time | Successful computational path |
| --- | --- | --- |
| layout construction | `O(Rank^2)` worst case for uniqueness sorting | none |
| checked element access | `O(Rank)` | none |
| subview | `O(Rank^2)` worst case | none |
| owner creation/resize | `O(N)` initialization | exactly one successful buffer allocation |
| clone | `O(N)` | exactly one destination allocation; no packing |
| evaluation | `O(N * Rank)` plus expression-node work | none |
| reductions | `O(N * Rank)` | none |
| Copy/Scal/Axpy/Dot/Nrm2 | `O(N)` | none |
| Gemv | `O(mn)` | none |
| Gemm | `O(mnk)` | none |

All M3 operations are synchronous serial CPU work. No provider dispatch,
precision conversion, fallback, hidden temporary, host/device transfer, or
hidden synchronization exists. The `none` entries above describe successful
computational storage, workspace, packing, transfer, and synchronization
behavior. Validation failures construct diagnostic `Status` objects backed by
`std::string` and can therefore allocate heap storage; M3 does not claim a
general no-heap guarantee for failure diagnostics.

## Review findings resolved

- **DOC-M3-01:** zero-extent mappings are vacuously unique/exhaustive; negative
  extents are validated before named-layout arithmetic.
- **DOC-M3-02:** bool and volatile dense elements are rejected; reductions
  reject external bool-valued expressions; linalg scalar types are exactly
  unqualified float/double.
- **M3-LEAD-01:** evaluation now recursively checks physical byte-span overlap,
  rather than relying only on alias-token equality.
- **V-M3-001:** rank-zero template paths use `if constexpr` guards and are
  strict-GCC warning clean.
- **DOC-M3-03:** mutable linalg sources now deduce independently and convert to
  const-element views internally.
- **DOC-M3-04:** huge zero-containing named mappings succeed without evaluating
  irrelevant overflowing stride products; nonempty overflow still fails.
- **DOC-M3-05:** `DenseExtents` recognizes only unqualified core
  `Extents<...>` specializations. Its recognition trait is private to
  `internal_dense_array`, preserving truthful noexcept move traits.
- The compiled source keeps `asc/dense/linalg.h` first as its owning header
  while remaining clang-format clean.
- Exact-view identity compares address, shape, strides, and space rather than
  the informational layout-kind enumerator.

## Production validation

### Formatting and strict self-contained compilation

Passed:

```bash
clang-format-19 --dry-run --Werror \
  include/asc/dense.h include/asc/dense/*.h \
  src/dense/reference_linalg.cc
```

For both `g++-11` 11.4.0 and `clang++-19` 19.0.0, every dense public header
passed individually with exceptions enabled and disabled:

```bash
<compiler> -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  -x c++ -fsyntax-only <header>

<compiler> -std=c++20 -fno-exceptions -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  -x c++ -fsyntax-only <header>
```

The compiled source passed both compilers with the same strict flags:

```bash
<compiler> -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror \
  -DASC_DENSE_BUILDING_LIBRARY -Iinclude \
  -c src/dense/reference_linalg.cc -o /tmp/m3-reference-<compiler>.o
```

Additional strict GCC/Clang syntax probes passed for:

- mutable sources to all seven linalg APIs;
- rank-zero layout/view/evaluation instantiation;
- accepted core `Extents<...>`;
- rejected fake, const, and volatile extents;
- nothrow DenseArray move construction and assignment.

`git diff --check` passed for the production paths.

### Actual CMake target

The provider-free static production target configured and built successfully:

```bash
cmake -S . -B /tmp/asc-cpp-m3-production-gcc-lib \
  -DCMAKE_CXX_COMPILER=g++-11 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m3-production-gcc-lib --parallel 4
```

Result: `asc_dense` and all predecessor targets built successfully.

### Integrated independent dense suite

After all production corrections:

```bash
cmake -S . -B /tmp/asc-cpp-m3-production-gcc \
  -DCMAKE_CXX_COMPILER=g++-11 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m3-production-gcc --parallel 4
ctest --test-dir /tmp/asc-cpp-m3-production-gcc \
  --output-on-failure -L dense
```

Result: **27/27 passed**. This includes 14 self-contained header cases with
exceptions on/off, layout/view/owner/evaluation/linalg runtime cases,
compile/multi-TU/negative contracts, the allocation-free benchmark, and three
dense consumer/package cases.

A separate strict runtime smoke passed mutable-source strided GEMV and Dot,
extreme finite scaled `Nrm2`, `beta == 0` NaN-output avoidance, and huge empty
named mappings.

### Independent matrix handoff

A Clang 19 shared integrated build found one verification-owned warning after
the final `DenseExtents` correction:

```text
tests/compile/m3_dense_contract.cc:22:
unused variable 'kRank' [-Werror,-Wunused-const-variable]
```

This is outside the production write scope and was reported to the lead and
verification agent. Production headers/source themselves compile strictly
with Clang 19, and the 18 dense tests whose executables were built before that
stop passed. The independent verifier/lead owns the corrected clean rerun.

GPU evidence: **skipped**. M3 contains no provider or GPU code.

## Residual limitations and risks

- `DenseView` cannot prove that its owner still lives; users must preserve
  owner and async lifetimes as documented.
- Arbitrary-stride uniqueness is intentionally conservative.
- Byte-span overlap is conservative for padded views and can reject disjoint
  logical elements within intersecting physical bounding spans.
- External expression adapters must truthfully implement readable host
  behavior and conservative `MayAlias`; the current storage-neutral expression
  protocol has no general placement query.
- Host memory is the only M3 dereference/execution space.
- No optimized provider, parallel algorithm, complex/mixed precision,
  factorization, solver, tensor contraction, or GPU behavior is claimed.
- Local production evidence covers GCC 11 and Clang 19. MSVC, AppleClang,
  hosted shared-library packaging, and sanitizers remain part of lead and
  portability verification.

No production release blocker is known within the assigned scope.
