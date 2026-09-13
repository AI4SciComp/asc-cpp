# Classic converted-factor solve contract review

Status: **implemented; bounded verification recorded; numerical gates open**.
Programme status: **FULL_PROGRAM_INCOMPLETE**.

The six pinned LAPACK 3.12.1 routines are SSYTRS2, DSYTRS2, CSYTRS2,
ZSYTRS2, CHETRS2 and ZHETRS2. Source commit is
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Complete sources, eighteen
dependency sources, hashes, twelve actual GNU Fortran 11.4 LP64/true-ILP64
prototype emissions and the public contract decision made before implementation
are retained under `continuation-20260912-01/block-solve-prerequisite-01`.
The implementation parent is pushed `c6eb9e5`; previous families remain frozen.

## Storage and native semantics

`lapack_indefinite_block_solve.h` adds twelve public query/execute declarations.
S/D use real A/B/WORK; C/Z use complex A/B/WORK. All dimensions, pivots and
INFO use the selected native INTEGER width. UPLO is one CHARACTER argument
with the audited GNU trailing `size_t` length. Actual emitted-prototype checks
erase only input-pointee const, preserving all types, pointer levels and order.
The initial guarded native probe passes 144 cases per ABI.

A is immutable raw square classic factor storage; exact-n immutable
`kBunchKaufman` pivots must share its provider, scalar and TRF/TF2/actual SV
origin. This is a caller provenance precondition. Completed singular factors
are accepted for native INFO semantics. Existing borrowed classic factor
factories admit only TRF/TF2; those factories and old solves are unchanged.
Equal adjacent negative pairs describe one directional interchange per 2-by-2
block. Unequal rook pairs are rejected. SY means transpose symmetry even for
complex values; HE means conjugate-transpose symmetry. The selected triangle
contains raw factors, so complete complex diagonal/block values are preserved.

Despite its documented input A and const C declaration, each native routine
calls SYCONV(C) before solving and SYCONV(R) before returning. Public A is
therefore copied into private live T workspace for **both** layouts. The C
const pointer refers to that mutable private storage. No public const buffer
is cast or mutated. Native completion must restore selected factor object
bytes, including signed zero and NaN payloads; restoration is checked before
publishing row-major B. Public factors and pivots remain byte-identical.

Active workspace is n scalar WORK entries, n provider INTEGER entries in
caller byte storage, and n*n scalar layout entries for factors. Row-major B
adds n*nrhs layout entries. Queries use formulas, inspect metadata only and
make no foreign call. There is no LWORK, native query or returned WORK value.
Plans bind routine, scalar/provider ABI, triangle, order, nrhs and both
layouts/leading dimensions. Empty n=0 or nrhs=0 succeeds without numerical
access, workspace or a foreign call, before pivot-value inspection.

Source and nested BLAS loops require active N+1, NRHS+1 and the terminal
strided RHS cursor 1+NRHS*LDB to fit native INTEGER. Existing checked solve
counts cover those bounds. Forced native factor LDA is max(1,N); original
public metadata remains validated. Layout products and total bytes are checked.
Operands, workspace and live metadata must be disjoint and accessible to the
explicit CPU context. Structural rejection preserves numerical data/scratch;
metadata alias rejection also preserves the report. Calls allocate nothing,
change no global state and use no fallback provider or numerical algorithm.

Native INFO is zero or an argument error: -1 UPLO, -2 N, -3 NRHS, -5 LDA,
-8 LDB. Structural preflight prevents these argument errors. Full-width INFO
starts at INTEGER minimum. Missing, partial-width or nonzero INFO, changed
native input pivots or unrestored factors produce a provider defect with raw
INFO and unusable output. Row-packed B publication is withheld; direct
column-major B may already have changed. INFO=0 does not certify finiteness
or conditioning. There is no positive singular diagnosis, RCOND, FERR or BERR.

## Current execution evidence and numerical decisions

The first double check passes 496 cases. Both engineering ABIs pass all twelve
ordinary mathematical/native-fidelity processes with 496 cases each: both
triangles, independent A/B layouts, TRF/TF2 origins, n=0/1/2/3/7/67,
nrhs=0/1/3, paired permutations, completed singular factors and scaled systems.
Known solutions and independent wide residuals are checked. Original factor
bytes, input pivots, padding, workspace bounds and allocation observations pass.
All direct native byte-fidelity and guarded ABI processes pass.

Each range process contains 104 cases: 96 representable exact solutions and
eight explicitly nonrepresentable controls. Actual classic TF2 generates the
scalar or 2-by-2 factors before each solve. Each ABI records six required
mathematical process failures and 160 failed assertions, with zero skips:

- All six variants form an overflowing scalar reciprocal for A=B=min_normal/8,
  although the exact solution is one. Each real variant fails sixteen assertions.
- All four complex variants also produce nonfinite solutions for a zero-diagonal
  2-by-2 block whose two offdiagonal components are 0.75*maximum finite. B is
  the finite product of that matrix with a vector of ones. The exact solution
  is representable. Each complex variant fails thirty-two assertions overall.

Native INFO remains zero and exact ASC/native byte fidelity passes in all these
cases. The min_normal/2, min_normal, unit and 0.25*maximum controls pass. No
finite-value assertion or tolerance is weakened and no native arithmetic is
replaced. These failures remain required numerical gates.

Both ABIs also pass all six fault processes, each with 432 cases including 144
empty noncalls. Faults cover omitted INFO, 16-bit/32-bit writes, negative and
positive INFO, pivot mutation and unrestored selected private factors. A full
32-bit write is valid only under LP64. Arbitrary final WORK contents are
accepted because WORK has no returned-value contract. Public factor/pivot
immutability, row-major withholding, direct column-major publication and full
INFO sentinels are checked. The initial missing private scalar-kind include
and failed build are retained under `block-solve-first-finding-01`.

Standalone GNU Fortran runtime probes retain both controls and failures under
both ABIs in `block-solve-range-cause-01`. Scalar reciprocals become infinite;
for the large complex block, AKM1=0/OFF remains zero while BKM1=B/OFF becomes
NaN despite its exact value of one. Twenty-four native calls per run preserve
A and return INFO=0. These diagnostic commands do not grant mathematical
acceptance. The numerical blocker is `BLOCK-INDEFINITE-SOLVE-RANGE`.

Both ABIs pass six structural processes with 328 rejected calls, thirty-two
protected-memory queries, twenty-four empty executions and twenty-six source
count checks per variant. The next engineering checkpoint records 31 passes
and the same six required mathematical failures across 37 processes per ABI.
Each ABI also passes six concurrency processes. Every variant runs sixteen
groups, with 512 native solves, 512 empty noncalls and 512 structural rejections;
Linux factor pages are read-only while four workers share immutable factors,
pivots, plans and providers. Each worker owns its writable buffers and reports.

Four relocated static/shared installed consumers pass 3,456 cases each, using
only public headers. They cover TRF/TF2/actual SV origins, both independent
layouts and triangles, nrhs=0/1/3, n up to67, empty and completed singular
factors, expected solutions/residuals, immutable factors/pivots and padding.
Package metadata and runtime dependencies remain isolated after relocation.

The final sixteen profiles cover static/shared LP64/true-ILP64 Release, Debug,
ASC ASan+UBSan and ASC thread-sanitizer builds. Each of twelve ordinary/sanitizer
profiles records 37 passes, six required mathematical failures and 160 failed
assertions. Each of four thread-sanitizer profiles passes all six processes.
The canonical total is 540 processes: 468 passes, 72 required mathematical
failures, 1,920 failed assertions and zero skips. GNU C++/Fortran are 11.4;
pinned Fortran/BLAS provider internals are not sanitizer-instrumented.

Eighteen strict translation-unit checks, four standalone-header checks,
four package-manifest contexts and six malformed-manifest fixtures, both
architecture/dependency checks and the actual maintained CI selector pass.
Final guarded GNU-emitted ABI probes pass 144 cases per ABI. Twelve dynamic
exports are added and none removed per ABI; all four installed production
libraries match the final producers after normal CMake shared installation.
Doxygen covers 147 public headers and 2,549 public members with zero warnings.
The external `block-solve-final-audit/audit.json` binds source hashes, actual
caches/compiler records, provider attestations, verbose logs, JUnit results,
installed artifacts and executable hashes. Six implemented-unverified rows
have 144 reviewed triangle/independent-layout/origin modes. No Reference row
is promoted to fully verified.

Wider platforms, provider-internal instrumentation, these numerical gates and
all earlier programme blockers remain open. Continue with RK factor producers
(`P05.required.hetf2_rk`), reviewing separate E storage and its pivot contract
before the dependent solve, condition and inverse operations.
