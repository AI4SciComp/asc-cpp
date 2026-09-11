# PTSVX bounded typed implementation review

This is the existing P05.required.ptsvx task, four Reference catalogue entries.
The prerequisite record binds the four pinned 3.12.1 definitions and unchanged
PT view/provider/workspace/report dependencies. No upstream algorithm is copied
or modified. PTTRF/PTTRS, PTCON, PTRFS, PTSV, robust PPSVX and native/I/O remain
preserved. Numerical and wider-platform acceptance are separate from this
checked first-party adapter's engineering completion.

| Routines | Real storage | Scalar storage | Native workspace |
| --- | --- | --- | --- |
| SPTSVX | float D/DF/RCOND/FERR/BERR | float E/EF/B/X | real 2N |
| DPTSVX | double D/DF/RCOND/FERR/BERR | double E/EF/B/X | real 2N |
| CPTSVX | float D/DF/RCOND/FERR/BERR | complex float E/EF/B/X | complex N + real N |
| ZPTSVX | double D/DF/RCOND/FERR/BERR | complex double E/EF/B/X | complex N + real N |

FACT=N/F only, selected by mutable factor-output versus nominal immutable
factor descriptors. No FACT=E, UPLO, pivots or hidden fallback. D/DF are real
N-element arrays; E/EF contain max(N-1,0) scalar entries. Original E is lower.
Native complex PTSVX uses lower LDL-adjoint factors. Supplied upper factors
represent U-adjoint D U, so their upper off-diagonal is conjugated into lower
input scratch. For real scalars the two orientations coincide. All four exact
native function-pointer types, including the admitted hidden FACT string length,
are statically compared with the provider SDK in the test-only interceptor.

Original A/B and supplied factors are immutable. Generated DF/EF go directly
to the provider as outputs, without old numeric reads. X and RCOND/FERR/BERR
are explicit staged outputs. Row B is packed; column B is direct; X always
stages and publishes into its independent padded layout. Native scalar/real
workspace, flat real estimate scratch, X/row-B/upper-E conversion scratch use
existing checked regions. All nine operands and metadata/workspace obey
existing disjointness and access rules. No allocation occurs in query/execute.
Queries, stale plans and short workspaces do not read numeric values. FACT=F
validates finite positive DF and finite EF only after workspace admission.

Plans bind scalar, provider, FACT, factor orientation, N/NRHS and both layouts
and leading dimensions, while allowing new addresses/values. Positive N with
zero RHS still calls the provider for factorization and condition estimation;
live dummy arrays serve inactive solution/estimates. N=0 completes locally,
writes RCOND=1 and zero estimates, and reports no native entry or INFO. Native
INFO=N+1 and COPY loop terminals require N below the integer maximum even
with zero RHS. Active refinement inherits the exact PTRFS N/NRHS/real-2N
bounds; ASC extent and aggregate byte requirements are checked separately.
No fabricated huge arrays or uninitialized-object reads are used as proof.

Generated FACT=N INFO=1..N publishes RCOND=0, retains partial DF/EF and leaves
X/FERR/BERR unchanged; diagnostic index is INFO-1. Typed-source review found
that this pivot failure cannot occur in FACT=F after its factor admission;
such a return is a provider defect, preserving all staged caller outputs.
Negative, unwritten, partial-width ILP64, out-of-range INFO and missing/negative
estimates likewise report a provider defect. Direct generated factors cannot
be rolled back. Valid INFO=N+1 warns after solution/refinement. Raw NaN/Inf
outputs are retained with a numerical warning, without clamp or rescaling.
Factor reuse uses FromRaw and the existing explicit owner CopyFrom; no PTTRF
report certificate is fabricated. Native cost is O(N+N*NRHS), with bounded
refinement iterations, not a new performance claim. RCOND/FERR are estimates.

Candidate06 Release passed ordinary/preflight/concurrency/fault/observation and
public-example tests in actual LP64 and true ILP64: seven of eleven selected
processes. Its four genuine mathematical failures contain 224 assertions per
profile. All 64 ASC observations are unchanged from candidate02 and match
typed direct calls. Raw paths/IDs/exit codes/archive identities are retained
in ptsvx-candidate-06-audit-01. Strict review caught an oversized helper and a
mixed-array auto declaration accepted by GCC but rejected by Clang. The
reviewed corrections passed all six integrated strict-analysis units in
ptsvx-root-style-01; Doxygen and public coverage passed in
ptsvx-documentation-01. Earlier failed attempts and source snapshots remain
intact.

Ordinary fixtures independently build dyadic LDL-adjoint systems at orders
0/1/3/5/9, zero/two RHS, exponents -16/0/16, independent B/X layouts and fresh,
lower-supplied, upper-supplied factors. Four independent workers each run both
modes32times with private data/context/workspace/report; their final distinct
failed pivot checks diagnostic isolation. Only immutable plans are shared.
Fault state is TLS and does not participate in ordinary concurrency.

The observer uses real initialized aligned containing arrays, PROT_NONE, fork
and default terminating signals; no recovery runs in a signal handler. Its
validated exact foreign-entry interception restores output access before
calling the real provider. 192 bounded mode/layout/shape/scalar cases include
768 deliberate output-read faults,192 queries,192 stale plans,every active
workspace shortened one byte,96 local completions,96 native output checks,
64 legitimate factor-input read controls and192 restored writes. It observes
ASC pre-native executed accesses, not optimized-away expressions or foreign
internals. Local output arrays are writable: local write-only behavior is
source-reviewed, while inactive input pages are protected. PROT_WRITE is not
claimed unreadable. Strict and sanitizer compiler settings have separate
records; no foreign instrumentation is inferred.

| Cause | Exact fixture | Required / observed / direct | Decision |
| --- | --- | --- | --- |
| PTTS2 reciprocal before RHS scaling | N=NRHS=1, A=B=a, E empty, a in denorm_min,2denorm_min,minnormal/8; S/D/C/Z, FACT N/F, both layouts | X=1 within64epsilon; real Inf or complex(Inf,NaN), downstream FERR/BERR NaN; direct matches | Retain solve gate; separately authorized scaled PT solve/provider decision needed |
| PTCON inverse overflow | Same tiny scalar systems | RCOND=1 within64epsilon; RCOND=0, final INFO=N+1=2; direct matches | Retain condition gate; scaled condition/provider decision needed |
| PTCON reciprocal/norm evaluation | Same scalar setup at maxfinite | RCOND=1; rounded subnormal inverse followed by reciprocal overflows, RCOND=Inf,INFO=0; direct matches | Same condition decision, distinct arithmetic cause |
| PTRFS unscaled residual weight | Same maxfinite setup | Finite nonnegative FERR; ABS(B)+ABS(D*X) overflows before epsilon multiplication,FERR=Inf,BERR=0; direct matches | Retain estimate gate; scaled residual/weight/provider decision needed |

Counts are four failed test processes,224 assertions,16 scalar/value cases,
32 scalar/value/mode cases and64 layout executions per profile; four inherited
arithmetic causes, not four independent newly discovered bugs. Tiny solve
failure contaminates later residual estimates; that consequence is not counted
as another root cause. The PPSVX-only robust authorization does not authorize
a PT-family numerical algorithm, provider modification or requirement waiver.
Independent integration/platform work may proceed without numerical promotion.

## Frozen integration verification

The single frozen product is tree
`198f1846f7422e1753869c399770a17448d0b77c`, archive SHA-256
`0a2ab0921f1f44976fd63b8ad6ce2ce06a9033ca50abdc45dc4695d5ab52c896`,
based on `8e3123ff0122735f9835bc6718e39fed4b817b2f`. Its 1,197 recorded
build/API/test/example/package inputs are retained in the external
`master-continuation-20260910-01/ptsvx-frozen-product-01` directory. This is an
staged integration snapshot, not a hosted-tested feature revision.

Static Debug and Release in both actual ABIs each passed seven engineering
checks and retained four mathematical failures (224 assertions per profile).
Both static TSan profiles passed four ordinary/concurrency processes; only
ASC/test code is instrumented. Initial static ASan profiles passed six checks,
retained the four mathematical failures and timed out in the observer. The
ASan-only timeout amendment preserved all 1,848 controls and raised its finite
budget from 240 to 1,800 seconds. Both observer-only retries passed in about
770 seconds. The original failed profiles remain failures in their raw records;
their other completed checks are reused rather than rerun.

All four fresh frozen-source Release producers compiled. Their first selections
each passed 13 of 18 processes, with the same four mathematical failures and
an observer timeout while the static ASan observers were active. Once those
ASan runs ended, each unchanged Release observer passed a sequential retry in
4.6–4.9 seconds under its original 240-second limit. This is consistent with
resource contention; the precise kernel mechanism was not established. No
source, binary, test assertion or timeout changed for these retries. Subsequent
profile scheduling separates sanitizer and nonsanitizer observers. The exact
selected IDs, original failures, retry durations and unchanged direct-call
observations are in `ptsvx-observer-and-fresh-audit-01/audit.json`.

All four fresh package/relocation checks passed. Each installed optional facet
exports the 16 public PTSVX query/execute declarations, and its copied maintained
consumer passes for all four scalars with factor reuse. Installed header bytes
match the frozen source; consumer flags use the relocated public include tree.
Runtime inspection finds the intended separately prepared provider/runtime
closure and no bundled provider/runtime files. Exact records are in
`ptsvx-package-audit-01/{static,shared}-{lp64,ilp64}/audit.json`.

Both shared Debug profiles passed seven engineering checks and retained four
mathematical failures with unchanged raw observations. Both shared TSan
profiles passed all four ordinary/concurrency processes; provider/runtime
objects remain uninstrumented. `ptsvx-shared-debug-tsan-audit-01/audit.json`
records their exact selectors and outcomes. Eight actual base-only consumers
per package and the five nonprovider shared modules have no LAPACK/Fortran
runtime dependency (`ptsvx-package-isolation-02/audit.json`). The preceding
audit attempt incorrectly classified the explicitly provider-enabled array-I/O
example as provider-free; its failed assertion and correction are preserved.

The separate installed-header matrix and shared sanitizer profiles remain in
progress. No hosted or mathematical acceptance is inferred from these local
checks.
