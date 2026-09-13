# Two-stage Aasen consumer contract and delivery

Status: six consumers are callable with required range mathematical failures
retained for `P05.required.hetrs_aa_2stage`.
This review alone does not promote a coverage row. The six producers are integrated at
`68bb0915666c94eb68a73cab83416e5ad60a14e3`; their required range and optimal TB-query
failures remain open. The full P00–P11 programme remains incomplete.

## Exact provider contract

The pinned inventory contains SSYTRS_AA_2STAGE, DSYTRS_AA_2STAGE,
CSYTRS_AA_2STAGE, ZSYTRS_AA_2STAGE, CHETRS_AA_2STAGE and ZHETRS_AA_2STAGE.
All use twelve pointer arguments: UPLO, N, NRHS, A, LDA, TB, LTB, IPIV, IPIV2,
B, LDB, INFO, followed by the GNU hidden CHARACTER length. There is no native
WORK, LWORK or workspace-query call. Actual LP64 and true ILP64 compiler
emissions are in `aasen-two-stage-solve-prerequisite-01/emissions.json` under the
existing continuation evidence area. They are compared with the pinned header
by the maintained ABI probe, retaining the actual native INTEGER width.

Same-origin A, TB, both pivot arrays, original LTB, triangle, scalar and symmetry
must remain together from SYTRF_AA_2STAGE/HETRF_AA_2STAGE. This is an explicit
caller precondition; metadata cannot authenticate common numerical origin.
A is immutable selected square shifted triangular factor storage. TB is an
immutable contiguous vector with the exact original length. Outer pivots use
an exact-N immutable `kAasen` raw view. Band pivots use a separate exact-N
contiguous immutable ASC index vector; no new public pivot-family meaning or
existing factor factory is introduced. Single-stage and classic/ROOK/RK factor
encodings are outside this consumer contract.

The source forms NB=INT(TB[0]) and LDTB=floor(LTB/N). Checked active execution
requires an exact real integer NB in 1..min(192,(LDTB-1)/3), including zero
imaginary component. The first min(N,NB) outer pivots are identities, later
pivots obey i+1 <= P[i] <= N; band pivots obey
 i+1 <= Q[i] <= min(N,i+NB+1). These validations precede mutable scratch/output.
LAPACKE's TB pointer lacks const, but all six pinned consumers and their GBTRS
closure read TB. A private source-proven const conversion preserves that ABI;
read-only memory and concurrent shared factors are required checks.

When N>NB, native execution applies forward outer LASWP, a shifted unit TRSM,
GBTRS with TRANS=N and KL=KU=NB, the other TRSM, and reverse outer LASWP.
For N<=NB only GBTRS executes. Complex SY uses transpose; HE uses adjoints.
Original-Hermitian imaginary-diagonal normalization does not apply to factors:
all selected factor coefficients are preserved, including ignored entries.
GBTRS reads band LU and its interleaved pivots, mutating only B. Its zero U
diagonal is not diagnosed by native positive INFO. The checked consumer follows
the existing band-solve policy: an exact zero diagonal yields kNumerical,
kSingular and the zero-based diagonal index without native entry or synthetic
INFO. No blanket finiteness scan or numerical fallback is introduced.

## Checked workspace and failure semantics

Queries inspect metadata only. They validate square A, matching RHS rows,
exact pivot counts, contiguous TB/band pivots, LTB>=4*N, placement, disjoint
reachable spans, original/effective strides and source INTEGER arithmetic.
They bind shape, LTB, original layouts/strides, options and provider/scalar
identity. Active execution needs 2*N placement-constructed native INTEGER
entries in caller bytes and live scalar packing for row A and row B. TB stays
in its existing band storage; no hidden dense conversion occurs. N=0 or NRHS=0
is a checked noncall with no array reads or workspace requirement.

Full-width INFO is seeded before the native call. The only valid native INFO
is zero; missing/partial, negative or positive values are provider defects.
Both private input pivot arrays must remain unchanged. Defects withhold packed
B; direct B may have changed. Immutable A/TB/public pivots remain unchanged.
Structural rejection preserves operands and scratch; unsafe metadata/report
aliases also preserve the report. Concurrent calls may share immutable factors,
provider and plans with distinct writable B, workspace and reports.

Source bounds cover 4*N before native validation, both INTEGER array sizes,
LTB/LDTB, NB bandwidth arithmetic, and GBTRS RHS/TBSV terminal cursors. LASWP's
32-column loop, forward/reverse pivot cursors and tail are checked against the
actual signed INTEGER maximum. Existing checked descriptor/packing byte totals
remain enforced. No provider changes, wider admission or successful mathematical
requirement is inferred from completion status.

## Finite delivery requirements

Reuse the established family matrix: both triangles, independent A/B layouts,
all six scalar classes, empty/scalar/singular controls, multiple RHS, and
independent minimum/intermediate/preferred producer TB and WORK capacities.
Retain ordinary independent solution/residual and factor reconstruction checks,
provider fidelity, the established finite extreme-scale cases, protected-memory
queries, alias/workspace/count guards, injected native status/pivot faults and
concurrent shared immutable inputs. Keep any inherited producer numerical cause
linked to its existing reproduction; do not repeat unchanged broad sweeps.

Required delivery includes actual emitted-ABI execution, four relocated public
consumers, strict analysis, normal/no-exceptions headers, exports, package and
architecture integration, the maintained CI selector, Doxygen, and the existing
static/shared LP64/ILP64 Release/Debug/ASC-ASan+UBSan/TSan profiles. Record actual
selected IDs, exit codes, failures and skips in the same current handoff before
normalizing coverage or committing this slice.

Current prerequisite execution: both actual ABI libraries compile; twelve
emitted signatures compile and each ABI executes 2,304 guarded native cases
successfully. Every active native solve checks known solutions/residuals; full
factor reconstruction covers each distinct producer input at NRHS=3. The same
factor inputs at NRHS=1 retain their solution/residual and preservation checks.
NRHS=31/32/33 exercises LASWP block/tail boundaries. Raw records preserve the
initial missing header-ownership configure failure and the subsequent stale
make target-list failure; neither was a numerical test invocation.

The first maintained public ordinary matrix contains 696 cases per scalar
class. It retains 608 predecessor consumer cases using paired minimum/preferred
producer capacities, plus 72 N=193 independent-capacity cases and 16 NRHS=33
cases. Their definitions are frozen before execution in the current prerequisite
records. Their results are recorded below; emitted-ABI success does not replace
the public delivery gates.

Current public ordinary evidence now passes twelve processes in each ABI, with
696 cases per scalar class and no skips. An ASC count-order defect rejected
zero-RHS queries because the common empty leading dimension is one; the fix
retains all dimension/LTB checks and returns before active stride admission.
Sixteen direct count assertions and every original ordinary case pass. The next
attempt found only an incorrect preflight output-validity expectation. Public
report semantics and the existing band solve require `kUnchanged` before native
entry; fixing that single oracle preserved every mathematical and byte check.
Both attempts, exact sources/binaries and raw exits remain in the count-repair
and report-oracle-fix records. An overbroad intermediate evidence glob also
flagged unregistered fault-file edits; its wrapper exit is retained, actual
compiled inputs were unchanged, and the successful rerun binds explicit inputs.

Current range/fault evidence executes eighteen processes per ABI: twelve pass,
six required mathematical processes fail, and none skip. Each of six range
classes covers 1,024 cases; each fault process covers 960 cases. All fidelity,
INFO/pivot fault, guard, publication and allocation checks pass. The range
failures contain 6,720 per-element finiteness assertions per ABI. Of those,
6,336 (1,056/class) occur in 288 parameter groups/class at the three already
recorded tiny scales; the exact producer GBTRF/GBTF2 cause is reused. Another
384 assertions occur in 32 groups for each complex class, at the offdiagonal
N=2 block with .75*max components. Its factors reconstruct correctly. The
preferred NB>=N path bypasses outer triangular operations and reaches GBTRS's
upper TBSV complex division. This extends the existing large-complex arithmetic
decision. Lower scales, scalar controls and real large-scale cases pass. The
source-bound failed-group records and `aasen-two-stage-solve-range-classification.json`
retain this split; native fidelity never substitutes for mathematical success.

Validation passes six processes in each ABI: 664 cases per scalar class,
protected-memory metadata queries and empty noncalls, original stride checks
and pure native-integer bounds. Concurrency passes all six scalar classes in
both ABIs with A, TB and both pivot buffers on read-only pages. Each class
executes 32 groups, four workers and four repetitions: 512 native solves with
independent known-solution/residual checks, 512 empty and 512 stale-plan calls.
The initial concurrency build failed before test execution due to a missing
generated configuration include directory; the target registration is repaired
and both original failure and passing invocations are retained.

The active dependency contract binds the six sources and fixed TRANS=N
GBTRS/LASWP/TRSM/TBSV/SWAP/GER or GERU closure. Source comparison confirms
matching S/D and C/Z executable statements after scalar normalization, except
C/Z GBTRS EXTERNAL declaration order. Read mutation targets and native cursor
bounds are recorded in `dependency-source-inputs.json` and
`dependency-scalar-differences.json` in the current prerequisite area. The six
reviewed routine contracts contain 24 triangle/layout/RHS modes each.

Strict analysis passes sixteen product/test translation units across both
actual ABIs. Preserved repairs add direct includes, split independent residual
accumulator declarations and extract layout-fault cases with explicit size
comparisons. Twelve compatible passing TU checks are reused; the last four
rerun only the ABI-probe include and validation helper fixes. No mathematical
input, predicate, tolerance or provider source changes. Final native, installed
and profile checks still determine this source's delivery status.

## Final scoped delivery

Sixteen final profiles use actual GNU Linux LP64 and true ILP64 with static
and shared libraries, Release, Debug, ASC-ASan+UBSan and separate TSan
concurrency. Twelve normal profiles each pass 37/43 processes; four TSan
profiles each pass 6/6. The canonical total is 540 executed processes: 468
pass, 72 required mathematical failures, zero skips. Across the twelve normal
profiles, 80,640 per-element finiteness assertions remain failed. Each invocation
records actual selected/executed IDs, raw exit and JUnit outcomes, exact source
and executable identities, configuration, pinned provider libraries/headers/
attestation and runtime hashes. No earlier execution is relabelled as fresh.
Pinned Fortran/BLAS internals are uninstrumented; these sanitizer results do
not establish provider-internal instrumentation or wider platform admission.

The current maintained native probe compares all twelve emitted GNU signatures
and executes 2,304 guarded cases in each ABI successfully. Current source has
eighteen passing strict translation-unit checks, including two isolated public
consumers. The original strict/build diagnostics and finite count/report-oracle
failures remain preserved with their source identities. Only the demonstrated
ASC empty-count defect and incorrect preflight report expectation changed
behavior or an assertion; every mathematical requirement remains unchanged.

Four installed consumers use static/shared LP64/ILP64, relocated paths with
spaces and C++-only Dense LAPACK package lookup. Each passes 1,920 cases with
two repeated solves per factor set. The finite set covers six scalar classes,
both triangles, independent A/B layouts and minimum/preferred producer TB/WORK
capacities, empty/scalar/larger cases and singular preflight. Independent known
solutions/residuals, immutable A/TB/P/Q, B padding and caller workspace guards
are checked. The source includes only installed public ASC headers and local
public-consumer helpers; package metadata and consumer compilation admit no
source or producer build include path, implicit BLAS/LAPACK/CUDA discovery or
unrelated ASC owning component.

P11 requires four normal/no-exceptions header processes, twelve new and zero
removed exported symbols per ABI, ten package manifest checks, three
architecture/dependency checks and the maintained selector's 1,506 configured
IDs. Doxygen must cover 157 public headers and 2,693 documented members without
warnings. Documentation generation and final installed/Release library identity
comparison follow this review update; the final audit enforces their terminal
results before atomic coverage/evidence normalization and owned staging.

Pushed parent `68bb0915666c94eb68a73cab83416e5ad60a14e3` passes general CI
and CodeQL analysis. Four hosted selected profiles each execute 1,461 processes
without skips: LP64 passes 1,260/fails 201; ILP64 passes 1,264/fails 197. Exactly
seven new producer required gates join every prior failure; provider checks
pass 111/111 each. Feature-ref alerts total 3,199 open, including the same
twelve security findings. All fifteen new notes were inspected with exact
source excerpts: fourteen descriptor-parameter notes and one required exact
integer TB block-width witness. No finding is dismissed or suppressed. Hosted
records distinguish GitHub run/head identity from absent independent ASC
checkout/tree artifacts; they grant no current dirty-source or PR merge-tree
admission. Numerical, security, provenance, dependency and platform gates
remain visible in the current owner packet.

Use the same `rk-inverse-profile-recovery-01/latest-handoff.json` for unfinished
delivery. Finish owned commit/push/draft47 update if pending, then continue
`P05.required.hesv_aa_2stage`, its actual dependencies and the full remaining
ready programme. Native20 and explicitly selected experimental RobustPpsvx
remain separate prior milestones; the full P00–P11 programme is incomplete.
