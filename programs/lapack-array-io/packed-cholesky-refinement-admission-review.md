# Ordinary packed positive-definite refinement admission

PPRFS4 remains required and unregistered. Audit
`74314ce9c4fd9c858d0f2362baa1a68c7edc50ab75e1808cd8d2c2ae76f6b517`
and its passing seal bind 14 passing raw records and the seal. Six records
cover the pure count build/test/strict/format; eight cover source closures,
precision pairs and full emitted ABI signatures. The fresh GNU11 Debug
C++20/no-exceptions test passes 25,310 checks with exact v22 Core reused.
Count source has 1107 files (v25's 1105 plus two additions), while source
proof uses the frozen 1105-file v25. Both actual maps are checked; inherited
metadata does not claim a new candidate archive. The ledger has 4,017 records.

The first audit had an incorrect dictionary key produced while adapting its
condition-family predecessor. Audit02 corrects that lookup and binds the
original script and failed execution log. No test assertion or source proof
was changed. This is root self-review, not independent approval. The source
review and frozen implementation direction below are bound by the audit.

# PPRFS source admission self-review

This root self-review concerns the exact pinned Reference-LAPACK 3.12.1
GNU 11.4 Linux x86_64 static LP64 and true ILP64 builds. It is a source
progress and integer admission argument, not numerical acceptance or an
independent review. The complete DPPRFS/ZPPRFS and selected DSPMV/ZHPMV
bodies were read. Complete single/double statement and label comparisons
cover both PPRFS pairs (139 real and 142 complex statements) and both packed
BLAS pairs (138 real and 141 complex statements). PPRFS single precision
explicitly converts NZ to REAL at three multiplications where double
precision converts implicitly. The reviewed normalization retains each
complete product/addition expression. Both BLAS pairs normalize exactly.

The four emitted function types per ABI retain all 14 explicit arguments
and the hidden CHARACTER size_t length: UPLO,N,NRHS,AP,AFP,B,LDB,X,LDX,
FERR,BERR,WORK,IWORK/RWORK,INFO,length. Each mechanical closure has 42 objects.
Thirty objects are byte-identical to the audited 81-object packed triangular
expert closure, including LACN2, machine helpers, COPY/AXPY and TPSV. All ten
objects of the audited PPTRS closure occur unchanged. Their union covers 34
objects; the remaining eight are the four PPRFS entries and four SPMV/HPMV
entries reviewed here. The audit rehashes the exact source/object bytes and
both earlier admission proofs. Remaining imports are cabs,cabsf,memcpy,memset;
there are no writable static symbols. XERBLA is conditioned on validated root
and transitive arguments, not overridden process-wide.

PPRFS validates UPLO,N,NRHS,LDB,LDX. With N0 or NRHS0, it writes each FERR/BERR
zero and returns. Active NZ=N+1, EPS and SAFMIN define SAFE1=NZ*SAFMIN and
SAFE2=SAFE1/EPS. For each RHS, COUNT starts 1 and LSTRES starts 3. Residual
B-A*X uses COPY followed by SPMV/HPMV with both increments 1, alpha -1 and
beta 1. The weighted denominator is abs(A)*abs(X)+abs(B); complex abs is
abs(real)+abs(imag). Original Hermitian AP diagonal imaginary components are
ignored by both HPMV and the weighted residual loop. AFP is instead a raw
triangular Cholesky factor: its complete diagonal coefficient is read by
PPTRS/TPSV. No factor validation or implicit factor reconstruction is native.

Correction continues only if BERR>EPS, 2*BERR<=LSTRES and COUNT<=ITMAX5.
A PPTRS correction plus AXPY is followed by COUNT++, so at most five
corrections occur and terminal COUNT is 6, independently of finite/NaN/Inf
input arithmetic. Every RHS starts a new correction counter. Weighted error
vectors then feed LACN2 with KASE0 and caller-local ISAVE3. Its already-bound
state machine and ITMAX5 establish finite reverse-communication progress;
real NINT receives stored signs +1/-1. KASE1 solves then multiplies W;
KASE2 multiplies W then solves. PPTRS/TPSV and the selected BLAS paths have
fixed integer loops. No LATPS/RSCL scaling retry is present in this closure.
Arbitrary nonfinite or zero factor values can produce invalid arithmetic but
cannot add an unbounded retry to these paths. FERR is divided by max(abs(X))
only when nonzero. This reasoning makes no finite/accurate FERR or X claim.

Integer admission protects 3*N, the N*(N+1) product before division in both
PPTRS solve orientations, NRHS+1, actual foreign B/X strides, and terminal
COUNT6. These bounds cover N+1/NZ, WORK(2*N+1) through 3*N, original packed
upper KK+K before subtraction, lower KK+1, packed p+1 terminals, and the
selected INCX=INCY=1 SPMV/HPMV cursors. The inherited packed admission also
protects p+N+1, which bounds these residual cursors. For N1, 3*N covers the
packed terminal. ASC descriptor byte spans and pointer-difference capacity
are checked separately; native dimensions cannot replace backing validation.
Pure native order endpoints are 46340 LP64 and 3037000499 ILP64. The literal
oracle independently accumulates the predivision product, work segments,
correction count, packed solve/residual cursors and RHS terminal under 129
virtual limits, plus actual endpoints. All 25,310 checks pass. It constructs
no giant or forged numerical descriptor. One test object is freshly compiled
with GNU11 C++20/no exceptions and the exact earlier v22 Core is reused.

## Frozen typed implementation direction

QueryPprfsWorkspace/Pprfs will accept immutable ordinary packed AP and AFP,
immutable full B, mutable full X, and contiguous underlying-real NRHS outputs
FERR/BERR. Four layouts are independent; AP/AFP share N and the selected
triangle, while caller guarantees common factor/original/RHS provenance.
All six operands, caller scratch and protected metadata must be disjoint.
Queries inspect metadata only. Plan identity binds routine/scalar/provider,
N,NRHS, every layout, both original/effective B/X strides and vector sizes
and increments. Structural or stale-plan failure precedes numeric mutation
and foreign entry; metadata aliases preserve the report, otherwise it resets.

Nonempty execution uses real 3N WORK plus N INTEGER, or complex 2N WORK plus
N underlying-real RWORK. Active row AP/AFP each add p live T objects of
explicit packing; row B/X each add N*NRHS live T objects. AP packing reads
only the real diagonal, AFP packing reads complete coefficients. No dense
conversion, implicit allocation, rescaling, transfer or fallback occurs.
Empty N or NRHS completes locally with FERR/BERR zero, absent INFO and no
AP/AFP/B/X/scratch read. Local unused native dimensions are not narrowed;
invalid flags and all ASC metadata contracts still apply.

Raw AFP is used as supplied, as in the packed reusable solve contract. There
is no new zero-diagonal or finiteness scan and no factor validity certificate.
Immediately before native execution, INFO is signed MIN and FERR/BERR are
NaN sentinels. Any nonzero, missing or partially written INFO is a provider
defect with full signed raw INFO; direct native writes survive and row X
publication is withheld. INFO0 with negative estimates is a provider defect;
nonfinite estimates are an accuracy warning with completed X and raw bounds
retained. INFO0 finite nonnegative estimates completes. FERR remains an
estimate, not a universal certified bound; no blanket finite-X promise is
introduced. These report rules require real/injected evidence before import.

Source and count admission confers no typed-call or numerical credit. Actual
refinement/residual and estimate fixtures, scalar/extreme/wrong-triangle cases,
all preflight/INFO/write/read/alias/allocation observations, installed clients,
six modes and full platform acceptance remain required. Foreign archives are
unsanitized. The existing mathematical failures, all 2113 required routines
and 130 missing required XBLAS entries remain visible and required.
