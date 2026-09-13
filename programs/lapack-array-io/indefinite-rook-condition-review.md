# Rook indefinite reciprocal condition estimation

Status: **implemented, numerical acceptance blocked; FULL_PROGRAM_INCOMPLETE**.

Six pinned routines are exposed by `asc/dense/providers/lapack_indefinite_rook_condition.h`:
S/D/C/Z `SYCON_ROOK` and C/Z `HECON_ROOK`. Each has an explicit formula query
and execution overload. The existing classic condition family and all frozen
acceptance records remain separate.

## Reviewed source contracts

The authoritative sources are LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`:
[SSYCON_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/ssycon_rook.f),
[DSYCON_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/dsycon_rook.f),
[CSYCON_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/csycon_rook.f),
[ZSYCON_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zsycon_rook.f),
[CHECON_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/checon_rook.f), and
[ZHECON_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zhecon_rook.f).
Exact inventory rows, source hashes, complete source snapshots and actual GNU
Fortran 11.4 emissions are retained outside the source tree at
`master-continuation-20260910-01/continuation-20260912-01/rook-condition-prerequisite-01`.

S/D use real factors and WORK; C/Z use complex factors and WORK, with matching
real ANORM and RCOND. Upper/lower selection consumes full raw coefficients in
the selected triangle. SY uses transpose symmetry; HE uses conjugate transpose.
Complex one-norm estimation uses Euclidean scalar modulus through LACN2.
Original-Hermitian diagonal normalization is not applied to raw factors.

The caller supplies the finite nonnegative original one-norm and same-operation,
same-provider raw factors and signed one-based pivots tagged `kRook`. Adjacent
negative entries encode independent ordered interchanges; they need not be
equal. Active execution checks both targets against the active principal block.
Classic tags are rejected even when the integer values could also describe a
rook operation. Raw descriptors cannot establish historical provenance. A
successful nominal factor certificate is deliberately not required: singular
raw factors are valid condition-estimator input.

Queries inspect metadata and ANORM without reading arrays or entering Fortran.
Plans bind routine, scalar, provider/ABI, triangle, symmetry, dimensions, layout,
original/effective leading dimension and whether ANORM selects a quick return.
Active work is exactly 2*n scalar entries, n converted provider-width pivot
entries, and n additional simultaneous IWORK entries for real S/D only.
Row-major needs n*n scalar packing entries. Complex paths use no separate
RWORK. LACN2's 3*n INTEGER intermediate is checked for both paths. No native
workspace query, internal allocation, alternate provider or global state is
introduced. Scratch, operands, outputs and live metadata must be accessible
and disjoint; callers provide separate writable storage for concurrent calls.

Structural rejection precedes array and scratch writes. Unsafe aliases with
report metadata preserve the report itself; other preflight failures reset it
with no foreign INFO. N=0 returns RCOND=1; nonempty ANORM=0 returns RCOND=0.
These are successful noncalls and do not read factor or pivot entries. With
positive norm, an exact full zero 1-by-1 D coefficient preserves the provider's
INFO=0/RCOND=0 early return. Otherwise an evaluated-zero solve divisor yields
ASC kNumerical/kSingular and the zero-based block index before any native call.

The source defines INFO=0 or argument errors -1/-2/-4/-6; all nonzero foreign
INFO after ASC preflight is retained as a provider defect. Full-width INFO is
seeded to its minimum and RCOND to -1 immediately before entering the provider.
This detects omitted or partial-width writes. Negative RCOND is unusable
provider output; nonfinite RCOND remains visible with an accuracy warning.
Finite nonnegative RCOND is an estimate, and zero does not invent a pivot
failure. Factors, pivots, input norm and padding remain immutable.

## Independent numerical blocker

`ROOK-INDEFINITE-CONDITION-RANGE` remains open. For A=scale*I, exact RCOND=1.
All six required range processes fail in both initial static Release ABIs;
direct native-fidelity processes pass after separating prior factor workspace
writes from the condition guard baseline. No tolerance or assertion was removed.
Each process tests 64 cases: orders 1 and 7, both triangles/layouts, both factor
origins, and four scales. Actual rook factors are checked against exact identity
multipliers, diagonal coefficients and pivots before condition estimation.

The observed GNU profile behavior is:

| Scale | Order 1 RCOND | Order 7 RCOND |
| --- | --- | --- |
| Twice minimum normal | 1 | 0 |
| Minimum normal / 1024 | 0 | NaN |
| Twice minimum subnormal | 0 | NaN |
| Maximum finite | Infinity | Infinity |

Every case retains native INFO=0. The tiny scalar solve forms an unrepresentable
inverse; LACN2 can also overflow an intermediate absolute-value sum before its
normalization. At the maximum scale, the final reciprocal of a rounded
subnormal inverse estimate overflows. These source expressions and direct
native agreement identify provider algorithm/range limitations. ASC preserves
the selected reference behavior. The representable final answer and live
mathematical failures preclude a full numerical acceptance claim.

## Bounded validation

Ordinary tests cover analytic diagonal blocks, singular factors and two
nontrivial independent rook interchanges with a separate wide 3-by-3 adjugate
oracle. Validation includes rejected aliases, pivot bounds/tags, workspace and
plan errors, protected unread arrays, and INTEGER intermediates. Fault tests
run all six real native procedures before checking omitted/full/partial INFO
and missing, negative or nonfinite RCOND publication. Direct ABI probes compare
compiler-emitted function types and guarded argument/output storage. Concurrent
calls use shared immutable factors/provider/plans and independent writable
outputs/workspace/reports. The maintained installed consumer uses public
headers and targets only. `rook-condition-final-audit/audit.json` binds sixteen actual GNU11.4 profiles:
twelve static/shared Debug/Release/ASC-ASan+UBSan selections each pass31/37,
retaining six required range failures and528assertions. Four TSan selections
each pass6/6 concurrent tests. The combined selection is468 processes:
396passed,72required failures,6336failed assertions,zero skips. Pinned Fortran
and BLAS remain uninstrumented; no broader platform/provider admission is
inferred from these bounded runs.

The final audit also binds four relocated installed consumers (96 cases each),
eighteen strict translation units, four normal/no-exceptions header processes,
48 guarded direct ABI calls per integer ABI, twelve compiler emissions, twelve
additive shared exports per ABI with no removals, and Doxygen142/142headers,
2477members andzero warnings. Six contracts/48modes are normalized at
implemented_unverified. All2113requiredReference rows still lack complete
full-program verification.

Initial failed evidence is preserved. A new guard initially included earlier
Hermitian factor-packing writes; its condition baseline was corrected without
removing checks. The large real ILP64 concurrency fixture initially reused
factor-only integer storage; the condition-only guarded capacity now holds both
pivot and estimator arrays. Two intermediate Debug builds stopped on a new
fixture's empty-memory-view initialization error before any test ran, then
completed after correction. Thirty-six focused concurrency refresh passes
replace only affected prior concurrency results. Other numerical tests were
reused unchanged. The first installed example used sizeof on an empty
std::array with null data; guarded backing with explicit logical zero length
fixes the example while preserving the empty test and installed producer.

The previous pushed rook checkpoint exposed an integration omission: its
independent header oracle retained140 after adding the141st header. The explicit
oracle now lists and requires142 with both rook headers, and all four
build/copied/installed/relocated manifest contexts pass, together with six
fixture steps. The original hosted CI failure and local recovery are retained
in `rook-hosted-read-01` and `rook-header-manifest-fix-01`.
