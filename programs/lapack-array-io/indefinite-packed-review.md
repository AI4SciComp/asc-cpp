# Packed Bunch–Kaufman producer contract

Active task: `P05.required.hptrf`, after two-stage drivers integrated at
`bfc95f44ff1e6ace5dfa39e05a75ba52b98203e2`. This slice contains exactly
`SSPTRF`, `DSPTRF`, `CSPTRF`, `ZSPTRF`, `CHPTRF`, and `ZHPTRF` from the pinned
Reference inventory. Checked entry points are `QuerySptrfWorkspace`/`Sptrf`
for S/D/C/Z and `QueryHptrfWorkspace`/`Hptrf` for C/Z. They remain in the
explicit Dense LAPACK provider component. No provider, notice, adoption,
platform, native20, array-I/O or experimental robust PPSVX policy changes.

## Source and ABI

Pinned source commit is `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`.
Each exact routine has five pointer arguments `UPLO,N,AP,IPIV,INFO`, followed
by the actual GNU `size_t` hidden CHARACTER length. There is no WORK argument
or native workspace query. The public query reads descriptors only.
Source-instance hashes and twelve actual GNU emissions are recorded under
`packed-indefinite-prerequisite-01` in the existing continuation evidence
root. Both actual LP64 and true ILP64 guarded native probes pass 1,296 cases
and 2,592 native calls per ABI. These compare original ignored Hermitian
imaginary diagonal NaNs against explicit real diagonals, inspect full-width
INFO/pivots and guards, and independently reconstruct completed factors.
This establishes the selected ABI scope; it does not admit other providers
or platforms.

## Storage, pivots and validity

`DenseBlasPackedMatrixView` provides order and row/column packed layout;
UPLO separately selects the stored triangle. Column-major offsets for
zero-based `(i,j)` are `j*(j+1)/2+i` for upper and
`j*n-j*(j-1)/2+i-j` for lower. Row-major offsets are
`i*(i+1)/2+j` for lower and `i*n-i*(i-1)/2+j-i` for upper.
Both layouts use explicit caller-owned live scalar packing and publish
all packed entries only after valid native completion. Column-major input
and output use contiguous copies; row-major storage is explicitly reordered.
HPTRF source takes real diagonal components before
pivot decisions, interchanges and updates; the guarded native comparison
confirms that original imaginary NaNs do not contaminate output. Packing
preserves those original imaginary components. Complex symmetric SPTRF retains full complex
input diagonals and uses ordinary transpose; HPTRF uses conjugation.

D occupies the selected diagonal and adjacent block offdiagonals; remaining
packed entries store multipliers. U/L are ordered products of permutation
and unit-triangular factors, rather than a single permuted triangular matrix.
For upper, positive IPIV[k] identifies a 1x1 interchange at k, and an equal
negative pair at k-1,k identifies a 2x2 block and swap of k-1 with -IPIV[k].
For lower the pair lies at k,k+1 and swaps k+1 with -IPIV[k]. Raw pivots remain
signed, one-based values. Native pivots use caller byte storage with explicit
array object lifetime and full-width minimum sentinels before the call.
All values, pair equality, pair boundaries and UPLO-dependent swap bounds
are validated before public pivots or row-packed factors are published.
Existing dense, ROOK, RK and Aasen factor factories gain no packed meaning;
callers retain common-call scalar/provider/triangle/symmetry provenance for
matching packed consumers.

The provider initializes INFO and completes all pivots even after a zero
column. Upper traversal reports the first zero encountered from n downward;
lower traversal proceeds from 1 upward. INFO=0 yields complete output.
Positive INFO publishes completed raw factors/pivots, numerical status,
documented-partial validity and diagnostic index INFO-1. A zero reported
D diagonal maps to singular; otherwise partial-result preserves the existing
nonfinite-input report policy. This does not claim finite-data validation or
invertibility from INFO alone. Missing, negative or out-of-range INFO and
malformed pivots or changed native array guards are provider defects. Public
pivots and matrix output in both layouts are withheld on a defect. Empty
execution validates metadata, uses no scratch, accesses no numeric arrays
and does not call the provider.

## Counts, workspace and aliases

The plan binds scalar/provider identity, order, exact pivot count, triangle,
symmetry and packed layout. Nonempty calls need n+2 native INTEGER objects
and n*(n+1)/2+2 live scalar objects in either layout. Each region contains a
live guard object before and after the logical native array. There is no
native scalar WORK region. All operand spans, scratch regions and metadata
objects are checked for access and overlap before mutation. Structural
rejection preserves numeric storage and scratch; unsafe metadata overlap
also preserves the report. Independent calls may share immutable provider
and plans with separate mutable storage, scratch and reports.

Packed Cholesky's INTEGER bounds are inapplicable. Upper SPTRF/HPTRF compute
`(N-1)*N` before division, and their candidate IMAX is at most N-1, bounding
`IMAX*(IMAX+1)`. Lower routines unconditionally compute `N*(N+1)` and use
`2*N` in rank-2 packed indices. Selected SPR/HPR, SCAL, SWAP and IAMAX calls
use unit increments. For N>=5, the selected upper/lower products dominate
all other raw products, intermediate additions and final loop cursors;
N<=4 fits either supported ABI. The pure count guard therefore admits upper
N=46341 and lower N=46340 for LP64, and rejects their successors. Its ILP64
limits are upper N=3037000500 and lower N=3037000499. The existing public
packed descriptor's stronger `N*(N+1)` and byte bounds remain in effect,
including for upper ILP64; this slice does not alter descriptor admission.
Actual byte totals are checked separately. There is no packing through dense
n-by-n product storage or inferred provider block size.

## Frozen finite delivery classes

The ordinary public matrix uses both triangles and packed layouts, S/D/C/Z
symmetric and C/Z Hermitian types, orders 0,1,2,3,4,5,8,17,65, scales
2^-20,1,2^20, and four input classes: interchanged indefinite blocks, all-zero
singular, an isolated zero row/column, and explicit 2x2 indefinite blocks.
That is 432 cases per scalar/symmetry class, separately executed for
independent reconstruction and byte-exact native fidelity. The test-owned
reverse Schur-complement reconstruction derives from the maintained classic
BK oracle, with independent packed indices and explicit finiteness checks.
It does not call a solve or copy provider code. Query immutability, allocation,
pivot/native/packing guards, complete/partial reports and empty nonaccess are
checked alongside mathematics.

Required range tests retain representable subnormal and large finite inputs,
separately from native fidelity. Required engineering classes include checked
INTEGER boundaries, protected metadata-only queries, workspace/alias/access
rejection, stale plans, full-width INFO/pivot fault injection, independent
parallel calls, both actual ABIs, Release/Debug/ASan+UBSan/TSan and static/shared
configurations, relocated installed consumers, strict compilation/static
analysis, self-contained headers, exports, documentation and package checks.
These finite classes do not become passing by excluding provider failures.

## Endpoint containment and scalar nonentry

The initial range tests exposed a pinned-provider memory defect on finite,
nonsingular, lower-packed order-three matrices with a subnormal isolated 2x2
block and a trailing scalar. Reciprocal overflow creates a NaN trailing
diagonal; the terminal iteration then selects an invalid 2x2 step and writes
AP[count] and IPIV[n], outside the logical native arrays, with INFO zero.
All six scalar/symmetry variants reproduced this in both actual integer ABIs.
The source review also bounds the corresponding upper endpoint at AP[-1]
and IPIV[-1]. One live guard at each endpoint contains these reviewed writes;
complete pivot and guard validation precedes publication. This is a scoped
interoperability repair, not a claim of general provider memory safety.

For order one, a significant NaN can select that terminal step before IMAX
has been initialized. The checked entry point returns `kNumerical`,
`kPartialResult`, `kUnusable`, and diagnostic index zero before any scratch
write or native call. Native INFO remains absent. Hermitian imaginary
diagonal NaNs remain ignored and execute normally. No finite input is
excluded, and no provider arithmetic or factorization algorithm changes.

The retained direct-native span probe uses actual containing arrays and
asserts logical endpoint integrity. Its failures are mandatory gates, not
expected-failure passes. The fidelity comparator now also owns both native
pivot guards and distinguishes a faithfully reported provider defect from a
usable factorization. Required mathematical tests still demand complete
usable factors and independent finite reconstruction on the same inputs,
with unchanged tolerances. Fault injection checks each of the four endpoints
independently, in addition to every prior INFO/pivot defect; all rejected
calls must preserve public matrix and pivot bytes in both layouts. Scalar
NaN controls count native calls and check scratch preservation directly.

## Current engineering checkpoint — 2026-09-14

Six checked producers and their 24 triangle/layout modes are registered as
`implemented_unverified`. Required mathematical/provider acceptance remains
open. Final source and execution identities are bound under
`completion-execution-01/packed-final-audit.json` in the existing continuation
evidence root; the initial and failed strict attempts remain preserved.

All sixteen actual LP64/true ILP64 static/shared Release, Debug, ASC-ASan+UBSan
and TSan profiles execute 612 processes: 468 pass, 144 required failures,
zero skips. Each of the twelve normal/sanitizer profiles passes 37/49 and
retains six finite-range mathematical failures and six native endpoint
failures. The four TSan profiles each pass 6/6. Each concurrency scalar class
uses 24 groups, four separate mutable operand/workspace/report sets, eight
repetitions, independently reconstructed serial baselines, singular and paired
blocks, and stale-plan rejection. The pinned Fortran/BLAS remains uninstrumented.

Four relocated static/shared public consumers pass 576 workflows each using
installed headers and only the requested provider component. They cover all
six scalar/symmetry classes, both layouts/triangles, empty and zero factors,
1x1/2x2 blocks, exact A=D reconstruction, signed pivots, ignored Hermitian
imaginary NaNs, guards and stale-plan preservation. Both actual emitted-ABI
probes pass 1,296 guarded cases each after the final test refactor. Nine source
translation units plus the installed consumer pass strict analysis with the
repository configuration and its complete header filter. No assertion, input
or tolerance was weakened to satisfy those checks.

Four standalone-header tests, ten package-manifest tests, three architecture
checks and twelve added/zero removed exports per ABI pass. Doxygen covers all
159 public headers and 2,717 public members without warnings. The actual CI
selector includes all 49 required packed runtime tests and both public-header
tests; its full required-set check passes. The older recovery statement that
CI selected all 43 tests was incorrect: the old regex omitted this family.
Both listing and execution regexes are now repaired.

The six routines remain callable but unverified. Provider range/endpoint,
security, XBLAS, notice and broader platform decisions remain open. Their
records permit independent work to advance under the user's instruction;
they grant no numerical waiver. The next implementation is the matching
SPTRS/HPTRS packed solve family. The full P00–P11 programme is incomplete.
