# Milestone 3 contract-first verification design

Status: frozen before Milestone 3 production inspection

## Independent basis and boundary

This design was written before inspecting any Milestone 3 production header or
source. Its only technical inputs are:

- the frozen Milestone 3 contract, ownership ledger, and provenance record;
- approved ADRs 0004, 0007, 0008, 0009, 0010, 0011, and 0013; and
- the architecture verification strategy.

The numerical expectations below were calculated directly from ordinary dense
array and linear-algebra definitions. No MdeCpp source, test, documentation, or
literal; deleted asc-cpp source or test; third-party implementation or vector
corpus; or `feature/asc-cpp-m8-hardening-downstream` Milestone 3 production,
test, benchmark, or review was inspected.

## Layout oracle and properties

For shape `e` and coordinate `i`, the independent offset oracles are:

```text
LayoutLeft rank 3:
  i0 + e0 * (i1 + e1 * i2)

LayoutRight rank 3:
  (i0 * e1 + i1) * e2 + i2

LayoutStride rank R:
  sum(i[d] * stride[d], d = 0 .. R-1)
```

For a nonempty non-negative strided mapping, required span is:

```text
1 + sum((extent[d] - 1) * stride[d], d = 0 .. R-1)
```

Rank zero has logical and required span size one. Any zero extent has logical
and required span size zero.

Deterministic property enumeration will cover ranks zero through three and
small extents zero through five. For every published mapping it will enumerate
all coordinates and verify:

- offsets equal the independent formula;
- each offset is below required span;
- every mutable mapping address is unique;
- left and right contiguous mappings are exhaustive and produce exactly
  `[0, logical_size)`; and
- logical traversal advances dimension zero fastest, independent of layout.

Explicit stride cases include:

| Shape | Strides | Expected offsets | Span | Property |
| --- | --- | --- | --- | --- |
| `2 x 3` | `1, 2` | `0,1,2,3,4,5` | 6 | left contiguous |
| `2 x 3` | `3, 1` | `0,3,1,4,2,5` | 6 | right contiguous |
| `2 x 3` | `1, 4` | `0,1,4,5,8,9` | 10 | unique padded |
| `2 x 3` | `2, 5` | `0,2,5,7,10,12` | 13 | unique strided |
| `2 x 2` | `1, 1` | repeated address | 3 | reject mutable |
| `2 x 2` | `0, 1` | repeated address | 2 | reject mutable |

Negative extents, negative strides, coordinate bounds, products, maximum
offsets, required spans, and conversion overflow are rejected before a mapping
is published. Conservative rejection of a mathematically unique stride pattern
is accepted only where the contract permits it.

## View and owner falsification

Compile traits will require:

- `DenseView` is trivially copyable;
- owners are move-constructible and move-assignable but neither copyable nor
  default shared owners;
- mutable views convert to const-element views;
- const-element views do not convert to mutable views; and
- unsupported element and rank combinations do not become valid overloads.

Runtime view tests cover null/non-null rules for scalar, empty, and nonempty
views; explicit memory space; left, right, and unique-stride mappings; checked
access at every boundary; non-host access rejection; const propagation;
rank-preserving subview pointer offset, shape, strides, and memory space; zero
subviews; and invalid or overflowing subviews without mutation or allocation.

A counting/failing host memory resource will verify:

- value-initialized arithmetic elements;
- exactly one release for every successful owner allocation;
- partial and failed construction rollback;
- move ownership and moved-from safety;
- deep clone content equality, distinct storage, explicit resource and serial
  context, and failure cleanup;
- successful discard-resize changes shape and storage, value-initializes the
  replacement, and documents prior-view invalidation; and
- failed discard-resize preserves pointer, shape, values, resource ownership,
  and allocation balance.

Rank-zero, zero-extent, mixed static/dynamic extents, left and right owner
mappings, negative dynamic extents, byte-count overflow, non-host resource, and
non-exhaustive owner mapping attempts are covered.

## Expression evaluation and reduction oracles

Evaluation tests use both dense terminals and an external non-ASC expression.
For a logical input:

```text
shape 2 x 3
values by coordinate:
  [ 1, -2,  3
    4,  0, -1 ]
```

the pointwise oracle for `2 * input - 1` is:

```text
[ 1, -5, 5
  7, -1, -3 ]
```

The result must be identical for left, right, and padded unique-stride
destinations. Rank-zero scalar expansion fills every logical coordinate.

Before mutation, tests falsify wrong rank, wrong extent, unavailable or
non-serial context, non-host memory, exact and partial aliases, and overflowing
metadata. Exact direct view self-assignment is a no-op. Every other possible
overlap, including an expression node referring to the destination, is
rejected with the destination byte-for-byte unchanged.

A global allocation probe and instrumented expression adapter require zero
allocation, packing, transfer, synchronization, or hidden dispatch during
evaluation. Reads occur exactly once per logical destination coordinate and in
dimension-zero-fastest order.

Reduction oracles are:

```text
input: 3, -1, 4, 2
sum:   8
min:  -1
max:   4
```

Empty sum is positive zero. Empty minimum and maximum fail with
`kInvalidArgument`. Left, right, and padded mappings give identical logical
results. The cancellation input `1e20f, 1.0f, -1e20f` has deterministic
left-to-right float sum `0.0f`; it distinguishes the frozen logical order from
a reassociated result.

## Independent linear-algebra oracles

All operations are tested for `float` and `double`, left/right/strided views,
host serial context, destination transactionality, and zero allocation.

### Vector operations

For:

```text
x = [1, -2, 3]
y = [4,  5, 6]
```

the exact oracles are:

```text
Copy(x)              = [1, -2, 3]
Scal(-2, x)          = [-2, 4, -6]
Axpy(3, x, y)        = [7, -1, 15]
Dot(x, y)            = 12
Nrm2([3, 4])         = 5
```

Exact `Copy` self-identity is a no-op. `Scal` is in-place. `Axpy` with the
same-index `x == y` produces `(alpha + 1) * y`. Partial or non-identical output
overlap is rejected before mutation.

Zero-length `Copy`, `Scal`, and `Axpy` succeed without access; empty `Dot` and
`Nrm2` return positive zero. Dot order is tested with
`[1e20f, 1.0f, -1e20f] dot [1,1,1] == 0.0f`.

Scaled sum-of-squares stability uses independent `std::hypot` oracles:

- `[max/4, max/4]` has finite norm `(max/4) * sqrt(2)` rather than overflow;
- `[min_normal, min_normal]` remains nonzero and approximates
  `min_normal * sqrt(2)` rather than underflowing to zero;
- infinity yields infinity; NaN follows ordinary IEEE arithmetic; and
- signed-zero input yields positive zero.

### Matrix-vector operations

For:

```text
A = [ 1, -2,  3
      4,  0, -1 ]
x = [2, -1, 0.5]
u = [2, -1]
```

the exact oracles are:

```text
A * x   = [5.5, 7.5]
A^T * u = [-2, -4, 7]
```

With `alpha = 2`, `beta = -1`, and prior `y = [10,20]`,
`y = alpha*A*x + beta*y` becomes `[1,-5]`.

For `beta == 0`, destination elements begin as signaling or quiet NaNs. The
finite product above must be written without reading the old destination;
`0 * NaN` is not permitted to contaminate the result.

Zero rows, zero columns, wrong `x` or `y` extent, output overlap with matrix or
vector, transpose shape mismatch, non-host memory, and unsupported context are
validated before mutation.

### Matrix-matrix operations

For:

```text
A = [1,2
     3,4]
B = [5,6
     7,8]
```

the four exact transpose oracles are:

```text
A   * B   = [19,22; 43,50]
A^T * B   = [26,30; 38,44]
A   * B^T = [17,23; 39,53]
A^T * B^T = [23,31; 34,46]
```

With `alpha = 2`, `beta = -1`, and prior
`C = [1,2;3,4]`, the non-transpose result is:

```text
[37,42;83,96]
```

For `beta == 0`, NaN-filled `C` must produce the finite product without reading
old `C`. A zero inner dimension produces `beta*C`, and with zero beta produces
positive zeros without reading `C`.

All matrix input/output overlap, partial overlap, inner-dimension mismatch,
destination-shape mismatch, invalid transpose value, non-host memory,
unsupported scalar, and unsupported context cases fail before destination
mutation.

## Numerical comparison policy

Exact integer and exactly representable binary results use equality and object
bits where signed zero matters. Ordinary float/double arithmetic uses a
documented absolute-plus-relative comparison:

```text
abs(actual - expected)
  <= absolute_tolerance + relative_tolerance * abs(expected)
```

Small fixed cases use a tolerance of at most 16 machine epsilons scaled by
problem magnitude. Scaled-norm extreme cases compare to `std::hypot` with a
small ULP/relative allowance. NaN, infinity, and signed zero are classified
explicitly rather than sent through an ordinary tolerance.

## Allocation and transaction instrumentation

The verification executable overrides global scalar/array allocation only
while a scoped probe is enabled. Dense operations are warmed up before the
probe. Every evaluation, reduction, and linear-algebra call must record zero
allocations.

The custom memory resource records allocate, deallocate, byte, alignment, and
live-block counts and can fail a selected request. Failure tests snapshot
destination bytes and metadata before invocation and require exact equality
after failure.

No test silently supplies workspace, packing storage, transfer hooks, or a
provider context.

## Negative compile and architecture plan

Lead-owned registration must:

- compile all seven new public headers alone under strict C++20 and, where
  supported, with exceptions disabled;
- compile a representative dense owner/view/evaluation use in multiple
  translation units;
- require compile failure for owner copy, const-view-to-mutable conversion,
  unsupported owner element, unsupported linear-algebra scalar, and wrong-rank
  linear-algebra operands;
- audit the exact public and compiled source inventory;
- reject utilities, sparse, random, retired array/linalg, provider, and
  third-party includes or target edges;
- require `asc_dense` to be a compiled static/shared target with direct and
  installed links exactly `ASC::core;ASC::expression`; and
- verify no unapproved component or target appears.

The Dense-only consumer must request `dense`, observe the transitive `core` and
`expression` closure, and observe utilities, sparse, random, facets, providers,
and `cpp` as absent. Build-tree, installed relocated path-with-spaces,
static/shared, and subproject cases are required.

## Threshold-free benchmark design

The benchmark uses `std::chrono::steady_clock`, a serial context, preallocated
host owners, and warm-up calls. It records:

- compiler and build configuration supplied by the build;
- operation and scalar type;
- logical shape;
- iteration count;
- total and per-iteration duration;
- a checksum preventing dead-code elimination; and
- allocation count inside the timed loop.

At minimum it times pointwise evaluation and a fixed square serial `Gemm`.
Inputs and destinations are allocated before timing. The timed loop must report
zero allocations. There is no speed pass/fail threshold; failure is limited to
incorrect results, invalid timing state, or observed allocation.

## Local acceptance matrix

The independent matrix requires:

- Debug/static and Release/shared builds with warnings as errors;
- focused ASan+UBSan;
- all compile, negative-compile, architecture, semantic, numerical,
  transaction, allocation, benchmark-smoke, package, relocation, subproject,
  and isolated-consumer gates;
- formatting and whitespace checks; and
- exact commands and result counts in `verification-review.md`.

GPU evidence is exactly **skipped** because Milestone 3 has no provider target
or GPU implementation.
