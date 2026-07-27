# Milestone 4 Documentation and API Review

Status: Complete; accepted after corrections

Date: 2026-07-26

Role: documentation and API reviewer

Writable scope:

```text
docs/modules/expression.md
docs/modules/sparse.md
docs/development/asc-cpp-m4-sparse-cpu/documentation-api-review.md
```

## Review boundary

This review is independent of production implementation and verification. It
uses the frozen Milestone 4 contract, accepted ADRs, actual public headers,
installed target behavior, and independently exercised examples. It does not
inspect or copy MdeCpp, the user-deleted implementation, historical sparse
tests, third-party sparse implementations, or provider SDK examples.

The review covers:

- public naming, constraints, and useful compile diagnostics;
- coordinate, CSR, and CSC canonical invariants;
- duplicate and explicit-zero policies;
- ownership, lifetime, const propagation, move state, and invalidation;
- external-view metadata and pointer obligations;
- failure transactions and partial-allocation rollback;
- allocation, workspace, conversion, and densification disclosure;
- expression placement, writable, sparsity, and alias contracts;
- CSR SpMV numerical order and destination requirements;
- memory placement, execution, transfer, and synchronization;
- concurrency and absence of hidden mutable state;
- component isolation and installed usability; and
- provider, performance, GPU, license, and provenance claim boundaries.

The current official Google C++ Style Guide was checked at
`https://google.github.io/styleguide/cppguide.html`. Its current C++20,
self-contained-header, Include-What-You-Use, type/concept/function/accessor
naming, explicit-ownership, and exception guidance agree with ADRs 0003,
0004, and 0009.

## Authoritative material read

- the complete team runbook version 2.0;
- the frozen M4 milestone contract, ownership ledger, dependency audit, and
  provenance record;
- ADRs 0001--0004, 0007--0010, 0012, 0014, and 0016--0018;
- current core, utilities, expression, random, and dense module guides;
- ASCCpp component, package-config, target, install/export, and isolated
  consumer conventions;
- the emerging M4 public headers and their direct dependencies;
- the M4-PORT-01/M4-PORT-02 production resolutions and focused verification
  evidence; and
- the official current Google C++ Style Guide.

No applicable repository `AGENTS.md` was present in the retained restart tree.

## Findings

### M4-DOC-01: top-level const writable concept mismatch

Initial severity: release-blocking API constraint defect

Initial evidence: `WritableExpression<const T>` could satisfy the concept
because adapter lookup removed cv/ref qualifiers, while `WriteExpression`
subsequently passed a const object to an adapter requiring `Write(T&, ...)`.
The resulting failure occurred inside a constrained public function body
rather than at concept selection.

Required resolution: reject a top-level-const destination at the
`WritableExpression` boundary or support it consistently through the complete
adapter and function contract.

Resolution: production now makes a top-level-const destination fail the
`WritableExpression` concept. This restores failure at the constrained API
boundary. Independently verified with a negative `static_assert` through the
installed public target.

### M4-DOC-02: sparse external structure/value overlap

Initial severity: release-blocking invariant and lifetime defect

Initial evidence: external coordinate-view creation validated the coordinate
and value spans independently but did not reject their byte-span overlap. A
mutable value write could therefore change bytes published as const canonical
structure.

Required resolution: reject conservative overlap between every structural
span and every mutable or const value span for coordinate and compressed
external views, before publication; add negative verification.

Resolution: independently inspected production now rejects nonempty
coordinate/value overlap and all pairwise compressed
outer-offset/inner-index/value overlap after checked address-range validation.
Independent installed runtime cases returned `kInvalidArgument`.

### M4-DOC-03: raw span access for non-host sparse views

Initial severity: API-contract ambiguity

Initial evidence: early public sparse views exposed ordinary iterable spans
for structure and values even when their explicit memory space was non-host.
The names and types could imply host iteration despite checked `ValueAt` and
`Find` correctly rejecting such access.

Required resolution: preserve backend-neutral raw address observation without
advertising unchecked host iteration.

Resolution: production removed the convenience span accessors and retained
raw `coordinate_data`, `value_data`, `outer_offset_data`, and
`inner_index_data` address descriptors, consistent with `DenseView::data()`.
The module guide states that a raw address does not authorize host
dereference. Checked stored-entry and lookup operations remain host-only.

### M4-DOC-04: incomplete sparse SpMV output

Initial severity: release-blocking numerical/API defect

Initial evidence: a mutable rank-one `CoordinateView` satisfied
`WritableExpression` even when its canonical structure omitted logical
coordinates. Early SpMV validation checked only its logical shape, after which
the coordinate adapter silently ignored writes to missing entries and the
operation returned success with an incomplete result.

Required resolution: validate total writable coverage before mutation for an
ASC sparse rank-one destination, or remove sparse-view writable participation.

Resolution: `ValidateTotalWritable` now requires a coordinate output to store
exactly every index `0..extent-1` in canonical order. Failure is reported
before the kernel runs. External writable adapters retain the documented
semantic obligation to make every in-shape coordinate a unique destination.
An independent installed runtime case passed a shape-two output containing
only coordinate one; SpMV returned `kInvalidArgument` and left its stored value
unchanged.

### M4-DOC-05: overloaded address-of in external SpMV operands

Initial severity: generic API compatibility defect

Initial evidence: SpMV used unary `&input` and `&output` when erasing
otherwise protocol-conforming external types for the compiled kernel. A type
could satisfy every declared concept while overloading unary `operator&`,
causing compilation failure or supplying the wrong address.

Required resolution: obtain the actual object address independently of an
overloaded operator.

Resolution: SpMV now uses `std::addressof` for both objects. Independent
installed compilation and runtime succeeded with an external input/output type
whose mutable and const unary address-of operators were deleted.

### M4-PORT-01: identity-only tokens miss partial span overlap

Initial severity: release-blocking alias defect

Initial evidence: root views published only their starting identity. When an
external range began before an overlapping ASC value range, its identity point
lay outside the ASC range even though later bytes intersected it. Equality or
one-direction point containment could therefore permit a mutating operation on
partially overlapping storage.

Required resolution: storage-neutral, symmetric, overflow-checked byte-span
alias metadata without adding storage ownership or a sibling dependency.

Resolution: `AliasToken::FromAddressSpan` validates an optional half-open byte
span, and `AliasTokensMayOverlap` compares span/span and span/identity pairs
symmetrically. Root dense and sparse views publish complete validated physical
value spans; dense subviews retain their root span conservatively. Identity
equality remains source-compatible, and zero-byte spans never overlap.

An independent installed probe passed both comparison directions for an
external span starting before an overlapping later span, disjoint half-open
boundaries, points inside and outside a span, a null zero-byte span, and
rejection of a null nonempty span.

### M4-PORT-02 and M4-DOC-06: volatile protocol constraints

Initial severity: release-blocking concept-boundary defect

Initial evidence: placement/writable protocols initially allowed
top-level-volatile descriptors to reach adapter calls that accept the declared
unqualified `const T&` or `T&`. After M4-PORT-02 rejected volatile placed and
writable descriptors, base `ReadableExpression<volatile T>` could still report
true while `ExpressionShape`, `ReadExpression`, and `MayAlias` failed inside
their bodies. Sparse `Evaluate` made that base mismatch reachable.

Required resolution: reject top-level volatile at the base readable concept so
all derived queries and protocols fail by constraints. Preserve top-level
const readable/placed use and reject top-level const writes.

Resolution: `ReadableExpression` now rejects top-level volatile and
const-volatile descriptors. `PlacedReadableExpression` and
`WritableExpression` inherit that boundary; writable additionally rejects
top-level const. Independent GCC/Clang compile assertions verified all four
negative volatile cases and retained const readable/placed behavior.

## Independent correction recheck

The installed review executable
`/tmp/asc-cpp-m4-doc-example/build with spaces/asc_cpp_m4_api_review`
exercised the corrections without using repository test helpers:

```text
DOC-01: !WritableExpression<const AddressVector> compile assertion   passed
DOC-02: overlapping coordinate/value external view                  passed
DOC-02: overlapping compressed structural spans                     passed
DOC-03: raw coordinate and outer-offset pointer return types         passed
DOC-04: incomplete coordinate SpMV output rejected unchanged         passed
DOC-05: deleted operator& external SpMV adapters compiled/ran        passed
PORT-01: reverse-direction partial span overlap                      passed
PORT-01: disjoint boundary, inside/outside identity, zero span       passed
PORT-01: null nonempty span rejected                                 passed
DOC-06: readable/placed/writable volatile compile assertions         passed
```

Both GCC 11.4 and Clang 19 also compiled every new/modified public header alone
under strict C++20 warnings, `-Werror`, `-pedantic-errors`, and
`-fno-exceptions`:

```text
asc/expression/writable.h
asc/expression/expression.h
asc/expression.h
asc/dense/view.h
asc/sparse.h
asc/sparse/coordinate.h
asc/sparse/compressed.h
asc/sparse/evaluate.h
asc/sparse/export.h
asc/sparse/linalg.h
```

## Documentation decisions

The module guide distinguishes:

- stored entries from mathematical nonzeros, because explicit zeros may be
  retained;
- builder insertion order from canonical finalized order;
- owner allocation from non-owning view creation;
- value mutability from structural immutability;
- recoverable checked access from unchecked adapter preconditions;
- successful computational no-allocation from possibly allocating diagnostic
  `Status` strings;
- format conversion from expression evaluation;
- sparse iteration from dense logical-domain traversal;
- identity-only aliasing from validated symmetric span overlap;
- zero-span behavior and external adapter range obligations;
- readable, placed, writable, const, and volatile concept boundaries;
- explicit host serial execution from future provider/device capability; and
- observed benchmark evidence from a performance baseline or provider claim.

GPU evidence for Milestone 4 is exactly **skipped**.

## Installed example

The exact example in `docs/modules/sparse.md` was extracted byte-for-byte into
a fresh external consumer. Its CMake project requested only
`ASCCpp 0.4 CONFIG REQUIRED COMPONENTS sparse`, rejected an imported dense,
utilities, random, or aggregate target, and linked only `ASC::sparse`.

Producer/package commands:

```text
cmake -S . -B /tmp/asc-cpp-m4-doc-build.AB4Tvq/build \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m4-doc-build.AB4Tvq/build --parallel 4
cmake --install /tmp/asc-cpp-m4-doc-build.AB4Tvq/build \
  --prefix /tmp/asc-cpp-m4-doc-build.AB4Tvq/prefix
```

Result: configure, strict build, and install passed with GCC 11.4.0. The
installation contained the sparse library, six sparse public headers,
expression writable protocol, and the conditional package exports.

Exact-example commands:

```text
diff -u <documented-code-block> \
  /tmp/asc-cpp-m4-doc-example/main.cc
clang-format-19 --dry-run --Werror \
  --style=file:/home/yicai/AI4SciComp/asc-cpp/.clang-format \
  /tmp/asc-cpp-m4-doc-example/main.cc
cmake -S /tmp/asc-cpp-m4-doc-example \
  -B "/tmp/asc-cpp-m4-doc-example/build with spaces" \
  -DCMAKE_CXX_COMPILER=g++ \
  -DASCCpp_DIR=/tmp/asc-cpp-m4-doc-build.AB4Tvq/prefix/lib/cmake/ASCCpp
cmake --build "/tmp/asc-cpp-m4-doc-example/build with spaces" --parallel 4
"/tmp/asc-cpp-m4-doc-example/build with spaces/asc_cpp_m4_documentation_example"
```

Result after the span-aware alias and DOC-06 corrections: exact-text
comparison, repository format, configure, strict
`-Wall -Wextra -Wconversion -Wpedantic -Wsign-conversion -Werror` compile,
link, and runtime all passed. Runtime independently verified CSR values
`{5.0, 6.0}` from the documented `2x3` matrix and external vector adapter. The
example now precomputes full input/output span tokens and uses
`AliasTokensMayOverlap`; it does not rely on identity-only aliasing.

## Final disposition

Accepted. The current public surface matches the frozen M4 contract and
accepted ADRs after DOC-01 through DOC-06 and M4-PORT-01/02. The expression
and sparse module guides document
the actual names, constraints, policies, canonical formats, ownership,
lifetime, invalidation, external pointer obligations, failure transactions,
allocation and conversion costs, aliasing, no-densification behavior,
execution and memory placement, numerical order, concurrency, packaging,
provider limits, provenance, and deferred scope.

There is no unresolved release-blocking documentation or API finding in this
review. Full repository validation, sanitizer/package matrices, portability,
benchmark evidence, and final milestone acceptance remain lead-owned.
