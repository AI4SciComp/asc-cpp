# Blocked indefinite inverse contract review

Status: **implemented; bounded verification recorded; numerical gates open**.
Programme status: **FULL_PROGRAM_INCOMPLETE**.

The twelve LAPACK 3.12.1 routines are pinned at
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`:

| Symmetry | Driver | Explicit block operation |
| --- | --- | --- |
| Real symmetric | SSYTRI2, DSYTRI2 | SSYTRI2X, DSYTRI2X |
| Complex transpose symmetric | CSYTRI2, ZSYTRI2 | CSYTRI2X, ZSYTRI2X |
| Complex Hermitian | CHETRI2, ZHETRI2 | CHETRI2X, ZHETRI2X |

Their source declarations, complete argument contracts, hashes and 24 actual
GNU Fortran 11.4 LP64/true-ILP64 prototype emissions are retained under
`continuation-20260912-01/block-inverse-prerequisite-01`. All twelve occur in the
pinned `lapack.h`; no private Fortran prototype is invented. The executed ABI
comparison removes only C input-pointee const qualifiers, retaining scalar
types, pointer levels, argument order and the trailing `size_t` CHARACTER
length. The final probes run 300 guarded direct cases per ABI, including empty,
scalar, singular, paired pivots, both triangles and five block variants.

## Public storage and workspace

`lapack_indefinite_block_inverse.h` exports 24 query/execute declarations.
S/D use real A and WORK; C/Z use complex A and WORK. Native dimensions,
block size/LWORK, signed pivots and INFO use the selected INTEGER width.
The mutable square selected factors and immutable exact-n `kBunchKaufman`
pivots must come from one same-provider, same-scalar classic TRF, TF2 or
corresponding actual SV operation. Completed singular factors are accepted.
Adjacent equal negative pivots encode one ordered interchange per 2-by-2
block. Unequal rook pairs and incorrect directional bounds are rejected.
Inversion may invalidate every borrowed factor view, including on singular
returns. No const borrowed factor is cast into mutable public storage.

UPLO selects the only referenced/published triangle. SY uses transpose
symmetry even for complex data; HE uses conjugate transpose. Complete raw
Hermitian diagonal and block coefficients are retained when packing factors.
Public pivots, the other triangle and padding remain unchanged. No full
symmetric/Hermitian matrix or normalized diagonal is materialized.

Active scalar WORK has `(n+nb+1)*(nb+3)` live entries. For TRI2, pinned ILAENV
returns nb=64 for SSYTRI2 and CHETRI2/ZHETRI2, and nb=1 for D/C/Z SYTRI2.
The source calls classic TRI when n<=nb and TRI2X otherwise. The public query
honors the documented product even when the source's native query returns n
for a small matrix. Guarded native `LWORK=-1` probes pass 72 cases per ABI:
C/Z SYTRI2 return zero at n=0, while the other four return one. These native
query differences do not become an unsafe scalar-to-integer sizing path.
The public formula query reads metadata only and makes no foreign call.
Compute WORK has no documented result-value interpretation.

TRI2X takes an explicit positive block size. The source does not validate NB;
NB=0 prevents its block loop from advancing, so public preflight rejects it.
NB>n is allowed subject to arithmetic bounds. TRI2X has no LWORK or native
workspace query. Both variants additionally need n provider-width signed
pivots in caller byte storage; row-major factors need n*n live layout entries.
N=0 is a successful noncall with no numerical reads/writes or scratch.

Plans bind routine variant, block size, scalar, provider/ABI, triangle,
symmetry, order, layout and original/effective LDA. Preflight checks NB+3,
N+NB+1, scalar/packing products and byte sums. TRI2 also needs a native INTEGER
LWORK product; TRI2X has no such product parameter. SY swaps, and the blocked
HE lower HESWAPR path, need terminal cursor `1+(N-2)*LDA` checked against
INTEGER. The nested upper TRTRI block loop additionally needs
`1+ceil(N/64)*64` to fit when n>64. Protected-memory queries/empty executions,
source boundaries, negative/zero/huge NB, stale variant/block-size plans and
132 structural rejections per scalar/variant are covered.

All numerical buffers, scratch and live metadata must be disjoint and
accessible to the explicit CPU context. Structural rejection preserves
numerical buffers/scratch; metadata/report alias rejection also preserves the
report. Otherwise it resets before further preflight. Calls allocate nothing,
change no global state, transfer nothing and use no fallback algorithm.
Concurrent calls can share immutable providers/plans/pivots while using separate
writable A/WORK/report objects.

## INFO and partial output

Source argument failures are -1 UPLO, -2 N, -4 LDA, and TRI2 -7 LWORK. These
are prevented structurally. A positive INFO reports an exactly zero complete
scalar 1-by-1 D entry with a positive pivot. The scan is upper n..1 or lower
1..n. TRI2X calls SYCONV(C) **before** that scan, moving 2-by-2 offdiagonal
entries to WORK, zeroing their A positions and permuting multipliers. A valid
positive INFO can therefore leave converted A and changed WORK. The wrapper
publishes this native selected partial output in both layouts and reports
kSingular/kDocumentedPartial with raw INFO and a zero-based diagnostic index.
It does not promise a usable inverse or the original factor representation.
The small TRI2 classic branch preserves A on positive INFO.

Full-width INFO begins at INTEGER minimum after preflight. An unwritten,
partial-width, negative or source-inconsistent INFO, or mutation of converted
input pivots, produces a provider-defect report with raw INFO and unusable
output. Row-packed publication is withheld on defect; direct column-major A
may have changed. Fault tests include a mixed 2-by-2 block and zero 1-by-1
entry to prove both conversion before positive INFO and publication withholding
on defects. There are 160 cases per scalar/variant. Native driver-to-TRI2X
nesting is observed as one public provider call.

INFO=0 records native completion without certifying finite values, conditioning,
exactly real Hermitian diagonals or singular 2-by-2 detection. There are no
RCOND, FERR or BERR results. No provider arithmetic is changed.

## Numerical decisions and retained failures

`BLOCK-INDEFINITE-INVERSE-RANGE` retains twenty required complex range test
processes per profile. Each scalar/variant executes 72 cases: 60 have
representable exact inverses; 12 are explicitly nonrepresentable. The failing
zero-diagonal 2-by-2 block has both offdiagonal components at 0.75*maximum
finite, with representable subnormal inverse components. Blocked complex
arithmetic returns nonfinite output with INFO=0; the small HE driver/classic
branch returns zero offdiagonals. The 0.25*maximum control passes. All S/D
range mathematics and every native range-fidelity process pass. Intermediate
source arithmetic uses complex T/D in TRI2X, including HE; this differs from
the real T/D in classic HETRI. The retained standalone GNU Fortran runtime
probe `block-inverse-range-cause-01` confirms complex SY T/T becomes NaN;
complex HE T becomes (Inf,0) and D becomes (-Inf,NaN). Driver and all four
explicit block variants return INFO=0 at both precisions/ABIs. The 0.25 control
and 0.75 failure are separate raw runs. No finite-result assertion is waived.

`BLOCK-INDEFINITE-HERMITIAN-DIAGONAL` retains ten required exact-diagonal
mathematical processes per profile. Blocked HE uses complex GEMM/TRMM and
publishes their sums without resetting the imaginary diagonal. Small roundoff
components remain while reconstruction, finite-value and both inverse-product
residual checks pass. All selected output bytes match direct LAPACK, including
the diagonal. A separate 60-case direct native TRF/TRI2/TRI2X probe per ABI
reproduces 1,237 exact-zero assertion failures, with no other invariant failure.
For upper n=67 TRI2, maximum imaginary diagonal is about 5.49e-12 in single
precision and 7.21e-21 in double; maximum real diagonal is about 0.25003.
This is recorded as an unmet strict acceptance condition, not an inferred
provider ABI defect. The exact-zero assertion remains; no tolerance relaxation
or wrapper normalization is applied.

The first double check passes 176 cases. Initial expanded execution retained
an inherited classic-concurrency oracle that incorrectly expected unchanged
singular A. That draft and failure logs are preserved. The full-byte assertion
now compares each concurrent singular output with a separately executed serial
partial-output baseline; original shared factors and all rollback assertions
remain. Every concurrency scalar/variant passes eight groups: 384 concurrent
native calls, eight serial partial-output calls, 64 empty noncalls and 64
structural rejections. No frozen classic test is edited.

Both engineering ABIs run 215 processes: 185 pass, 30 required mathematical
processes fail, 6,176 failed assertions and zero skips per ABI. Sixty ordinary
mathematics/fidelity processes each run 88 cases, with independent long-double
Gauss-Jordan inverse and both product residuals; actual TRF/TF2 origins,
paired permutations, both triangles/layouts, n up to67 and scaled cases remain.
All four relocated static/shared installed consumers pass 2,880 cases each,
covering TRF/TF2/actual SV origins, both variants, four explicit block sizes,
empty/scalar/paired/larger/singular inputs and selected storage guards.

The final sixteen profiles cover static/shared LP64/true-ILP64 Release, Debug,
ASC ASan+UBSan and ASC thread-sanitizer builds. Each of twelve ordinary/sanitizer
profiles records 185 passes, 30 required mathematical failures and 6,176 failed
assertions; each of four thread-sanitizer profiles passes all thirty processes.
The canonical total is 2,700 processes: 2,340 passes, 360 required failures,
74,112 failed assertions and zero skips. GNU C++ and GNU Fortran are 11.4;
the pinned Fortran/BLAS provider internals are not sanitizer-instrumented.
Original source hashes, caches, compiler records, provider attestations,
verbose logs, JUnit results and executable hashes are bound by
`block-inverse-final-audit-02/audit.json` in the external continuation evidence.

Eighteen strict translation-unit checks, four standalone-header checks,
four package-manifest contexts and six malformed-manifest fixtures, and both
architecture/dependency checks pass. Each ABI adds 24 dynamic exports and
removes none. All four installed production libraries match their final
producer artifacts after normal CMake shared-library installation. Doxygen
covers 146 public headers and 2,537 public members with zero warnings.
The mapping records twelve implemented-unverified routines and 360 reviewed
modes; no Reference row is promoted to fully verified.

Full provider instrumentation, broader platforms, these numerical gates and
complete Reference verification remain open. Continue
classic TRS2 next (`P05.required.hetrs2`), using existing factor producers before
TRI3 and its additional RK factor-storage dependencies.
