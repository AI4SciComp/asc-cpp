# Reference GESVX bounded implementation evidence

This slice is frozen and locally verified, not yet integrated or a completed
P04 claim. Actual S/D/C/Z GESVX calls exist in `reference_lu_driver.cc`, with
explicit optional-provider declarations in `providers/lapack_lu_driver.h`.
All four complete pinned source argument sections, executable bodies and
authoritative lapack.h prototypes were read before binding. The exact provider
source, compiler, integer/complex/character ABI and archive identities in
`provider-abi-review.md` apply; no provider patch or source change is inferred.

## Distinct checked modes

Gesvx is FACT=N, with immutable A/B and separate AF/pivots/X outputs.
GesvxEquilibrated is FACT=E, with explicitly mutable A/B, R/C and actual
LapackEquilibration output. GesvxFactored is FACT=F, with immutable already
equilibrated A, corresponding raw AF/pivots/scales, and mutable original B for
the documented scaling. Each has its own formula Query*Workspace route;
query metadata, constness and numerical output mutation are not conflated.

The flat LapackEquilibration enum expresses EQUED=N/R/C/B. Underlying-real
LapackSolveStatistics retains exact RCOND and reciprocal pivot growth copied
from real WORK(1) or complex RWORK(1). RCOND describes the equilibrated matrix;
BERR is computed during scaled-system refinement, while FERR is adjusted for
the original solution when the source rescales X. X solves the original system. Real WORK is
max(1,4*n); complex WORK is 2*n plus max(1,2*n) RWORK. The kInteger byte region
contains n foreign pivots and then n disjoint IWORK entries for real routines;
complex needs only the pivots. No ASC kPivotConversion-width assumption is
made. Every row-major A/AF/B/X has explicit concatenated caller packing, and
output-only AF/X are not read during packing. Actual foreign leading dimensions
are narrowed; original ASC source strides are bound separately in options.

All operands, scalars/statistics, EQUED output and live workspace regions are
pairwise disjoint. Used supplied R/C values must be finite positive; unused
vectors may be length zero or n and are never read. FACT=F requires common raw
provenance, not a fabricated factor certificate. Exact-zero U preflight leaves
all outputs unchanged, with absent INFO. FACT=N/E tags LU pivot family, but a
partial/warning report cannot become a complete successful-factor view.

Raw INFO=1..n preserves the completed singular factorization, applied scaling,
RCOND=0 and source-defined leading-column growth while X/FERR/BERR stay
unchanged. INFO=n+1 is an accuracy warning with completed solution and estimates,
not exact singularity. Postflight nonfinite estimates/statistics receive a
numerical accuracy warning; negative values are provider-invalid. Raw values
and completed X remain available. Negative/impossible INFO, invalid returned
EQUED and invalid produced pivots are provider defects. No INFO is invented.
Empty n produces the exact source-defined diagnostics/work-first-element
without foreign entry; nrhs=0 still factors/estimates nonempty A.

## Actual diagnostic tests and unfinished gates

External `build-lu-driver.sh` compiles live new sources against immutable
dependency snapshot `lu-refinement-frozen-28p9o8nQ`, not the root's concurrent
new LU layout source. Optimized LP64 and true ILP64, all four scalar processes,
passed diagnostic compile/test revisions 01, 02 and 03. Logs use
`logs/p04-lu-driver-{lp64,ilp64}-{compile,test-s,test-d,test-c,test-z}-03.log`.
These are not frozen final evidence.

Those tests cover actual FACT=N/E and FACT=F reuse, all sixteen independent
A/AF/B/X layouts, N/T/C, order 1/3/8, 1/3 RHS and binary scales -60/0/60.
Independent original-system residuals and long-double LU reconstruction of
the scaled matrix check the mathematics. Separate supplied-factor fixtures
force EQUED=N/R/C/B, verify documented B scaling under all transpose modes,
retain A/AF/pivots/R/C bit-for-bit, and use NaNs in unused R/C to exercise
the no-unused-value-read contract. Actual singular INFO, actual n+1 warnings,
empty n and zero-RHS behavior are exercised. Allocation probes wrap first and
repeated driver calls, not just preparation.

Adversarial workspace and injected provider-output checks are now added:
missing/misaligned/overlapping/device workspace, stale plan flags/units/budget,
negative and impossible INFO, bad pivot/EQUED, NaN RCOND/FERR, negative BERR and
infinite growth, with preserved diagnostics and raw results. Diagnostic
revision 04 failed compilation because the test referenced a byte limit on
LapackWorkspace instead of LapackWorkspacePlan; that test typo was corrected
without weakening the assertion. Both-ABI revision 05 and all four scalar
processes passed.
Source-only strict tidy revision 01 found a const-qualification issue in the
private completion helper; it was corrected. Full live tidy revision 01
crashed with a Clang preprocessor stack trace and is not strict proof. A new
immutable diagnostic snapshot `lu-driver-check-Oaa85BKF` is running the strict
check in `logs/p04-lu-driver-tidy-snapshot-01.log`; no success is assumed.

Next exact work: inspect `logs/p04-lu-driver-{lp64,ilp64}-compile-05.log` and
test outputs, finish full descriptor/scale/large-empty-stride and workspace
budget tests, original/AF/B/X padding and growth/condition checks, actual
extreme-input estimate-quality fidelity evidence, and documented partial
scaling outputs. Then freeze real header/source/test hashes and dependencies;
run both-ABI optimized and ASan/UBSan lanes, self-contained header, strict
format/tidy/Doxygen and exact static archive closure before registration.
Installed isolation and full integrated profile tests remain the integrator's
separate required gates. No final identity or mathematical completion is
claimed for GESVX yet. All remaining P04 and later runbook packages remain in
scope; initial driver numerical success does not close the program.

## Preserved tiny-input mathematical limitation

The direct scalar fixture has n=nrhs=1, original A=AF=B=minimum_normal/1024,
one-based pivot 1, and EQUED=N for supplied factors. The independent exact
system has X=1 and reciprocal condition 1. Both pinned LP64 and true ILP64,
all S/D/C/Z and N/T/C, instead return the following in FACT=N/F:

- Raw INFO=2 (n+1), RCOND=0, X=1, reciprocal growth=1.
- Real FERR=Inf; complex FERR=NaN for TRANS=N and Inf for T/C.
- Finite guarded BERR=SAFE1/(2*tiny+SAFE1), where SAFE1=2*minimum_normal.

This is not exact singularity and not successful finite RCOND/FERR mathematics.
The source-derived reason for RCOND=0 is GECON's guarded inverse-norm
estimation: xLATRS scales its solve, and GECON refuses to undo scaling when
SCALE < abs(WORK(IX))*SMLNUM (complex uses CABS1), or SCALE=0. That early exit
retains the initial RCOND=0; the reciprocal of this nonzero tiny scalar is not
representable even though the scale-invariant condition number is exactly one.
GESVX subsequently finishes solve/refinement and sets n+1 because the retained
RCOND is below the machine threshold. This explains the source route, not a
claim that ASC observed every internal Fortran branch dynamically.

GERFS computes BERR directly from the residual and absolute denominator; its
SAFE1/SAFE2 branch adds SAFE1 to numerator and denominator. For the exactly
solved scalar residual=0 and denominator=2*tiny, the displayed guarded BERR is
therefore finite despite being near one. FERR follows a different estimator
path: unscaled xGETRS is applied before the small error weights, exposing an
intermediate inverse overflow (the same independently reproduced limitation in
`lu-refinement-review.md`). The complex N NaN versus T/C Inf distinction is
preserved from actual direct calls, not normalized away.

The exact same input in explicit FACT=E returns EQUED=R, RCOND=1, X=1, INFO=0
and finite FERR, after changing A to 1/1024. This is a distinct user-selected
mode, never an implicit fallback. A second A=AF=B=2*minimum_normal fixture
returns finite diagnostics in every mode; both safeguards and actual scaling
are exercised rather than deleting the extreme fixture.

The original independent mathematical-success probe is retained as
`lu-driver-direct-mathematical-expectation.cc`, with identity in
`logs/p04-driver-direct-mathematical-expectation-sha256.txt`.
Both ABI original runs fail 24 mathematical expectations and retain all 72
printed cases each in `logs/p04-driver-direct-{lp64,ilp64}-run-01.log`.
The separate source-fidelity probe `lu-driver-direct-probe.cc` and
`build-driver-direct-probe.sh` pass both ABIs in compile/run revision 02,
checking X, raw INFO/RCOND/growth, exact mutation mode and guarded BERR with
the distinct nonfinite FERR results. These passing checks do not satisfy the
unmet finite-condition/estimate mathematical gate. Wrapper fidelity and
warning-report tests passed in driver numerical revision 09 and the final
frozen revision 11. They do not turn this warning route into a mathematical
success claim.

Recommended blocker record: retain a required mathematical-success gate for
tiny unscaled FACT=N/F scalar systems across both ABIs/all four scalars, linked
to the existing GERFS tiny FERR limitation and this GECON guarded-estimation
route. No provider patch, changed source identity or owner approval is implied.

## Final frozen local verification

The earlier diagnostic chronology above is preserved, not the current task
status. Final immutable snapshot `lu-driver-frozen-KCABs2Vd` contains these
exact files; `driver-identities.txt` records the same SHA256 values:

- `include/asc/dense/providers/lapack_lu_driver.h`:
  `c9faaeb03b49fa0408d331901cccb7ffdeb9dc3eb06c39217d90dbc9ad922f3d`.
- `src/dense/lapack/reference_lu_driver.cc`:
  `a0daaf90e677dccde805af4e7f7e37564d174c5ce72c87c385f8804d017cb43b`.
- `tests/dense_lapack/lu_driver_test.cc`:
  `ddbefc0f5ac9cda5212bbf446b40606bd191fbae0861151aebefbd6d9d210265`.
- `tests/dense_lapack/lu_driver_test_support.h`:
  `970633379a438b1d61b1c784ebbe185197a616bcab5d1d31f803a23bf08120ba`.
- `tests/dense_lapack/lu_driver_faults.h`:
  `b07c664d6e30141a08b612d63c80270b26e65b41cda5bb3967ca2bfe58836b95`.
- `tests/dense_lapack/lu_driver_faults.cc`:
  `251c31534d1b4103179288bd8cc376fd4076aa067d8d67cd9e2c1137a4ececc3`.

The snapshot derives from `lu-refinement-frozen-28p9o8nQ`, with its frozen
Core/Dense/provider foundation and exact pinned LP64/true ILP64 provider
identities. It does not compile the integrator's concurrent original-LU layout
changes. Both optimized ABI builds and all four scalar executions exited zero
in `logs/p04-lu-driver-{lp64,ilp64}-compile-11.log` and
`logs/p04-lu-driver-{lp64,ilp64}-test-{s,d,c,z}-11.log`.

Both ASan/UBSan builds and all four scalar executions also exited zero in
`logs/p04-lu-driver-{lp64,ilp64}-asan-compile-02.log` and
`logs/p04-lu-driver-{lp64,ilp64}-asan-test-{s,d,c,z}-02.log`. These instrument
the new adapter, provider foundation and tests, not baseline Core/Dense,
Fortran/BLAS or runtime libraries. They are partial-instrumentation evidence,
not full-provider sanitizer proof.

Strict Clang 18 format, self-contained C++20 header, Doxygen HTML/XML,
source tidy and test/fault tidy all exited zero, respectively in
`logs/p04-lu-driver-format-frozen-02.log`,
`logs/p04-lu-driver-header-frozen-02.log`,
`logs/p04-lu-driver-doxygen-frozen-02.log`,
`logs/p04-lu-driver-tidy-source-frozen-02.log` and
`logs/p04-lu-driver-tidy-tests-frozen-02.log`. Earlier strict snapshot checks
found test includes/style issues, then the first final tidy found two missing
direct test includes. Those failed logs remain; direct includes and bounded
test helpers were corrected without disabling diagnostics or assertions.

Exact pinned static archive closure passed for both ABIs in
`logs/p04-lu-driver-{lp64,ilp64}-static-closure-02.json`. Actual external
numerical/runtime leaves are cabs, cabsf, logf, lroundf, memcmp, memcpy and
memset. The initial copied allowlist was too narrow and its failure is retained;
the final list records inspected actual symbols, not suppressed unresolved
calls. XERBLA argument-error routes are preflight-excluded. Static archive
closure and allocation wrappers do not constitute shared-runtime interposition.

Final tests additionally cover actual condition/growth against independent
equations, all matrix/workspace padding, supplied unused zero-length and NaN
scales, malformed operand aliases/strides/placements, exact empty wide ASC
strides and stale-stride plans, provider-invalid outputs and first/repeated/
failing-call allocation probes. No test is skipped or credited by declaration.
For arbitrary nonfinite inputs or unrepresentable solutions the adapter does
not promise finite X; quality checks apply to the documented scalar/error
outputs and preserve raw data.

Remaining gates are atomic registration and manifest identities, integrated
provider-free/provider regressions, installed isolation, and the explicitly
unmet tiny FACT=N/F condition/error mathematics above. The next bounded local
task is the separate GEEQU/GEEQUB empty wide-row-stride correction against its
unchanged prior snapshot. The full P04 and later runbook scope remains open.
