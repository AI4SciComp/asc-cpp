## 2026-09-13: Two-stage Aasen drivers ready for feature integration

Branch `feature/lapack-array-io`, pushed parent
`d58b8c315ddcf8d5e07bc544fb8cb35a861474f1`. Inspect actual git and
`aasen-two-stage-driver-delivery/commit.txt` for the delivery commit. All three
user instruction files, prior milestones and raw failures remain preserved.
The full P00–P11 programme remains incomplete.

Six SYSV_AA_2STAGE/HESV_AA_2STAGE drivers add twelve public declarations and
144 reviewed modes. They factor original A and solve B with caller-owned
persistent TB, two pivot outputs, scalar WORK, native INTEGER lifetimes and
explicit row/HE-A and row-B packing. Queries read metadata only. Empty N is a
checked noncall; nonempty N with zero RHS still factors A and preserves B.
Dual native query arithmetic, original strides, aliasing, both pivot sequences,
block width, final WORK and positive-INFO zero-diagonal witnesses are checked.
Valid positive INFO publishes partial factors with B unchanged; defects
withhold packed A/B and public pivots. No factor-factory or pivot-family API
expansion, provider change, fallback, robust route or waiver is introduced.

Sixteen profiles execute 552 processes: 468
pass, 84 required failures, zero skips. Twelve normal
profiles pass 37/44 each; four TSan profiles pass 6/6 each. The normal profiles
retain 412,416 factor/solution finiteness assertions from existing tiny-scale
GBTRF/GBTF2 and large-complex GBTRS/TBSV causes, plus 48 native optimal-TB
capacity assertions from the existing CSY rounding cause. Exact failed range
groups match preserved producer/consumer evidence. No required input,
mathematical predicate or tolerance is weakened.

Four relocated installed consumers each pass 1,920 driver cases. Eighteen
strict TUs pass. Current emitted-ABI probes pass 4,896 ordinary and 576
query-fidelity cases; 576 separate required query cases retain eight capacity
assertions. Four standalone headers, twelve new/zero removed exports per ABI,
ten package and three architecture checks pass. CI selects 1,552 tests;
Doxygen covers 158 headers and 2,705 public members without warnings. Four final
Release libraries match installed products. Strict diagnostics and before/after
source records remain preserved. Pinned Fortran/BLAS internals are uninstrumented.

Parent d58b8c3 general CI/CodeQL pass. Four hosted selections each execute
1,506 processes with no skips: LP64 passes 1,299/fails 207; ILP64 passes
1,303/fails 203. Exactly six new consumer range gates join every prior failure;
provider checks pass 111/111 each. Explicit feature CodeQL alerts total 3,244,
including the same twelve security findings. All 45 new notes were read with
source excerpts; none dismissed or suppressed. Hosted run/head identity is
distinct from absent independent ASC checkout/tree artifacts and grants no
current dirty-driver or PR merge-tree admission.

Validator counts: required 2113; reviewed contracts 288;
Reference callable-unverified 248; partial
296; not started 1569; verified
0. Native remains 20/20; experimental RobustPpsvx is separate.
Finish owned commit/push/draft47 update if pending, then continue
`P05.required.hptrf`, the actual packed-indefinite dependency chain and all
other ready programme rows through the same latest handoff. No new family
instruction is needed.

## 2026-09-13: Two-stage Aasen consumers ready for feature integration

Branch `feature/lapack-array-io`, pushed parent
`68bb0915666c94eb68a73cab83416e5ad60a14e3`. Inspect actual git and
`aasen-two-stage-solve-delivery/commit.txt` for the delivery commit. All three
user instruction files, prior milestones and raw failures remain preserved.
The full P00–P11 programme remains incomplete.

Six SYTRS_AA_2STAGE/HETRS_AA_2STAGE consumers add twelve public declarations
and 144 reviewed modes. They solve with immutable same-origin A, persistent TB,
outer kAasen pivots and a separate band-pivot vector, retaining exact original
LTB/provider/scalar/triangle/symmetry. Metadata-only queries and empty noncalls
read no numerical inputs. Caller workspace owns two native INTEGER arrays and
row packing; no native WORK/LWORK/query exists. Exact block-width/pivot checks
and zero band-U diagonal rejection precede mutable scratch and B. Singular
preflight returns kUnchanged with no call or synthetic INFO. Only full-width
INFO0 and unchanged private pivots are valid native results; defects withhold
packed B. Source-proven read-only TB preserves the pinned mutable-pointer ABI.

Sixteen profiles execute 540 processes: 468
pass, 72 required mathematical failures, zero skips.
Twelve normal profiles pass 37/43 each; four TSan profiles pass 6/6 each. All six
classes retain tiny-scale inherited GBTRF/GBTF2 failures; four complex classes
also retain large-complex division through GBTRS upper TBSV. Across the twelve
normal profiles, 80,640 required finiteness assertions fail. Ordinary independent
solutions/residuals and reconstruction, native fidelity, structural/fault and
read-only shared-factor concurrency pass. No mathematical requirement, valid
input or tolerance is weakened; no provider patch, fallback or waiver is made.

Four relocated installed consumers each pass 1,920 cases with two solves per
factor set. Eighteen strict TUs, 4,608 actual emitted-ABI native cases, four
standalone headers, twelve new/zero removed exports per ABI, ten package and
three architecture checks pass. CI selection includes 1,506 configured tests;
Doxygen covers 157 headers / 2,693 public members without warnings. Four final
Release libraries match the installed products. Initial empty-count admission,
singular report-oracle, build and strict failures remain preserved with exact
source and raw exit identities. Pinned Fortran/BLAS internals are uninstrumented.

Parent 68bb091 general CI / CodeQL succeed. Its four hosted selected profiles each
execute 1,461 processes with zero skips: LP64 passes 1,260 / fails 201;
ILP64 passes 1,264 / fails 197. All earlier failures and exactly seven new producer required gates
remain; provider 111/111 passes. Feature CodeQL has 3,199 open alerts including the
same 12 security findings. All 15 new notes were read; none dismissed/suppressed.
`aasen-two-stage-solve-remote-01/hosted-followup` distinguishes GitHub run/head
identity from missing independent ASC checkout/tree artifacts. No dirty-source
or PR merge-tree hosted admission is inferred.

Validator counts: required 2113; reviewed contracts 282;
Reference callable-unverified 242; partial
296; not-started1575; verified
0. Native remains 20/20; experimental RobustPpsvx is separate.
Finish owned commit/push/draft47 update if pending, then continue
`P05.required.hesv_aa_2stage` and all other ready programme rows using the same
latest handoff and current review. No single-family instruction is required.

## 2026-09-13: Two-stage Aasen producers ready for feature integration

Branch `feature/lapack-array-io`, pushed parent
`80e1d01576b92a799541901c9a0fd73f90db26f2`. Inspect actual git and
`aasen-two-stage-delivery/commit.txt` for the delivery commit. All three user
instruction files, prior milestones and raw failures remain preserved.
The full P00–P11 programme remains incomplete.

Six SYTRF_AA_2STAGE/HETRF_AA_2STAGE producers add twelve public declarations
and 216 reviewed modes. They factor original selected symmetric/HE A using
persistent caller TB and two output pivot arrays. Independent TB/WORK
capacities choose the actual block width. Queries are metadata-only and empty
checked calls do not enter the provider. Caller native INTEGER lifetimes,
packing, source counts/strides/aliases and INFO/both pivots/block width/singular
band-diagonal witnesses are checked. Consistent singularity publishes factors
and pivots with documented partial validity. Existing factor factories stay
unchanged; retain same-origin A/TB/pivots, triangle/symmetry and exact LTB.

Sixteen profiles execute 552 processes: 468 pass, 84 required failures, zero skips.
Twelve Release/Debug/ASC-ASan+UBSan profiles pass 37/44 each; four TSan profiles
pass 6/6 each. Required tiny-scale nonfinite reconstructions reproduce the
existing GBTRF/GBTF2 reciprocal-overflow cause in all six scalar classes:
165,888 mathematical assertions across the twelve normal profiles. Native
CSYTRF_AA_2STAGE optimal TB queries round down at N=30001, reducing NB 192 to 191;
48 required query assertions remain failed. Minimum TB capacity still fits.
All ordinary reconstruction/native fidelity, range fidelity and larger-scale
controls pass. Neither provider changes nor numerical waivers are inferred.

Four relocated installed consumers each pass 864 reconstruction cases.
Eighteen strict translation units, 1,944 emitted-ABI cases, four standalone
headers, twelve new/zero removed exports per ABI, ten package and three
architecture checks pass. CI selection includes 1,461 tests; Doxygen covers
156 headers and 2,681 members without warnings. Final Release libraries match
the installed products. Initial HE fidelity signed-zero oracle failures and
strict repairs remain preserved with exact sources and raw results.

Parent driver hosted CI and CodeQL succeed. Four selected profiles each execute
1,415 tests without skips: LP64 passes 1,221/fails 194; ILP64 passes 1,225/fails 190.
Exactly nine new driver gates join all retained prior failures; provider checks
pass 111/111. Source-bound artifacts are in
`aasen-two-stage-remote-01/hosted-followup`. Explicit feature CodeQL alerts total
3,184 open, including the same twelve security findings; all 32 new notes were
read and remain open. No security closure or hosted two-stage credit is inferred.

Current required rows: 2,113; reviewed contracts 276; Reference callable-
unverified 236; partial 296; not-started 1,581; verified 0. Native operations remain
20/20, with experimental RobustPpsvx separate. Finish owned commit/push/PR update
if pending, then continue `P05.required.hetrs_aa_2stage`, its drivers and the
other ready programme rows. Use the same latest handoff and producer review
for exact next actions; no single-family prompt is needed.

## 2026-09-13: Aasen drivers ready for feature integration

Branch `feature/lapack-array-io`, pushed parent `cb05d524f2dfea67de9d42185c7b975549496642`.
Inspect actual git and `aasen-driver-delivery/commit.txt` for the delivery commit.
All three untracked user instruction files, previous milestones and raw failures
remain preserved. This is the full remaining programme, not a completed project.

Six SYSV_AA/HESV_AA routes add twelve public declarations and 192 reviewed modes.
They factor original symmetric/HE A and solve B, publishing positive Aasen
pivots. Queries are metadata-only; empty N is a noncall, zero RHS still
factorizes. Caller workspace/native INTEGER lifetimes/packing, source counts,
strides/aliases and final WORK/INFO/pivot/singular witnesses are checked.
Consistent singularity publishes factors/pivots but withholds packed B;
direct B may change. Factors can be reused through the checked Aasen solver.

Sixteen profiles execute 600 processes: 492 pass, 108 required failures, zero skips.
Twelve Release/Debug/ASC-ASan+UBSan profiles each pass 39/48; four TSan profiles
each pass 6/6. Retained requirements include 12,288 mathematical and 192 workspace
assertions plus 48 native empty-call STOP processes. Native fidelity and factor
reconstruction pass; large-complex solution failures extend AASEN-SOLVE-RANGE.
Complex SY empty recommendations and CSYSV_AA legal LWORK0 nested TRF_AA STOP
extend AASEN-EMPTY-WORK. Direct native records confirm exit 93. Provider fixes,
new algorithms and waivers are not authorized by these results.

Four relocated consumers pass 768 driver/factor-reuse cases each. Twenty strict
TUs (twelve unchanged first results explicitly reused), 2,016 emitted-ABI cases,
four standalone headers, twelve new/zero removed exports per ABI, ten package
and three architecture checks pass. Doxygen covers 155 headers/2,669 members
without warnings; CI selector includes 1,415 processes. Installed libraries match
final Release identities. Original configure/fixture/strict failures remain
preserved with their corrections; numerical requirements are unchanged.

Parent solve generalCI/CodeQL jobs succeed. Its four hosted selected profiles
each execute 1,365 tests with no skips: LP64passes 1,180/fails185; ILP64passes 1,184/
fails 181. Exactly the five new solve gates join retained prior failures;
provider tests pass 111/111 each. Source-bound artifacts are under
`aasen-driver-remote-01/hosted-followup`. Feature/PR CodeQL queries show 3,152 open
alerts including twelve security findings; 44 new descriptor notes remain open.
No hosted driver credit, security closure or wider provider admission is inferred.

FULL_PROGRAM_INCOMPLETE: required 2,113; reviewed 270; Reference callable-
unverified 230; partial 296; not-started 1,587; verified 0. Native operations remain 20/20,
experimental RobustPpsvx separate. Finish owned commit/push/PR update if pending,
then continue `P05.required.hetrf_aa_2stage` before its consumers/drivers and
other ready rows. The same latest handoff and driver review preserve the exact
next command and evidence paths. No new single-family instruction is required.

## 2026-09-13: Aasen solve consumers ready for feature integration

Branch remains `feature/lapack-array-io`, with pushed parent
`c02b549c48247c747df81bef66b7209b42a26529`. Inspect actual git and
`aasen-solve-delivery/commit.txt` for the delivery commit. The three user
instruction files and all prior milestones/failure records remain preserved.

Six SYTRS_AA/HETRS_AA routes add twelve public declarations and 96 reviewed modes.
Immutable common-origin A/pivots retain single-stage Aasen provenance. Caller
WORK/native INTEGER/row A-B packing, counts, original/effective strides and
aliases are checked. Full INFO/private input pivots and seeded singular WORK
witnesses are validated. Consistent positive GTSV INFO reports singular,
unusable solution; packed B is withheld and direct B may change. Empty N/RHS
is a validated noncall before numerical reads.

Sixteen profiles execute 552 processes:492 pass,60 required failures,zero skips.
Twelve Release/Debug/ASC-ASan+UBSan profiles each pass 39/44; four TSan
concurrency profiles each pass 6/6. Across normal profiles, 6144 mathematical and
192 native query assertions fail. AASEN-SOLVE-RANGE retains large-complex
solution/residual failures with representable matrices/factors/solutions;
producer reconstruction, native fidelity and real/lower-scale controls pass.
AASEN-EMPTY-WORK retains complex-SY native N0 query-2 below minimum1.
The first completed CTest result was recovered after a parser double-counted
repeated diagnostic summaries; its source/binaries/logs are unchanged and
that invocation was not repeated. No requirement or test is weakened.

Four relocated public consumers pass 768 cases each. Eighteen strict TUs,
2,016 guarded emitted-ABI cases, 4 standalone headers, 12 new/no removed exports per
ABI, 10 package and 3 architecture checks pass. Doxygen covers 154 headers and 2,657
members without warnings; the maintained CI selector includes 1,365 processes.
Installed libraries match final Release producers. Pinned Fortran/BLAS
internals are uninstrumented; wider admission remains pending.

Hosted parent profiles preserve exactly the prior failures plus 7 producer
gates, zero skips, and provider 111/111 each. GeneralCI's provider-free inventory
omission was reproduced and repaired through the existing explicit provider
header list. Branch-specific CodeQL queries show 3,108 open alerts including the
same 12 security findings; 15 new producer notes were read and remain open. No
hosted consumer-tree or security-closure credit is inferred.

FULL_PROGRAM_INCOMPLETE: required 2,113, reviewed 264, Reference callable-
unverified 224, partial 296, not-started 1,593, verified 0. Native20 stays 20/20;
experimental RobustPpsvx remains separate. Complete owned commit/push if
pending, then continue `P05.required.hesv_aa` and the entire dependency-ready
queue. Its driver overwrites WORK[0] after solve and factorizes with zero RHS:
review these exact semantics before adapting consumer guards. The same latest
handoff, solve review and `next-driver-read-notes.json` preserve the next action.

## 2026-09-13: Aasen producers ready for feature integration

The worktree remains `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`,
with pushed parent `7dd45816ff3bdd4715223f2cc2f92fc230ea3c49`. Inspect actual git
and `aasen-factor-delivery/commit.txt` for the delivery commit. All three user
instruction files, prior milestones and failed records are preserved.

Six SYTRF_AA/HETRF_AA routes add twelve public declarations and 72 reviewed
modes. Selected A holds tridiagonal T and shifted triangular multipliers;
positive one-based pivots have Aasen provenance. Caller-owned WORK, native
INTEGER lifetimes, row/original-HE packing, source bounds, aliases and complete
INFO/pivot/WORK writes are checked before publication. Empty calls are validated
noncalls; singular factors may complete with INFO zero.

Sixteen profiles have 564 canonical processes: 480 passes, 84 required failures,
zero skips. Twelve Release/Debug/ASC-ASan+UBSan profiles each pass 38/45; four TSan
profiles each pass 6/6. AASEN-FACTOR-RANGE retains tiny reciprocal and large complex
coupled reconstruction failures; AASEN-EMPTY-WORK retains native CSY/ZSY empty
optimal WORK zero below minimum one. Across normal profiles, 3,887,424 mathematical
and 96 native WORK assertions fail. Every numerical requirement remains active.

Each normal selection reuses 43 unchanged original core processes plus two
current WORK processes after one direct-include correction. Original 45-process
runs and the first 43+2 composite remain separate, with actual exit 8; all TSan
invocations exit 0. Full verbose logs preserve assertions truncated in JUnit.
Earlier style, header-oracle, Doxygen-link and audit-marker failures are retained.

Four relocated consumers pass 288 analytic cases each; 20 strict TUs, 576 guarded
native ABI cases, 4 standalone headers, 12 added/no removed exports per ABI,
10 package checks and 2 architecture checks pass. Doxygen has 153 headers and
2645 members without warnings; the maintained selector includes 1319 processes.
Installed libraries match final producers. Pinned Fortran/BLAS internals remain
uninstrumented; wider provider/platform admission remains pending.

Previous RK-driver hosted profiles retain exactly the prior failures plus six
driver range gates, zero skips; provider 111/111 each and general CI pass. CodeQL
analysis passes but 3093 alerts remain open, including 12 security findings.
Those are 7dd458 evidence and do not certify this Aasen working tree.

FULL_PROGRAM_INCOMPLETE: required 2113; reviewed 258; Reference callable-unverified
218, partial 296, not-started 1599, verified 0. Native20 stays 20/20 and experimental
RobustPpsvx remains separate. Finish the owned commit/push if pending, then
continue `P05.required.hetrs_aa` and its exact source dependencies. The existing
latest handoff and `indefinite-aasen-review.md` identify all commands and evidence.

## Latest checkpoint: RK drivers ready for feature integration

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Parent is pushed `7add1a8fbf45a948d55da027862a5df278837995`; inspect actual git
and `rk-driver-delivery/commit.txt` for the delivery commit. User instruction
files, prior inverse/native/I/O milestones and all failed evidence are preserved.

Six SYSV_RK/HESV_RK routes add twelve public declarations and 192 reviewed modes.
The named native driver produces separate RK A/E/pivots and solves B; zero RHS
still factorizes, and zero order is an ASC noncall. Caller-owned scalar WORK,
private INTEGER pivots and A/B packing have checked counts and lifetimes.
Full-width INFO/pivots, E structure and WORK are checked before packed publication.
Valid singular output publishes factors and preserves B. Driver-origin factors
are reused through checked TRS_3, CON_3 and TRI_3 in installed consumers.

Sixteen final profiles execute 540 processes: 468 passes, 72 required mathematical
failures, 11,520 failed assertions and zero skips. Twelve Release/Debug/ASC-ASan+
UBSan profiles each pass 37/43; four TSan concurrency profiles pass 6/6. Tiny
scalar reciprocals and large complex two-block failures remain unchanged under
BLOCK-INDEFINITE-RK-SOLVE-RANGE. Engineering success does not waive them.

Four relocated consumers pass 768 cases each. Eighteen strict translation units,
576 actual emitted-ABI guarded cases, four standalone headers, twelve added/zero
removed exports per ABI, ten package checks and two architecture checks pass.
The CI selector includes 1,272 processes and explicitly requires all 43 new
runtime processes. Doxygen covers 152 headers and 2,633 members without warnings.
Original fixture, style, Doxygen-input and audit-marker failures are preserved
with corrections. Installed production libraries match final producers.
Provider Fortran/BLAS internals are uninstrumented; wider admission and all
previous blockers remain.

The original inverse push's hosted 1,227-test profiles match prior failures plus
31 required inverse gates; upstream provider tests pass 111/111 each and general
CI passes. CodeQL still has 3,059 open alerts, including twelve security findings.
This is older commit evidence; the new driver delivery has no hosted credit yet.

FULL_PROGRAM_INCOMPLETE: required 2,113; reviewed 252; Reference callable-unverified
212, partial 296, not-started 1,605, verified zero. Native20 and separately selected
experimental RobustPpsvx remain distinct. Finish the owned staged/feature commit
if pending, then continue P05.required.hetrf_aa and its actual dependency chain.
The existing latest-handoff.json and active review retain exact commands and
evidence paths.

## Latest checkpoint: RK inverses integrated, required gates remain open

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is pushed `cd0d9aa`; the exact new commit is recorded
externally in `rk-inverse-delivery/commit.txt`. Inspect actual git first.
All pre-existing user instruction files, frozen records, failed attempts and commits remain
preserved. No sibling workspace or provider patch was created.

Twelve SYTRI_3/SYTRI_3X and HETRI_3/HETRI_3X routes have 240 reviewed modes and
24 public declarations. Selected A is mutable; exact-n E and own-index paired
RK pivots are immutable. All E slots are readable, including ignored NaNs.
Driver NB1 always delegates X; explicit positive NB, scalar/native INTEGER/row
packing workspace, rounded WORK and source bounds are checked. Full-width INFO,
private pivots and driver WORK writes are validated. Empty calls inspect no
numerical arrays; source-consistent singular output preserves original factors.

Sixteen final profiles execute 2,676 processes: 2,304 passes, 372 required
failures, 85,632 failed mathematical assertions, 72 failed native empty-WORK
cases and zero skips. Twelve normal/sanitizer profiles pass 182/213; four TSan
profiles pass all thirty concurrency processes. Complex range, exact Hermitian
diagonal and native CSY/ZSY/ZHE empty-WORK gates remain explicit. Initial probe,
fixture, strict and stale header-count failures remain preserved. Two shared
sanitizer profiles combine 190 preserved completed tests with 23 resumed tests
each after runtime interruption. Static TSan retains 27 terminal passes plus
three resumed passes per ABI. Both shared TSan profiles were previously
unstarted. All four composites retain unknown original whole-CTest exit status.

Four relocated consumers pass 3,360 cases each. Twenty strict TUs, four standalone
headers, 720 final direct emitted-ABI cases, warning-free Doxygen for 151 headers/
2,621 members, 24 added/no removed exports per ABI, package, architecture and
actual CI-selector checks pass. Installed libraries match Release producers.
`rk-inverse-final-audit/audit.json` binds final sources and raw evidence. ASC/tests
are instrumented; Fortran/BLAS internals are not. Wider-platform admission,
CodeQL alerts, XBLAS and all earlier numerical/programme blockers remain open.

FULL_PROGRAM_INCOMPLETE: required 2,113; reviewed contracts 246;
implemented_unverified Reference 206; in_progress 296; not_started 1,611;
verified Reference 0. Continue `P05.required.hesv_rk` SYSV_RK/HESV_RK drivers and
dependencies without repeating frozen acceptance.

## Historical recovery records

## Active work: RK inverses after pushed cd0d9aa

The actual workspace remains `feature/lapack-array-io` at pushed `cd0d9aa`.
Only the two preserved user instruction files were dirty at recovery. Completed
families and all earlier numerical failures remain frozen; no new worktree.

Twelve TRI_3/TRI_3X sources and 24 GNU LP64/ILP64 declarations are reviewed.
Each ABI passes 360 guarded small inverse cases. Sixty native workspace queries
confirm driver NB=1; twelve empty execution controls retain six required
WORK(1) contract failures per ABI. Range probes pass every storage/INFO guard
but retain 40 failed representable-inverse cases among 1,080 cases per ABI.
Native expression traces isolate complex E/E and modulus overflow. Original
failed compile/extraction/record-path attempts are preserved outside the tree.

The pre-implementation decision is retained under
`rk-inverse-prerequisite-01/public-contract-decision.json`. Twenty-four public
declarations, backend, package/header registrations and 213 runtime tests are
now implemented. Each ABI's first complete engineering run has 92/123 passes,
31 required failed processes, 7,136 mathematical failed assertions plus six
native WORK-contract failures, and zero skips. All ordinary failures are exact
Hermitian imaginary-diagonal checks; all fidelity/ABI controls pass. Full
original CTest logs were recovered before their temporary copies were replaced.

All thirty fault, thirty validation and thirty concurrency processes pass in
each ABI. Per scalar/block variant: faults have 360 cases (60 empty noncalls);
validation has 160 structural rejections, protected queries/empty noncalls and
source/stride bounds; concurrency has sixteen groups, four workers/eight
repeats, 768 worker native calls and 256 noncalls/rejections with protected E/P.
Four isolated relocated static/shared LP64/ILP64 consumers pass 3,360 cases each.
Original compile/wiring and deliberate-byte-write findings remain preserved.

Initial strict checks are running against a retained sixteen-file C++ snapshot.
Final profiles, strict/style completion, emitted ABI rebinding, exports,
standalone headers, Doxygen, final consumers if sources change and normalized
capability records remain unfinished. No inverse Reference row is promoted.
See `indefinite-rk-inverse-review.md`. FULL_PROGRAM_INCOMPLETE remains in force.

## Previous completed delivery and historical recovery

## Latest checkpoint: RK condition estimators integrated, numerical gates open

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is pushed `7c00b77`; the exact new commit is recorded
externally in `rk-condition-delivery/commit.txt`. Inspect actual git first.
Both user instruction files, frozen records, failed attempts and commits
remain preserved. No sibling workspace or provider patch was created.

Six SYCON_3/HECON_3 routes have 96 reviewed modes and twelve public declarations.
Immutable RK A/E/pivots preserve Hermitian coefficients and ignored-E semantics.
ANORM explicitly requires finite nonnegative values; RCOND uses the associated
real type. Active 2*n scalar WORK, private INTEGER and row-A packing are checked.
Empty/zero-norm calls inspect no numerical arrays. Full-width INFO and output
writes are validated; nonfinite RCOND remains a numerical warning and finite
values do not certify accuracy.

Sixteen final profiles execute 540 processes: 468 passes, 72 required numerical
failures, 2,496 failed assertions and zero skips. Twelve ordinary/sanitizer
profiles pass 37/43; four TSan profiles pass all six concurrency processes.
`BLOCK-INDEFINITE-RK-CONDITION-RANGE` preserves native tiny reciprocal and
complex estimator-sum failures. Initial fixture-capacity/compile/style failures
remain recorded; all prior programme blockers are intact.

Four relocated consumers pass 1,248 cases each. Eighteen strict TUs, four
standalone headers, 288 guarded direct emitted-ABI cases, warning-free Doxygen
for 150 headers/2,597 members, twelve added/no removed exports per ABI, package,
architecture and actual CI-selector checks pass. Installed libraries match
Release producers. `rk-condition-final-audit/audit.json` binds final sources and
raw evidence. ASC/tests are instrumented; Fortran/BLAS internals are not.
Wider-platform admission and the separate CodeQL alert gate remain open.

FULL_PROGRAM_INCOMPLETE: required 2,113; reviewed contracts 234;
implemented_unverified Reference 194; in_progress 296; not_started 1,623;
verified Reference 0. Continue `P05.required.hetri_3`, including TRI_3X inverse
dependencies, then RK drivers without repeating frozen acceptance.

## Historical recovery records

## Active work: RK CON_3 condition estimators after pushed 7c00b77

The RK solve delivery is committed and pushed as `7c00b77`; its sixteen
profiles and required range failures are frozen. Only the two preserved user
instruction files were dirty before CON_3 implementation. Existing branch and
workspace remain in use.

Six exact CON_3 sources and twelve actual GNU emissions are reviewed. Each ABI
passes 144 guarded and 144 small mathematical native controls. Range probes
retain 32 failed mathematical cases per ABI among 184 finite-norm cases, plus
eight nonrepresentable-norm controls; all 192 input/metadata/work guards pass.
Reverse-communication traces isolate complex min-normal/2 estimator sum
overflow despite representable solve components and true inverse norm.
Original failed compile attempts and corrected evidence are preserved outside
the source tree. See `indefinite-rk-condition-review.md`.

Twelve public declarations and backend integration are registered. All twelve
ordinary mathematical/fidelity processes pass 104 cases each in both ABIs.
The original ILP64 run aborted four real processes at the frozen fixture's
560-byte INTEGER capacity guard; a new CON_3-only fixture supplies 1,072 live
bytes plus guards for n=67. Original failures and source snapshots remain in
`rk-condition-workspace-finding-01`; corrected engineering records use `-02`.

Each ABI's adapter range run has seven passes and six required mathematical
failures, 208 failed assertions and zero skips. Each class executes 128 range
cases; four complex classes include eight finite-norm admission rollbacks each.
All native-fidelity processes and the 144-case guarded ABI probe pass. See
`rk-condition-range-adapter-{lp64,ilp64}-01/audit.json`. No assertion is waived.
All six fault, validation and concurrency processes pass in each ABI. Per
class, faults exercise 1,280 cases (768 empty/zero-norm noncalls); validation
has 236 rejections, 24 protected queries, 16 empty noncalls and ten source
count boundaries; concurrency has 16 groups, four workers/eight rounds,
768 native calls and 256 zero-norm/structural noncalls with read-only A/E/pivots.
Four isolated relocated static/shared LP64/ILP64 consumers pass 1,248 cases
each in `rk-condition-installed-01-*`. Six reviewed contracts/96 modes are
normalized against the original pre-implementation decision outside the tree.

Strict production and fault observer checks pass. Initial test strict checks
found include/conditional/function-size issues; original files/logs and helper
refactoring are preserved in `rk-condition-test-style-finding-01`. Follow-up
strict checks are running; installed source also requires helper extraction
and explicit byte-comparison intent. Preserve all passing first consumer runs.
Final source/profile/installed-style/header/export/Doxygen/index audits remain
unfinished. No CON_3 Reference row is yet promoted.
FULL_PROGRAM_INCOMPLETE remains in force.

## Previous completed delivery and historical recovery

## Latest checkpoint: RK solves integrated, required numerical gates open

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is pushed `1178656`; the exact new commit is recorded
externally in `rk-solve-delivery/commit.txt`. Inspect actual git first.
Both user instruction files, frozen records, failed attempts and existing
commits remain preserved. No sibling workspace or provider patch was created.

Six SYTRS_3/HETRS_3 routes have 96 reviewed modes and twelve public declarations.
Immutable RK A/E/pivots retain raw Hermitian coefficients and ignored-E
semantics. Own-index directional pivots are checked. No scalar WORK exists;
active private INTEGER and row layout packing are checked. Empty execution
is a metadata-only noncall. Full-width INFO and private pivot mutation are
validated before publishing packed B; INFO=0 has no finiteness certificate.

Sixteen final profiles yield 540 processes: 468 passes, 72 required mathematical
failures, 1,920 failed assertions and zero skips. Twelve ordinary/sanitizer
profiles each pass 37/43; four TSan profiles pass all six concurrency processes.
Native scalar reciprocal and complex large-block failures remain explicit as
`BLOCK-INDEFINITE-RK-SOLVE-RANGE`. The earlier native empty-E and all other
programme blockers remain intact.

Four relocated consumers pass 3,168 cases each. Eighteen strict TUs, four
standalone headers, 288 guarded direct emitted-ABI cases, warning-free Doxygen
for 149 headers/2,585 members, twelve added/no removed exports per ABI, package,
architecture and actual CI-selector checks pass. Installed libraries match
final Release producers. `rk-solve-final-audit/audit.json` binds source and raw
evidence. ASC/tests are instrumented; provider Fortran/BLAS internals are not.
Wider-platform admission and the separate CodeQL alert gate remain open.

FULL_PROGRAM_INCOMPLETE: required 2,113; reviewed contracts 228;
implemented_unverified Reference 188; in_progress 296; not_started 1,629;
verified Reference 0. Continue `P05.required.hecon_3`: RK condition estimators,
then remaining inverse/driver dependents without repeating frozen acceptance.

## Historical recovery records

## Active work: RK-dependent TRS_3 solves after pushed 1178656

The RK producer delivery is committed and pushed as `1178656`; its sixteen
profiles and native empty-E blocker are frozen. Only the two preserved user
instruction files were dirty before this consumer work. The same branch and
workspace remain in use.

S/D/C/Z SYTRS_3 and C/Z HETRS_3 have six reviewed pinned sources and twelve
actual GNU ABI emissions. Guarded 144-case native runs pass in each ABI.
Native range probes retain twenty mathematical failures per ABI: twelve tiny
scalar reciprocal cases and eight large complex 2-block cases, with INFO=0,
exact representable solutions and passing input/padding guards. No provider
arithmetic was changed. Raw evidence and the public decision are external
under `rk-solve-prerequisite-01`.

Twelve new query/execute declarations and a new source preserve immutable
A/E/pivots, source INFO, ignored E entries and checked row packing/integer
workspace. Both ABIs pass all twelve ordinary mathematical/fidelity processes, 496 cases
each. Each range/ABI run has seven passes, six required mathematical failures,
160 failed assertions and zero skips. All native fidelity and guarded ABI
checks pass. Fault, structural and concurrency checks now each pass six processes
per ABI with zero skips. Structural tests exercise 408 rejections per variant,
32 protected queries, 24 empty noncalls, 26 integer source-count boundaries
and 32 LP64/16 ILP64 original-stride queries. Concurrency shares protected
read-only A/E/pivots in 32 groups per variant with four workers and four repeats.
Installed consumers and final strict checks are running; final profiles,
documentation and normalized-record work remain unfinished. See
`indefinite-rk-solve-review.md`; no new Reference row is yet promoted.

## Previous completed delivery and historical recovery

## Latest checkpoint: RK producers integrated, native empty-order gate open

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is pushed `ffc4bd8`; the exact delivery commit is recorded
externally in `rk-factor-delivery/commit.txt`. Inspect actual git first.
Both user instruction files, frozen acceptance records, failed attempts and
existing commits remain preserved. No sibling workspace was created.

Twelve S/D/C/Z SYTF2_RK/SYTRF_RK and C/Z HETF2_RK/HETRF_RK routes have
120 reviewed modes and twenty-four public query/execute declarations. Typed
persistent E and global permutation/triangular storage are explicit. Existing
ROOK factor factories reject RK provenance. Native INFO and partial-factor
semantics remain intact; original Hermitian and row-major A are privately
packed. Source workspace minimum, rounding and reduced-block thresholds are
checked and exercised. Queries and empty execution are metadata-only noncalls.

Sixteen profiles yield 552 processes: 540 passes, twelve required native-empty
failures, 144 failed assertions and zero skips. Each ordinary/sanitizer profile
has 43 passes and the unchanged required empty-E failure. Four TSan profiles
pass six concurrency processes each. All ordinary and range mathematical and
native-fidelity checks pass, with explicit nonrepresentable controls.
The pinned unblocked routines write upper E(1)/lower E(0) when n=0;
`BLOCK-INDEFINITE-RK-EMPTY-E` remains open. ASC empty noncalls pass.

Four relocated consumers pass 1,440 cases each. Eighteen strict TUs, four
standalone headers, 336 active guarded emitted-ABI cases, 148-header/2,573-member
warning-free Doxygen, twenty-four added/no removed exports per ABI, package
manifest and architecture/dependency checks pass. Installed libraries match
final producers after normal CMake installation. The external
`rk-factor-final-audit/audit.json` binds final sources and all evidence.
Provider Fortran/BLAS internals remain uninstrumented; wider-platform admission
and the separate CodeQL alert gate remain open.

FULL_PROGRAM_INCOMPLETE: required 2,113; reviewed contracts 222;
implemented_unverified Reference 182; in_progress 296; not_started 1,635;
verified Reference 0. Continue `P05.required.hetrs_3`: RK dependent solves,
then condition/inverse/driver families, preserving all earlier blockers.

## Historical recovery records

## Active work: RK factor producers after pushed ffc4bd8

The actual branch remains `feature/lapack-array-io` in the existing workspace.
Recovery found pushed `ffc4bd8` with only the two preserved user instruction
files untracked; no completed work was discarded or repeated. RK changes are
now uncommitted: twelve producers and twenty-four declarations with explicit
E storage, metadata-only workspace plans and guarded output publication.

Before implementation, all twenty-four actual GNU prototype emissions and
168 active native cases per ABI passed. Twelve native empty-order cases per
ABI fail: unblocked upper writes E(1), lower E(0), INFO=0. Those raw failures
are retained, and a required native-empty CTest keeps the gate visible. ASC
empty execution is a validated noncall and leaves all numerical storage alone.

The smallest double LP64 check passes 280 mathematical cases and 280 native
fidelity cases. Both engineering ABIs pass all twelve ordinary processes.
Each range/ABI run passes thirteen processes and retains the native-empty
failure: twelve failed assertions, zero skips. Every range process executes
360 representable factor cases and twelve explicitly nonrepresentable controls.
See `indefinite-rk-review.md` and external `rk-factor-*` evidence. Fault checks
are underway; structural, concurrency, installed, strict, final profiles and
normalized documentation/evidence checks remain unfinished. Inspect actual
processes and logs before resuming. FULL_PROGRAM_INCOMPLETE remains in force.

## Latest checkpoint: SYTRS2/HETRS2 evidence complete, range gates open

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is pushed `c6eb9e5`; the new exact commit is recorded
externally in `block-solve-delivery/commit.txt`. Inspect actual git first.
Both user instruction files, frozen acceptance records and all failed attempts
remain preserved. No new workspace or provider numerical source was created.

Six classic TRS2 routes have 144 reviewed triangle/layout/origin modes.
Both layouts copy immutable raw factors into private workspace because native
SYCONV temporarily writes and restores A. Selected factor bytes and private
input pivots are checked before publishing B; full-width INFO preserves source
semantics. Active WORK is n, with no LWORK/query/returned-WORK contract.
Empty n=0 or nrhs=0 is a noncall before any numerical array inspection.

Sixteen profiles yield 540 processes: 468 passes, 72 required mathematical
failures, 1,920 failed assertions and zero skips. Each of twelve ordinary/
sanitizer profiles has six range failures. All four TSan profiles pass six
processes. Scalar reciprocal overflow and complex large-block B/OFF NaN remain
required gates; native INFO=0, factor restoration and exact byte fidelity pass.
Direct GNU Fortran probes retain both controls and failures. Linux concurrency
uses read-only shared factor pages; all structural/protected-memory checks pass.

Four relocated consumers pass 3,456 cases each across TRF/TF2/actual SV origins.
Eighteen strict TUs, four standalone headers, 288 guarded emitted-ABI cases,
147-header/2,549-member warning-free Doxygen, twelve added/no removed exports
per ABI, package-manifest and architecture/dependency checks pass. Installed
libraries match final producers after normal CMake installation. The external
`block-solve-final-audit/audit.json` binds all final sources and evidence.
Original c6eb9e5 hosted artifacts were recovered without reruns: CI/CodeQL
analysis pass; selected-family831tests/profile retain required failures and
zero skips. The separate CodeQL alert gate remains open.

FULL_PROGRAM_INCOMPLETE: required2,113; reviewed contracts210;
implemented_unverified Reference170; in_progress296; not_started1,647;
verified Reference0. Continue `P05.required.hetf2_rk`: RK unblocked factor
producers, then blocked producers/dependent consumers. Review separate E
storage, pivot encoding and actual pinned declarations before implementation.

## Historical recovery records

## Active work: SYTRS2/HETRS2 after pushed c6eb9e5

TRI2/TRI2X delivery is committed and pushed as `c6eb9e5`. Its exact external
record is `block-inverse-delivery/commit.txt`; the successful final audit is
`block-inverse-final-audit-02/audit.json`. The failed Doxygen input-list build
and incomplete first audit remain retained; the corrected Doxygen run is
`block-inverse-documentation-final-02`. Both user instruction files remain
untracked and untouched.

Six TRS2 routes and twelve declarations are now uncommitted in this workspace.
They use immutable raw classic factors/pivots and forced private A packing in
both layouts, with native restoration and full-width INFO checks. The first
double check passes 496 cases. Each engineering ABI runs 25 processes:
19 pass and six required range processes fail, with 160 failed assertions
and zero skips. All ordinary/native-fidelity and guarded ABI cases pass.
Each ABI additionally passes six fault processes, 432 cases per scalar variant.
Scalar reciprocal overflow and complex large-block failures remain visible.

See `indefinite-block-solve-review.md` and external `block-solve-*` records.
Structural, concurrency, installed-consumer, strict/final profile/documentation
and normalized evidence checks are unfinished. Inspect actual processes and
logs before resuming; do not discard files or repeat completed profiles.
FULL_PROGRAM_INCOMPLETE remains in force.

## Latest committed checkpoint: TRI2/TRI2X evidence complete, numerical gates open

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is pushed `a80a7ec`; the new exact commit is recorded
externally in `block-inverse-delivery/commit.txt`. Inspect actual git first.
Both user instruction files, frozen acceptance records and failed attempts
remain preserved. No new workspace or provider numerical source was created.

Twelve TRI2/TRI2X routes have 360 reviewed triangle/layout/origin/block modes.
Native query probes confirm scalar-specific block sizes and small-order sizes;
public queries honor the documented product. Positive INFO publishes native
converted partial factors, without a reusable-original-factor promise.
Full-width INFO, immutable pivots, workspace dimensions and source cursors
are checked. Sixteen profiles yield 2,700 processes: 2,340 passes, 360 required
mathematical failures, 74,112 failed assertions and zero skips. Each of twelve
ordinary/sanitizer profiles has twenty complex range and ten strict exactly-real
Hermitian diagonal failures. All four TSan profiles pass thirty processes.
Native ordinary/range fidelity, real mathematics, fault and structural checks pass.

Direct GNU Fortran probes show complex block overflow propagation. Direct native
HE calls reproduce small imaginary diagonals while reconstruction/residuals pass;
the exact-zero assertion is preserved. A draft concurrency oracle inherited
classic unchanged-singular behavior; its failed logs and source are retained,
and its full-byte assertion now compares with a separate serial partial result.
No frozen test, assertion or tolerance was weakened.

Four relocated consumers pass 2,880 cases each. Eighteen strict TUs, four
standalone-header tests, 600 guarded emitted-ABI cases, 146-header/2,537-member
warning-free Doxygen, 24 added/no removed exports per ABI, package-manifest and
architecture/dependency checks pass. Installed production libraries match the
final producer after normal CMake installation. `block-inverse-final-audit-02/audit.json`
binds these records. Original hosted artifacts at a80a7ec were recovered without
rerunning: CI/CodeQL analysis pass; selected-family mathematical failures remain.

FULL_PROGRAM_INCOMPLETE: required2,113; reviewed contracts204;
implemented_unverified Reference164; in_progress296; not_started1,653;
verified Reference0. Next is `P05.required.hetrs2`, classic SYTRS2/HETRS2 using
existing factor producers. Review the documented input A versus temporary native
conversion/restoration. Preliminary read-only notes are outside source. TRI3's
RK separated-E factor-storage dependencies are also unfinished.

## Historical recovery records

## Latest checkpoint: classic inverse evidence complete, complex numerical gates blocked

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is pushed `00835ce`; the exact new commit is recorded
externally in `classic-inverse-delivery/commit.txt`. Inspect actual git first.
Both user instruction files, frozen acceptance records and previous failures
remain preserved. No worktree or provider numerical source was changed.

Six SYTRI/HETRI routes have 72 reviewed triangle/layout/factor-origin modes.
Pinned C declarations and actual GNU emissions agree. Complex SY honors the
documented 2*n WORK contract; classic equal negative pivot pairs are validated.
Sixteen actual profiles yield 468 processes: 420 passes, 48 required
mathematical failures, 1,056 failed assertions, zero skips. Four complex range
processes fail in each of twelve ordinary/sanitizer profiles; all four TSan
profiles pass six concurrency processes. S/D representable-range mathematics
and all six direct native fidelity processes pass. Corrected GNU Fortran probes
confirm SY T/T produces NaNs and HE magnitude overflow yields zero inverse
entries for a large2x2 block with representable exact inverse components.

Four relocated consumers pass 432 cases each acrossTRF/TF2/SV origins.
Eighteen strict TUs, four standalone headers, 120 emitted-ABI cases,
145-header/2513-member warning-free Doxygen, twelve added/no removed dynamic
exports perABI, package-manifest and architecture/dependency checks pass.
Installed production matches final producer after CMake installation.
`classic-inverse-final-audit/audit.json` binds evidence. The inadmissible first
standalone arithmetic probe is retained separately from its corrected successor;
C++ range tests always used actual classic factors and valid workspace.
No assertion or tolerance was weakened.

FULL_PROGRAM_INCOMPLETE: required2113; reviewed contracts192;
implemented_unverified Reference152; in_progress296; not_started1665;
verified Reference0. Next is SYTRI2/HETRI2 and explicit-block TRI2X dependencies,
`P05.required.hetri2`. Their singular conversion/workspace behavior needs its own
source review; preliminary read-only observations are preserved outside source.

## Historical recovery records

## Active continuation: classic inverses in progress

Recovered commit `00835cea9b7acd884f422c6091dffa45e9d131eb` and confirmed its
push completed. Existing workspace and branch are unchanged; only the two user
instruction files were untracked before new work. All frozen records remain.
Six classic SYTRI/HETRI inverse routes are now implemented and undergoing
verification. The initial LP64/ILP64 Release selections each pass 33/37 with
four required complex range failures and zero skips; the first double-real
check passed. `indefinite-inverse-review.md` and external
`classic-inverse-prerequisite-01` record scalar/workspace/pivot/INFO contracts.
No classic delivery commit or completed-family verification is claimed yet.

## Latest checkpoint: rook inverse evidence complete, complex numerical gates blocked

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is pushed `7ed2e67`; the exact new commit is recorded
externally in `rook-inverse-delivery/commit.txt`. Inspect actual git state first.
Both user instruction files, frozen acceptance records and previous failures
remain preserved. No worktree or provider numerical source was changed.

Six SYTRI_ROOK/HETRI_ROOK routes have 72 reviewed triangle/layout/factor-origin
modes. Sixteen actual profiles yield 468 processes: 420 passes, 48 required
mathematical failures, 1,056 failed assertions, zero skips. The four complex
range processes fail in each of twelve ordinary/sanitizer profiles; all four
TSan profiles pass six concurrency processes. S/D representable-range mathematics
and all six direct native fidelity processes pass. GNU Fortran probes identify
SY T/T becoming NaN and HE ABS(T) overflowing for a large2x2 block with a
representable exact inverse. The provider blocker remains explicit.

Four relocated consumers pass 432 cases each acrossTRF/TF2/SV origins.
Eighteen strict TUs, four standalone headers, 120 final emitted-ABI probe cases,
144-header/2501-member warning-free Doxygen, twelve added/no removed exports per
ABI, all four package-manifest contexts and architecture/dependency checks pass.
Installed production artifacts match the final producer after CMake installation.
`rook-inverse-final-audit/audit.json` binds this evidence; all failed attempts
and pure helper refactors are retained. No assertion or tolerance was weakened.

FULL_PROGRAM_INCOMPLETE: required2113; reviewed contracts186;
implemented_unverified Reference146; in_progress296; not_started1671;
verified Reference0. Next is six classic SYTRI/HETRI inverse routines,
`P05.required.hetri`, using existing classic factor dependencies.

## Historical recovery records

## Active continuation: rook inverses in progress

Recovered and pushed driver commit `7ed2e670b23b624d296164457c675cc482c090c7`.
Only the two preserved user instruction files were untracked at recovery.
New work in this workspace implements six SYTRI_ROOK/HETRI_ROOK inverse APIs;
no inverse acceptance or delivery commit is claimed yet. The public mutable
factor contract, exact GNU ABI emissions and source review are retained in
`indefinite-rook-inverse-review.md` and external `rook-inverse-prerequisite-01`.
The first build failure was a test setup using a nonexistent context factory;
`rook-inverse-first-01` remains intact. The test now uses the existing Serial
context factory. All prior milestones and numerical blockers remain frozen.

## Latest checkpoint: rook driver evidence complete, numerical gate blocked

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is `9d2f65b`; the exact new commit is recorded externally
in `rook-driver-delivery/commit.txt`. Inspect actual git state first. Both user
instruction files, frozen milestones and previous failure evidence are preserved.

Six SYSV_ROOK/HESV_ROOK routes have 288 reviewed modes. Sixteen actual profiles
yield 468 canonical processes: 396 passes, 72 required mathematical failures,
6,912 failed assertions and zero skips. Four relocated consumers pass 768 cases
each; twenty strict TUs, four standalone headers, actual emitted ABI probes,
Doxygen, exports and all four package header-manifest contexts pass.
`rook-driver-final-audit/audit.json` binds the evidence. No numerical failure
or tolerance was waived. Successful actual driver reports permit rook factor
reuse without fabricating a factorization origin.

Hosted 9d2f65b artifacts were recovered without reruns. The missing condition
source in the independent compiled-source oracle and unresolved Doxygen links
are fixed through explicit registrations. Architecture/dependency and strict
Doxygen checks now pass locally. Selected hosted families retain 497 tests per
profile: LP64 424 passes/73 failures, ILP64 428 passes/69 failures, zero skips.
CodeQL analysis succeeded; the separate alert gate and wider admission remain open.

FULL_PROGRAM_INCOMPLETE: required 2113; reviewed contracts 180;
implemented_unverified Reference 140; in_progress 296; not_started 1677;
verified Reference 0. Next unfinished family is six SYTRI_ROOK/HETRI_ROOK
inverses, with existing factorization dependencies. Continue in this workspace.

## Historical in-progress recovery record

## Active continuation: rook drivers implemented, verification in progress

Recovered actual branch `feature/lapack-array-io` at pushed commit `9d2f65b`.
The two untracked user instruction files remain unchanged. Current uncommitted
work adds six SYSV_ROOK/HESV_ROOK drivers and admits their actual successful
reports for rook factor reuse. It also registers the independent 143-header
oracle. No worktree or provider change was made.

Evidence remains under `master-continuation-20260910-01/continuation-20260912-01`.
`rook-driver-prerequisite-01` retains source contracts and actual GNU compiler
emissions; both executed ABI probes pass 288 cases. Initial LP64/ILP64 Release
runs each pass 31 of 37 tests with six required mathematical failures and no
skips. Native-output faults, structural rollback and concurrent calls pass.
Tiny scalar solves reproduce the pinned reciprocal-overflow limitation while
factor-only and native-fidelity checks pass. All failures remain required.

The static LP64/ILP64 relocated consumers pass. Strict style, shared consumers,
remaining profiles, final documentation and evidence mapping are in progress.
Do not commit or infer full family acceptance from this provisional record.
`rook-driver-first-01` preserves an initial missing generated Make target;
explicit reconfiguration then enabled the first successful double test.

The original fb20ed9 hosted artifacts have now been audited without reruns:
458 tests per profile, LP64 391 passes/67 failures, ILP64 395 passes/63 failures,
zero skips; each provider ran 111 checks successfully. See
`rook-hosted-family-recovery-01/audit.json`. Frozen milestones and all earlier
blockers remain unchanged. Overall status remains FULL_PROGRAM_INCOMPLETE.

## Prior committed checkpoint

## Latest checkpoint: rook condition evidence complete, numerical gate blocked

Workspace `asc-cpp-lapack-array-io`, branch `feature/lapack-array-io`.
Implementation parent is `fb20ed9`; the exact new commit is recorded externally
in `rook-condition-delivery-02/commit.txt`. Inspect actual git state first.
Both user instruction files and all earlier evidence remain preserved.

Six SYCON_ROOK/HECON_ROOK routes have48 reviewed modes and implemented_unverified
records. Sixteen profiles yield468canonical processes:396pass,72required
mathematical failures,6336failed assertions,zero skips. Four installed consumers,
eighteen strict TUs, guarded LP64/ILP64 direct ABI probes, Doxygen and exports
pass. The corrected independent142-header oracle passes all four package
contexts. `rook-condition-final-audit/audit.json` binds results and targeted
fixture corrections without discarding the failed attempts.

FULL_PROGRAM_INCOMPLETE: required2113, mapped contracts174,
implemented_unverifiedReference134, in_progress296, not_started1683,
verifiedReference0. The frozen Native20, array-I/O and robust PPSVX milestones
remain separate and unchanged. All previous numerical and release blockers
remain, with `ROOK-INDEFINITE-CONDITION-RANGE` added explicitly.

Next unfinished family: six required SYSV_ROOK/HESV_ROOK drivers.
Continue in this workspace. Completed hosted runs were recovered without reruns;
`fb20ed9` CI's stale header count is fixed locally, CodeQL analysis succeeded,
and the failed selected-family run still needs its original artifact audit.

## Historical recovery before rook condition checks completed

Recovered branch `feature/lapack-array-io` at `fb20ed9`; tracked tree was clean
and both user instruction files were preserved untracked. Six SYCON_ROOK /
HECON_ROOK routes, contract review, tests, installed consumer and documentation
are now being verified in this same workspace. No new worktree was created.
The existing PPSVX/native20/array-I/O records remain frozen.

Evidence is under `master-continuation-20260910-01/continuation-20260912-01`.
`rook-condition-prerequisite-01` has six source contracts, twelve compiler
emissions and both executed 48-case ABI probes. Ordinary/fault/validation
checks pass. Six independent range gates remain failing with direct native
agreement. Profile runs and installed-consumer checks are still in progress;
no new completion or verification claim is made here.

Two new test-fixture corrections are recorded explicitly: condition concurrency
needs simultaneous pivot/IWORK capacity for real ILP64 order67, and the empty
installed example needs logical zero pivot length with valid backing metadata.
Failed original logs and source copies are retained. Rerun only affected cases
or profiles that never reached tests. The prior pushed CI failed because the
independent header oracle still expected140; current explicit142-header oracle
passes all four package contexts. `rook-hosted-read-01` preserves that finding.

# LAPACK and array-I/O programme recovery

Latest continuation: the eighteen rook factor/solve routines are implemented
with 168 reviewed modes. [The rook record](indefinite-rook-review.md) binds
sixteen actual profiles: 552 passes, 72 required mathematical failures and
zero skips, plus four relocated consumers and the strict/header/export checks.
The six tiny scalar-solve cases remain blocked by provider reciprocal-before-
SCAL arithmetic. Existing classic, PT and other range failures are preserved.
Metadata, public surface, documentation and CI selector checks pass on parent
`a0ff67a`; consult actual git status and `rook-delivery/commit.txt` in the
continuation evidence directory before repeating any work.
The next independent task is six SYCON_ROOK/HECON_ROOK condition routines.
Full programme status remains **FULL_PROGRAM_INCOMPLETE**.

## Prior classic and initial rook checkpoints

Latest continuation: classic indefinite contracts now cover eighteen routines
and 168 modes. [The range record](indefinite-continuation-review.md) retains
twelve mathematical failures and twelve provider-comparison passes per actual
profile, with zero skips. The completed [INFO correction](indefinite-info-review.md)
supplies 276 unchanged engineering passes and four installed consumers.
Classic indefinite concurrency now passes 96 new tests across sixteen actual
profiles, including TSan, with zero skips. The next task is rook factor/solve
implementation after the retained source, pivot and ABI review.
Full programme status remains **FULL_PROGRAM_INCOMPLETE**.

## Active rook continuation

The eighteen rook API/backend routes are now present as uncommitted work on
`a0ff67a`, with [the live rook record](indefinite-rook-review.md). The same
commit was pushed successfully; all prior evidence and user-owned untracked
instruction files remain. Both initial expanded Release profiles pass 44/50,
retaining only six reciprocal-overflow solve gates and 1,152 failed assertions.
Direct ABI declarations and the corrected pinned GNU NaN behavior are recorded
in `rook-emitted-abi-*-01`. Continue strict checks, expanded profiles, installed
consumers and the atomic coverage/evidence update; do not restart prior work.


## Checkpoint recovered on 2026-09-12

Status: **FULL_PROGRAM_INCOMPLETE**.

The active workspace is `/home/yicai/AI4SciComp/asc-cpp-lapack-array-io`,
branch `feature/lapack-array-io`, recovered at
`6fd7c90185a5af60443b23fcae2f05d525294681`. The preceding recovery checkpoint
is [the PT review](positive-tridiagonal-review.md#recovery-checkpoint-2026-09-12).
All 15 preexisting modified/untracked files remain present. The index was
empty; no previous source changes were discarded or committed by recovery.
No live compiler, CTest or previous programme runner was found.

The retained native acceptance, array-I/O acceptance and experimental robust
PPSVX integration remain frozen. PTTRF/PTTRS is implemented at `fb8c86f`;
the subsequent PTCON, PTRFS, PTSV, PTSVX, GECON, GEEQU/GEEQUB and GERFS
commits are present. Their mathematical and profile limitations remain in
their family reviews and [the decision record](owner-decisions.md).

## Interrupted work

The dirty implementation slice is GESVX verification, with two maintained
test sources, CMake registration, workflow selectors, contract/evidence
extensions and programme records. Its frozen source identity is tree
`aa4012465076f33457b79f897b1761992be7c67f`, based on `663c7a4`.
The existing Release and Debug shared executions completed for both ABIs,
retaining four numerical failures; both shared TSan executions completed
successfully. Neither shared ASan/UBSan profile has started. The shared
audit and coherent delivery commit remain unfinished. Completed profiles
must be reused, not restarted because the connection was interrupted.

The recorded inventory has 2,113 required Reference rows: 80 implemented
but unverified, 322 in progress and 1,711 not started; zero Reference rows
are fully verified. Native20 retains its separate accepted scope. These
counts come from the recovered records and require validator confirmation
when the next contract update is finalized.

## Active continuation

1. PTTRS reproduction is complete; the provider numerical blocker below is
   preserved. Continue independent work rather than repeating this fixture.
2. Reconcile the requested structured families against actual pinned rows and
   existing implementations. `SBTRF/SBTRS/HBTRF/HBTRS` have no rows in the
   pinned inventory; do not invent those interfaces.
3. Finish the interrupted GESVX verification without rerunning its completed
   profiles, then continue dependency-ready structured/general solver work,
   P06-P09, remaining P10 obligations and continuous P11 delivery.
4. Keep unresolved mathematical, provider, dependency, license and platform
   gates explicit while implementing independent required families.

New raw evidence stays under the existing external root:
`asc-cpp-evidence/lapack-array-io/master-continuation-20260910-01/continuation-20260912-01`.
`initial.json`, `initial-status.z` and `initial.patch` preserve this recovery's
starting identities. No sibling worktrees or new project roots were created.

## PTTRS numerical disposition

Fresh builds of the maintained PT test and direct-comparison targets completed
in the existing Release LP64 and ILP64 builds. Each selected five-test run
has one passing direct-provider comparison, four failed required mathematical
processes, 48 failed assertions and zero skips. Exact commands, current source
hashes, configured test IDs, raw logs and JUnit are in
`continuation-20260912-01/pt-reproduction-{lp64,ilp64}`. No ordinary or frozen
acceptance subset was rerun.

Classification: **pinned provider arithmetic; missing range-safe scalar solve**.
All four PTTS2 implementations evaluate `1 / D(1)` before xSCAL, CSSCAL or
ZDSCAL for N=1. With A=B equal to the smallest or twice-smallest positive
subnormal, that intermediate reciprocal overflows even though the system's
condition is one and its exact solution is one. The real provider returns
Inf; the complex provider returns Inf/NaN. Native INFO is zero. The unchanged
ASC implementation reports those values with its documented accuracy warning;
the independent direct call reproduces them. PTTRF succeeds on these inputs.

This is neither an incorrect Hermitian/UPLO mapping nor an invalid numerical
oracle. A range-safe provider correction or an explicitly distinct algorithm
could repair the mathematical result, but the current checked Reference route
promises the selected provider's behavior without hidden scaling or replacement.
The pinned provider and that public contract remain unchanged. Numerical
acceptance is blocked; the existing required tests and decision record remain
active. GESVX and the independent structured-solver work continue.

## Continued implementation checkpoint

GESVX's two missing shared sanitizer profiles are now complete. The original
eight-profile shared audit passes: six profiles each retain four mathematical
failures among 21 tests, and both TSan profiles pass four of four, with zero
skips. See [the family review](lu-driver-review.md#shared-completion-after-transport-recovery).
No GESVX production code, API or numerical requirement was replaced.

The next active structured-solver correction is PBTRF/PBTF2/PBTRS native INFO
publication. Both actual ABIs reproduced false success when the provider
omits INFO; ILP64 also reproduces a partial-width zero write. The two production
INFO seeds now use the full-width minimum sentinel. Both Release suites pass
13/13 with the additive regression checks. The correction's static Debug,
static sanitizer and shared Release suites pass 15/15 per ABI; all four
relocated installed consumers pass. Strict checks and the atomic
header/coverage update also pass. See [the bounded correction record](band-info-review.md).
Full PB family verification remains open. GBTF2 is the next unfinished
implementation; the four required rows have no existing public implementation.

## GBTF2 continuation delivered locally

The four GBTF2 APIs now exist with source-derived private ABI declarations,
checked workspace and pivots, exact routine provenance, GBTRS reuse, maintained
tests and installed consumers. Eight static Debug/Release/sanitizer and shared
Release profiles have 33/37 passes, with four mathematical failures and zero
skips each. The pinned provider's reciprocal-before-SCAL arithmetic produces
a nonfinite multiplier for a tiny scaled identity whose exact multiplier is
zero. The direct provider comparison agrees; the failing assertions remain.
All four relocated installed consumers and the additive export audit pass.

See [the GBTF2 review](lu-band-unblocked-review.md). The inventory records four
new reviewed in-progress contracts: 80 implemented-unverified, 326 in progress,
1,707 not started, and zero fully verified Reference rows out of 2,113 required.
Status remains **FULL_PROGRAM_INCOMPLETE**. Next: complete the existing
GBTRF/GBTRS normalized contracts and their inherited range disposition, reusing
the ordinary and failure evidence just collected.

## GBTRF/GBTRS contract checkpoint

The existing eight APIs and production source remain unchanged. Their 28
normalized modes now have source-derived contracts. Eight new range profiles
pass 8/12 each, with four GBTRF mathematical failures and zero skips; both
factor branches overflow a reciprocal before scaling an exact zero multiplier.
Scalar GBTRS N/T/C/layout cases pass at the same scales. All four matching
relocated default consumers pass on the existing packages. See the
[continuation record](lu-band-continuation-review.md). The reviewed-contract
count is now 120; all Reference route state counts and the incomplete programme
status remain unchanged. PBTRF/PBTF2/PBTRS normalized contracts are next.

## PB contract and range checkpoint

The twelve existing PBTRF/PBTF2/PBTRS APIs retain the committed INFO correction.
All 64 typed modes now have reviewed contracts. Eight new range profiles pass
4/4 each with zero skips; 116 matching engineering passes and four matching
relocated installed consumers are reused by an exact dependency audit. See
[the continuation record](band-cholesky-continuation-review.md). Reference
counts are 92 implemented-unverified, 314 in progress, 1,707 not started and
zero fully verified; 132 contracts are reviewed. Status remains
**FULL_PROGRAM_INCOMPLETE**. Continue the missing PB concurrency/profile gates,
then the existing classic indefinite families.

## PB concurrency and remaining Linux profiles

The local PB matrix now passes 23/23 in twelve static/shared Debug, Release
and ASan/UBSan profiles, plus 4/4 in four TSan profiles, across both actual
ABIs. This composes 144 new and 148 explicitly matched earlier process results,
with zero skips; all four existing relocated consumers remain matched. The
[PB continuation](band-cholesky-continuation-review.md) records the exact scope.
The 18 existing classic indefinite APIs are next; both foreign call sites still
initialize INFO to zero and require additive fault reproduction.

The recovered remote GERFS workflows completed while the session was absent.
Their numerical failures remain preserved. The separate CodeQL alert check
fails even though its analysis job succeeded; see the
[CodeQL recovery record](codeql-recovery-review.md). The additive optional
LAPACK analysis jobs require actual hosted execution. No frozen acceptance
was rerun or alert dismissed. **FULL_PROGRAM_INCOMPLETE** remains accurate.

## Block inverse continuation

Recovered at `a80a7ec7fbb7465b19fdb7c336af561f35741847`, synchronized with
`origin/feature/lapack-array-io`; no tracked edits were lost. The two user
instruction files remain untouched. TRI2/TRI2X contract review and native
workspace query probes are under the existing continuation evidence directory,
`block-inverse-prerequisite-01`. The corrected probe passes 72 cases per ABI;
failed compiler/command setup attempts remain recorded. The next implementation
is S/D/C/Z SYTRI2/SYTRI2X and C/Z HETRI2/HETRI2X. This is an in-progress
checkpoint, not numerical acceptance. `FULL_PROGRAM_INCOMPLETE`.

## 2026-09-13: active Aasen producers after pushed RK drivers

HEAD is `7dd45816ff3bdd4715223f2cc2f92fc230ea3c49` on the existing feature
worktree/branch. The RK inverse and driver deliveries are committed and pushed;
PR #47 is updated. The current writer has implemented six single-stage
SYTRF_AA/HETRF_AA producer routes and their public header, source/build surface,
maintained tests and installed consumer locally. These changes are uncommitted.
The three pre-existing user instruction files remain untracked and untouched.

Current task is `P05.required.hetrf_aa`. [The maintained review](indefinite-aasen-review.md)
records exact source contracts and the first numerical disposition. Six source
contracts/72 option modes and twelve actual GNU declarations are reviewed;
native prerequisites pass288 cases per ABI. Initial ordinary suites pass13/13
and engineering suites pass18/18 per ABI, zero skips. Range suites retain6
mathematical failures and6 fidelity passes per ABI; all323952 failed assertions
per ABI are in full verbose logs, while initial JUnit payloads were truncated.
No failure is disabled, inverted or waived. `AASEN-FACTOR-RANGE` joins the
existing owner packet without recording approval of a numerical strategy.

Strict analysis is finding ordinary include/style issues, retained in
`aasen-factor-strict-01`. Repair those, finish the guarded ABI probe and public
installed consumer, run the finite sixteen profiles and required integration
checks, then normalize evidence and commit/push owned changes. Continue the
inventoried Aasen consumers after delivery. The existing local
`rk-inverse-profile-recovery-01/latest-handoff.json` is the exact live command
record; do not restart RK or repeat completed prerequisite tests blindly.

On pushed7dd4581, general CI and CodeQL analysis succeeded, while selected
LAPACK workflows were still running at the latest read. The alert API reports
3093 open alerts, including the same12 security findings (4critical/8high),
with current source hashes preserved in `aasen-factor-remote-01`. None is
dismissed or treated as closed by successful analysis. Current integrated
coverage remains required2113, callable-unverified212, in-progress296,
not-started1605, verified Reference0; reviewed integrated contracts252;
native20 stays20/20. The uncommitted Aasen work has no normalized row promotion.
