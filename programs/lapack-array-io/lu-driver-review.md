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

## Master continuation: normalized GESVX modes

The 2026-09-12 continuation starts after the GERFS delivery at
`663c7a4c4b1388c3a2c78093e61f2e6e2f94f9f4`. The older external handoff pointers
above are historical. GESVX already has24public query/execute declarations,
production integration, ordinary/INFO/pivot tests and actual relocated public
consumer evidence. This continuation retains that implementation and its
requirements, adding missing maintained observation, concurrency and genuine
mathematical gates. No provider, algorithm, API, tolerance or floating-point
setting changes.

`master-continuation-20260910-01/gesvx-continuation-contract-01/review.json`
binds the four pinned source definitions, the complete executable bodies,
the common parameter contract and every scalar-specific declaration/documentation/
initialization difference, and23selected existing ASC inputs. S/D use float/
double matrices; C/Z use complex float/double. R/C, estimates and statistics
use the underlying real type. General overlaid LU and one-based raw pivots
are not Hermitian factors; no UPLO applies. Complex transpose and conjugate
transpose remain distinct. Six explicit FACT/EQUED routes, three transposes,
16independent A/AF/B/X layouts and four N/NRHS shapes give1,152cases per scalar.
Four routines remain four catalogue entries, callable-unverified.

### Mode transformations and publication

Write the equilibrated matrix as Ae=Dr*A0*Dc, with each unused scaling diagonal
set to identity. For TRANS=N, the driver solves Ae*Y=Dr*B0 and publishes
X=Dc*Y. For T/C, it solves op(Ae)*Y=Dc*B0 and publishes X=Dr*Y; the positive
real scales are unchanged by conjugation. FACT=N uses both identities. FACT=E
computes optional scales explicitly. FACT=F receives Ae, its corresponding
raw LU/pivots and selected scales, together with the original B0. RCOND
estimates Ae; BERR belongs to the refinement system; FERR is adjusted for the
selected solution scaling. These distinct systems must not be compared using
an unadjusted original-system condition oracle.

| Route | Numeric input and output contract |
| --- | --- |
| FACT=N | A/B immutable. AF, caller pivots, X, FERR/BERR and statistics are outputs whose old values are unread before native entry. |
| FACT=E | A/B explicitly mutable. AF, caller pivots, R/C, actual EQUED, X, estimates and statistics are outputs. Query binds no old output EQUED value. R/C may be partial when internal equilibration fails; returned EQUED selects usable scales. |
| FACT=F, EQUED=N/R/C/B | Ae/AF/pivots/selected scales immutable and from one origin. Query legitimately reads raw pivots and selected finite positive scales. Unused scale vectors may have zero or N entries and are unread. After workspace validation, execution checks exact-zero U before mutation. Original B may be scaled. Old X/estimates/statistics are not inputs. |

Real scratch is max(1,4N) scalars and2Nforeign integers; complex scratch is2N
scalars, max(1,2N) reals andNforeign integers. Converted pivots precede real
IWORK. All row-major matrices pack consecutively into caller scratch; AF is
read only in FACT=F and X is never packed as input. All eleven operand,
statistics, EQUED and live workspace spans are disjoint. N and native leading
dimensions are checked against the actual ABI; source row strides remain
ASC-sized metadata. N=0 still needs the growth scratch element, writes it1,
writes statistics1/1 and estimates0, and sets EQUED=N for FACT=E without native
INFO. N>0, NRHS=0 still enters factorization/condition estimation.

INFO1..N preserves completed singular factorization, scaling, RCOND0 and
growth while leaving X/FERR/BERR unchanged. INFO=N+1 retains a computed
solution and diagnostics with an accuracy warning. Invalid, unwritten,
partial-width or excessive INFO, invalid returned EQUED or produced pivots
remain provider defects. Post-return estimate/statistic checks preserve raw
values and completed X; negative diagnostics take precedence over nonfinite
ones. No raw INFO is invented and no universal finite-X promise is added.

### Numerical disposition

Both causes use the unchanged scalar fixture N=NRHS=1,
A=AF=B=minnormal/1024 and pivot1 in FACT=N/F, N/T/C and all-column/all-row
layouts. The original direct probe requires exact X=1, RCOND=1, INFO=0 and
finite FERR/BERR. The maintained gate preserves that oracle exactly.

| Unique cause | Expected and observed property | Evidence and required decision |
| --- | --- | --- |
| GECON guarded inverse-scale restoration | The scalar system has condition1. RCOND remains0 when the inverse estimate cannot be restored without overflow, although X=1 and growth=1. GESVX consequently returns INFO2, an accuracy warning rather than exact singularity. | The source guard and direct both-ABI reproduction already recorded above and in `lu-condition-review.md` apply unchanged. Preserve the scalar condition gate; a separately authorized provider or scale-invariant estimation strategy is needed. |
| GERFS unscaled solve before small error weights | Required FERR is finite. Real N/T/C giveInf; complex N givesNaN and T/C giveInf. X=1 and the guarded BERR equation pass. | The direct probe and `lu-refinement-review.md` reproduce both actual ABIs. Preserve the finite-estimate gate; a separately authorized provider or scaled weighted-inverse strategy is needed. |

The original direct processes failed24composite expectations in72printed cases
per ABI. The maintained tests add the already required two layouts and retain
individual report/property assertions: four failed processes contain240failed
assertions, four failing scalar fixtures have48FACT/transpose/layout executions,
and there are two inherited causes. This is not four new independent bugs.
Explicit FACT=E and the2minnormal control pass; they are not fallbacks for N/F.
The two decisions are in the existing owner packet. PPSVX-specific numerical
authorization does not extend to GESVX.

### Maintained tests and actual evidence

Paths below are relative to the existing external
`master-continuation-20260910-01` evidence root. Raw logs remain external.

- `gesvx-modes-audit-01/audit.json`: both-ABI Release runs03 pass four
  concurrency processes and retain four genuine mathematical failures,
  240assertions each. Four workers use independent buffers, context, workspace
  and reports, reusing immutable matching plans for32calls per mode/layout/
  transpose. Diagonal real/complex fixtures have independent known solutions
  and actual supplied N/R/C/B scales. Singular, invalid pivot or missing
  workspace, stale plan and normal outcomes check diagnostic isolation.
  Workers do not use global fault injection or allocation-audit state.
- `gesvx-release-audit-01/audit.json`: both-ABI observer02 passes24,028checks
  in216children. There are148intentional forbidden reads,24restored writes,
  24legitimate supplied pivot/scale/U and A/B packing reads, and23,832phase
  checks. Query/stale/exact-one-byte-short paths,2,304native entries including
  zero RHS, and2,304local N=0 cases are observed. Signature, three trailing
  character lengths, FACT/TRANS/EQUED, dimensions, actual packed/direct buffer
  addresses and foreign pivot storage are checked before page access is restored.
- The observer uses initialized, aligned containing arrays and terminating
  child faults, with no fault-handler recovery or undefined pointer/object
  proof. Selected supplied scales/pivots stay readable during admission; old
  outputs and unused scales are protected until the validated boundary.
  Local estimates/statistics are writable, and their write-only completion is
  separately source-reviewed. Post-return quality scans are allowed. This
  observes ASC preparation; it does not claim foreign-code instrumentation.
- `gesvx-existing-evidence-reuse-01/audit.json`:72ordinary/INFO/pivot processes
  reused across static Debug/Release/ASC-only sanitizer and both ABIs through
  48selected compilation dependencies. Only four already reviewed GESV RHS
  documentation lines differ. Provider archives/compiler/runtime are unchanged.
- `gesvx-installed-reuse-01/audit.json`: four actual relocated public advanced
  consumers, testID5, and24GESVXexports/package. Four scalars, N/T/C,16layouts,
  ordinary/scaled2x2fixtures and FACT=N/E/F returned-factor reuse are exercised.
  Ten selected source inputs and the product/package/ABI closure match the
  actual PTSVX installs. No private source include, evidence-directory dependency
  or bundled provider is necessary. Provider-free consumers remain isolated.
- Modes strict03 and observer strict02 pass. The initial new diagnostic enum
  control, missing empty-view constructor and two oversized observer functions/
  wrapper-comment errors are preserved with their corrections. Only affected
  tests were rebuilt; the mathematical function and observer phase results are
  unchanged. No failed requirement was converted to expected success.

Debug profiles each pass five engineering tests and retain four mathematical
failures240assertions. Static sanitizer/race verification, shared profiles,
final delivery and fresh hosted checks are in progress and receive no advance
credit. Existing public API/export/Doxygen inputs remain unchanged. The
accepted native20/array-I/O subset and separate RobustPpsvx capability remain
closed. Other dependency-ready families continue independently of the two
numerical decisions and unavailable wider provider platforms.

### Static completion

`gesvx-static-audit-01/audit.json` records six completed static Debug, Release
and ASC-only ASan/UBSan profiles in actual LP64 and true ILP64. Each passes
five engineering processes and retains four mathematical failures240assertions.
Both static TSan profiles pass4/4. Every observer retains the same20phase
records,23,832phase checks and196calibrations. No skips occurred. Exact selected
IDs, commands, statuses and raw-value comparisons are preserved. Only ASC/test
code is instrumented; the pinned provider/runtime is not. Shared verification
and delivery remain the next finite steps.

### Shared completion after transport recovery

Recovery at `6fd7c90` found six shared profiles already complete. Only the
previously unstarted LP64 and ILP64 ASan/UBSan profiles were executed. The
existing `audit_gesvx_shared_01.py` then passed over all eight retained
profiles without rerunning Debug, Release or TSan.

`gesvx-shared-audit-01/audit.json` binds the original frozen tree
`aa4012465076f33457b79f897b1761992be7c67f`. Each of the six shared
Debug/Release/ASan+UBSan profiles passes 17 of 21 tests, retaining exactly
four required mathematical failures and 240 failed assertions. Both shared
TSan profiles pass four of four. Every observer's 20 phase records match the
static baseline; the mathematical output signatures also match. Zero skips
occurred. Sanitizers cover ASC and test code, not the pinned foreign provider.

Before the next PB correction began, all 1,210 frozen build/API/test/ABI inputs
were rehashed against the recovered workspace. Ten installed-consumer source
dependencies still match the four retained relocated consumers. Coverage,
backlog, documentation consistency, local links and whitespace checks passed
in `continuation-20260912-01/gesvx-current-contract-checks`. The bounded local
engineering slice is complete; numerical acceptance, normalized full-profile
evidence, wider provider platforms and fresh hosted delivery remain open.
