# PTCON checked condition estimation

This P05 slice adds S/D/C/Z PTCON query/execute interfaces over the existing
nominal positive-definite tridiagonal factor views. The four scalar routines
remain four entries of the unchanged 2113-row Reference catalogue. The current
source is integrated as an optional first-party adapter to the unchanged
Reference provider. Installed use is tested; mathematical acceptance remains
blocked by the two precise provider causes below.

## Reviewed API and arithmetic boundaries

`lapack_positive_tridiagonal_condition.h` declares eight public functions. The
factors contain real D and real/complex E in the existing lower LDL^H or upper
U^HDU representation. Upper conversion conjugates E; PTCON itself takes no UPLO
argument. The original matrix one-norm is a finite nonnegative scalar input.
Complex tridiagonal entry magnitudes use the Euclidean complex modulus. ASC
does not reconstruct the original matrix or certify the supplied norm's origin.

Queries inspect descriptor/provider metadata and that scalar norm, without
reading factor values or old RCOND. A plan binds scalar, order, physical
orientation, provider and whether the estimation path is active. Nonzero norm
magnitude and disjoint buffer addresses may change on immutable-plan reuse.
Active execution validates finite positive D and finite E only after workspace
admission, then uses exactly N live underlying-real objects in `kReal`. There
is no dense conversion, hidden allocation, transfer or synchronization. Native
computation, validation and storage are O(N). The pure integer helper rejects
active N equal to the native signed limit because the loops and AMAX reduction
form N+1; inactive quick returns admit the existing representable order bound.

PTCON executes its native N=0 and ANORM=0 quick returns, publishing RCOND=1 and
0 respectively with actual INFO=0. Those paths read no factor values and need
no caller workspace. This new routine contract does not change PTTRF/PTTRS's
existing local empty completion behavior. RCOND is staged with an impossible
negative sentinel; INFO starts at the actual native INTEGER minimum. Missing,
partially written or nonzero INFO, or missing/negative RCOND, are provider
defects that preserve caller RCOND. Returned NaN/infinity is published unchanged
with an accuracy warning and documented partial validity. A finite zero is a
raw estimate, not a proof of singularity. No clamp or fallback is introduced.
LDL^H factors do not acquire an unrelated Cholesky `factor_family` report tag.

## Finite test and observation scope

The independent ordinary oracle explicitly constructs L D L^H and performs
bounded dense Gauss-Jordan inversion, rather than calling a LAPACK solve or
repeating PTCON's comparison recurrence. Orders 0/1/2/5/9, both physical factor
orientations, scales 2^-8/1/2^8, all four scalars and signed/nonreal dyadic
multipliers retain a 256-epsilon relative comparison. Hermitian tridiagonals
are diagonally unitarily similar to their Stieltjes comparison matrices; this
preserves entry-modulus one-norms and the inverse norm. The comparison inverse
has nonnegative entries, explaining why the finite PTCON recurrence agrees
with this independent norm oracle for these systems. This does not assert an
exact condition estimate for unrelated LAPACK estimators or arbitrary factors.

Required scalar extremes use the analytic identity cond([a])=1 for every
positive finite a; they require no wider-exponent intermediate oracle. The
original tolerance is 16 epsilon. Neither a provider-fidelity check nor the
ordinary example converts a failed mathematical assertion into acceptance.
The test keeps exact one-byte workspace rejection, stale active/quick plans,
invalid norms/factors, alias rejection, allocation checks, native count bounds
and independent real-provider concurrency. Four threads share immutable factors
and a plan, with independent contexts/workspaces/outputs/reports and distinct
supplied norm multipliers. No global allocation or fault wrapper is used as a
concurrent production-state oracle.

The private thread-local wrapper checks all four exact native signatures and
faults for absent/half-width/negative/positive INFO and absent/negative/NaN/
infinite RCOND. The half-width control is specific to admitted little-endian
x86_64. Normal controls enter the real pinned provider. The Linux
observer uses initialized, aligned containing arrays, PROT_NONE and default
child fault termination. It requires 48 forbidden pre-entry read controls,
24 legitimate factor validation-read controls and 48 native output writes,
including 32 native quick returns. Caller output and real scratch regain
access only at actual foreign entry. Inactive factors stay protected through
the provider; queries, stale plans and exact short workspace run with all
numeric arrays protected. Factor-read controls restore at entry, so a fault
must precede that boundary. No signal handler resumes faults; no provider or
runtime instrumentation is claimed. This observation scope is Linux-specific.

The standalone public example constructs an ordinary 2x2 Hermitian matrix,
factors it through PTTRF, solves independently known right-hand sides through
PTTRS in both padded layouts and checks PTCON against the explicit inverse.
It reuses borrowed lower and separately owned upper factors after overwriting
the original storage. The consumer uses public headers and the exported ASC
target; the initial external executable alone is not installed-API evidence.

## Numerical disposition

Provider source remains pristine Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`. Four pinned source hashes and actual
typed-factor prerequisites are in `master-continuation-20260910-01/ptcon-prerequisite-01/review.json`.
Both actual static ABIs reproduce the following without a source or environment
change. These arithmetic categories overlap already recorded condition-estimator
limitations; they are not four new bugs because four scalar processes fail.

| Cause and precise fixture | Expected and observed | Direct comparison and source expression | Required decision |
| --- | --- | --- | --- |
| Unscaled inverse overflow. N=1, D=ANORM=a, empty E; a is `denorm_min`, `2*denorm_min` or `min()/8` of the underlying real type; S/D/C/Z and both factor orientations. | Exact RCOND=1; actual RCOND=0, INFO=0. ASC publishes the raw finite result with success. The condition-one gate fails. | Direct typed calls return the identical zero and INFO=0. `WORK(N)=WORK(N)/D(N)` overflows from an initial one; the final reciprocal becomes zero. | Retain the Reference failing gate. Numerical acceptance needs an explicitly approved provider/algorithm strategy that handles scaled inverse estimation; no silent substitution or waiver. |
| Reciprocal ordering after subnormal rounding. Same scalar fixture with a=`max()`. | Exact RCOND=1; actual RCOND=+Inf, INFO=0. ASC publishes the value with `kNumerical`, `kAccuracyWarning` and documented partial validity. | Direct calls return identical infinity and INFO=0. The representable subnormal inverse rounds to a power of two; `(ONE/AINVNM)/ANORM` overflows its first reciprocal before dividing by ANORM. | Retain the finite condition-one assertion and raw diagnostic. Numerical acceptance needs an explicitly approved robust reciprocal/norm evaluation strategy. |

Per ABI there are four failed test processes, 32 failed assertions, 16
scalar-labeled cases (eight underlying-real fixtures) and 32 orientation
executions, grouped into the two causes above. Exact hexadecimal inputs,
returned/direct values, INFO and ASC reports are retained in
`ptcon-candidate-{lp64,ilp64}-02/tests/command.log` and indexed by
`ptcon-initial-audit-01/audit.json`. No extra large reproduction campaign is
needed to classify these scalar paths. These new failures are separate from
the historical 83-process full Reference selection, which did not contain PTCON.

## Integrated execution and preserved attempts

The maintained source includes the adapter, eight declarations, reusable
behavioral/fault/observation tests, standalone example, installed exports,
independent header/source inventories, ABI header manifest and required hosted
selectors. Schema2 registration03 changes only the four new Reference rows
from the pre-PTCON baseline, preserving native20 and all three old execution
records through an explicit index bridge. Counts are 390 registered (48
callable-unverified plus 342 partial), 1723 not started, 76 reviewed routine
contracts and zero verified, out of 2113 required rows.

The final tested archive has tree
`79a45d13202f171f6b3c6c98b8490f12c2662584`, SHA256
`25d881216b7245f8912ea57699b95e754b8a00337e4b12d175db84f3698b5c42`,
and 1157 build/API/test/example/ABI inputs. Its source and exact commands are
retained in `ptcon-frozen-product-02` and the following command records under
`master-continuation-20260910-01`. Later narrative/index updates do not claim
whole-tree execution of a different product source.

| Executed final scope, in each actual ABI | Result and exact evidence |
| --- | --- |
| Static Debug, Release, ASC-only ASan/UBSan | Each 7/11 passes; exactly four required mathematical failures and 32 failed assertions, no skips or sanitizer diagnostics. `ptcon-{debug,release,sanitizer}-{lp64,ilp64}-integrated-03`; all outcomes and actual IDs in `ptcon-final-static-audit-01/audit.json`. |
| Static TSan | Four ordinary scalar processes pass in each ABI, including real-provider independent-buffer concurrency. `ptcon-tsan-{lp64,ilp64}-integrated-03`. |
| Shared Debug and ASC-only ASan/UBSan | Each 7/11 passes with the same required failures. Shared production PIC objects are observed through the maintained test-only wrapping mechanism; ordinary consumers use the production library. `ptcon-shared-{debug,sanitizer}-{lp64,ilp64}-02`. |
| Shared TSan | Four ordinary scalar processes pass per ABI. `ptcon-shared-tsan-{lp64,ilp64}-02`. |
| Fresh Release static/shared producers | Each 14/18 passes: the 11 PTCON checks, two public header modes and five architecture/configuration/surface gates. Only the four required mathematical processes fail. `ptcon-fresh-{static,shared}-{lp64,ilp64}-02`; `ptcon-final-shared-audit-01/audit.json`. |
| Fresh build/install/relocation/public consumer | All four package processes pass. Each solves, inspects PTCON diagnostics and reuses returned factors through copied public examples and exported targets, without source/private include paths. Eight PTCON symbols and the exact installed header are audited in `ptcon-package-audit-01/audit.json`. Runtime closure, dependency isolation and absence of bundled provider/runtime/observation libraries pass. |
| Independent installed header inventories | All four selections pass 11/11, including the five direct checks and six CTest fixture setup/cleanup processes. Exact configured selection and IDs are in `ptcon-installed-header-{static,shared}-{lp64,ilp64}-02`. |

All final numerical profiles retain the same 32 raw input/direct/ASC
observations as the initial candidate. Actual LP64 and true ILP64 use their own
prepared, attested Fortran providers; changing a C++ typedef was not used as ABI
evidence. Provider and runtime code are not sanitizer or race instrumented.
Non-Linux/provider-platform admission remains separate and unexecuted where the
required supported environment is unavailable.

Historical attempts remain intact: initial candidate01's command-label
collision occurred before compilation; candidate02 passed seven of eleven
processes with the same numerical failures. The initial frozen tree
`cf8ae2997cc4685fda6da02ff99477eb57dfa486`, archive
`7a0317d1b425e8ce527717b638203b64b1d7f7e7bd8deecfa10241f8e20bad18`,
was preserved after root header-filter review found two private-header style
issues. Reviewed amendments remove local constness that prevented a Status move
and specify `std::uint8_t` for a private fault enum. No numerical expression,
public declaration, fixture, tolerance or provider changed. Strict root product
and test checks pass in `ptcon-root-style-02`; wrapper/fault/observer/consumer and
all-file formatting pass in `ptcon-root-style-03`. All affected final profiles
above were rebuilt. Installed-header runner01 rejected an incorrect preflight
count before CTest; runner02 obtains CTest's actual fixture closure and preserves
the failed attempt.

Scoped coverage validation passes against registration03. Doxygen,
documentation consistency, links and whitespace checks have separate command
records. Feature delivery and new hosted run identities are recorded in the
programme state and draft PR; an older green revision is not PTCON evidence.
The accepted native20/array-I/O milestone remains
`SUBSET_REVIEW_READY_FULL_PROGRAM_INCOMPLETE`. Neither this callable Reference
slice nor the separate opt-in RobustPpsvx capability closes full-program or
Reference numerical acceptance. The next dependency-ready work is PTRFS,
followed by PTSV/PTSVX and the existing remaining queue.


## Completed hosted evidence

The preserved final runs for commit `e8509cc342a86c8904d87daf8e96107acf5d2326`
are now audited in `master-continuation-20260910-01/ptcon-hosted-audit-01/audit.json`.
CI run 34587486556 and CodeQL run 34587486442 passed. Family run 34587486569
failed its required mathematical gates: each of four static/shared actual-ABI
profiles passed 155 of 168 tests, with PTTRS four, PTCON four, SGEDMDQ one and
GBRFS four failed processes, and zero skips. Each pinned provider suite passed
111 of 111. PTCON passed its seven ordinary/control/example tests and retained
four mathematical failures with all 32 original observations unchanged. This
evidence belongs to the PTCON revision and does not certify the later PTRFS
source or grant mathematical acceptance. No hosted job was rerun for this audit.
