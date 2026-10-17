# Classic indefinite inverse contract review

Status: **implemented; complex numerical acceptance blocked; bounded verification recorded**.
Programme status: **FULL_PROGRAM_INCOMPLETE**.

The six reviewed LAPACK 3.12.1 sources are pinned at
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`:
- [SSYTRI](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/ssytri.f)
- [DSYTRI](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/dsytri.f)
- [CSYTRI](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/csytri.f)
- [ZSYTRI](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zsytri.f)
- [CHETRI](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/chetri.f)
- [ZHETRI](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zhetri.f)

Full sources, hashes, recovered repository state and all twelve actual GNU
Fortran 11.4 LP64/true-ILP64 prototype emissions are preserved outside the
source tree under `continuation-20260912-01/classic-inverse-prerequisite-01`.
All six routines are declared in pinned `lapack.h`. The wrapper calls those
declarations. Actual GNU Fortran 11.4 emitted signatures are compared after
erasing only C input-pointee const qualifiers; scalar types, pointer levels,
argument order and trailing CHARACTER length still match exactly. Guarded
LP64/true-ILP64 probes execute 60 cases each. No portable Fortran ABI or other
compiler/provider admission is inferred.

## Storage, provenance and mutation

S/D use real A and WORK, C/Z complex A and WORK. Native dimensions, INFO and
signed IPIV use the selected INTEGER width. The public API accepts explicitly
mutable square selected factor storage and immutable raw kBunchKaufman pivots. Both
come from one same-provider same-scalar classic factorization (including direct
drivers). A successful nominal borrowed factor is deliberately not the mutable
operand: inversion invalidates every borrowed view of overwritten storage.
Completed singular factors remain admissible to preserve native positive INFO.

UPLO selects the only triangle referenced and overwritten. SY uses transpose
symmetry even for complex values; HE uses conjugate-transpose symmetry. Each
adjacent equal negative pivot pair encodes one interchange for a 2-by-2 block.
Unequal negative rook pairs are rejected; the families are not interchangeable. Public pivots,
unselected coefficients and padding remain unchanged. Row-major factors use
selected-triangle packing; full raw Hermitian diagonals/block coefficients
are preserved. The input is a factor, not original Hermitian matrix storage.
The native singular scan compares a full complex diagonal to complex zero;
its later inverse arithmetic reads the real component of Hermitian diagonals.

## Workspace, preflight and execution

Native WORK has n live scalar entries for S/D SYTRI and C/Z HETRI.
CSYTRI/ZSYTRI document 2*n live complex entries; the query honors that public
contract even though these pinned implementations address at most n-1.
WORK has no query/result interpretation.
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

All sixteen static/shared LP64/true-ILP64 Debug/Release/ASC-ASan+UBSan/TSan
profiles are complete: 468 processes, 420 passes, 48 required mathematical
failures, 1,056 failed assertions and zero skips. Each of twelve ordinary or
ASan+UBSan profiles passes 33 of 37 tests; the four complex mathematical range
processes remain required failures. All four TSan profiles pass six concurrency
tests. Actual GNU11.4 C++ compiler/cache/provider attestations are captured for
every profile. ASC and tests are instrumented; the pinned Fortran/BLAS provider
is uninstrumented. No wider OS/provider admission follows from these checks.

All four relocated public consumers pass 432 cases each. Eighteen strict
translation units, four standalone-header tests, 120 actual-emission direct ABI
cases, four package header-manifest contexts plus six fixtures, and exact
architecture/dependency checks pass. All dynamic defined symbols are compared:
twelve exports are added and none removed in each ABI. Doxygen covers 145 public
headers and 2,513 documented members with zero warnings. Installed production
libraries match the final producer after CMake's normal shared-RPATH transform.

`classic-inverse-final-audit/audit.json` binds eighteen frozen source and build
registration inputs, actual profile snapshots, raw JUnit/verbose logs, consumer
binaries, ABI emissions and the corrected Fortran cause probe. The mapping
extension preserves frozen Native20 rows and historical executions, without
promoting any Reference row to fully verified. Required complex numerical gates,
provider instrumentation, broader platform/provider admission and complete
normalized execution records remain open. Continue SYTRI2/HETRI2 and their
explicit-block TRI2X dependencies (`P05.required.hetri2`) in this workspace.

## Initial execution evidence

The first double-real check passes 88 cases. Both Release integer ABIs then
run 37 processes: 33 pass, four required complex mathematical tests fail,
88 failed assertions per ABI and zero skips. All six ordinary tests, all six
direct-native fidelity tests, guarded ABI calls, full-width INFO and immutable
pivot fault tests, structural rejection tests and concurrency tests pass.
Each ordinary scalar process covers 88 cases and checks an independent long-
double inverse, both product residuals, actual equal-negative-pair permutation,
raw factor origins, singular INFO, selected storage, padding and no allocation.
Structural tests exercise 116 rejections per class, including a valid rook pair
that is invalid for classic storage, one-entry-short WORK and directional
negative pair bounds. Each concurrency class has eight groups, 384 native calls,
64 empty noncalls and 64 preflight rejections with shared immutable pivots/plans.

Both relocated static public consumers pass 432 cases across TRF, TF2 and
actual SV factor origins. The source review confirms SYSV/HESV call TRF;
TRS2 restores temporary SYCONV(C) conversion with SYCONV(R), while NRHS=0
returns before any conversion. Native driver/factor implementations are unchanged.
The public inverse accepts matching same-provider raw factors as a caller
precondition, including completed singular factors.

`CLASSIC-INDEFINITE-INVERSE-RANGE` affects C/Z SYTRI and HETRI. Each range
process retains 72 cases, 60 with representable exact inverses and 12 with
nonrepresentable inverse entries. The latter classification is explicit, without
requiring impossible finite results; native fidelity still executes. The four
failing complex cases per class use a zero-diagonal 2-by-2 block whose real and
imaginary offdiagonal components are each 0.75*maximum finite. Its exact inverse
components are representable subnormals. All calls return INFO=0. Complex SY
forms NaN in T/T and returns NaN offdiagonals; HE ABS(T) overflows and the inverse
offdiagonals become zero. The 0.25*maximum finite control passes. S/D mathematical
range tests and all native fidelity processes pass. No required test is waived.

`classic-inverse-range-cause-02` contains the corrected standalone GNU Fortran
arithmetic/direct-call probe with valid classic [-1,-1] pivots and documented
2*n complex SY workspace. The first adapted probe is retained in `-01` but its
raw direct calls are excluded from admission because it retained rook pivot
encoding and the smaller workspace. The C++ range tests already use actual
classic factorization and the correct workspace. No provider arithmetic,
assertion or tolerance was changed.

Evidence is outside the source tree under `continuation-20260912-01`:
`classic-inverse-prerequisite-01`, `classic-inverse-first-01`,
`classic-inverse-engineering-{abi}-01`, and the distinct ABI, installed and
arithmetic-probe directories. The final bounded validation and source identity are recorded separately
above; initial attempts remain preserved. FULL_PROGRAM_INCOMPLETE remains
the programme status.
