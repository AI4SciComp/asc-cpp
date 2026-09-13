# Packed triangular estimators: source admission self-review

This is root self-review for S/D/C/Z TPCON/TPRFS at the pinned Reference-LAPACK
3.12.1 commit6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca. It covers the actual
GNU11 Linux x86_64 static LP64/true ILP64 archives. It is not independent
approval, a numerical certificate or full routine/platform/program acceptance.

## Actual closure and ABI

The conservative expert closure contains81 objects per ABI, no writable
static symbols and only cabs,cabsf,memcpy,memset imports after conditioning
the XERBLA diagnostic edge. common-comparison.json rehashes every actual
archive member and source candidate, checking unique archive member names.
Its SHA256 is b0de15e6c35df36b70033973093d2cdd766babb716ff1cec4f9020d0fd01b3df.
Exactly57 object/source rows are identical to the earlier full triangular
expert review;24 new objects are the four precisions of TPCON,TPRFS,LANTP,
LATPS,TPMV,TPSV. The older26 removed objects include LATRS and DLANGE: their
norm-overflow recovery path must not be attributed to LATPS.

The existing32 actual GNU compiler emissions cover all16 inventoried packed
routines and both native integer ABIs. Eight complete TPCON/TPRFS function
types also pass the candidate's permanent signature test. Each has three
trailing size_t character lengths, exact signed native int/long, scalar and
underlying-real outputs, pointer order/depth and void return. No A leading
dimension exists. Private installed ABI declarations are not public ASC API.

The14-pair fixed-format comparison covers S/D and C/Z roots,LACN2,LANTP,LATPS
with complete executable statement ordering and branch labels retained.
The additional6-pair BLAS comparison covers TPMV,TPSV,SCAL. Complete D/Z
root,LANTP selected norm branches,LATPS,TPMV,TPSV bodies were reviewed.
The57 common byte-identical dependencies retain the prior bounded source
review, including LACN2,LADIV,RSCL,unit-stride BLAS and machine constants.

## Integer and storage admission

The public checked packed descriptor proves the ASC product n*(n+1),p=n*(n+1)/2,
element bytes,alignment and full reachable backing range. There is no n-by-n
packing. Row-packed A uses p caller scalar objects, excluding stored unit
diagonals; B and X independently use n*nrhs caller objects when row-major.
Each packed source index follows a selected packed traversal. Work roles are
real3*n plus native IWORKn, or complex2*n plus real RWORKn. Packing, typed
workspace capacities and their summed bytes are checked before any write.

The private count helper protects 3*n and the pre-division product n*(n+1).
LATPS/TPSV can traverse both orientations through the norm estimator. The
ascending transpose LATPS cursor ends at p+n+1; the descending one at -n.
For n>=2 the protected product covers p+n+1; for n=1 the3*n bound covers it.
The same bounds cover lower KC+N intermediates before subtraction, n+1 DO
terminals and all3n vector endpoints. Active TPRFS requires nrhs<MAX and the
actual native LDB/LDX representable and at least n. The independent oracle
walks these integer expressions under virtual maxima3..600 and separately
checks LP64/ILP64 boundaries;1446 checks pass. This is a count proof, not
a simulation of every foreign arithmetic expression using tiny integers.

TPCON n=0 completes locally with RCOND1 and absent INFO. TPRFS n=0 or nrhs=0
completes locally with all exact nrhs FERR/BERR outputs zero, without narrowing
unused native dimensions. Query keys retain original ASC B/X strides and
all packed/RHS layouts, flags, provider and scalar identities separately from
the actual foreign dimensions. Valid backing spans are always required.

LANTP norm1/infinity and LATPS/TPSV do not read stored unit diagonals. LANTP
uses complex ABS; TPRFS weighted residuals use componentwise CABS1. TPRFS
computes op(A)*X-B and the safeguarded componentwise denominator, then
estimates the weighted inverse. It never refines X. The selected N/T/C
residual paths retain complex transpose versus conjugate-transpose semantics.

## Progress, including packed norm overflow

LACN2 owns ISAVE(3) on the caller stack; KASE starts0, states stay1..5 and the
only repeated estimation cycle is bounded by ITMAX5. Real sign conversion
uses exactly+1/-1. IAMAX/MAX1 returns1..n for n>0 and increment1, including
NaN comparisons. The fixed triangular traversals and unit-stride BLAS loops
have protected integer terminals; LADIV has bounded branches. TPRFS uses
TPSV, not the scale-retry RSCL path, so floating propagation adds no loop.

TPCON does call RSCL with the SCALE returned by LATPS after its zero/underflow
guard. RSCL would not progress with an infinite denominator; its actual
admission therefore requires reviewing SCALE, not merely citing the guard.
LATPS differs from LATRS: overflowing column sums can produce TMAX=Inf,
TSCAL=0 and NaN scaled norms. It has no DLANGE overflow-recovery scan.

For the actual IEEE binary32/binary64 builds, SMLNUM is safe-minimum/precision
and BIGNUM=1/SMLNUM. If TMAX is finite, SMLNUM*TMAX cannot overflow; it is
below2^25 for binary32 and2^54 for binary64. TSCAL is1 or a positive quotient
whose reciprocal stays below2^26/binary32 or2^55/binary64, allowing the extra
complex HALF factor. SCALE starts1, initial X rescaling only decreases it,
and every later multiplier is nonnegative and at most1 (or NaN):

- 1/XJ occurs only with XJ>1, either directly or because TJJ>SMLNUM and
  XJ>TJJ*BIGNUM. The small-divisor ratio (TJJ*BIGNUM)/XJ executes only when
  the denominator exceeds its nonnegative numerator. Dividing further by
  CNORM>1 cannot increase it.
- Transpose/conjugate dot-product scaling starts1/MAX(XMAX,1), then HALF;
  any diagonal compensation is capped by MIN(1,...). SCALE multiplication
  there occurs only when REC<1. Zero/NaN divisor branches assign SCALE0.
- All other SCALE updates are a HALF multiplier or assignment of zero.

Consequently final SCALE/TSCAL is finite or NaN for positive finite TSCAL.
If TMAX=Inf, TSCAL=0 and every scaled norm is zero or NaN. The fast-solve
test cannot succeed. In each selected diagonal step TJJS is zero or NaN,
including an implicit unit diagonal. Transpose scaling cannot change USCAL0
through a positive norm comparison; the zero/NaN divisor branch sets SCALE0.
The final division is0/0, hence NaN, not Inf. If TSCAL itself is NaN, the final
division is NaN. Returning/rescaling CNORM may retain NaN/Inf but the next
call falls into the same exhaustive TSCAL cases. No negative norm is created.

Thus the RSCL denominator passed by this TPCON path is finite positive or
NaN; zero is excluded by TPCON's guard. For finite positive denominators,
RSCL decreases the finite denominator or numerator by a machine exponent
range until the final-ratio branch. NaN comparisons select that branch
immediately. This establishes source progress on the pinned arithmetic
build without claiming finite or accurate estimates. The candidate's
ordinary finite-extreme mathematical failures remain required failures.

## Diagnostics, aliasing and measured limits

Valid native root flags, dimensions, LDB/LDX, and transitive UPLO/TRANS/DIAG,
NORMIN and unit increments exclude XERBLA; no handler is replaced. There is
no source allocation or variable local array in the admitted roots. The
actual closure's remaining libm/libc imports are recorded explicitly; the
absence of direct allocator symbols alone is not runtime allocation proof.

All8 INFO locals start native MIN. Negative, positive, unwritten and partial
writes are provider defects; no positive singular-pivot contract is invented.
Negative RCOND/FERR/BERR is a defect. NaN/Inf remains a visible accuracy
warning and partial output. A,B,X are immutable and have no publication.
Metadata aliases reject before report reset; other structural failure resets
the report but does not touch numeric/workspace storage or enter the provider.

Initial candidate01 passes its public,fault,count,signature suites in all6
configurations. Each retains8 ordinary required mathematical failures,
reproduced by direct pinned calls: tiny nonzero scalar RCOND0 versus1, and
finite tiny/maximum scalar weighted bounds with infinite FERR. BERR retains
the explicit safeguarded denominator. No source patch, hidden rescaling,
fallback or tolerance relaxation is introduced. These cannot close complete
mathematical acceptance. ASC-only sanitizer evidence leaves foreign archives
unsanitized. Stronger load observation, full preflight/mode crossproducts,
platform/concurrency and complete P00-P11 gates remain required.

## Expanded candidate04 executed evidence

Audit `0c5e0276bfa885467fcee9a960d4391abff0b0f1e05746543f994a4526cfe8f6`
binds119 original records and54 fresh final adapter/test objects. Six final
configurations each execute12 tests:4 pass,8 required mathematical failures,
zero skips. Prior62-primary-TU production libraries/inputs and both pinned
providers are explicitly rehashed. No root or installed-package credit yet.

Each final public consumer passes6656 workflows/111104 assertions; failure
checks pass10752 profiles/71680 assertions; pure counts pass1446 checks;
eight full native signatures pass. Four final changed-test strict checks
pass. The14 initial strict and8 public-header isolation checks apply to
unchanged production/header/public/count/signature bytes. Normal and
no-exceptions public isolation uses no private/foreign include directories.

TPCON's212 assertions per scalar retain17 failures: the original tiny scalar
condition failure plus16 maximum-scale matrix cases. The48 added matrix
fixtures use order2/3, both packed layouts and triangles, both norms, and
scales1,max/4,max. Their analytically inverted sign-alternating triangular
matrix has nonnegative inverse and exact RCOND1/(2*n). The32 unit/max-quarter
controls pass; max-scale order2 returns0 and order3 NaN. Direct native calls
match, with INFO0. TPRFS retains25 assertions and2 finite scalar FERR failures
per scalar. These remain ordinary mathematical failures, not waived tests.

Candidate02 initially used an all-positive triangular matrix with a
sign-changing inverse and wrongly demanded epsilon equality between an
estimate and the exact condition number. That test-oracle defect is preserved
in its raw records and corrected by a fixture with the same norm/overflow
magnitude and unchanged tolerance. It is not evidence of a provider defect
at max/4. All original scalar and public assertions remain. Candidate02
strict rejected a missing direct LAPACK types include; candidate03 strict
rejected a missing direct Core types include in the new math fixture. Both
are fixed by includes in04; no check is disabled.

The expanded workspace tests add scalar/auxiliary alignment, region overlap,
rejected device placement tags on live host storage, short native integer or
real workspace, plan/workspace metadata aliases, byte-limit identity and
separately constructed stale provider/scalar keys. They check exact error
codes and unchanged output/workspace bytes; metadata aliases preserve INFO.
Actual invalid option/shape and complete operand alias mode crossproducts,
stronger read observation, concurrency and platform gates remain open.

All8 estimator routes remain required and unregistered. Next import the11
unchanged candidate04 C++ files, register partial v18 coverage and ordinary
math gates, and run fresh root integration. The program remains302 Reference
partial/1811 not_started of2113 required,20 native implemented_unverified and
zero fully verified. Required130 missing XBLAS routines remain visible.
