# Rook indefinite inverse contract review

Status: **implemented; complex numerical acceptance blocked; bounded verification recorded**.
Programme status: **FULL_PROGRAM_INCOMPLETE**.

The six reviewed LAPACK 3.12.1 sources are pinned at
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`:
- [SSYTRI_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/ssytri_rook.f)
- [DSYTRI_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/dsytri_rook.f)
- [CSYTRI_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/csytri_rook.f)
- [ZSYTRI_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zsytri_rook.f)
- [CHETRI_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/chetri_rook.f)
- [ZHETRI_ROOK](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zhetri_rook.f)

Full sources, hashes, recovered repository state and all twelve actual GNU
Fortran 11.4 LP64/true-ILP64 prototype emissions are preserved outside the
source tree under `continuation-20260912-01/rook-inverse-prerequisite-01`.
None of these six routines is declared in pinned `lapack.h`. Private GNU ABI
prototypes retain the emitted pointer types and trailing size_t CHARACTER
length. Executed guarded probes and exact emitted-signature comparisons are
required before provider admission; portable Fortran ABI is not inferred.

## Storage, provenance and mutation

S/D use real A and WORK, C/Z complex A and WORK. Native dimensions, INFO and
signed IPIV use the selected INTEGER width. The public API accepts explicitly
mutable square selected factor storage and immutable raw kRook pivots. Both
come from one same-provider same-scalar rook factorization (including direct
drivers). A successful nominal borrowed factor is deliberately not the mutable
operand: inversion invalidates every borrowed view of overwritten storage.
Completed singular factors remain admissible to preserve native positive INFO.

UPLO selects the only triangle referenced and overwritten. SY uses transpose
symmetry even for complex values; HE uses conjugate-transpose symmetry. Each
negative pivot partner encodes a distinct ordered interchange. Public pivots,
unselected coefficients and padding remain unchanged. Row-major factors use
selected-triangle packing; full raw Hermitian diagonals/block coefficients
are preserved. The input is a factor, not original Hermitian matrix storage.
The native singular scan compares a full complex diagonal to complex zero;
its later inverse arithmetic reads the real component of Hermitian diagonals.

## Workspace, preflight and execution

Native WORK is n live scalar entries and has no query/result interpretation.
There is no LWORK parameter or upstream workspace query. The ASC formula query
needs no numerical reads or foreign call. Active execution additionally needs
n native signed pivot entries in caller byte storage, and n*n live scalar
packing entries for row-major A. Column-major A requires no packing. N=0
requires neither scratch nor numerical access and is a successful noncall.

Plans bind actual scalar-prefixed routine, provider/ABI, triangle, symmetry,
order, layout and original/effective LDA. A single-column unused stride is
normalized only at the foreign boundary. Checked arithmetic includes N+1 and
SY's strided SWAP cursor 1+(N-2)*LDA. HE performs the corresponding conjugating
swaps elementwise and its BLAS vectors have unit strides. Workspace byte sums
and packing products are also checked. All operands, scratch and live metadata
must be accessible and disjoint. Structural rejection preserves numerical
buffers and scratch; unsafe metadata/report aliases preserve the report too.
Otherwise the report resets before further preflight with absent INFO and no
provider call. No allocation, global state, transfer or alternate algorithm
is introduced; concurrent calls require disjoint writable buffers/reports.

## INFO and numerical semantics

Source argument errors are -1 (UPLO), -2 (N), -4 (LDA). The wrapper prevents
these structural calls. Positive INFO names an exactly zero 1-by-1 D entry,
scanned upper from n to 1 and lower from 1 to n. This scan precedes all native
numerical writes; raw factors remain and no inverse is produced. The report
retains exact positive INFO, zero-based diagnostic index, kSingular and
kDocumentedPartial. The wrapper checks source-consistent INFO against that
immutable input scan. Full-width INFO begins at INTEGER minimum; missing,
partial-width, negative or inconsistent INFO is a provider defect with raw
INFO retained and output unusable. Changed native input pivots also trigger a provider defect. Packed inverse publication is withheld on
defect; directly exposed column-major A may already have changed.

INFO=0 means native completion. The source does not certify finite values,
conditioning or singular 2-by-2 blocks. Real SY and complex HE divide a 2-by-2
block by the magnitude of its offdiagonal and form a scaled determinant;
complex SY uses the complex offdiagonal directly. One-by-one blocks form a
reciprocal before matrix-vector updates. No wrapper scaling or substitute
algorithm is added. Mathematical acceptance and direct-provider fidelity are
separate tests; nonrepresentable exact inverse entries must not be mislabeled
as failures requiring finite results. There are no RCOND, FERR or BERR outputs.

## Final bounded validation

Six contracts contain 72 reviewed triangle/layout/factor-origin modes. All
sixteen static/shared LP64/ILP64 Debug/Release/ASC-ASan+UBSan/TSan profiles are
complete: 468 processes, 420 passes, 48 required mathematical failures,
1,056 failed assertions and zero skips. Each of twelve ordinary/sanitizer
profiles passes 33 of 37 tests, with four complex mathematical failures. All
four TSan profiles pass six concurrency tests. ASC and tests are instrumented;
the pinned Fortran/BLAS provider is not. No other OS/provider admission follows.

All four relocated public consumers pass 432 cases, covering TRF_ROOK,
TF2_ROOK and SV_ROOK factor origins. Eighteen strict translation units, four
standalone-header tests, 120 final actual-emission ABI cases, all four package
header-manifest contexts plus six fixtures, and exact architecture/dependency
checks pass. Shared exports add twelve symbols and remove none in each ABI.
Doxygen covers 144 headers and 2,501 public members without warnings. Installed
production libraries are byte-identical to the final producer after the normal
CMake shared-RPATH installation transform.

Final source hashes, actual compiler/cache/provider snapshots, raw JUnit and
verbose logs are bound by `rook-inverse-final-audit/audit.json`. The mapping
extension preserves every historical execution and frozen Native20 row; no
Reference row receives fully verified status. Required complex numerical gates,
provider-internal instrumentation, broader platform/provider admission and
complete normalized execution records remain open. Continue the independent
six classic SYTRI/HETRI inverses (`P05.required.hetri`) in this workspace.

## Initial collected results

LP64 and ILP64 Release each run 37 processes: 33 pass, four required
mathematical tests fail, zero skip. Each of six ordinary processes exercises
88 cases with an independent long-double Gauss-Jordan inverse and both product
residuals; both independent negative rook partners are checked on order three.
Each native-output fault process covers 128 cases including positive singular
INFO, missing/partial/inconsistent INFO and changed native input pivots.
Each structural process covers 104 rejections and protected unread query/empty
storage; eight concurrency groups per scalar use shared immutable pivots/plans,
384 native inverse calls, 64 empty noncalls and 64 preflight rejections.
Both relocated static installed consumers pass 432 cases using factors from
TRF_ROOK, TF2_ROOK and actual SV_ROOK drivers. These initial checks remain preserved alongside the final bounded matrix.

`ROOK-INDEFINITE-INVERSE-RANGE` affects C/Z SYTRI_ROOK and HETRI_ROOK.
Each range process retains 72 cases: scalar, seven-entry identity, and a zero-
diagonal 2-by-2 block; two triangles/layouts and six scales. Sixty cases have
representable exact inverses. Twelve tiny cases have nonrepresentable inverse
entries; the mathematical test records that classification without imposing a
false finite-output requirement, while direct-native fidelity still executes.
S/D mathematical processes and all six fidelity processes pass. Complex SY
processes fail 32 assertions each; complex HE processes fail 12 each, totaling
88 required failed assertions per profile. No test is marked expected-failure.

At scale = 0.75*maximum finite, the block offdiagonal has that value in both
real and imaginary components. The exact inverse components are representable
subnormals. Actual factorization completes with unchanged block storage.
SY's intermediate AKKP1=T/T becomes NaN in the admitted GNU profile; its inverse
offdiagonal is NaN. HE's ABS(T) overflows to infinity, its scaled determinant is
negative infinity, and inverse offdiagonals become zero. INFO remains zero.
The scale = 0.25*maximum finite control passes. Direct native calls and a
separate GNU Fortran arithmetic probe reproduce both mechanisms in LP64 and
ILP64; `rook-inverse-range-cause-01` preserves the outputs and failed initial
link attempts caused by the true-ILP64 archive suffix. No provider or wrapper
numerical kernel was changed to hide these failures.

Evidence remains outside the source tree: `rook-inverse-first-01` preserves the
initial test-factory compile failure, `first-02` passes the smallest double
check, `first-seven-01` passes all six variants and the ABI probe;
`rook-inverse-engineering-{abi}-01/02/03` retain all expanding selections.
`rook-inverse-prerequisite-01/executed-abi-{abi}` passes exact GNU emitted types
and 60 guarded direct calls per integer ABI; the final emitted-probe refresh passes another 60 guarded cases per ABI. Strict refactoring snapshots are retained
in `rook-inverse-style-refactor-01`. No numerical threshold changed.
