# Triangular band estimators: source admission self-review

This review covers S/D/C/Z TBCON and TBRFS at pinned Reference-LAPACK
commit 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca, GNU 11 Linux x86_64 static
LP64 and true ILP64. It is root self-review, not independent approval or
complete mathematical, memory-mode, platform or program acceptance.

The two actual archive closures contain 81 unique members each. Exactly 57
complete object/source/import records match the prior packed estimator review;
24 band-specific objects are the four precisions of TBCON, TBRFS, LANTB,
LATBS, TBMV and TBSV. The comparison SHA256 is
5614344f17fc24b6d4c82b7b6af876e4e282794bde4654f2e2a863c5dfc2ff46.
No closure object has writable static data. After excluding only the checked
XERBLA diagnostic edge, external imports are cabs, cabsf, memcpy and memset.
This source analysis does not replace runtime allocation observations.
The prior full precision-pair statement comparisons and 24 native compiler
emissions remain applicable to unchanged pinned sources. New permanent
signature tests must check the eight complete TBCON/TBRFS function types.

## Storage and integer admission

The neutral band descriptor checks complete N*LD backing and bytes, KD+1,
alignment and placement, including KD>=N. Row-major conversion uses
N*(KD+1) caller scalar objects and copies only selected entries, excluding
implicit unit diagonals. A stays banded. B and X each require N*NRHS caller
objects when row-major. Column-major operands are direct. Original strides
and actual foreign dimensions must be separately represented in plan identity.

TBCON requires 3*N even for complex LACN2, which also bounds ascending N+1
loop terminals and workspace offsets. For upper one-norm LANTB, K+2-J and
the nonunit inner terminal need K+2. Slow upper LATBS forms KD+I-JLEN from
left to right, requiring KD+min(KD,N-1). Lower TBSV and infinity-norm LANTB
form J+KD before MIN; N+KD is protected. KD+1 and actual LDAB are narrowed
only for active calls. N=0 completes locally with RCOND=1 and absent INFO.

TBRFS additionally computes NZ=KD+2 and the NRHS+1 terminal. Its upper
weighted residual indexes AB(KD+1+I-K,K), forming KD+1+I first. Nonunit
reaches I=N; unit reaches N-1. Lower indexing again needs N+KD. Actual
LDB/LDX and all addresses are checked separately. N=0 or NRHS=0 completes
locally with exact zero FERR/BERR, without narrowing unused foreign arguments.

The immutable `p05-triangular-band-expert-counts-01` audit
559f25614998fb833c83c4a8953424c01fe6d8f1a99b642486000eb5dbb19553
binds six successful commands and its successful completion seal. The pure
oracle passed 71,282 checks: literal virtual signed-machine expressions/DO
controls, actual LP64/ILP64 metadata boundaries and invalid-input cases.
One GNU Debug C++20/no-exceptions test object was fresh; exact root v19 Core
was reused. Strict/format checks pass. These tests create no huge fictitious
spans and make no provider calls. Seven records are normalized in the ledger.

## Progress and diagnostics

LANTB uses complex modulus for matrix norms. TBRFS uses component magnitudes
for complex weighted residuals. TBRFS preserves X, forms op(A)*X-B and
estimates weighted inverse error; it does not refine X. Its residual N/T/C
branches preserve complex transpose/conjugate distinctions. The inverse
estimator solves use TBSV, so TBRFS has no RSCL rescaling loop.

LACN2 owns three caller-stack state integers and has an ITMAX=5 bounded
estimation cycle. Selected BLAS increments are exactly one. Fixed band
traversals have protected integer bounds. The common LADIV and machine
constant dependencies retain the earlier review by exact object/source match.

TBCON computes LANTB, then calls LATBS with NORMIN=N followed by Y. Its
zero/underflow SCALE guard alone would not exclude a non-progressing infinite
RSCL denominator. The D/Z LATBS branches were therefore reviewed explicitly.
Unlike LATRS, LATBS has no overflowing-column-norm recovery scan.

For the actual IEEE binary32/binary64 builds, LATBS sets SMLNUM to safe
minimum divided by precision and BIGNUM=1/SMLNUM. Nonnegative column norms
produce TSCAL=1, or 1/(SMLNUM*TMAX) for real and HALF/(SMLNUM*TMAX) for complex.
For finite TMAX, SMLNUM*TMAX cannot overflow; the reciprocal of positive
TSCAL is below 2^26/2^55, including the complex HALF factor. SCALE starts at
one. Its initial reduction and all later REC or HALF multipliers are
nonnegative and at most one, or NaN. Reciprocal XJ is used only under XJ>1;
small-divisor ratios execute only when their denominator exceeds the
nonnegative numerator. Further division by CNORM>1 reduces them. Transpose
compensation is capped by MIN(1,...), and is applied only under REC<1.
Zero or NaN divisor branches assign SCALE=0. Consequently final SCALE/TSCAL
is finite or NaN for positive finite TSCAL.

If TMAX=Inf, TSCAL=0 and scaled norms are zero or NaN. The fast-solve test
cannot succeed. Each selected slow diagonal TJJS is zero or NaN, including
an implicit unit diagonal. The transpose norm comparison cannot change
USCAL=0 by a positive diagonal compensation; zero/NaN divisor branches set
SCALE=0. Final SCALE/TSCAL is NaN. NaN TSCAL likewise returns NaN. Reused
column norms remain nonnegative or NaN and fall into the same cases.
TBCON therefore passes RSCL a positive finite or NaN denominator; its guard
excludes zero. Finite positive ratios make exponent progress, and NaN
comparisons select RSCL's final branch immediately. This establishes bounded
source progress, not accuracy or finite-output guarantees.

Valid root flags, dimensions/strides and transitive unit increments exclude
XERBLA without overriding the process handler. All eight root INFO locals
must start at native MIN; no positive INFO contract is invented. Negative,
unwritten, partial or impossible INFO remains a provider defect. Negative
estimates are defects; nonfinite estimates remain visible numerical warnings.
Aliasing and plan rejection must precede numeric/workspace mutation.

## Reproduced complex LATBS equation defect

CLATBS/ZLATBS lower slow transpose and conjugate-transpose branches guard
their dot product with JLEN>1, omitting a single off-diagonal term. D/S use
JLEN>0. For t equal to the smallest positive normal scalar, choose
A=t*[[1,0],[1+i,1]], X=[1,1] and B=op(A)*X. The complex native calls return
INFO=0, SCALE=1 and X=[2+i,1] for transpose, or [2-i,1] for conjugate
transpose. The equation residual after symbolic division by t is sqrt(2).
No underflowing product is used by the independent residual oracle.

`p05-triangular-band-source-01/latbs-single-term-03` executes 16 cases per
scalar and ABI: scales 1/t, KD=1/3, T/C and NORMIN=N/Y. Real S/D and complex
unit-scale controls pass; eight tiny cases fail in each complex precision.
Both ABI CTest runs return 8: two tests pass, two fail, zero skips. These
remain ordinary required mathematical failures. The initial candidate01
configure failure (missing crtbeginS.o search path) is preserved; candidate02
adds explicit local compiler/runtime search flags without changing fixtures.
No provider patch, fallback, hidden rescaling or tolerance relaxation occurs.
TBCON's reachable conjugate LATBS dependency retains this known defect;
direct LATBS public capability and complete TBCON acceptance remain open.

Checked TBCON/TBRFS adapter and public headers are being authored in the
preserved external expert draft. The eight mapping rows remain required and
not_started until a tested implementation is integrated atomically.

The completed direct-failure audit is
7b4b39c078f15d2591b7d1e2ec10ad94ba8088ad45afa974cd2b3e2361da64e9.
Its successful seal binds 17 original command records and eight fresh final
Fortran driver objects, plus current provider/source/runtime identities and
actual link maps. Eighteen records are normalized, preserving the raw failing
CTest exits. Candidate02's audit incorrectly assumed a shared Fortran runtime;
its failed audit is preserved. Candidate03 explicitly links the exact shared
runtime files and verifies actual resolution. No fixture or threshold changed.
