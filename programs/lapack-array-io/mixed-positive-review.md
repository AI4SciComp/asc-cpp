# DSPOSV/ZCPOSV: checked mixed positive-definite solves

This slice adds two actual pinned Reference routines and four query/execute
functions in `lapack_mixed_positive.h`. It reuses the existing mixed statistics
and explicit workspace/factor-view architecture. It is first-party adapter code;
no upstream algorithm, provider, dependency, floating-point setting or numerical
requirement changes. General mixed solves remain a separate completed slice.

## Reviewed source and API

A's selected triangle defines the symmetric/Hermitian system. B is immutable;
A/B/X layouts are independent. Real column-major A is direct; row-major A packs
only the selected triangle. Every complex A explicitly packs that triangle and
zeros imaginary diagonals. ZCPOSV's parameter documentation says imaginary
diagonals need not be set; its ZLAT2C helper nevertheless tests/narrows their
numeric values. The existing first-party `PackTriangle` real-diagonal support
avoids creating this input dependence, without altering the provider.

Low-factor success preserves caller A exactly, including ignored storage.
Working fallback publishes the selected Cholesky triangle. INFO>0 reports the
nonpositive leading minor and publishes documented partial factors, retaining
old imaginary diagonals only after native return; caller X stays unchanged.
Invalid/unwritten/partial-width INFO or ITER withholds packed A and X. Direct A
writes survive provider defects. Staged X starts with live NaN sentinels and
publishes only after valid INFO=0/ITER. Unwritten/nonfinite computed X remains
visible as an accuracy warning; no clamp or fabricated estimate is used.

Successful working fallback produces the existing Cholesky family report and
supports `LapackCholeskyFactorView`/`Potrs` reuse with matching provider and
triangle. Low factors live only in explicitly supplied lower scratch and do not
certify caller A. Shared `LapackMixedSolveStatistics` comments now describe the
selected factor precision generally; values and layout are unchanged.

Query binds scalar pair, triangle, all layouts/strides, N/NRHS and provider
identity. Rejection occurs before numeric mutation, preserving old statistics;
local N=0 has no numeric reads/native INFO/ITER. N>0,NRHS=0 still enters norm and
factor work. Counts check N*N+1, N/NRHS loop terminals and the pinned POTRF block
increment64. DSPOSV WORK includes N norm entries even with no RHS; both lower
workspaces keep one live extra scalar for the inactive SX actual address.
There is no pivot workspace, allocation, transfer or copied dense algorithm.

ITER=0..30 reports native low success/refinement; -1/-2/-3/-31 retain the actual
implementation/conversion/low-factor/iteration-limit fallback reasons. -1 is a
labeled synthetic mapping control because the pinned DOITREF is true. ITER is
not a condition or forward-error certificate; RCOND/FERR/BERR are not inferred.
The actual threshold uses a working residual and norm product, whose extreme
range limitations are preserved. Cost is O(n^3), O(n^2*NRHS) per refinement
(up to30), a possible working second factorization, and O(n^2+n*NRHS) workspace.

## Finite tests and numerical oracles

Both scalars, both triangles and all eight independent A/B/X layouts cover
n=0,1,3,9 and NRHS=0,1,3. Ordinary Hermitian fixtures have diagonal4+i and
nontrivial complex off-diagonals with strict diagonal dominance (even the
conservative CABS1 row-sum bound is below2.845 for n<=9). Known solutions and
original rounded B drive independent forward/residual checks at128 double
epsilons. A=2I is retained; A=4I is an additional exact-factor initial-low control.
Scale2^150 exercises native conversion fallback. All ignored triangles contain
sentinels; complex imaginary diagonals contain maximum finite double, which
would wrongly trigger ZLAT2C range fallback if admitted as numerical inputs.

The two-by-two low-definiteness-collapse fixture is
A=[[1,1],[1,1+2^-30]]. Its exact infinity condition is
(2+2^-30)^2/2^-30, approximately2^32; its known-solution rounded RHS is checked
by independent backward residual, without a condition-independent forward
claim. The all-ones nonpositive fixture and Hilbert8 iteration-limit fixture
are separate. Hilbert8 also uses backward residual because of its conditioning.
These are explicit new positive-family oracle choices, not tolerance changes to
previous general-family or Reference PPSVX/PT tests. Scalar A=B uses denorm_min,
twice denorm_min, minimum normal and maximum finite, with exact X=1 and the
unchanged128-epsilon analytic assertion.

Tests retain exact undersized active workspaces, metadata/stale triangle/stride,
aliasing, alignment, guards, no-allocation calls and independent four-thread
provider runs. Fault state is thread-local; ordinary concurrency uses the real
provider. Two exact native signatures, including GNU hidden character length,
are asserted in the wrappers. Linux observation protects valid initialized X
arrays until wrapped foreign entry, with intentional forked pre-entry reads
that must signal SIGSEGV. Queries, stale/short rejection and local N=0 run with
all numeric arrays protected. No signal-handler resumption or foreign-code
instrumentation is claimed.

The maintained installed example uses exported ASC/provider targets, ordinary
well-conditioned independent fixtures, both triangles/layouts and working
Cholesky reuse for a second RHS. It needs no historical evidence path or private
header. A separate provider-free consumer checks runtime isolation.

## Execution records and preserved attempts

Raw commands, exact configured test IDs, statuses, source snapshots and JUnit
are under `master-continuation-20260910-01/`. Prepared provider/compiler settings
remain their own actual LP64 and global ILP64 profiles. Provider identities and
pinned source commit/tree are the unchanged ones in `mixed-general-review.md`.
`positive-prerequisite-01` compiled and ran both actual native ABIs, reproducing
real refinement, conversion, low-factor and iteration-limit branches.

Attempt `positive-debug-lp64-01` configured but did not build: the new CMake
include was initially absent. No tests ran or received credit. Attempt02 built
all targets and ran all eight tests: five passed and three failed. Two ordinary
processes asserted ITER=0 for every A=2I fixture; sqrt(2) is not exactly
representable, so the native low Cholesky solves legitimately required one
refinement in selected RHS cases. The old assertion `iter == 0` becomes the
actual valid `0 <= iter <= 30`, retaining A=2I and every accuracy assertion.
A separate A=4I fixture now requires ITER=0. The complex synthetic successful
fallback test wrongly expected its old imaginary diagonal to remain unchanged;
its expected output now has the documented normalized factor diagonal. Other
preflight/defect unchanged-output assertions remain. Original tests are saved in
`positive-review-amendment-01` and the failed run snapshot/JUnit.

`positive-debug-lp64-03` passes8/8, actual IDs229–236, zero skips. The eight are
two ordinary, two required mathematical, two fault, one observation and one
public example. `positive-style-02` passes strict analysis of the product,
ordinary/fault/observer tests, wrappers and example; style01 findings remain.
All other five lanes `positive-{debug,release,sanitizer}-{abi}-01` pass8/8,
with the LP64 Debug final run specifically03. Both `positive-tsan-{abi}-01`
pass the two ordinary/concurrency tests229/231; ASC alone is instrumented and
the existing process-local setarch workaround is retained. Both actual-ABI
`install-positive-{abi}-01` runs install, relocate, build/run the public example
and pass independent provider-free runtime isolation. `positive-documentation-01`
generation/audit pass with130 headers. `positive-debug-{abi}-headers02` passes
both normal/no-exceptions header probes771/772; the first header attempts selected
unbuilt probes, failed with Not Run and remain preserved without passing credit.
Both `positive-integration-{abi}-01` runs pass all eight configured affected
package/architecture/header/documentation commands. None of these scoped results closes retained PT/PPSVX mathematical
failures or full Reference/platform/owner acceptance.

## Frozen tracked-source installation

Product commit `e84ab830d814e8ace909722f629a2ab5d146d0bb`, tree
`5b7d3be55c3c7087baf490f86fd15a91f50873e1`, is archived directly with git.
`positive-fresh-{abi}-01` configures and builds that tracked source with isolated
matching dependencies. Twenty mixed/header tests pass per ABI; the separately
selected package process fails because the runner omitted the existing
Utilities build target. Failed commands and install prefixes remain preserved.
`positive-fresh-package-{abi}-02` then builds that actual target and executes
the original configured package command with a fresh guarded scratch prefix.
Both ABIs pass full build/install/relocation/public-example and isolation checks.
No product fix, copied provider bundle or historical source include is needed.
These are twenty passing original tests plus the successful package replay,
not a relabeled 21/21 result for the first failed process.

Fresh PR family run34515880531 on that product executes49 tests per actual ABI:
45 pass, exactly the four retained PT mathematical processes fail, and none
skip. CodeQL34515880545 passes. CI34515880520 completes with its other jobs
passing, but the Clang18 format job fails on one ternary comparison that the
local Clang19 formatter spelled differently; subsequent tidy steps do not run.
`positive-hosted-final-01` retains JUnit and job records, and
`positive-hosted-ci-final-02` records completed CI. The reviewed repair adds
parentheses around the same comparisons, preserves all numerical requirements,
and passes both formatters. All six affected ordinary/concurrency two-test
profiles and both TSan profiles pass again. This does not retroactively turn
the original hosted CI green; fresh checks follow the repaired feature commit.
