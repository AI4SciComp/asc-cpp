# LAPACK and array-I/O programme recovery

Latest continuation: classic indefinite contracts now cover eighteen routines
and 168 modes. [The range record](indefinite-continuation-review.md) retains
twelve mathematical failures and twelve provider-comparison passes per actual
profile, with zero skips. The completed [INFO correction](indefinite-info-review.md)
supplies 276 unchanged engineering passes and four installed consumers.
The next task is independent classic indefinite concurrency, then rook routes.
Full programme status remains **FULL_PROGRAM_INCOMPLETE**.

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
