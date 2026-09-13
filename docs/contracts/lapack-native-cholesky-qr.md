# Native Cholesky and Householder QR contracts

These independently written C++20 algorithms belong to provider-free
`ASC::dense`. They introduce no seventh module, foreign runtime, Random
dependency or BLAS/Random numerical change. Include the explicitly named
`asc/dense/lapack/cholesky.h` or `asc/dense/lapack/qr.h`. All operations support
float, double and the corresponding `std::complex` types without implicit
precision conversion. They accept checked full-storage BLAS descriptors and
caller-owned reports, not automatically allocated factor/result owners.

This is implementation self-review, not repository-owner or license approval.
The normalized mapping covers the actual S/D/C/Z POTRF, POTRS and GEQRF
operations in the pinned Reference-LAPACK3.12.1 inventory. Native implementation
does not promise that provider's blocked algorithm, operation order or bitwise
results. Reference routines, specialized variants and their complete mode/ABI
evidence remain separate. No native ORG/UNG/ORM/UNM expert credit is inferred
from the Q conveniences below.

## Common storage and execution

An explicit existing serial CPU context admits only host memory. Pinned,
managed and device placement is rejected, not copied. Both matrix layouts and
valid padding are traversed directly. No allocation, implicit packing, transfer,
synchronization or provider dispatch occurs. Independent calls on disjoint
buffers are reentrant; owners, descriptors and live scalar scratch objects must
outlive the call, and callers exclude concurrent mutation.

All live reachable storage ranges, including complete supplied scratch spans,
are conservatively disjoint. Structural validation precedes numerical writes.
Reports reset before ordinary validation, except a report/storage overlap is
rejected before resetting the aliased object. Native INFO stays absent and
called_provider stays false. Successful reports are complete, family-tagged
and native. Reusable borrowed factor views require the same call's successful
report and unchanged factor buffers; they do not themselves scan numeric data
or certify rank/conditioning. Rejection preserves numerical buffers.

## Selected-triangle Cholesky

`Potrf(context, triangle, A, report)` requires square A. Lower means
`A=L*L^H`; upper means `A=U^H*U`, using transpose for real scalars. Only the
selected triangle is referenced or overwritten; the other triangle is not
scanned for symmetry. For complex Hermitian input, imaginary diagonal parts
are ignored and selected output diagonals have positive-zero imaginary parts.
No full finiteness scan or condition estimate is performed.

The first nonpositive or NaN real Schur-complement pivot stops the operation.
Its unsquared real value is retained on the diagonal, earlier selected factor
columns/rows remain inspectable and later selected entries retain input values.
The numerical report is `kNotPositiveDefinite/kDocumentedPartial` with the
zero-based failing pivot index. Such buffers cannot become a successful factor
view. Successful completion is not a certificate of finite/conditioned input.

`Potrs(context, factor, B, report)` reuses an unchanged successful native
`LapackCholeskyFactorView<T>` and overwrites B with the solution of `A*X=B`.
B has n rows and any nonnegative RHS count; factor and B layouts are
independent. Before a nonempty solve, the selected factor diagonal must be
positive, finite and real-valued; otherwise B is unchanged. No factorization
or symmetry scan is repeated. Both Cholesky operations use zero numerical
scratch. Empty order or empty RHS operations do not access numeric entries.

The mapping enumerates four POTRF modes (two layouts and two triangles) and
eight POTRS modes (two factor layouts, two RHS layouts and two triangles)
per scalar. Required tests include empty/scalar sizes, reconstruction, repeated
multiple-RHS residuals, finite scaling extremes, first/last nonpositive pivots,
ignored triangles/imaginary diagonals, invalid factor provenance, all relevant
alias/placement/shape failures, allocation observations and installed isolation.

## Packed Householder QR and explicit Q conveniences

`Geqrf(context, A, tau, scratch, report)` accepts m-by-n tall, square, wide
and empty A, exact contiguous `min(m,n)` scalar tau, and disjoint contiguous
scalar scratch of at least n entries for nonempty A (otherwise zero). Upper
trapezoidal R shares A with strictly lower reflector tails. The implicit
reflector diagonal is one. `Q=H0*...*H(k-1)`, with
`Hi=I-tau(i)*v(i)*v(i)^H`. Complex reflectors use real beta and annihilate
the column with `Hi^H`; Hi is not assumed Hermitian. Zero tails with real
alpha produce tau zero, preserving alpha. No positive-diagonal, numerical-rank
or minimum-norm guarantee is added.

The independently written kernel uses scaled norms and safe alpha-beta
calculations, including subnormal input. Nonfinite referenced input is rejected
before numerical writes (`kNumerical/kNotRun/kUnchanged`). A computed norm or
update that is not representable yields partial/unusable output and a
zero-based reflector diagnostic, never a successful reusable factor.

`FormHouseholderQ` explicitly writes full m-by-m or economy
m-by-min(m,n) Q into separate caller storage; it preserves factors and tau.
Its scratch needs Q's column count when the output and reflector sequence are
nonempty. `ApplyHouseholderQ` applies full Q to a separate C from left or
right, with real N/T or complex N/C. Scratch needs C's column count for left
or row count for right when the operation is nonempty. These conveniences use
all reflectors of a successful native factor, not arbitrary expert partial-k
contracts. They do not allocate/form an intermediate Q during application.

The exact GEQRF mapping enumerates its two A layouts per scalar. Each requires
independent reconstruction, orthogonality/unitarity, direct reflector
annihilation, full/economy Q, all legal application side/transpose and
independent output-layout tests. Empty/tall/wide/scalar/rank-deficient cases,
extreme scales, zero vectors, phase conventions, preservation/padding,
workspace/alias/placement failures, numerical failure, allocation observations
and installed Dense-only consumers are separate required test classes.

## Evidence and installed use

Frozen native Cholesky03 and native QR04 diagnostics are identified in the
durable program reviews. Scoped mathematical, sanitizer and allocation tests
are not automatically full project, installed, other-platform or
contract-bound mode evidence. The implementation rows remain
`implemented_unverified` until that complete linkage is closed.

The public-only [Dense factorization example](../../examples/dense_factorizations/)
demonstrates successful/repeated Cholesky solves, retained nonpositive-definite
diagnostics, QR reconstruction/unitarity and explicit Q application for all
four scalar types. Configure it independently against a relocated installed
package to check that neither a foreign provider nor Sparse/Random is imported.
The larger runbook's eigen/SVD/least-squares acceptance workflows remain
required and are not represented by this example.
