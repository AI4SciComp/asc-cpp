# Checked GEDMD snapshot analysis

This P09 slice implements SGEDMD, DGEDMD, CGEDMD and ZGEDMD through eight
public query/execute declarations. GEDMDQ remains a separate unfinished slice.
The Reference provider remains pinned to LAPACK3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, source tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`.
Only the existing Linux x86-64 GNU11.4 static, actual LP64 and true global ILP64
provider profiles are exercised. This implementation review is not owner or
wider-platform approval. No upstream numerical algorithm is copied or patched.

## Source, API and report review

The accepted operation is the pinned GEDMD snapshot algorithm, with four
explicit scaling policies, four explicitly selected SVD algorithms, three
vector modes, three extra-output modes, legal residual selection and rank
selectors -1, -2 or a maximum1..N. M>=N is required. The adapter constructs no
unknown full operator. Interpretation as approximating A requires Y=A*X.
Singular values and residuals refer to the source-selected scaled snapshot
model. Finite input alone does not guarantee convergence or finite results at
arbitrary extremes; provider normalized-range assumptions remain visible.

Each matrix has independent row/column layout and padding. The API requires
full capacities for X,Y,Z,B,W,S and n-entry eigenvalue, singular-value and
residual buffers even when an output is inactive. Real eigenvalues are exposed
as complex pairs without ordering changes; real mode columns preserve the
native conjugate-pair representation and joint normalization. Factored modes
are X(:,1:k)*W(1:k,1:k). Exact-mode B is A*U*W, without division by eigenvalues.
S is raw overwritten eigensolver state, not a Rayleigh quotient certificate.

Queries inspect metadata only, without a native workspace query or numeric
read. Plans bind the provider, scalar, dimensions, all layouts/strides, rank
selection and exact tolerance bits. Every input and output is staged in live
typed caller-owned workspace. Output publication is bounded by returned rank:
X leading k POD columns, requested Z/B columns, eigenvalues and residuals,
defined W and raw S k-by-k blocks. All n singular values publish. Y publishes
leading k residual columns when requested, otherwise all scaled snapshots.
Inactive output, tails and padding remain unchanged. Nonfinite X/Y or allzero
X preflight rejects without native entry; allzero means every X entry is zero.

INFO and rank use full-width sentinels. INFO0 publishes success. INFO4 publishes
defined warning output with kAccuracyWarning and non-OK numerical status.
INFO2/3 preserves caller numeric output and rank, and reports nonconvergence.
Missing or impossible INFO/rank is a provider defect, without publication.
Local N=0 sets rank zero with absent native INFO and no numeric access. Aliases
involving operands, workspace, rank, report, provider and plan are rejected
according to the existing metadata-before-reset contract. There is no implicit
allocation, transfer, synchronization, fallback or provider selection.

The adapter makes O(M*N+N*N) packing/publication passes and reserves
4*M*N+2*N*N staging scalars (plus n complex eigenvalues in complex APIs), 3*n
real scratch entries, and independently checked native WORK/RWORK/IWORK.
The selected provider SVD/eigen algorithm determines the numerical cost; no
constant-cost or performance claim is implied.

## Bounded resource proof

Private first-party count helpers evaluate the actual pinned integer/resource
expressions; they do not implement provider numerical algorithms. They keep
mathematically sufficient capacities distinct from native integer expressions
and source-rounded floating query values. The provider's S/C versus D/Z
rounding distinctions are preserved, including upward D/Z GESDD queries.
The n=16777217 float rounding-loss and n=50000003 double GESDD upward-rounding
regressions use pure checked arithmetic, not fabricated containing arrays.

Selected GESVD/GESDD/GESVDQ/GEJSV, GEEV and nested QR/Hessenberg/Schur workspace
expressions are checked before native entry. Execution checks also cover
foreign loop terminals, staging matrix cursors, integer quadratic products,
the tall-GESVD conditional threshold and all selected-rank eigensolver paths.
The existing GELSD small-leaf restriction is not blindly applied to GESDD's
full explicit-vector work arrays. Spare caller WORK capacity is capped at
the checked preferred count before conversion to the foreign integer type.

Source identities, scalar differences and formula notes are retained in
`master-continuation-20260910-01/dmd-source-review-01`,
`dmd-counts-source-audit-01` and the exact per-execution input manifests.
Live query comparisons cover320 GEDMD,12 GESVD and18 GEEV calls per scalar and
ABI. All arrays and object lifetimes are real and valid during those queries.
The larger integer-only boundary cases establish arithmetic rejection, not
execution at those enormous dimensions. Provider code is not instrumented.

## Maintained tests and completed candidate execution

The real-provider matrix contains2304 serial cases per scalar: all four SVDs,
four scalings, legal vector/extra/residual combinations, three rank policies,
minimum/preferred WORK and complementary independent matrix layouts. The
nonsquare m4,n3 fixture has exact rank2 and a known full operator: real
eigenvalues1+/-2i or distinct genuinely complex2-by-2 dynamics. Independent
checks cover POD orthogonality, eigenpair/operator residuals, scaled Frobenius
and singular-value identities, exact/refinement modes, inactive tails and
padding. Separate rank1 and native inconsistent-column warning cases check
the actual returned projected model. No tolerance or numerical requirement is
relaxed. A public full-rank diagonal fixture is maintained separately.

Each scalar test also runs four real-provider threads with independent
contexts, inputs, workspace, rank and reports, reusing matching immutable
parent-prepared plans. Different input scales distinguish each worker's
singular values. Global allocation-audit counters are excluded from concurrent
calls; serial calls still audit allocation. Fault wrapping state is thread
local. Four signature checks include the actual hidden Fortran string lengths.
Exact one-byte-short regions, stale plans, metadata aliases, numerical
preflight and omitted/narrow/invalid INFO/rank are required checks.

The Linux observer protects initialized containing arrays with PROT_NONE until
interception at actual GEDMD entry restores access. Each run includes128 active
calls,16 local empty calls,896 intentional forbidden output-read controls and
256 legitimate X/Y input-read controls. Every calibration child must terminate
with SIGSEGV; child-only PR_SET_DUMPABLE avoids WSL crash-collector delay.
There is no unsafe signal-handler recovery. Queries and stale/short-workspace
paths execute while numeric pages are protected. The observable region is
seven caller output arrays before native entry; rank/report scalar objects
and uninstrumented provider code are outside its scope. Correct final output
and ASan alone are not substituted for this calibrated observation.

All six `dmd-counts-{debug,release,sanitizer}-{abi}-full-17` runs pass11/11,
configured IDs254–264, zero skips. Each preserves its exact source inputs,
compiler/link settings, selected test names, raw logs, JUnit and exit codes.
These executions compile the candidate source directly into test executables;
they are not installed-library evidence. Strict resource, product and test
checks pass in `dmd-adapter-style-01` (counts), style02 (product), and style03
(all four maintained test translation units). Earlier compile/style findings,
the ineffective modes12 extension attempt and their repairs remain retained.

## Integrated Linux/static delivery

All six `dmd-counts-{debug,release,sanitizer}-{abi}-integrated-18` executions
pass12/12, IDs254–265. The ordinary test names retain `candidate_s/d/c/z` for
selector continuity, but these binaries now link the maintained library source.
Both TSan integrated18 profiles pass4/4 real-provider concurrent scalar tests;
ASC/tests are instrumented, the Fortran provider is not. Both Debug headers20
runs pass normal/no-exception public-header tests806/807.

The public consumer's two declaration-style findings are repaired without
changing assertions, and strict `dmd-consumer-style-02` passes. All six
consumer-final19 repeats pass. Its earlier exact source remains preserved in
`dmd-consumer-repair-01/main.before.cc`. Strict documentation in
`dmd-documentation-01` passes133 public headers/2313 documented members with
zero warnings. Both `install-dmd-{abi}-01` runs pass all11 commands: actual
candidate install, relocation, public-only configure/build/execution, installed
surface/runtime inspection and provider-free Dense dependency isolation.

Both `dmd-integration-{abi}-01` runs retain7/8, with full package tests passing
in206.479 seconds LP64 and206.162 seconds ILP64, under the original300-second
limit. The only failure was the independent header oracle's stale132 count:
the new header was already in its explicit list. All three count/diagnostic
occurrences now say133; the exact failed configured commands pass in
`dmd-header-count-repair-02`. Attempt01 stopped at an assertion before editing.
The seven other gate inputs are unchanged; their completed results are reused.
The frozen source tree `33794ba02726b6bf5f7f76ffa88d79bd638a323a` then passes
15/15 in each `dmd-fresh-{abi}-01`: numerical/validation/observer/public/header
and full package tests, zero skips. Package execution takes215.144 seconds
LP64 and215.22 seconds ILP64. The unchanged300-second timeout applies.
`dmd-executed-input-comparison-01` binds integrated adapter/test inputs to the
frozen source and establishes that all903 prior C++ product/header/test files
are unchanged. Final review/state edits are narrative only and separately
compared; they do not imply hosted execution of the later commit.

`dmd-record-checks-01` passes scoped coverage, backlog generation, whitespace
and all changed C++ formatting checks. Four GEDMD routines have576 legal mode
cases each and are callable-unverified; GEDMDQ has no registration credit.
Counts are2113 required,36 callable-unverified,338 partial,1739 not started,
0 verified and60 reviewed contracts:374 registered Reference routines total.
The compact native20 mapping extension preserves its exact execution commit,
tree, contracts, artifacts and prior evidence; it is not later whole-tree credit.
Native20/I/O, experimental first-party robust PPSVX and all existing required
mathematical failures retain their separate acceptance dimensions.

## Next existing family: GEDMDQ

The bounded direct-provider probe `dmdq-prerequisite-02` executes32 cases per
scalar/ABI with m5,n4 trajectory snapshots, diagonal eigenvalues2 and3 (or
2+i and3-i), all four SVD choices, representative scaling/vector/extra modes,
minimum/preferred workspace and returned Q/R reconstruction. Each ABI retains
one failed process:8 single-real JOBZ=Q cases fail, the other120 cases pass.
SGEDMDQ source line703 omits WNTVCQ when choosing the nested GEDMD JOBVL.
For JOBS=Y, JOBZ=Q, JOBF=E, SVD1 minimum WORK, INFO=0 and K=2, the returned
Q*Z columns have norm squared55.4138239477 and1.71089796129, residuals
5.9006946294 and1.30790053069 instead of normalized eigenvectors.
The same unchanged assertions fail in actual LP64 and true ILP64. These are
one source cause across eight execution variants, not eight independent bugs.
The first probe stopped after the LP64 failed process; it did not execute ILP64.

This does not affect GEDMD: its requested vector modes are selected correctly
and pass the independent equations. GEDMDQ's full source/count/ownership
contract and maintained adapter/tests remain actionable, with this genuine
provider failure retained as a required gate. The provider cannot be patched
or the documented mode silently replaced under current authorization. The
precise strategy decision is in the maintained owner packet.
