# PTRFS initial numerical and API review

The original ASC adapter calls the unchanged pinned Reference-LAPACK3.12.1
SPTRFS/DPTRFS/CPTRFS/ZPTRFS. The existing original-matrix descriptor means
lower off-diagonal E. Complex upper factors require explicit conjugate packing
of original E; the first draft's generic accessor/upper reinterpretation was
rejected and fixed in ptrfs-original-orientation-amendment-01. Both initial
compile failures and all candidate archives are preserved. No public descriptor
or provider was changed.

The accepted original factors are real positive D and real/complex E in the
existing lower/upper typed factor semantics. B and old X are numeric inputs;
old caller FERR/BERR are outputs. Active X is always staged transactionally,
B is packed only for row layout, and complex upper original E is conjugated
into checked explicit storage. Native work is real2N or complexN+realN.
Estimate staging is a flat live2*NRHS real array, sized as NRHS two-real units;
it is not native LWORK or an array of complex/pair objects. Negative/missing
estimates or nonzero/missing/half-width INFO preserve all caller X/FERR/BERR.
Nonfinite native results publish unchanged with an accuracy warning. Queries,
stale plans and exact one-byte-short workspace reject without numeric reads
or mutations. Local N0/NRHS0 requires no native call and writes zero estimates.

## Mathematical fixtures and derivation

For each scalar type and both factor orientations, N=1,NRHS=1,D=DF=A=B=a,
empty E/EF, initial X=1. Exact X remains1 for every positive finite a.
The pinned source uses NZ=4 even at N1, u=xLAMCH(Epsilon)=epsilon/2,
SAFE1=4*minnormal. For exact X1, residual0 and denominator2a, the safeguarded
backward estimate is1/(1+a/(2*minnormal)) when a<=2*minnormal/u, otherwise0.
The finite weighted forward expression is8u+4*minnormal/a when safeguarded,
otherwise8u. The test evaluates ratios in that safe order, without requiring a
wider exponent type, and retains64epsilon tolerances. At minnormal/8 these
properties give BERR16/17 and FERR32+8u. This scalar analytic property does not
assert that an estimated FERR is a universal theorem for arbitrary factors.

| Cause and fixture | Required/actual and direct comparison | Source and disposition |
| --- | --- | --- |
| Zero-residual correction corrupted by unscaled reciprocal: a=denorm_min,2*denorm_min,minnormal/8. | Required X1 and finite safeguarded estimates. All S/D/C/Z return XNaN (both complex components NaN), FERR/BERRNaN,INFO0; ASC kNumerical/AccuracyWarning/DocumentedPartial. Direct typed calls match. | Guarded BERR is near1, triggering correction despite exact zero residual. PTTRS forms ONE/DF before scaling zero, so Inf*0 creates NaN and destroys an exact solution. The later inverse estimator also has an unscaled reciprocal, but these executions do not isolate an independent FERR-only failure from the already corrupted correction. Retain the gate; explicit robust provider/algorithm strategy is required. |
| Unscaled residual-weight denominator overflow: a=maxfinite. | Required X1,FERR8u finite,BERR0. Actual X1,FERRInf,BERR0,INFO0 for all scalars and both orientations; direct calls match; ASC qualifies nonfinite publication. | ABS(B)+ABS(D*X) overflows before the small epsilon weight is applied. The subsequent inverse multiplier cannot recover the finite weighted result. Retain the finite weighted-error requirement; scaled residual/error evaluation needs explicit numerical strategy approval. |

Per actual ABI, four failed processes contain80 failed assertions from16
scalar-labeled cases/eight underlying-real fixtures,32 orientation executions
and the two active arithmetic causes above. There are72 tiny-case assertions
(24executions times X/FERR/BERR) and8max-case FERR assertions. Raw hex values,
expected/direct/ASC outputs and INFO/report status are in
ptrfs-initial-audit-01/audit.json and ptrfs-candidate-{lp64,ilp64}-02/tests.xml.
Candidate03 adds passing real-provider concurrency and thirteen-state exact
signature/fault tests; candidate04 adds the calibrated observer. Both keep all
32 numerical observations identical. Final candidate06 passes all seven ordinary/control/public-example processes
in both actual ABIs; four required mathematical processes retain80 assertions.
All six strict translation-unit checks, including new private headers, pass.
The maintained root now contains the adapter, explicit workspace/count support,
reusable tests, installed exports, independent header inventories and copied
public consumer. Final integrated safety, package and hosted execution are
recorded separately; no planned command receives execution credit.

## Integrated scope and source identities

Pinned provider commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, remains unchanged. Actual LP64 and
true default-integer-8 Fortran ILP64 have separate prepared provider packages;
no C++ typedef substitution is treated as Fortran ABI evidence.

The eight public declarations are QueryPtrfsWorkspace/Ptrfs for S/D/C/Z.
Plans bind scalar, order, RHS count, both strides/layouts, factor orientation
and provider. Buffer addresses and numeric values may change. Metadata-only
queries and admission preserve the interface's no-allocation contract. Active
factor validation precedes input staging only after workspace admission. Work
is O(N*NRHS) with at most five native refinement corrections; explicit storage
is O(N*NRHS+N+NRHS). No provider patch, tolerance change, hidden fallback,
rescale, diagnostic clamp or floating-point-environment change occurs.

The maintained public example uses only public declarations and ASC::dense_lapack.
It independently constructs an ordinary2x2 Hermitian matrix, obtains PTTRF
factors once, and refines known right-hand sides across independent padded B/X
layouts. It reuses borrowed lower and owned upper factors after overwriting
the original factor storage. FERR/BERR remain qualified estimated diagnostics;
passing this ordinary consumer does not close the extreme-value gate.

The observer region is active caller FERR/BERR and native scalar/real work,
protected until the exact intercepted foreign entry. Explicit estimate scratch
must be writable for pre-call sentinels and is distinguished from caller
outputs. It detects64 deliberate forbidden reads,64 legitimate factor/initial-X
reads and32 real native entries per ABI. Another64 local completions protect
unused inputs and check output writes. That local write check does not claim
that Linux write-only pages prevent reads. Default child fault termination,
properly initialized containing arrays and exact full-width signatures avoid
fabricated pointers and fault-handler resumption. ASC/test code alone is
instrumented where configured; provider/runtime code is uninstrumented.

Four threads reuse an immutable plan/factor with independent caller storage,
workspaces, contexts and reports,128 real-provider calls per scalar. The global
allocation audit is inactive during concurrency, and synthetic faults use
thread-local state in separate processes. The finite required rows are these
existing mode/layout/empty-path checks; no shared mutable workspace safety is
promised.

External archives and all failed attempts remain in
`master-continuation-20260910-01/ptrfs-candidate-{lp64,ilp64}-01` through06.
`ptrfs-final-external-audit-01/audit.json` binds the executed ordinary/fault/
observation/concurrency/example IDs,11 processes perABI, all80 genuine failed
assertions and exact raw numerical signatures. `ptrfs-style-amendment-01`
preserves mechanical review fixes; `ptrfs-root-import-01` identifies the exact
reviewed files. These records are not a full-program or installed-package pass.

## Recovered shared verification status

The four preserved shared runs below have completed. Recovery read their
command records, configured test listings and JUnit results without rerunning
configuration, compilation or tests. Each selected test IDs 232 through 242,
passed configuration and compilation, and returned CTest exit 8.

| Preserved run | Passed | Required mathematical failures | Skipped |
| --- | --- | --- | --- |
| `ptrfs-shared-debug-lp64-01` | 7 | 4 | 0 |
| `ptrfs-shared-sanitizer-lp64-01` | 7 | 4 | 0 |
| `ptrfs-shared-debug-ilp64-01` | 7 | 4 | 0 |
| `ptrfs-shared-sanitizer-ilp64-01` | 7 | 4 | 0 |

All four use frozen tree `a5f03f3dc40ff47dde5c36d8e00986b44a304a17`
and their own recorded shared provider attestation. The recovery checked the
12 PTRFS source/build/API/test/example inputs against the frozen manifest and
current integration checkout. The two sanitizer runs instrument ASC and test
objects with ASan+UBSan; provider and runtime DSOs remain uninstrumented.
No sanitizer diagnostics appeared in the recorded test output.

All ordinary, fault, calibrated observation and public-example tests passed.
Each observer recorded 64 forbidden-read controls, 64 legitimate input-read
controls, 32 native entries and 64 local completions. Each run retains all
80 mathematical assertion failures and exactly matches the initial 32 raw
numerical observations. Across these four executions, 28 of 44 test processes
passed and 16 failed, with 320 failed assertions and no skips. The two causes
in the disposition above remain unresolved; repeated profiles do not add
unique mathematical cases or causes.

`master-continuation-20260910-01/ptrfs-shared-status-audit-01/audit.json`
binds the original commands, timestamps, selected IDs, outcomes, raw signatures
and artifact hashes. The previous programme state and this review were preserved
beside that audit before this update. At that recovery checkpoint, shared TSan,
installed-package gates and PTRFS hosted verification remained pending.
Passing the build-tree public example
does not establish installed-consumer acceptance. No source or test was changed
by this status recovery, and no catalogue verification credit was added.

## Completed local delivery checks

The outstanding `ptrfs-shared-tsan-{lp64,ilp64}-01` runs each pass all four
ordinary/concurrent scalar tests. `ptrfs-shared-tsan-audit-01/audit.json`
records their actual selected IDs and execution outcomes. ASC and test objects
are instrumented; provider and runtime code are not. The previously completed
static Debug/Release/ASan+UBSan and TSan evidence remains unchanged and is
indexed by `ptrfs-static-profile-audit-01/audit.json`.

All four `ptrfs-fresh-{static,shared}-{lp64,ilp64}-01` frozen Release producers
were reused after checking their preserved 18-test results: 14 passed and only
the four required mathematical processes failed. Their original 32 numerical
observations match in every profile. No producer or earlier test was restarted.
The previously unexecuted package phase now passes once in each configuration.
Each actual installation relocates before building and running the maintained
public consumer. `ptrfs-package-audit-01/audit.json` confirms the eight exported
overloads, matching public header, resolved runtime closure and absence of
bundled provider/runtime files. Package checks retain the provider-free native/I/O
consumers and dependency-isolation controls.

The four `ptrfs-installed-header-{static,shared}-{lp64,ilp64}-01` runs each pass
all 11 configured tests, including their required setup/cleanup fixtures.
`ptrfs-delivery-checks-01` passes final format, Doxygen, documentation consistency,
scoped coverage and generated-backlog checks. All 1,169 compiled/build/API/test/
example/package/ABI inputs match the single frozen candidate. The unchanged six
strict source/header checks in `ptrfs-root-style-01` are reused explicitly.
Later programme summaries and the regenerated backlog receive no whole-tree
hosted certification from these source comparisons.

This completes the permitted local GNU/Linux static/shared delivery checks in
both actual ABIs. The two numerical causes above remain required failing gates;
Reference verification stays zero of 2,113. Feature synchronization and fresh
hosted results are recorded against their actual revision separately. The next
dependency-ready implementation is S/D/C/Z PTSV; its existing prerequisite review
is `ptsv-prerequisite-01/review.json`. Neither the PTRFS numerical decision nor
the pending notice packet blocks that independent first-party adapter work.


## Hosted result and bounded consumer correction

Commit `6739b63f59b2d873471399ba2bafb7483a28798e` completed CI run
34620080987 (19 successful jobs) and CodeQL run 34620081027. Family run
34620080990 executes 181 tests in each static/shared actual-ABI profile:
164 pass, 17 required mathematical processes fail, and none skip. The failures
are PTTRS four, PTCON four, PTRFS four, SGEDMDQ one and GBRFS four. Both new
PTRFS public-header tests are included alongside the 11 family processes.
All 32 PTRFS numerical observations remain identical; each provider passes
111/111. `ptrfs-hosted-audit-01/audit.json` preserves these exact outcomes.

Subsequent review found that the PT, PTCON and PTRFS public-example mains
lacked the existing normal-return guard. Their observed solutions completed,
but future Fortran STOP/exit(0) could otherwise masquerade as success. Each
example now carries an exact local copy of the unchanged test guard and creates
it first in main, with no private source-tree include dependency. The unchanged
guard calibration covers real pinned STOP, exit(0) and ordinary return.
`pt-consumer-return-guard-amendment-01` records the preserved original sources,
format correction and three strict passes. All 12 copied installed consumers
pass against the unchanged relocated static/shared actual-ABI libraries.
Only three example mains and three local helper headers change; 1,166 other
frozen build/API/test/package/ABI inputs remain identical. Current artifact
indices change for 16 PT routine rows under a new explicit baseline bridge;
contracts, route states, modes, historical executions and all native20 rows
remain unchanged. The later guard revision needs its own hosted evidence.
