# Experimental robust PPSVX integration review

Finite verdict: **PPSVX_ROBUST_INTEGRATED_EXPERIMENTAL_FULL_PROGRAM_INCOMPLETE**.
This closes the authorized first-party Linux/static integration slice. It does
not certify the pinned Reference PPSVX family or the full programme.

The user's subsequent 2026-09-10 instruction conditionally authorizes bounded
review, concrete first-party fixes, opt-in root integration and ordinary feature
commits of the existing experiment. It supersedes the experiment-only admission
limit in the [original review](packed-cholesky-expert-robust-experiment.md).
It does not authorize upstream changes, copying, bundling, notice approval,
automatic Reference credit, merge or release. PR #47 remains a draft.

The decision binds staged manifest
`7adf632698d0e28d87b15ffe377546f298f9bdae99336bdb70120e2fc5981e73`
and original pending patch
`c3d28360d9a0c0ca624aa07b2f2f69c0a637f338d672b994f37ac3a5c05c1caf`.
Both real files and all 1,174 manifest entries were verified once. The original
patch, experiment, candidate18–22 attempts and all worktrees remain preserved.
The integration starts from `ebe83292e5cf0bcb32e525c1f1b535915f2d954c`.
The named companion adoption document was not found; the explicit instruction,
experiment review, final handoff and executed `resume_review.py` provide the
current task pointer. No missing document is represented as inspected.

All 20 original patch entries were classified before selective integration.
The six first-party implementation/header/test files and necessary ASC checked
integer-composition helper are adopted. Nine unadmitted Reference PPSVX files
are excluded. Build lists and header metadata are amended for the selected
source, with a separate OFF-by-default option, installed feature Boolean,
maintained root tests, copied public example and supplemental observer harness.
The Reference driver, provider lock, source-derived inventory, coverage mapping
and notice remain unchanged. This is not a wholesale staging import.

## Bounded source, API and report findings

The three numerical implementation files remain byte-identical to the reviewed
experiment. The review found no blocking algorithm defect in the admitted
Linux/static profile. It preserved the separately named APIs, algorithm IDs,
explicit workspace, rounded-factor reuse, N/E/F transformations and absent
native INFO. The [public contract](../../docs/contracts/robust-ppsvx.md) records
the resulting guarantees and limitations.

| Finding | Reviewed action and regression |
| --- | --- |
| Publication rejects overflow, nonzero-to-zero loss and excessive subnormal relative rounding loss before any output publication. | Preserve this behavior; extend the original loss fixture across N/E/F, triangles/layouts and add solution overflow and subnormal rounding-loss cases. Check unchanged caller arrays, range status, accuracy-warning report and no factor authorization. No failed diagnostic is clamped. |
| Low finite RCOND is an accuracy warning, distinct from publication loss. | Add an analytic nearly singular 2-by-2 correlation fixture in all four modes/scalars and both triangles, checking finite estimates, `kNumerical`, documented partial output and no factor authorization. |
| RCOND estimates the reciprocal norm product for C and the rounded factor product M; FERR is an estimate qualified by factor quality and arithmetic. | Document those qualifications without altering the analytic scalar predicate, cases or tolerances. BERR's safe floor and convergence remain distinct. |
| Each condition estimate uses n basis solves; each RHS's forward estimate also uses n weighted basis solves. | Document O(n^3*(1+nrhs)) time and O(n^2+n*nrhs) scratch, including FACT F. Factor reuse does not make the full expert operation quadratic. |
| The original publication-loss test needlessly evaluated denorm_min/max in long double. | Replace that proof step with the exact inequality from positive denorm_min and max>1. Preserve the original input and assertions. Explicitly require wider long-double exponents for the separate extreme Gauss-Jordan oracle; do not claim that requirement holds on every platform. |
| The new public example initially expected a new factor authorization in FACT F. | Correct the example to require authorization for N/E and its absence for F. The existing adapter intentionally consumes F factors; solution, diagnostics and unchanged-factor checks remain. Preserve both failed example runs. |

Style review also split the ordinary example's fixture construction from its
solve/reuse sequence, removed an unused include and used repository naming and
termination conventions. D003 public/API compatibility exceptions remain in
force. The live Google C++/Python guides and repository format/tidy files were
checked; no numerical architecture was replaced for style.

## Execution record

Raw commands, selectors, statuses, input identities and retained failed attempts
are under external `asc-cpp-evidence/lapack-array-io/ppsvx-robust-integration-01`.
The frozen integrated product is `1e15d843d8968f659c393af85e6bdcf99af3345a`,
tree `ca291bf16f76ef78144f91caffc97bb84da442fa`. Its full reviewed amendment
from the preserved starting revision has SHA-256
`a019780c300f373d97f94ed0005ca8d7385d6eb3421e2a57b5a8283583b04f5c`;
the original patch remains unchanged. The local audit records individual test
IDs, commands, exit codes, source inputs and provider identities. No zero-test
selection or unexpected skip receives credit.

| Executed gate | LP64 | True ILP64 | Scope |
| --- | --- | --- | --- |
| Integrated GNU11 Debug | 11/11 | 11/11 | Four unchanged required scalar gates, bounded generalization, ordinary behavior, concurrency and allocation safety |
| Integrated GNU11 Release | 12/12 | 12/12 | Same 11 tests plus the package integration test |
| ASC-only GNU11 ASan/UBSan | 11/11 | 11/11 | ASC code instrumented; provider/runtime uninstrumented |
| ASC-only GNU11 TSan | 1/1 | 1/1 | Independent real algorithm calls, buffers, scratch and reports; foreign code uninstrumented |
| Clang19 ASan/UBSan and calibrated load observation | 11/11 | 11/11 | Actual adapter and maintained tests; 20 intentional forbidden-read controls detected |
| Fresh independent checkout numerical build | 11/11 | 11/11 | No historical source-tree include path |
| Fresh relocated package | 1/1 | 1/1 | 34 nested installed family tests, including three robust tests, plus the copied public robust example and existing isolation controls |
| Clang19 copied installed public example | 1/1 | 1/1 | Consumer compatibility with the admitted GNU-built installed facet |
| Provider-free Dense and Sparse I/O consumers | 1/1 each | 1/1 each | Provider discovery disabled; runtime dependencies contain no LAPACK, BLAS, Fortran or quadmath |
| Corrected metadata/package closure | 14/14 | 14/14 | Eight affected audits and six required package fixtures |

Both default-OFF graphs omit the robust implementation and export a false
feature Boolean. The installed experimental library exports 24 robust public
query/execute symbols and no unadmitted ASC Reference PPSVX symbols. The public
example uses only intended installed targets and independent ordinary fixtures;
the extreme failures are neither suppressed nor part of its oracle.

Strict adapter/header, mathematics, maintained consumer and example checks pass.
Formatting, Python formatting/lint, coverage and links pass. Doxygen reports
127/127 headers, 2,228 documented public members and zero warnings. The fresh
checkout numerical/package runs used `8b0f5315`; the explicit changed-input
comparison to `1e15d843` preserves every implementation, test, compiler setting,
build option and installed-export input. Only the metadata correction below
required new closure. Local provider identity SHA-256 values are
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`
(LP64) and `8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`
(ILP64), both built from the unchanged pinned provider commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. ILP64 uses the recorded global
Fortran integer build route, not merely a C++ typedef.

The first integrated revision `8b0f5315eabbb245518466b1bc6af88819fbaacc`
passed both Release numerical/package selections, ASC-only ASan/UBSan, TSan,
Clang supplemental observation, fresh numerical builds and relocated family
consumers. Its full Release runs each executed 957 tests: 871 passed, 78 retained
Reference mathematical gates failed, and eight integration metadata audits
failed. Hosted provider-free CI reproduced the metadata omissions. These
failures are preserved and are not relabeled as a passing full run.

The reviewed correction removes a transitive Core dependency from the new
capability's direct-edge list, adds the authorized experimental capability to
the exact-name oracle, and adds its public header to the independent source,
Dense-facet exclusion and exported-header inventories. The explicit total
increases from 126 to 127 because exactly one public header was adopted.
Validator logic, existing entries and numerical requirements are unchanged.
The affected eight audits and six required package fixtures then pass 14/14
in each actual ABI. This correction changes metadata and its exact oracles;
the numerical implementation, tests, build options and installed exports remain
byte-identical. A separate amendment identity preserves the original commit
and patch instead of amending history.

The supplemental Clang harness compiles the actual adapter and maintained tests
against an installed, admitted GNU context. The existing observer is separately
compiled; `-O1 -fsanitize-coverage=trace-pc,trace-loads` brackets initialized
caller arrays through queries and Execute. Twenty intentional output reads must
be detected, supplied-factor input reads remain legitimate and normal output
writes must succeed. The robust route has no foreign-entry phase, so whole-call
observation is appropriate. It makes no claim about uninstrumented installed
or foreign instructions or a new provider-platform admission.

The original [Reference numerical disposition](packed-cholesky-expert-disposition.md)
remains required: four failing processes, 176 failed assertions, 44 unique
scalar/mode/value cases and five overlapping arithmetic causes. The five-cause
table is not replaced by first-party success. The original unchanged Reference
tests were separately replayed against integrated ASC libraries in both ABIs:
all four processes still fail, with all 176 ordered failed assertion lines
identical to candidate22. Their canonical signature SHA-256 is
`e2df0fd618ffe9613e77625f7d316abffbaedba8849698bb79b9127276c95abc`.
The exact fixtures, values, native INFO and direct-provider comparisons remain
in the disposition table and its 44-case raw artifact.

## Admission dimensions

Fresh hosted checks bind feature `1e15d843d8968f659c393af85e6bdcf99af3345a`
and PR merge `a8f7e30560c873ceb4dbdf7d6e70324ff168dd92`; both have tree
`ca291bf16f76ef78144f91caffc97bb84da442fa`. The base remains
`46412183b2ae86101b2361c52376a8db8efff264`. PR/push
[CI 34487946167](https://github.com/AI4SciComp/asc-cpp/actions/runs/34487946167) /
[34487939731](https://github.com/AI4SciComp/asc-cpp/actions/runs/34487939731)
pass 19/19 jobs each, and both CodeQL runs 34487946187 / 34487939752 pass.
These default provider-free platform jobs preserve their scope.

The new opt-in [PR robust workflow 34487946145](https://github.com/AI4SciComp/asc-cpp/actions/runs/34487946145)
and [push workflow 34487939821](https://github.com/AI4SciComp/asc-cpp/actions/runs/34487939821)
each pass both actual ABI jobs. Every job independently prepares the pinned
provider, passes 111/111 upstream processes, builds the actual integrated
profile and passes the listed 12/12 ASC numerical/package tests with zero skips.
Only logs, JUnit, command records and attestations are uploaded. The hosted
provider identities are distinct from the local builds and retained in the
external hosted audit. No dependency archive or runtime is bundled.

The earlier `8b0f5315` CI failures and automatically cancelled robust workflows
remain historical failed/incomplete attempts; they do not acquire the later
revision's success. This final programme-record update has its own scoped
validators and explicit unchanged-product-input comparison to `1e15d843`.
It does not claim its later whole tree was the tree tested above.

| Route | Availability and acceptance |
| --- | --- |
| First-party S/D/C/Z robust PPSVX | Maintained experimental opt-in source, installed declarations and exports; Linux/static verification is scoped to actual executed records. No universal acceptance. |
| Reference SPPSVX/DPPSVX/CPPSVX/ZPPSVX | Four external, unregistered candidates. All four required mathematical gates remain failed; no row promotion. |
| Native20/array-I/O | Preserve `SUBSET_REVIEW_READY_FULL_PROGRAM_INCOMPLETE`; new PPSVX records do not redefine its scope. |
| Reference catalogue | 2,113 required rows; 350 partially registered, 1,763 not started, zero strictly complete or verified. New query/execute overloads do not enlarge the denominator. |
| Platforms and package | Initial GNU11.4 Linux x86_64 static LP64/ILP64; Full Clang/provider, shared, macOS and Windows provider admission remains unexecuted or unadmitted. Supplemental Clang consumer/adapter results do not expand that scope. Provider guards are unchanged. |
| Owner/provenance/release | Conditional first-party adoption/integration is authorized. Notice/metadata packet and release authorization remain separate and pending. |

The actual pending notice packet remains
`380a8792f8c23263439ac79e8c02bb1959089b674c1221c6397e9bc26585992c`,
proposed notice `9ebeaf30a78e77f5fa4a4ba94ed66c68bdb92acd1dfdc8b1456e0c114ffbed41`.
Its bound repository materials were unchanged when rechecked, and no later
owner decision was found in local records or PR comments/reviews. No notice is
applied and no provider/runtime file is distributed by this integration.

After this finite integration gate, the next dependency-ready P05 family is
positive-definite tridiagonal factor/solve, S/D/C/Z PTTRF/PTTRS (eight existing
not-started catalogue rows). It can reuse the admitted provider/context,
workspace, vector/matrix descriptors and report foundation. Its first task is
the typed real-diagonal/real-or-complex-off-diagonal factor contract and pinned
signature/storage review, preserving the distinct complex UPLO solve semantics.
No implementation or verification credit for that family is claimed here.
