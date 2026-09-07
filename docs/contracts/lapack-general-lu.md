# General LU capability contract

This contract maps the actual S/D/C/Z `getrf` and `getrs` operations from
Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. The immutable
[source inventory](lapack-upstream-inventory.json) records the declarations,
argument documentation and source hashes, including alternative upstream LU
implementations. Those alternatives do not create additional scalar operations.
This is a self-reviewed ASC implementation contract, not repository-owner or
license approval, and not a full LU-family or LAPACK completion claim.

## Provider-free native API

Include `<asc/dense/lapack/lu.h>` and link only `ASC::dense`. The `Getrf`
overload taking an `ExecutionContext` factors a rectangular matrix in its
caller-owned storage. `Getrs` takes that context, a transpose option, a
successful borrowed `LapackLuFactorView<T>`, mutable RHS storage and a mandatory
fresh report. The supported `T` are `float`, `double`, `std::complex<float>`
and `std::complex<double>`. No precision conversion is implicit.

Both calls require serial execution and host memory as defined by the existing
execution context. Device, managed and pinned-host descriptors are not an
implicit transfer request. Independent calls on disjoint storage may run
concurrently; no provider registry or process-wide state is changed.

Checked `DenseBlasMatrixView` descriptors validate nonnegative dimensions,
leading dimensions, address arithmetic and complete reachable backing spans.
Both row-major and column-major layouts are supported directly, including
padding. Solve factors and RHS may have different layouts. Padding is never
used as numeric scratch. GETRF needs exactly `min(m,n)` writable `index_t`
pivots with increment one. GETRS requires square factors and `n` RHS rows;
the RHS count may be zero. Empty operations do not access numeric elements.

## Factors, pivots and equations

GETRF stores unit-lower `L` (diagonal implicit) and upper `U` together in A.
Every chosen pivot interchanges the entire row, including previously computed
L multipliers. Applying the raw sequential swaps in increasing step order to
the original A yields `L*U`. Consequently the permutation P in the upstream
equation `A=P*L*U` is the inverse of that forward-swap action.

Pivots are signed 64-bit ASC integers containing one-based sequential swap
indices, not a final permutation vector. Their family is
`kLuPartialPivot`; the checked raw pivot view retains that family. Complex
pivot comparison uses `abs(real)+abs(imag)` and keeps the first maximum.
The native implementation is independently written unblocked elimination;
it does not promise the upstream blocked algorithm's floating-point operation
order or bitwise agreement.

GETRS solves `op(A)*X=B` without changing or recomputing the factor and pivots.
`kNone`, `kTranspose` and `kConjugateTranspose` are all legal; the last two
have equal mathematical meaning for real scalars but remain independently
validated enum values. Existing native BLAS triangular solves are reused when
layouts match; mixed-layout substitution needs no packing. A successful factor
view borrows storage: callers must keep its factors and pivots alive and
unchanged. The native solve rejects nonnative provider provenance.

## Workspace, aliasing and failure mutation

The native operations require zero numerical scratch and allocate no temporary
storage, transfer no memory and synchronize no provider. Matrix/pivot reachable
ranges must be disjoint. For solves, RHS storage must be disjoint from both
factor and pivot reachable ranges. These conservative checks include gaps
within the reachable spans. The caller's report and descriptors must remain
valid independent objects throughout the call.

All structural validation precedes numerical mutation. Reports are reset even
on rejection; structural failure leaves numeric buffers unchanged with
`kNotRun` and `kUnchanged`. Native operations never fabricate foreign `INFO`
and never set `called_provider`.

GETRF treats an exactly zero pivot as singular without introducing a tolerance.
It completes elimination, returns `kNumerical`, and records `kSingular`,
`kDocumentedPartial`, and the first zero pivot's zero-based
`diagnostic_index`. Raw factors and pivots remain inspectable. They cannot be
promoted to a successful factor view. GETRS defensively checks zero U diagonals
before a nonempty solve and rejects without modifying B. Nonfinite input follows
scalar arithmetic; success is not a finiteness or conditioning guarantee.

## Reviewed modes and evidence boundaries

The [mapping](lapack-coverage.yaml) enumerates the two factor layouts and all
twelve solve combinations (two factor layouts, two RHS layouts, three
transpose options) per scalar. Sizes are covered by documented representatives:
empty, scalar, square, tall and wide for factorization; empty, scalar and
multiple-RHS square systems for solve. Scale, singularity, noncommuting swaps,
malformed descriptors and allocation checks are separate required test classes.

`tests/dense/lapack_lu_test.cc` independently reconstructs `L*U`, computes
scaled residuals and forward errors using wider verifier arithmetic, checks
padding and raw pivots, and emits actual successful numerical case records.
An emitted case is not evidence for an unexecuted mode, failure test, installed
consumer, another implementation or another provider ABI. The durable
verification summary identifies the actual frozen snapshots and selections;
ledger verification additionally requires matching machine-readable evidence.

The explicitly enabled reference-provider overloads are a separate route.
Their current checked column-major slice does not close this contract's
row-major cases. Reference workspace, ABI/INFO and integer-conversion behavior
must receive their own evidence. `getrf2`, `getf2`, `getri`, `gesv`, `gesvx`,
`gecon`, `gerfs`, `geequ`, `geequb` and the remaining required helpers remain
separate inventory rows; these eight mappings do not implement them.
