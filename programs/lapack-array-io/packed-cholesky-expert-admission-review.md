# Ordinary packed positive-definite expert admission

PPSVX4 remains required and unregistered. Source/count audit
`f6b4c2104e51f514b8263af15292c44b3713deb972419b0e888286858001e15c`
and its passing seal bind fourteen passing raw records: six count/build/test/
strict/format and eight source/ABI/precision checks. The1129-file count
snapshot is the exact1127-file v27 plus two owned files. Inherited metadata
identifies that base; it does not fabricate a newly frozen candidate archive.

The first audit lacked the selected22-object PPTRF closure proof because it
only imported the earlier broad source-count audit. Audit02 adds the actual
passing PPTRF candidate03 proof and rehashes it. The failed script/log are
preserved and bound. No source assertion, test or requirement was removed.
The fifteen admission records and nine scoped provider-free v27 records bring
the normalized ledger to4461. Typed PPSVX implementation and acceptance are
the next independent work while full PPRFS root checks run.

# PPSVX source admission self-review

This concerns pinned Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, GNU11.4 Linux x86_64 static
LP64 and true ILP64. It is source/integer admission, not numerical acceptance,
independent approval or implemented ASC capability. PPSVX4 remains required.

The full D/Z PPSVX, LAQSP/LAQHP, LANSP/LANHP, LACPY, ISNAN/LAISNAN bodies
were read. Complete fixed-format statement comparisons retain executable
ordering and branch-label targets. Both PPSVX precision pairs have105
statements; LAQSP44, LAQHP48, LANSP108 and LANHP113 also match exactly under
precision-type normalization. LACPY and ISNAN/LAISNAN precision pairs are
checked separately, with explicit SIN/DIN dummy renames only where needed.
Eight compiler-emitted native types match all21 arguments: eighteen explicit
FACT,UPLO,N,NRHS,AP,AFP,EQUED,S,B,LDB,X,LDX,RCOND,FERR,BERR,WORK,
IWORK/RWORK,INFO and three hidden size_t CHARACTER lengths. Pointee const and
the already audited complex representation are the only ABI normalizations.

Each conservative closure has109 objects. The unchanged PPRFS42, PPCON60,
PPEQU6 and selected PPTRF22 closures cover84 unique objects. The remaining25
are PPSVX4, LAQSP/LAQHP4, LANSP/LANHP4, LACPY4, ISNAN/LAISNAN4 and five
LASSQ/module objects. PPSVX always supplies norm I; this branch never calls
LASSQ. Those five objects remain visible and byte-bound in the conservative
closure, without credit for their separate algorithms. ISNAN calls LAISNAN,
whose sole comparison has no loop or foreign call. LACPY Full has fixed
N-by-NRHS loops and reads no padding. No writable static symbols occur.
Remaining imports are cabs,cabsf,memcpy,memset. This is source allocation
evidence; actual ASC/native allocation observations are still required.

FACT N/E sets EQUED N. FACT F accepts only EQUED N/Y; used supplied scales
must be finite positive before native entry, excluding S-related XERBLA.
Root flags, N,NRHS and both actual leading dimensions are validated. FACT E
calls PPEQU and calls LAQSP/LAQHP only when INFEQU0. The latter choose scaling
using SCOND>=0.1 and SMALL<=AMAX<=LARGE, where SMALL=safe_minimum/precision.
Scaling is CJ*S(I)*AP in source order, including CJ*CJ*real(diagonal) for
Hermitian diagonals. It is not reassociated to avoid overflow. These fixed
loops can produce nonfinite arithmetic without a retry or integer conversion.
If EQUED Y, the driver scales B before factorization, and rescales X plus
FERR after refinement. FACT F expects already scaled AP and its matching AFP;
the caller supplies their common provenance. There is no inferred certificate.

FACT N/E copies all p=N*(N+1)/2 coefficients AP to AFP and calls PPTRF.
Positive INFO returns RCOND0 before X/FERR/BERR are written. Otherwise
LANSP/LANHP I computes a norm, PPCON estimates reciprocal condition, LACPY
copies B to X, PPTRS solves, and PPRFS refines. The norm uses ordinary
nonnegative sums and complex Euclidean offdiagonal modulus with real-only
Hermitian diagonals. It can return Inf or NaN; no public finite-ANORM
precondition is presumed inside this driver. Such ANORM does not change the
PPCON LATPS/LACN2/RSCL progress argument: ANORM is tested at entry and used
only in the final reciprocal expression, not in the scaling retry state.
The separately bound arbitrary-raw-factor PPCON scale-product argument
therefore applies. PPRFS has at most five corrections and bounded LACN2
state transitions. All other selected loops have fixed admitted endpoints.
No provider patch, rescaling, fallback or error-handler override is added.

There is no native top-level empty return. In particular, NRHS0 still invokes
equilibration/factorization and condition estimation for N>0. Only N0 is an
ASC-local completion. Integer admission always protects the predivision
N*(N+1), LACN2's3N, p+N+1 terminal cursors, N+1 warning INFO, actual B/X
strides and NRHS+1. For NRHS>0 it also protects correction COUNT6. These
bounds dominate LAQSP/LAQHP's intermediate JC+N and JC+I before subtraction,
LANSP/LANHP's p+1 cursor, PPEQU and PPTRF selected traversals. Real COPY's
seven-entry loop exits no later than p+7, dominated by N*(N+1) for N>=4;
N<=3 uses the cleanup return. Complex COPY ends at p+1. Pointer/backing and
byte products remain separate ASC checks, not replaced by native dimensions.

The pure C++20/no-exceptions helper and literal source-cursor oracle pass
25,331 checks, including all virtual limits3..131, N0..48, four RHS values,
COPY cleanup/unrolled exits, scaling/norm/estimator cursors, correction count,
actual leading bounds and both ABI endpoints46340/3037000499. NRHS0 and
positive RHS are checked separately; N0 admits unused wide native dimensions.
No enormous or forged numerical allocation is used. One test object is fresh;
the exact v22 Core archive is explicitly reused. Strict analysis/format pass.

## Frozen typed implementation direction

Follow the established POSVX mode split as Ppsvx (FACT N),
PpsvxEquilibrated (E), PpsvxFactored (F), with matching formula queries and
four scalar overloads. AP and AFP use ordinary packed descriptors; B and X
are full matrices. All four layouts are independent and the triangle is common.
FACT N keeps AP/B immutable, writes AFP/X, and has no S input. FACT E may
scale AP/B and writes S, AFP/X and actual equilibration. FACT F preserves
AP/AFP/S, may scale B and writes X. Used S is finite positive; unused S is
unread and may have length zero or N. Supplied exact-zero AFP diagonal is a
local singular failure before writes; other raw coefficients get no broad
finiteness or positive-definiteness scan. Complex AFP reads full coefficients.

Queries read metadata only. Identity binds mode, triangle, all layouts,
N/NRHS, original and effective B/X strides, S/FERR/BERR vector metadata,
supplied equilibration and exact provider. All nine live operands/scalar
outputs and protected metadata/scratch are disjoint and provider-accessible.
Preflight and stale-plan failures precede writes/foreign entry; metadata
aliases preserve report, otherwise report resets before validation.

Active native work is real3N WORK plus N INTEGER, or complex2N WORK plus
N real RWORK. Row AP/AFP require p live T entries each and row B/X require
N*NRHS each, in that order. Complex AP in FACT N/E also requires p packing
for column-major input: native COPY otherwise reads ignored imaginary
diagonals. Packing AP reads only real diagonals, AFP reads full coefficients;
output-only AFP/X packing does not read old outputs. Integer scratch starts
actual containing-array lifetimes. No dense conversion or hidden allocation.

To preserve unread old FERR/BERR on factorization failure while detecting
missing bounds, reserve explicit kScratch with NRHS entries of size
2*sizeof(Real), aligned for Real, backed by an actual2*NRHS Real array.
Its first and second halves stage FERR/BERR. This is a declared pair-count
unit, not a native2*NRHS integer expression. Byte/pointer bounds are checked.
Stage both arrays with NaN before native entry; INFO starts signed MIN and
RCOND is a NaN sentinel. Staging is unnecessary for local N0 completion.

Validate full signed INFO and actual EQUED before publishing packed results.
Missing/partial/negative/impossible INFO or changed immutable EQUED is a
provider defect. Direct native operand writes remain; packed publication is
withheld. INFO1..N in FACT N/E is not-positive-definite with diagnostic INFO-1:
partial AFP, actual AP/B scaling and RCOND0 survive, X/FERR/BERR stay unchanged.
Partial complex AFP publication preserves old imaginary diagonal components.
FACT F cannot return a factorization failure. INFO0 or N+1 publishes staged
raw bounds. Negative diagnostics are a provider defect; nonfinite diagnostics
or N+1 are an accuracy warning with usable computed X and raw bounds retained.
No universal finite-X or certified FERR bound is promised. Successful reports
may identify newly computed Cholesky factors; failures and supplied raw factors
never certify successful factor provenance. All report branches require tests.

Local N0 writes RCOND1 and FERR/BERR0, E returns actual equilibration none,
and no input/scratch is read or native dimension narrowed. NRHS0 with N>0
retains native factor/condition work. Full mathematical, real/injected
failure, alias, ignored-read, allocation, package, concurrency, mode and
platform acceptance remains required before complete routine credit.

Both live Google guides were retrieved at2026-09-08 21:28 UTC (September9
local), with exact HTML hashes and URLs in live-guides.json. D003 repository
compatibility exceptions remain unchanged. All2113 routines,130 missing
required XBLAS and the remaining P00-P11 work remain required.
