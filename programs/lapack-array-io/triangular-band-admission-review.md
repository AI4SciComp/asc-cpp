# Pinned triangular-band solve source admission

The four TBTRS roots and four TBSV implementations come from the pinned
3.12.1 tree. `precision-review-01.json` compares complete S/D and C/Z TBTRS,
TBCON, TBRFS, LACN2, LANTB and LATBS statement streams; `precision-blas-01.json`
does the same for TBMV/TBSV/SCAL. Source hashes and normalized statement hashes
are explicit. These comparisons do not establish the remaining estimator
source/progress or mathematical requirements.

The selected TBTRS/TBSV path uses exactly INCX=1. The real and complex TBTRS
bodies validate UPLO/TRANS/DIAG, N/KD/NRHS, LDAB>=KD+1 and LDB>=max(1,N), then
return for N=0. A nonunit diagonal scan runs before the RHS loop even for
NRHS=0, stopping at the first exactly zero diagonal without changing B.
Its normal terminal is INFO=N+1; normal completion subsequently stores INFO=0.
Each of the NRHS solves calls TBSV on B(1,J), INCX=1. Unit diagonal is never
read, and transpose versus complex conjugate transpose remain distinct.

The private pure integer helper admits the actual selected control flow:
KD+1 and the NRHS+1 terminal must fit; nonunit scans require N+1. Active
lower TBSV computes J+K before MIN(N,J+K), so N+KD must fit. It also evaluates
J+1 even for empty inner loops. Upper transpose traverses J=1..N through N+1.
Upper no-transpose with unit diagonal traverses decreasing controls through
zero and does not need N+1. With NRHS=0, no TBSV branch executes; a unit call
has neither numeric operand reads nor a diagonal-loop terminal. Actual native
LDAB/LDB are checked separately from the original row-major ASC strides.

All selected loops have data-independent finite integer bounds. Floating
NaN/Inf or tiny divisors change arithmetic outcomes or finite conditional
branches, not loop progress. There is no estimator rescaling loop on this
solve path. Array addressing is covered separately by the descriptor's full
checked n*ld byte range and RHS reachable span. The source counter oracle
literally walks selected DO controls at virtual signed limits 3,7,31 and
checks both actual integer boundaries, without fabricating large live spans.
Its executed 186643 checks and strict gate pass in counts candidate01.

Both actual static solve closures contain ten unique members: four TBTRS,
four TBSV, LSAME and XERBLA. No member has writable static data. Root and
transitive checked flags, dimensions, LDAB and INCX exclude the XERBLA branch;
its diagnostic imports are explicitly conditioned out, not replaced. The
remaining closure has no external imports. This source allocation analysis
is distinct from the still-required runtime allocation and failure probes.

The neutral descriptor candidate03 accepts KD>=N, validates full physical
backing/alignment/placement, and retains existing BLAS behavior. Portable
storage tests and separate Linux protected-memory tests pass on GNU11 with
exceptions disabled. The latter constructs live scalar objects in a real
mapping before PROT_NONE, exercising metadata-only construction for all four
scalar types without forged capacity. It records zero observed C++ allocations
on that scoped path. Earlier build/strict failures remain preserved: a test
signedness mismatch, oversized test function and explicitly spelled long
were corrected without removing assertions. Five final strict/header/format
checks pass. Other platforms and broader memory modes remain required.

TBTRS adapter candidate01 is prepared but unexecuted at this checkpoint;
TBCON/TBRFS adapter work is not started. All twelve routine rows remain required
and not_started; no capability credit follows from signatures or source audit.

## Bound evidence checkpoint

Admission audit `f54ade6ae3d2671a2fa1d4dd146757328697f6ccbd80179fd2ae80468978918b` binds 27 original records and eight freshly compiled pure-test/support
objects, with the existing Core library explicitly reused. The completion
record binds all successful links/binaries and Core context. This adds no
routine capability; all twelve triangular-band rows remain required.
