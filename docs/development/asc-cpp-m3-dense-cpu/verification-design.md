# Milestone 3 Independent Verification Design

Status: Derived independently from the frozen contract

Date: 2026-07-26

Scope owner: independent verification agent

## Inputs and independence

This plan was derived from:

- the Milestone 3 frozen contract and ownership ledger;
- accepted ADRs 0001, 0004, 0007--0011, and 0013;
- the architecture verification strategy;
- the installed public `core` and `expression` contracts.

The verifier did not inspect MdeCpp tests, deleted asc-cpp tests, or an
upstream implementation, test, or vector corpus. Numerical expectations are
calculated directly from the documented mathematics.

## Test organization

The independent suite is split into self-contained executables so failures
identify the violated contract:

| File | Contract under test |
| --- | --- |
| `tests/dense/layout_view_test.cc` | mapping construction, offsets, uniqueness, exhaustiveness, bounds, subviews, and const propagation |
| `tests/dense/array_test.cc` | owner traits, initialization, move, clone, resize, resource failure rollback, and view invalidation contract |
| `tests/dense/evaluate_test.cc` | expression adaptation, shape checks, scalar expansion, logical traversal, aliases, transactions, reductions, and no allocation |
| `tests/dense/linalg_test.cc` | independent Copy/Scal/Axpy/Dot/Nrm2/Gemv/Gemm numerical and transaction cases |
| `tests/compile/m3_dense_contract.cc` | positive concepts, types, traits, signatures, and provider-neutral public headers |
| `tests/compile/m3_dense_multi_tu*.cc` | representative header-only template ODR use across translation units |
| `tests/compile/m3_dense_negative_*.cc` | const-correctness and unsupported-owner/scalar failures |
| `tests/consumer/dense/` | isolated installed `ASC::dense` consumer |
| `benchmarks/dense/allocation_free_benchmark.cc` | allocation observation and recorded serial timing without a performance threshold |

Tests use no third-party framework. Each executable returns nonzero and prints
the source line when a requirement fails.

## Storage and mapping properties

For a shape `(e0, ..., eR-1)` and coordinate `(i0, ..., iR-1)`, derive:

- `LayoutLeft`: stride 0 is one and
  `stride[d] = product(e[0:d])`;
- `LayoutRight`: stride `R-1` is one and
  `stride[d] = product(e[d+1:R])`;
- explicit stride: `offset = sum(i[d] * stride[d])`;
- rank zero: logical and required span size one;
- any zero extent: logical and required span size zero.

Enumerate all coordinates of small shapes and require injective offsets for
every accepted mutable mapping. Check exact offsets for left, right, and a
unique padded mapping. Reject negative extents, negative strides, repeated
addresses, invalid nonempty/null views, out-of-bounds coordinates and
subviews, and every reachable product/sum/span overflow before an object is
published.

Subview tests prove that offsets and extents are rank-preserving, strides and
memory space are unchanged, no allocation occurs, and failure does not alter
the source descriptor. Mutable-to-const conversion must compile; the reverse
must not.

## Ownership and resource behavior

Compile traits require:

- owners are movable and not copyable;
- views are trivially copyable;
- a const owner yields only `DenseView<const Element, Rank>`;
- unsupported element or owner-layout categories do not instantiate.

A counting/failing `MemoryResource` independently records calls, bytes,
alignment, and exactly-once deallocation. Runtime tests require:

- value initialization for supported scalars;
- zero-size owner construction without invalid pointer use;
- move transfers the allocation and leaves one final deallocation;
- `Clone` creates distinct storage with equal logical values through an
  explicitly supplied host resource and serial context;
- successful discard-resize uses the original resource, publishes a new
  shape, and value-initializes the replacement;
- allocation failure leaves pointer, shape, mapping, and values unchanged and
  does not leak.

Tests never dereference a view after successful resize; that behavior is
documented as invalidation and is covered by ownership state comparison rather
than intentional undefined behavior.

## Expression evaluation and reductions

Dense views are required to satisfy `ReadableExpression` with exact element
type/rank, terminal operation category, structure-preserving sparsity, shape,
read, and conservative alias identity.

Evaluate into left, right, and padded destinations:

- a terminal view;
- nested add/multiply/negate nodes;
- rank-zero scalar expansion;
- exact direct self-assignment;
- an external adapted expression to preserve storage neutrality.

Before-mutation failures cover wrong shape, non-host memory, non-serial
context, and conservative non-exact overlap. Sentinels prove the destination
is unchanged. A global allocation observer around the execution call proves
no heap allocation in the evaluator.

Reductions are checked in dimension-zero-fastest logical order. Empty sum is
zero. Empty minimum and maximum return `kInvalidArgument`. NaN, infinity, and
signed zero cases are checked only to the extent ordinary deterministic IEEE
arithmetic defines an observable result.

## Independent linear-algebra oracles

All ordinary expectations are computed in the test from explicit mathematical
definitions, not by calling another ASC operation.

- `Copy`: arbitrary positive source/destination strides, exact identity no-op,
  empty vector, and rejected partial overlap.
- `Scal`: positive stride, zero length, exact in-place semantics, NaN/Inf
  propagation.
- `Axpy`: `y[i] = alpha*x[i] + y[i]`, independent strides, exact same-index
  alias where permitted, dimension mismatch, and rejected partial overlap.
- `Dot`: fixed hand-computed vectors, non-unit strides, zero length, and
  dimension mismatch.
- `Nrm2`: 3-4-5, zero vector, infinity/NaN, and
  `{max/4, max/4}` to distinguish scaled sum-of-squares from overflow-prone
  naive accumulation.
- `Gemv`: left/right/padded matrices, no-transpose and transpose, degenerate
  dimensions, alpha/beta combinations, partial-overlap rejection, and
  `beta == 0` with destination initialized to quiet NaN to prove no read.
- `Gemm`: left/right/padded matrices, all four supported transpose pairs,
  rectangular and degenerate dimensions, alpha/beta combinations,
  output-overlap rejection, and `beta == 0` with quiet-NaN destination.

Float and double receive representative coverage. Comparisons use exact
results for integer-representable arithmetic, otherwise operation-specific
relative/absolute tolerances. The stable-norm extreme case uses a relative
tolerance scaled to the expected finite magnitude.

## Compile, dependency, packaging, and runtime safety

The lead integrates:

- each public dense header as the only project include with exceptions enabled
  and disabled;
- strict C++20 and warning configurations;
- positive/multi-TU contracts and useful negative compilations;
- forbidden include and exact target-edge scans;
- the dense-only build-tree/installed/relocated/path-with-spaces consumer;
- static and shared configurations;
- ASan/UBSan for every runtime executable.

The isolated consumer includes only installed public headers and links only
`ASC::dense`. It constructs a host array, creates a view, evaluates an
expression, and runs one serial algebra operation.

## Benchmark evidence

The project-owned harness performs repeated expression evaluation and GEMM
after setup, records compiler/configuration, shapes, iteration counts, elapsed
time, and an observable checksum. Global allocation counters are reset after
setup and must remain zero inside the measured operations. There is no timing
threshold and no speed claim.

## Evidence classifications and exclusions

CPU provider-free serial behavior is eligible for `runtime-tested` evidence.
No optimized CPU provider exists in M3. GPU evidence is exactly `skipped`
because the milestone contains no GPU target, source, compilation, runtime
execution, or parity claim.

Factorizations, solvers, complex/mixed precision, general broadcasting,
rank-reducing slices, negative strides, runtime rank, provider selection,
packing, workspace, asynchronous execution, sparse storage, and random
generation are not tested as available features. Where the public type system
exposes a relevant attempted use, a compile or runtime rejection is sufficient.
