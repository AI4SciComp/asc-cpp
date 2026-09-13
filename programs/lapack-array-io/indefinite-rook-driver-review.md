# Rook indefinite direct drivers

Status: **implemented, numerical acceptance blocked; bounded verification recorded**.
Overall programme status remains **FULL_PROGRAM_INCOMPLETE**.

The new public header `asc/dense/providers/lapack_indefinite_rook_driver.h`
exposes formula queries and execution overloads for S/D/C/Z SYSV_ROOK and C/Z
HESV_ROOK. The six routines execute the pinned native drivers. Successful
same-operation reports are accepted by `ReferenceRookFactorView::Create` and
subsequent TRS_ROOK calls with the actual originating driver name preserved.
The existing factorization and solve numerical kernels are unchanged.

## Source contracts

The authoritative LAPACK 3.12.1 sources are pinned at
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`:
[SSYSV_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/ssysv_rook.f),
[DSYSV_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/dsysv_rook.f),
[CSYSV_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/csysv_rook.f),
[ZSYSV_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zsysv_rook.f),
[CHESV_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/chesv_rook.f), and
[ZHESV_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zhesv_rook.f).
Inventory rows, complete source snapshots, hashes and actual GNU Fortran 11.4
LP64/ILP64 emissions are retained in the external continuation evidence under
`rook-driver-prerequisite-01`.

S/D use real A, B and WORK; C/Z use complex A, B and WORK. Native dimensions,
INFO and signed IPIV use the selected provider INTEGER width. All six routines
are declared in the pinned `lapack.h`. The direct compiler-signature comparison
erases only read-only input pointee const qualifiers introduced by that header;
scalar types, pointer levels, argument order and trailing CHARACTER size type
must match the actual GNU emissions. This is admission of the recorded GNU
profile, not a portable Fortran ABI claim.

A is square selected upper/lower storage, overwritten with the raw rook block
factor. Negative pivot partners encode two independent ordered interchanges.
B has n rows and nrhs columns and is overwritten by X only on successful native
completion. A and B independently select row/column layout and padding. SY
uses transpose symmetry, including complex symmetric matrices; HE uses conjugate
transpose. Original Hermitian imaginary diagonal components are ignored by
packing real diagonals and selected offdiagonals, including for column-major A.
Full factor coefficients are retained during subsequent reuse.

The drivers always solve with TRS_ROOK. Caller WORK controls TRF_ROOK blocking:
minimum is one scalar and preferred is the pinned 64*n expression, with the
source's exact SROUNDUP_LWORK and floating-to-INTEGER query conversions checked.
SY drivers query TRF_ROOK internally; HE drivers compute n*ILAENV directly.
The wrapper's formula query performs no native call or numerical array read.
Plans bind routine, scalar, provider/ABI, triangle, symmetry, dimensions,
independent layouts and original/effective leading dimensions. Native converted
output pivots need n INTEGER entries. Row-major A and every original Hermitian
A need n*n packing entries; row-major B needs n*nrhs more. Operations allocate
no hidden storage and introduce no alternate provider or global state.

N=0 is a successful wrapper noncall without scratch or numerical reads/writes.
N>0 with NRHS=0 still performs the native factorization using a valid local
dummy B and LDB=max(1,n). Operands, scratch and live metadata must be accessible
and disjoint. Concurrent operations require distinct writable buffers and
reports. Structural errors preserve numerical storage; unsafe report/metadata
aliases preserve the report itself, while other errors reset it without
foreign INFO. Caller provenance remains necessary for borrowed factor reuse.

Source argument INFO values are -1 (UPLO), -2 (N), -3 (NRHS), -5 (LDA), -8
(LDB), and -10 (LWORK). Positive INFO retains completed selected raw factors
and validated rook pivots, with B unchanged. An exact zero diagonal identifies
a singular outcome; pinned NaN-pivot positive INFO is a distinct partial result.
Neither permits successful factor reuse. INFO=0 records completion without an
extra finite-value or condition-number guarantee. No RCOND/FERR/BERR outputs
exist in these direct drivers.

After all preflight checks, the wrapper seeds full-width INFO and every native
IPIV entry to INTEGER minimum and WORK[0] to -1. Unwritten/partial INFO,
malformed native pivots, or inconsistent returned WORK are provider defects.
Raw INFO survives; packed A/B and public pivots are withheld. Direct native
buffers may already have changed. A successful report carries `kRook` and the
actual SYSV_ROOK/HESV_ROOK name, without a synthetic factorization report.

## Retained numerical blocker

`ROOK-INDEFINITE-DRIVER-RANGE` affects all six drivers. For the nonsingular,
well-conditioned scalar equation A=B=scale, exact X=1 is representable.
Each range process exercises both triangles, independent A/B layouts, minimum
and preferred workspace, zero/one RHS, and four scales:

| Scale | Factor-only result | Solve result |
| --- | --- | --- |
| Twice minimum normal | Preserved finite A | Finite X near 1 |
| Minimum normal / 1024 | Preserved finite A | Nonfinite X |
| Twice minimum subnormal | Preserved finite A | Nonfinite X |
| Maximum finite | Preserved finite A | Finite X near 1 |

Every case has native INFO=0. The tiny-scalar TRS_ROOK path forms an
unrepresentable reciprocal before scaling B. Direct native calls reproduce the
same output. All six mathematical tests remain ordinary required failures;
fidelity tests pass separately. In each initial LP64/ILP64 Release profile,
128 cases per scalar/symmetry class produce 32 failing solve cases and 96
failed assertions, totaling 576 assertions across six failed processes. No
assertion, tolerance, test classification or provider source is weakened.

## Validation checkpoint

Each initial LP64/ILP64 Release selection contains 37 processes: 31 pass, six
required mathematical tests fail, and none skip. Numerical tests reconstruct
factors through an independent reverse Schur-complement oracle, include both
nontrivial rook interchanges, verify original-matrix residuals and known
solutions, and reuse factors under actual driver reports. Orders include
0, 1, 2, 3, 7 and 67, with reduced/preferred work and scaled controls.

Native-output fault tests execute the real driver before injecting INFO,
pivot and WORK defects. Structural tests preserve all arrays and scratch on
rejection and use protected unread arrays for query/empty-call checks.
Concurrency shares immutable plans and provider identity, interleaving ordinary,
singular, zero-RHS and stale-plan calls with separate buffers. Actual emitted
signature probes pass 288 direct native cases in each integer mode.

The final sixteen-profile matrix contains 468 processes: 396 pass, 72 required
mathematical tests fail with 6,912 failed assertions, and none skip. Each of
twelve static/shared LP64/ILP64 Debug/Release/ASC-ASan+UBSan profiles passes
31 of 37 tests; each of four TSan profiles passes all six concurrency tests.
ASC and tests are instrumented; the pinned Fortran/BLAS provider is not.

All four relocated public consumers pass 768 cases each using only exported
headers and targets. Twenty strict translation units, four standalone-header
checks, 576 actual emitted-prototype ABI cases, all four package header-manifest
contexts and exact architecture/dependency checks pass. Each shared integer ABI
adds 12 public symbols and removes none. Doxygen covers 143 headers and 2,489
public members with zero warnings.

Source-external evidence under
`master-continuation-20260910-01/continuation-20260912-01` includes
`rook-driver-final-audit`, `rook-driver-index`, compiler/configuration snapshots,
all failed intermediate attempts and targeted helper-refactor checks. Installed
consumer packages are byte-identical to the final producer after normal CMake
installation; build-tree shared RPATH bytes are compared separately. Raw earlier
header Not Run results, failed helper builds and strict documentation failures
are retained and resolved rather than skipped or reclassified.

The previous hosted condition commit omitted one compiled source from the
independent source allowlist and linked a programme page outside Doxygen INPUT.
The exact allowlist now includes both added sources, and strict documentation
includes the two referenced pages. Existing assertions and FAIL_ON_WARNINGS
remain enabled. Original hosted artifacts confirm these causes and are retained.

Frozen Native20, array-I/O and robust PPSVX records remain unchanged. All earlier
programme blockers remain open; broader platform/provider admission, full
normalized execution evidence and the six mathematical failures still prevent
full numerical acceptance. The next independent required family is
SYTRI_ROOK/HETRI_ROOK.
