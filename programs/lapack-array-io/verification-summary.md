Current verdict: **SUBSET_REVIEW_READY_FULL_PROGRAM_INCOMPLETE**.
Tested feature `c020803af564fdf478bf4626797fedcf1fedbbde`, PR merge `10f18f94269249e3e17a145dbbfd939875f28a3f`,
tree `dfd7f6823b1e0d53531a847f7a4050aa8a399c5c`. PR/push CI each pass 19/19
and both CodeQL runs pass. Local GNU 11 shared passes 295/295, Clang 19
ASan/UBSan 248/248; maintained installed 81 nested tests and seven native tests
pass, with zero unexpected skips. The exact identities, commands,
hashes and source bridges are in the [current checkpoint](stabilization-review.md).

W1–W4 close the finite native/I/O technical scope: 80 binary profiles in both
independent directions; 20 validator-verified native rows / 112 modes / 1,120
class slots; maintained 24/288 load cases and exact precision/locale fixtures;
108 portable cleanup profiles on required static/shared platforms. Original
18 lint and 40 alias repairs remain closed. No technical I/O matrix cell remains.
The separate 62 GNU-wrap cleanup
cases are Linux/static evidence; no other-platform libc interposition claim.

Reference: 2,113 required, 350 partial / 1,763 not started, zero strict complete
callable/verified. The 130 absent XBLAS definitions and 78 historical mathematical
failures remain separate. Owner acceptance and amended provenance packet
380a8792 remain pending. No merge
or release authorized. Candidate22 and its concurrency/installed slice are
complete with Reference failures retained. The subsequently authorized
[first-party robust PPSVX experiment](packed-cholesky-expert-robust-experiment.md)
passes the unchanged scalar failures, bounded generalization, safety/concurrency
and isolated public installations in both actual ABIs. Its numerical code and
integration patch remain external; no Reference or root-API promotion follows.
Next: review/adopt that concrete algorithm and API/report contract before the
existing wider platform and root-admission gates. The pending notice is separate.
Final summary-only commit/patch has separate external checks and input bridge.

## Historical subset summary (5cc3a935)

Current verdict: **STABILIZATION_PASSED_SUBSET_ACCEPTANCE_PENDING**.
Tested candidate `5cc3a9350f7ea176552340de41a38ef73de0aebe`, merge `184c0ac1f359756c9477e9c1399719780ca060a6`,
tree `34c7712b7d67090114aa522e6206000638273d2d`; base develop unchanged. PR/push CI each 19/19
and both CodeQL runs pass. Windows each 229/229; macOS each 291/291;
full local GNU11/shared Werror 293/293 and ASan/UBSan 246/246. Raw identities,
commands, totals/skips and original failure closures are in the
[current checkpoint](stabilization-review.md) and external
`subset-acceptance-20260909-01/final-checkpoint-03/manifest.json`.

A1 closes 18 findings in the exact expanded audit. All 32 POTRS and 8 GEQRF
alias slots now execute in the configured platform matrix and an installed
native consumer; 20 native routes still await normalized row evidence/ledger review.
The maintained installed harness passes 77 nested tests; 62 cleanup cases retain
2,808 assertions. Dense scalar/rank I/O adds 324 profiles, with source-matched
runtime/sanitizer/platform execution. Independent 60-case binary readback and
expanded 24/288 printer load diagnostics also pass externally; maintained
integration and the finite remaining I/O cells are still open. No package or
routine is promoted from these subset passes. The 2,113 denominator, 350 partial
Reference routes, 1,763 not started, 130 absent XBLAS definitions and 78 historical
mathematical failures remain unchanged and describe separate dimensions.

Next: Integrate the tested independent binary readback into the maintained installed harness, preserving all 60 independent fixtures and exact structure/value comparisons. Use state.json's exact fresh-scratch command after integration.
Concrete owner packet approval is still pending. Later program-summary changes
are a separate local commit and do not inherit whole-tree CI identity.

## Historical stabilization summary

Current verdict: **STABILIZATION_PASSED_SUBSET_ACCEPTANCE_PENDING**.
Product 5f3c6d9 / candidate15 is pushed at 5243995; tested merge bf99463e has the
same tree 8c4a1711, base develop 46412183. PR #47 remains an open draft.
PR/push CI each pass 19/19 jobs; both CodeQL pass. Windows static/shared Debug/
Release each 227/227, macOS four lanes 289/289; Linux and package/sanitizer totals,
raw logs and checkout identities are in the [current checkpoint](stabilization-review.md).
No unexpected CTest skips. The last Sparse I/O DLL-observation harness failure
is closed; all original failures are preserved. Final program records are a
local documentation-only follow-up with an external commit/patch handoff.

Installed Dense/Sparse consumers each pass 1/1 with public exported targets and
relocated prefixes. Dense factor reuse gives independent residuals0/0 and exact
124-byte reload; malformed input consumes 95 bytes and preserves destination.
Sparse reloads exact COO/CSR/CSC without Dense linkage. New installed text/binary
checked-close tests pass 60/60 configurations and2,760 assertions; scoped Matrix
Market cleanup passes1/1 and24 assertions per consumer. Wider ranks, layouts,
scalar/failure/platform combinations and reusable harness integration remain open.

All20 native routines remain implemented_unverified, with 40 partial alias slots.
Reference has 350 executable partial registrations,1,763 not started of2,113;
strict complete callable 0 and verified 0 do not mean zero executable routes.
Actual pinned LP64/trueILP64 foundation 10/10 each is separate from provider-free
CI. Historical 78 required mathematical failures, 130 absent required XBLAS
routines and full P00–P11 obligations remain. The current incremental validator
passes; strict full validation still fails at cbbcsd full profile incomplete.
The corrected top-level coverage SHA matches the actual frozen contract.

Next bounded task: fix 18 baseline-reproduced legacy BLAS test/benchmark lint
findings, retaining all assertions and global checks. Concrete
[redistribution review](redistribution-review-packet.md) remains pending.
`acceptance-checkpoint-final-01/manifest.json` seals the current evidence.

Historical results below retain their original identities. Old “current,”
“pending” and “next” statements are superseded by the checkpoint above.

Current local product5f3c6d9 / candidate15 repairs the sole remaining C14
hosted failure: two Sparse I/O assertions omitted Windows DLL resource visibility.
Four affected tests pass1/1 each under GNU11Debugshared, GCC14Releasestatic/shared
and Clang19ASanUBSan, zero skips. Formatting and full Sparse I/O strict analysis pass; fresh hosted verification
remains pending. All other1,130non-program files are
identical to C14; no production/provider/header/package change is present.

C14 PR/push CI each finish18pass/1fail; bothCodeQLpass. WindowsstaticDebug and
Release each227/227; sharedDebug227/227, sharedRelease226/227. The sole test
failure consists of40process-count assertions at two sites; original logs and
all numerical/rollback/long-diagnostic assertions are preserved. Candidate15
maps the expected observable count using the unchanged repository helper and
adds exact resource-attempt checks. Full acceptance is not yet awarded.

Current product6f5cd1c / frozen candidate14 passes full provider-free GNU11
Debug shared291/291, GCC14 Release static290/290 and shared292/292, zero
failures/skips. All three affected selections pass18/18. Formatting and the
parser/thread strict checks pass. Fresh hosted verification is next; previous
remote0d623ba/merge9d5153ff CI12pass/7fail and passingCodeQL retain their identities.

`subset-acceptance-03/checkpoint.json` maps63 current executed API/test/configuration
records to C14 source hashes and preserved full logs. Actual unchanged-source
LP64/trueILP64 foundation10/10 each, C10sanitizer3/3, libc++14Core2/2, ABI and
installed04Dense1/1 Sparse1/1 retain scoped validity. Both installed close-fault
harness02 consumers pass46assertions/CTest1/1 and strict checks. Its wider
60-configuration matrix remains running. The separately expanded legacytesttidy
has18remaining diagnostics, all reproduced on the isolated base; it remains
failed and an open P11 quality obligation.

All2113Reference requirements,350partialroutes,1763notstarted,native20
implemented_unverified,40partialnativeclassslots,78historicalrequiredmath
failures,130missingrequiredXBLAS definitions and concrete owner review remain
open. See [current review](stabilization-review.md). Earlier results retain
their exact frozen identities and do not grant full subset acceptance.

Earlier stabilization product is `fbe21fe514910e86c05bad2c1aca6c954b838acf`,
locally committed and byte-identical to frozen candidate08. Its portable decimal
fallback and explicit native alias gap pass affected GNU11/GCC14 tests,
C07 identical-source ASan+UBSan26/26, real libc++14 Core2/2, pinned LP64 and
true ILP64 foundation10/10 each, fresh installed Dense1/1 and Sparse1/1,
three strict TUs, formatting and strict Doxygen/links. C08 full runs finish GNU11 shared289/291, GCC14 static288/290 and shared290/292: only the two compiled-source inventory audits fail. C09 adds exactly the missing Core parser source to the explicit inventory; both actual audits pass2/2. All compiled inputs are identical. Fresh hosted full-candidate verification remains pending; no single-invocation C09 full pass is claimed.

The prior pushed head254db534 / tested merge79cc79f has 13 successful CI jobs
and passing CodeQL, with four diagnosed macOS missing-from_chars failures and
two MSVC alias-fixture padding failures. Current fbe21fe repairs those sources;
Linux compilation alone does not close their platform evidence. All2113
Reference requirements,350 partial routes,1763 not started, native20
implemented_unverified,40 partial native class slots and78 historical required
mathematical failures remain unchanged. See [current review](stabilization-review.md)
and [the concrete pending owner packet](redistribution-review-packet.md).

Historical first local stabilization checkpoint: product `6c72b638b46f159e41db2f26346d6caa8ecc9788`.
GNU11 Debug shared290/290, GCC14 Release static289/289 and shared291/291;
zero failures/skips in these closing runs. ASC sanitizer22/22 plus MM3/3;
actual LP64/ILP64 LU+ABI10/10 each; installed Dense1/1 and Sparse1/1;
strict11 TUs, Doxygen126 headers/2204 members/zero warnings. Original failures
and exact source bridges are retained in `local-checkpoint-01/manifest.json`.
Hosted results and complete subset acceptance remain pending; see
[stabilization review](stabilization-review.md). Full2113 remains incomplete.

# Initial stabilization recovery (2026-09-09)

The current integration recovery is `9bc62db` (product V27 `87cd646`),
39 commits beyond remote V7 `b1b78d7`. The 350 partial reference routes and
1,763 not-started rows retain all 2,113 requirements; native20 are separate,
implemented-unverified. The historical v19 and earlier sections below retain
their original source/configuration identities.

The owner-requested R0–R4 stabilization milestone now precedes PPSVX
candidate19. See [the current checkpoint](stabilization-review.md) for fresh
CI log causes, candidate fixes, executed evidence and remaining acceptance.
The [concrete redistribution packet](redistribution-review-packet.md) is
assembled and awaits owner review; no approval is asserted.

# Historical v19 triangular-band solve integration

The program has314 Reference rows in progress and1,799 not started of2,113
required;20 native implementations remain separately unverified and zero
routines have complete verified status. TBTRS4 is registered with the new
neutral band descriptor. Four GNU affected suites pass52/52, both ASC-only
sanitizer suites7/7, four relocated23-family packages and provider-free
static/shared16/16 each pass. Zero skips. Six static checks and117-header
Doxygen coverage pass. See [the bound review](triangular-band-solve-review.md).

There are2,795 normalized records, preserving original failures. The384 fresh
primary root objects and60 provider-free primary objects are separate from
explicitly retained older evidence. Historical v18 full suites still have62
required mathematical failures;130 missing required XBLAS routines and all
remaining P00-P11/mode/platform gates stay open. TBCON/TBRFS8 closure comparison
is executed source evidence only; implementation remains required. The three
prose updates pass six fresh checks with2,968 prior compiler inputs/artifacts
rehashed unchanged. The bounded v19 slice is ready for its local commit.

# Historical v18 packed estimator integration

The program has 310 Reference rows in progress and 1,803 not started out of
2,113 required. The 20 native implementations remain separately unverified;
no routine has complete verified status. Both full Release suites execute
853 tests: 791 pass, 62 required math gates fail, zero skip. Affected Debug
passes 46/62 and ASC-only sanitizer 21/37 per ABI, retaining 16 required math
failures each. All 378 primary objects are freshly compiled; four relocated
22-family packages, 14 strict and six static checks pass. Foreign archives
remain unsanitized. See [the root review](packed-triangular-expert-review.md).

There are 2,746 normalized records, including original configure/format/test
failures. The three subsequent prose updates pass six fresh documentation checks;
all 1,347 prior numerical compiler inputs and production artifacts rehash unchanged. The next triangular-band slice has
source/count/descriptor admission evidence and an unregistered solve
candidate. Its routines remain required; no full P00-P11 or missing-XBLAS
requirement is removed. See [band admission](triangular-band-admission-review.md).

The final unregistered band-solve candidate passes six7/7 suites and five
current strict/format checks. Its125 new records preserve earlier strict and
audit failures. The corrected C++20 protected-memory harness is separately
bound; root/package integration remains next. See [the solve review](triangular-band-solve-review.md).

# Historical v17 packed inverse and solve integration

Eight packed TPTRI/TPTRS routes are integrated as partial capability:302 Reference
rows in_progress,1811 not_started of2113 required;20 native implemented_unverified,
zero fully verified. Both full GNU Release runs pass783/837 with54 required math
failures; affected Debug38/46 and ASC-only sanitizer17/25 each retain8 existing
math failures. Zero skips. All packed basic tests, four21-family relocated
packages,12 strict and6 static checks pass. All62 primary ASC TUs are freshly
compiled per lane (372 objects); foreign archives remain unsanitized.

See [the bound root review](packed-triangular-review.md). Six subsequent
documentation checks pass and every prior numerical compiler input rehashes
unchanged. There are2399 normalized records. TPCON/TPRFS8 remains a preserved
unregistered candidate; its initial numerical suites retain8 additional ordinary
math failures per lane. Complete routine/mode/platform and P00-P11 gates,
including130 missing required XBLAS routines, remain open.

# Historical v16 documentation and next implementation

Six documentation/coverage checks pass;1487 prior artifacts and compiler inputs rehash unchanged. No new numerical/full-suite claim. See [the documentation review](triangular-expert-documentation-review.md). The2205 normalized records retain all prior failures. Next: packed TPTRI/TPTRS8 implementation;all P00-P11 requirements remain open.

# Historical v16 triangular estimator integration

TRCON/TRRFS8 integrated as v16:294 Reference partial/1819 required not_started,20 native implemented_unverified,zero fully verified. Full01 each773/831 passes,54 required math and4 header-count failures; Debug01 each28/40 passes,8 math and4 header-count failures. Corrected02 header replay10/10 each fixes all4 checks with6 fresh fixtures; ASC-only Clang sanitizer02 each13/21 passes and8 math failures. Zero skips;366 fresh primary ASC objects;four relocated20-family packages,14 strict and6 static checks pass. Foreign archives remain unsanitized. No full02 suite or full routine/program verification.

See [the exact root review](triangular-expert-root-review.md) and [packed source admission](packed-triangular-admission-review.md). The2199 normalized records retain original failures and exact external record hashes.

# Verification summary

The complete program is not verified. The following exact checkpoints are
kept distinct from newer uncommitted integration work and from full-profile
routine/mode closure. No package-wide success is inferred from a scoped run.

Resumed V8 now has actual full LP64/true-ILP64431 each, installed11 each,
provider-free Release277/shared279, zero skips. Debug276/277 includes a
retained consumer timeout; unchanged targeted retry1/1 passes. Independent
review found a previously untested finite diagonal-overflow mathematical
failure in all four TRSYL scalars/both ABIs. Corrected tree `2affa1a` rejects
that admission before mutation and passes bothABI15/15 scoped and all-ASC-C++
ASan/UBSan15/15, strict2TUs eachABI, Doxygen88headers/1751members/zero warnings.
The four new contract tests fail unchanged on old LP64. These corrected
scoped results do not convert the old full-suite source into corrected full
evidence; the required upstream mathematical mode remains incomplete.
See the [Sylvester review](sylvester-review.md) and indexed external records.

V8 and its corrected Sylvester admission are committed locally as
`455c235f8a0c20e4994a035a6b64b81e9950a453`. The pre-correction full candidate
was tree `37be13252b938e8761842ca6060d31bfb03cf9b6`, archive
`108cf13f670440bc25fab56f88b835d9d28b3ff9198671e25db74a6c56f88d69`.
Its completed results above remain distinct from corrected-source evidence.

Current V9 imports 24 classic indefinite expert and eight GELSS/GELSD routes,
four public headers and two public consumers. The mapping has 198 partial
reference rows, 1915 not started, all 2113 required/incomplete, zero fully
verified and 20 native implemented-unverified. Candidate01 full provider suites each pass488/492; provider-free Debug/Release
273/277 andshared275/279, with the same four registration failures and zero
skips. Both ABI sanitizer suites pass70/70; strict18/22 TUs pass per ABI.
Registration/test-style corrections and a test-only Fortran STOP return guard
are being prepared for candidate02; no full V9 pass is claimed.
See the [V9 integration review](factorizations-v9-review.md).

The [Sylvester review](sylvester-review.md) distinguishes the earlier scoped
13/13 tests per actual ABI in Release, all-ASC-C++ ASan/UBSan and dynamic-libc
lanes, zero skips; 4240 actual numerical cases per ABI; strict six TUs;
85-header/1663-member Doxygen; separate standalone headers, 60 guarded direct
ABI calls and 416 public-consumer solves plus four warnings per ABI. Runtime
leaf closure and allocation positive controls passed. The direct consumer was
not an installed-package test. Every earlier compile/oracle/runner failure is
retained. New expert/SVD/structured handoffs are excluded from this snapshot;
their available evidence and required failed gates do not become V8 coverage.

Latest v5 command checkpoint: frozen tree
`f021fb84e345c355affe65d8c06119c14a18e594`, archive SHA256
`af1a759135aea53d8b9fed7c999cd876198674c69bbc5abb2d66e7a82f658155`.
Full LP64/trueILP64 each344/344, provider-free Debug/Release each277/277 and
provider-free shared Release279/279 pass with zero skips. Both relocated
provider packages pass5/5 public-only consumers; Doxygen79/79 headers and1573
public members passes with zero warnings. Exact mapping SHA256 is
`a58be7ec2d1ec1a4a5c9545abf976d5a1e90a58e366aea2fe17d7bd1c9eb651a`:
20 native implemented-unverified,92 reference in-progress,2113 required and
zero fully verified rows. Earlier failed candidate02 full runs and Doxygen
warnings are retained, not relabeled. See
[the v5 review](factorizations-v5-review.md) and the
[checkpoint index](verification-checkpoints.json).

The newer v6 checkpoint is excluded from those v5 results:20 Cholesky expert
and12 least-squares rows are now centrally registered as in_progress, with
two new public-only consumers. Mapping124 partial reference/20 unverified
native/2113 required passes incremental validation; zero fully verified rows.
The LARF1F/LARFB INTEGER cursor guards pass new integer-only tests/strict lint;
the frozen old least-squares regression fails and the corrected source passes.
Direct public consumer attempt03 passes both ABIs. Corrected-source full,
sanitizer and relocated installed command checks now pass as recorded below;
exact normalized mode closure remains open. GELSY's forced-zero-column
minimum-norm failure and prior tiny-input math limitations remain explicit in
[blockers.md](blockers.md); none is hidden by provider-fidelity tests.

The v6 checkpoint is committed and pushed as
`31e93935f8db4ac685c2224abf7860a058040117` to the actual
[draft pull request47](https://github.com/AI4SciComp/asc-cpp/pull/47).
Its product bytes equal the frozen v6 candidate02 tree outside durable program
records. That historical draft checkpoint has been superseded by V7 below.

Current v7 integration adds twelve band Cholesky, eighteen classic indefinite
and eight rank-revealing reference routes:162 reference rows are in-progress,
1951 are not started, all2113 remain incomplete, and zero are fully verified.
The87-header
inventory and mapping SHA256
`80e087f4826466894f7d623121c7c45f279bbcbcb938b2c8544aecaeea503259`
pass incremental validation. Root-authored band, indefinite and rank public
consumers pass direct LP64/trueILP64 diagnostics and strict Clang18 checks;
candidate02 full LP64/trueILP64 passes415/415 each with zero skips. Root completed
full rank review/import and the31-record source/log audit. Final candidate03
contains the later private ABI assertion style amendment and independently
passes full LP64/trueILP64 each415/415, provider-free Debug/Release each277/277
and shared279/279, all zero skips. Root read both actual nested installed logs:
ten consumers execute and pass per ABI. Affected all-ASC-C++ ASan46/46 each,
strict26 translation units per ABI, format46 changed C++ files and
Doxygen87/1742/zero warnings pass. Foreign archives/runtimes are not sanitized.
All failed candidate01 builds/strict and candidate03 premature auxiliary
attempts remain retained; no earlier pass is attributed to newer source. See the
[v7 integration review](factorizations-v7-review.md).
V7 is committed/pushed as `b1b78d789fb8f5366f13f86c411832df842b2755`;
the same draft47 now describes that exact product. The app connector returned
403 for the body edit; the already-configured authorized GitHub CLI completed
the scoped update, recorded externally at `pr47-v7-update-cli-01/record.json`.
No permission, reviewer, merge or release setting was changed.
Retained PBCON lower-complex scaled condition, GELSD zero-RHS/wide-minimum
process-safety and single-precision tree-storage, and GELSS wide-path returned
right-vector gates are listed in [blockers.md](blockers.md).

Root-owned P08 ordinary TRSYL4 is implemented separately and not in those162
registered rows. A scoped direct snapshot passes4240 nonempty numerical cases
per actual ABI, including real Schur blocks, complex conjugation, explicit
scaling and warning outcomes with allocation audits. Initial9-test contract
selections fail4 each; corrected snapshot `p08-sylvester-contracts-g73W72tm`
passes9/9 and strict5 translation units per ABI. The later13-test alias snapshot
`p08-sylvester-alias-MX9v4Gek` passes Release, all-ASC-C++ ASan and dynamic
libc lanes13/13 per ABI,zero skips, plus strict02 six translation units each.
Each libc lane has13 actual positive-control messages. Its initial strict
runner erroneously submitted a literal plus-sign filename; that extra failed
command is retained separately from the six successful real source checks.
Root's new public-only consumer passes1/1 per ABI with416 independent solves
and four actual warning calls in `p08-sylvester-consumer-llyXs68F`. This is neither
installed nor full-P08/normalized-mode proof. TRSYL3/TGSYL, all remaining
spectral and specialized operations, and full P11 remain required.

The first v6 frozen archive accidentally omitted the five new public headers
because the explicit staging command did not include their directory. Both
provider configure runs correctly fail; no provider tests ran. Its source tree
`9b48e5fa9a89ae022e22ed94f1eef28eea76f6fe`, archive
`033cf3a410765e359dfb5a36d7e03ae9c7ad177294491912e1a9ef8dd3f90097`,
records and completed provider-free regressions are retained. Debug/Release
each269/277 and shared271/279 pass, with eight failures each and zero skips.
Its Doxygen pass
does not verify the omitted headers. Candidate02 includes the original reviewed
header bytes and reruns all lanes; no implementation/test predicate changes
are justified by this staging error.

Corrected v6 candidate02 is frozen as tree
`98f341bc6f4c2f655cae9a503ecbda5b266f3e32`, archive
`2b5d8ee5018998b86d5498ac949cf70bae359ca3cb723e9392a0b1d80021c06c`.
Its Doxygen checks pass84/84 headers,1654 public members, zero warnings;
Markdown passes. Both actual-ABI affected Clang19 ASan/UBSan suites pass19/19,
zero skips, including new expert/least-squares and corrected QR/count tests.
All CPU Core/Dense/registered-reference C++ sources and selected tests are
instrumented; Fortran/BLAS archives and dynamic runtimes are not. First harness
link failures omitted QR-placement's required fault-support TU; repaired
external harness02 changes no product bytes or test predicates. Original logs
and harness text/hash remain retained. All five GNU configure/build lanes pass;
full LP64/trueILP64 each365/365, provider-free Debug/Release each277/277 and
shared279/279 pass, zero skips. Both relocated provider packages execute7/7
public-only consumers. All19 affected strict TUs per ABI pass. Exact record
and installed-log hashes and remaining scope are in
[the v6 integration review](factorizations-v6-review.md). This is not full
routine/mode verification or owner/license approval.

| Slice | Actual evidence | Scope limit |
| --- | --- | --- |
| Frozen provider-free baseline | Debug and Release each 219/219 passed, zero skips | Commit `46412183b2ae86101b2361c52376a8db8efff264` only |
| Complex Dense storage snapshot | Debug/Release each 27/27 Dense/Random tests; ASan+UBSan 3/3; GCC11/Clang19 no-exception compilation | Snapshot `49665b0ae3636df234acbd94c3cb62f66639e7b2604d14789c47a9ca9a3c4959`; combined-tree integration still pending |
| Source inventory | 17 extraction tests and exact offline regeneration pass | 3551 qualified identities; 2113 required; reviewed ASC semantic mappings still pending |
| Coverage validator | 28 independent synthetic tests pass | Rejects missing rows and unsupported verification claims; not numerical evidence |
| Independent ASC format codec | 16 Python fixture tests pass | Normative/fixture oracle only, not production C++ parser, bounded-memory or rollback proof |
| External reference dependencies | LP64 and true global ILP64 Release static, all four precisions; each 111/111 upstream CTests passed with zero skips | These upstream tests do not verify ASC wrappers; XBLAS disabled |

External indexes: `baseline-46412183/index.json` and
`p01-scalar-02/index.json` beneath `../asc-cpp-evidence/lapack-array-io`.
Upstream logs are under `logs/lapack-{lp64,ilp64}-*`; their exact dependency
attestations are recorded below. Checked foundations and bounded printers are
committed. Native LU, production codecs and the provider facet have newer
implementation and test evidence, with integration and remaining scope tracked
separately below.

## Integrated foundation/display checkpoints

The first implementation commit is
`a797316baffc223bb58acbf3ec6d7539e168b0d9`, tree
`bab2720c9069e613428a827fe5d9c52234a7fd65`. Its new foundation and display
code was tested in tree `832edcc11bd1d29f5cce7a93d3989c1b308669de`:
Debug and Release each passed **244/244 CTests with zero skips**, including
installed component isolation, BLAS and Random regressions. The later tree
adds only explicit Doxygen return tags and matching ABI hashes; strict
Doxygen then passed **60/60 headers, 1199 public members, zero warnings**, and
the link checker checked 73 Markdown files. These exact identities are kept
distinct; no full-suite run on the documentation-only revision is inferred.

[Sanitized checkpoint records](verification-checkpoints.json) retain actual
counts, command exits, source content identities and artifact hashes. The first
live-tree diagnostic run failed 9/241 integration checks (Python working
directory and changing/stale header inventories); these failures were fixed,
not disabled. The first strict Doxygen run failed 46 return-tag checks, which
were corrected without reducing documentation strictness. Failed logs remain
external alongside passing evidence.

Both LP64 and true global ILP64 reference dependencies now pass 111/111
upstream CTests. Their source is the same exact pinned commit. Build identities:
LP64 `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`;
ILP64 `8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
XBLAS is disabled in both. Standalone ASC LU ABI and bounded static-object
allocation checks now exist; installed facet, broader runtime/link-map and
complete routine/mode gates remain separate.

## Native and reference LU snapshots

`p04-native-lu-04/index.json` (SHA256
`f6aa8471106d6deca91e82a3e3ddbcffa9388ef410e26faa4846c39444cef3f2`)
indexes the native S/D/C/Z GETRF/GETRS source snapshot and actual tests:
Debug static, Release static/shared and Clang19 ASan/UBSan each passed their
six-test scoped selection, with zero skips. Relocated installed Dense-only
static/shared consumers each passed and called all eight native exports.
The LU test emitted 240 successful numerical case records per run, alongside
executed validation/singularity assertions. These are reconstruction/residual
cases, not 240 upstream routines. The source header/implementation/test hashes
remain unchanged at this integration checkpoint.

`provider-lu-lp64-01` and `provider-lu-ilp64-01` contain actual standalone
C/C++/Fortran probes, cold-process scalar tests, instrumented ASC provider
tests and source identities. Each ABI passed 40 reconstruction and 36 solve
residual cases, including 67-by-67 blocked GETRF, plus singular/failure tests.
The [ABI review](provider-abi-review.md) identifies exact provider inputs and
the bounded allocation/sanitizer scope. Test injections are not numerical
convergence evidence. Row-major packing and installed facet integration are
not established by these standalone results.

The working mapping now self-reviews eight native operation contracts and
marks them **implemented-unverified** pending exact contract-bound evidence
closure. The eight reference rows remain **in-progress**, since their
column-major slice does not complete the required layout contract. All
**2113** required reference rows still require complete implementation and
verification; no advanced/specialized family, dependency or mode is removed.

## Production array I/O integration

Dense text/binary production tests include all 12 wire scalars, integer aliases,
independent fixtures, rank/layout/strided views, malformed input, every-byte
truncation and corruption for tiny frames, injected stream failures, staged
rollback, explicit path truncation and checked EOF limits. A deterministic
bounded mutation driver executes 8192 mutations plus two valid seeds; it is
not coverage-guided fuzzing. Standalone normal and ASC-I/O-instrumented
ASan/UBSan runs pass; the upstream libraries are not part of this parser lane.

Integrator diagnostic `logs/p03-integration-ctest-01.{log,xml}` passed native
LU, Dense I/O and Dense mutation tests but failed the still-changing Sparse
test's empty-storage live-allocation oracle. The resource test double was
counting null zero-byte successes as live allocations despite Core releasing
no storage for those requests. Successful request budgets and actual live
storage are now tracked separately in that test; complete integrated reruns
remain required. Failed logs are retained, not converted into passes.

Strict Doxygen diagnostic `docs-p03-diagnostic-01` found two split `@ingroup`
commands and five missing Sparse getter return tags. These were corrected;
no documentation check is relaxed. Integrator header review also added the
direct `<concepts>` include used by Core scalar parsing. Earlier passing
source identities are not silently attributed to those later edits.

The P00 independent Python codec retains all 16 original tests after a
behavior-preserving style refactor; strict Pylint now passes. A generated
Python bytecode cache was moved recoverably outside source to
`source-python-cache-01` rather than committed.

Adversarial long externally supplied Status messages exposed hidden allocations
in copying error propagation. New borrowed array-stream boundaries retain only
ErrorCode/native code and bounded report progress; owner allocation failures
move their original owning diagnostics through private Result access, preserving
all public accessor behavior and message contents. Prepared 8192-byte messages
and 4096-byte provider names are tested without allocating their fixtures inside
the probe. The first resource subset failed 15 assertions in the initialized
Dense factory, revealing an additional copy; after its correction,
`logs/p03-resource-ctest-02.{log,xml}` passed 3/3 tests, zero skips. The strict
Clang 18 resource/example check also passed (`p03-resource-example-tidy-02.log`).
Frozen full regressions remain required; this does not certify every unrelated
existing Core validation diagnostic as allocation-free.

The [provider integration review](provider-integration-review.md) records actual
LP64/true-ILP64 registered, installed, source/build-hidden, minimum-CMake and
instrumented ASC tests for the column-major eight-operation slice. These are
separate frozen snapshots, not evidence for row-major or the remaining LU family.

## Combined regression and subsequent adversarial corrections

Staged tree `97f806d9e0d51684d022b9154b458e54d45c8892`, archive SHA256
`1d2e51da68566a7bccd79df9e46f8485076d00ba294ddd46dbc7a9d77b5b8018`,
passed the full provider-free Debug and Release suites: **259/259 each, zero
skips**. Records are `p03-p04-97f806d/ctest-{debug,release}/record.json`.
The increase from 244 is eight normal/no-exception checks for four new headers
and seven actual native-LU/array-I/O/resource test executables. Existing BLAS,
Random, component-consumer, metadata and hardening tests remain present.

Strict Doxygen then exposed three unsupported private friend declarations in
XML as apparent public compounds despite the configured friend-hiding option.
Only those private declarations were placed in an internal documentation
section; no public documentation check was relaxed. Tree
`80be6bb886b503594e4faf461be7c54d26546a51`, archive SHA256
`d582ec415def2a23b610ece1284540d2f3ea05c2d282bede7694b078235d0a27`,
passes 66/66 headers, 1300 documented public members, zero warnings and 77
exported Markdown files. These records are in `p03-p04-docs-80be6bb`.

The combined suites did not expose all hostile-size diagnostic paths. A new
64-byte Dense binary header whose payload fits u64 but whose full frame
overflows, plus three empty-shape owner-layout cases, produced four allocation
assertion failures against the frozen old implementation
(`logs/p03-overflow-old-test-01.log`). New internal nonnegative size arithmetic
and Dense owner-layout preflight reject these before public Core operations
construct owning diagnostics. The public Core Checked*/Extents/Layout behavior
is unchanged. Dense I/O, 8194 deterministic mutation cases and the enlarged
resource/overflow test now pass the actual three-test diagnostic subset
(`logs/p03-overflow-ctest-02.{log,xml}`); strict Clang18 checks pass. An earlier
mistyped CTest selection found zero tests and failed; it is not credited.
Sparse's independent overflow tests and huge-empty COO preservation checks are
being rerun on their corrected frozen fixture. A bad test-limit setup in its
previous snapshot is retained as a failure, not skipped.

Package review also found an unbound installed metadata record and a flawed
required-provider rejection oracle. The corrected exact metadata digest is
bound in the trusted generated config, and a successful required-provider
control prevents rejection tests from failing for the wrong reason. The new
LP64 and true-ILP64 snapshots each pass 20 selected tests (ten provider tests,
four metadata closures and six fixture actions), including 28 package cases.
See the integration review for exact identities, raw paths and trust boundary.
The earlier required-negative cases are not credited independently.

## Corrected coherent implementation checkpoint

Tree `d3675f3ecf53ee91cb54747e6379afdc42ed3538`, archive SHA256
`ce75f381e24019458a9c04468b6cc07ae3afb458af6dea2e0212f5a40a2db08e`,
includes the arithmetic, metadata and tooling corrections above. Its full
provider-free Debug and Release suites each pass **259/259**, zero skips.
Clang19 ASan/UBSan passes all **11 selected tests**, with ASC Core, Dense,
Sparse and Random rebuilt with instrumentation; system libraries are not
instrumented. Strict Doxygen passes **66 headers and 1300 public members**,
zero warnings; Markdown validation covers 77 exported files. Raw command,
JUnit and unchanged-source records reside in `p03-p04-d3675f3`; their hashes
are retained in the sanitized checkpoint index. This is not a full-provider
or whole-suite sanitizer claim. New unregistered LU/Matrix Market sources and
the next layout-workspace correction are deliberately outside this snapshot.

Sparse's separate final snapshot13 additionally passes Debug, Release,
Clang19 ASan/UBSan and shared-library selections each10/10, plus relocated
static/shared Sparse-only examples each1/1. It executes all72 scalar/storage/
wire combinations, 208 shape/special cases, 24582 mutation/seed cases and the
corrected overflow/large-empty owner tests. Its source archive SHA256 is
`63a394d4f777ad9b80bd1a0d4f96cc95c692550798cd61b84423becb25ebbcff`;
`p03-sparse-13/verification-summary.md` SHA256 is
`ae3218f983c3f98ff23bf240bd4fa68ba19f1b8f885f3b1653cf2c06b734adf7`.
Strict Clang18 and GCC11/Clang19 normal/no-exception header checks also pass.
Earlier failed fixture and implementation diagnostics remain preserved.

The sanitized checkpoint index preserves each older record's actual scope
rather than attributing its pass to later edits. Complete contract-bound
LAPACK evidence still credits zero verified reference routines.

## Expanded LU integration in progress

The next `incremental-lu-v2` candidate registers 24 additional actual S/D/C/Z
GETRF2, GETF2, GETRI, GESV, GEEQU and GEEQUB operations, and corrects the
ASC-sized layout-workspace role. GEEQU/GEEQUB support both layouts; other
registered routes in this candidate remain column-major. Thirty-two reference
rows are in progress, zero fully verified; the complete denominator stays2113.
Scoped source/ABI/numerical/allocation reviews remain distinct from final
installed and normalized contract evidence. LP64 GEEQUB subnormal mathematical
success remains unmet despite accurate tested reporting of its upstream failure.

First integrated tree `cc07b3abdc0873e04c15ebf676690c78f331993a`, archive
SHA256 `9603251bccaf3ca1e657414d5d0e6bb18799391e671a0c569c8a5ca4b4f5a8e1`,
builds completely and passes strict Doxygen68/68 headers,1351 public members,
zero warnings. Its full LP64 and true ILP64 runs each execute281 tests with
277 passing and4 failing; provider-free Debug executes259 with258 passing
and1 failing, zero skips in every run. Failures are the stale exact capability
name oracle, omitted optional export in two package inventories, and a bad
empty-memory-view construction in the newly added installed consumer. No
test is disabled. Raw records reside in `p04-lu-v2-01`.

The three corrected test files are frozen as tree
`6bcfa768af2f6a923d49f50fc9c89821cc90360e`, archive SHA256
`6a63cdf502a25b0e7291eb3b2c2b36471b15ee2c4627d8d1d4208d5e84f11dd5`.
Fresh full LP64 and true-ILP64 runs each pass **281/281**, and provider-free
Debug and Release each pass **259/259**, all with zero skips, in
`p04-lu-v2-02`; production bytes are unchanged from the first candidate.
The new installed consumer then received style-only decomposition/direct-include
corrections; strict Clang18 passes (`installed-consumer-tidy-03.log`). Its exact
corrected source independently passes1/1 actual installed CTest for each ABI
against the relocated02 packages in `p04-installed-lu-style-01`. This change
does not alter library code or weaken numerical assertions. Raw failures and
passing record/log/JUnit hashes are preserved in the checkpoint index.
Later row-major LU and Matrix Market work remains outside both snapshots.

## Both-layout LU checkpoint

The frozen incremental-lu-v3 source tree
`8c53d8d41893191b090a4e2847b8894b5b6a8e62`, archive SHA256
`893f7c56b49ddc656465ffa49b9b14dec6d472e5d8ea7d368454a787b450cb6c`,
passes full LP64 and true ILP64 suites each **285/285**, zero skips. Installed
mixed-layout consumers, original BLAS/Random regressions and component isolation
are included. All four new layout scalar processes pass Clang19 ASan/UBSan in
both ABIs with ASC adapters/foundations and instrumented baseline Core/Dense;
foreign libraries are not instrumented. Strict Doxygen passes68 headers,
1351 members and zero warnings. The unchanged production in candidate01 also
passes the full provider-free Debug259/259 suite.

Candidate01's two provider build failures (missing test-local snapshots) and
double-scalar sanitizer abort (invalid empty leading dimension) are retained.
Candidate02 corrects only these tests, without disabling checks or weakening
assertions. Detailed semantics, packing/INFO publication rules, exact artifact
hashes and scoped test limits are in [lu-layout-review.md](lu-layout-review.md).
The mapping still records32 reference operations in progress and zero fully
verified routines out of the unchanged2113 required denominator. Equilibration's
wide-empty original row-stride edge is a separate follow-up; this layout change
covers the original24 GETRF/GETRF2/GETF2/GETRS/GETRI/GESV routes.

Core/Dense/Sparse Matrix Market integration is a separate frozen candidate
`a26e0e7587b56f8838edd4f54d6acbd458e98800`, external
`p10-matrix-market-01`, archive SHA256
`e865f24fa6c2b6316a3581f33b6a369abdb74a5e200bd98e78a972355a7dffb6`.
Its Doxygen71 headers/1413 members/zero warnings and selected ASC-instrumented
Clang19 ASan/UBSan6/6 checks pass. Full Debug/Release each267 pass4 fail/271;
shared Release269 pass4 fail/273, zero skips. The four exact header inventories
omitted the new Core/Dense/Sparse headers; no numerical check failed or was
disabled. This is not credited to the LU checkpoint.

Corrected Matrix Market tree `74ff2fb12edd81b011bddc2efa44239bcbbad6f6`,
archive SHA256
`f8e1e1d4cacbaff08e5e2e97d6903440532a8f2f421541855d6c51cd9e684361`,
passes full provider-free Debug271/271, Release271/271 and shared
Release273/273, zero skips, in `p10-matrix-market-02`. Production, numerical
tests and examples are unchanged from candidate01. Both candidates' failed and
passing raw record/log/JUnit hashes remain indexed. Doxygen again passes71
headers/1413 members/zero warnings and Markdown passes. Actual relocated
consumers execute9 aggregate tests plus1 Dense-only and1 Sparse-only test in
each lane. The shared ELF inspection reports no LAPACK/Fortran dependency or
Dense-Sparse edge. Its Release baseline path is empty, so this observation is
not an equivalence check against the retained older Debug ABI baseline.
See [matrix-market-review.md](matrix-market-review.md) for exact scope.

Native Cholesky/QR and GECON/GERFS/GESVX are separately frozen after scoped
numerical/sanitizer tests. They remain unregistered here; reference Cholesky/QR
and LU helpers are active independent work, not completed families. Explicit
tiny-input numerical limitations still require mathematical-success disposition.

The real incremental draft is [PR #47](https://github.com/AI4SciComp/asc-cpp/pull/47),
targeting `develop`. Its existence is not owner/license approval or remote CI
success. Newer uncommitted work is not represented as pushed implementation.

Required gates remain the complete P00–P11 runbook, including native and
reference numerical reconstruction/failure tests; LP64 and true ILP64 provider
ABI tests; array malformed-input/rollback/resource tests; no-hidden-allocation,
transfer or densification evidence; BLAS/Random regressions; installed component
isolation; strict full-profile closure; platform, sanitizer, docs/style and
provenance evidence. Zero tests, missing tools and unexpected skips are failures
or missing evidence, never passes.

## Advanced LU v4 available integration gates passed

The [v4 review](lu-v4-integration-review.md) records candidate01 tree6146d75
and its unchanged44 partial-reference/zero-verified ledger. Full LP64 and true
ILP64 each execute315 tests (312 pass,3 fail); provider-free Debug and Release
each execute271 (269 pass,2 fail), all zero skips. Strict Doxygen74/1457/zero
warnings and Markdown pass. The failures are an unsorted exact header baseline
and a condition-independent installed refinement oracle, both corrected with
retained evidence. Corrected consumer diagnostic07 passes both actual ABIs
and strict Clang18 analysis. Corrected treeeebeb3a, archive SHA256
`3b39468e3e13c0dbe66685c595fcdb167613f6529dcf67053a61691661a9f9cc`,
passes fresh full LP64/trueILP64 each315/315 and provider-free Debug/Release
each271/271, zero skips; installed advanced/original LU consumers each2/2
perABI and strict Doxygen74/1457/zero warnings/Markdown pass. Mapping SHA256
is `955af4f0258eefa156d5e2434277724e8570fb7c6d729409a92b40a01be9fb77`;
all44 reference rows remain in_progress, zero fully verified. The exact
validator passes this partial accounting. Native/provider Cholesky/QR,
original LU INTEGER guards and LU helpers remain separately frozen and
excluded from this v4 snapshot. Provider-specific workspace/context admission
is a newly explicit required gate, not an excuse to stop independent work.


## V9 current composed checkpoint

V9 full02 actual LP64/ILP64 each493/493, provider-free Debug277/277 Release277/277 shared279/279, zero skips; installed13 eachABI audited in full raw logs. All-ASC-C++ ASan71 eachABI passes, foreign archives/runtimes unsanitized. All23 strict02 TUs passed except probe corrected03; current preflight strict05 passes bothABIs. Current05 six scoped lanes each8/8; control03 six lanes each1/1; docs04 92headers/1827members zero warnings. Current05 all82 changed C++ format, coverage and public-header contract checks pass. Full02 is not relabeled full05; exact source deltas and command artifacts preserved. Full normalized modes/platforms/shared-provider/required mathematical failures remain open.

Exact identities, retained failures and source composition are recorded in
[factorizations-v9-review.md](factorizations-v9-review.md). Next implementation
is the independently reviewed eight general-band LU routes; no remote write
is authorized for this resumed session.


## V10 general-band composed checkpoint

V10 composed checkpoint: full01 LP64/ILP64 each505/509, provider-free Debug273/277 Release273/277 shared275/279; exact same four stale header-count failures and zero skips. Only test oracle count92->93 changes in02; five fresh header lanes each4/4 pass. Full01 original records remain failures, no full02 claim. Installed14 eachABI, all-ASC-C++ ASan16 eachABI, strict8TUs eachABI, Doxygen93headers/1848members/zero warnings, format12/coverage/publicsurface pass. Fresh verbose unchanged01 numerical replay eachABI4/4 passes and records21factor+234solve profiles per scalar. Foreign archives/runtimes unsanitized. Normalized mode, platforms, shared provider, XBLAS and mathematical/provenance gates remain required.

See `lu-band-review.md` for source identities and the audited55 command records.


## V12 implementation checkpoint

V12 implementation checkpoint: exact frozen product matches live root; format58Cpp/coverage/publicsurface/Markdown and Doxygen102headers1969members0warnings pass. Full5lanes/ASan90perABI/strict36TUsperABI still running. Original ten Band and four GT ordinary required math failures retained. Root GETRF/GETRS independent six4-test replay confirms old4/4fail bothABI, fixedRelease/ASan4/4pass each; separate correction not imported yet.

External `p05-band-expert-integration-v12-01/implementation-checkpoint-01/audit.json` SHA256 `ead2db495ac6f3c6aab84d3dc97f43cf62b6c8fa1abdef2829db45fc89be07a4` records current completed command hashes. No full-suite success is inferred from a pending process.


## V11 completed full command evidence

Full LP64/ILP64 each545/550, exactly4required GT extreme mathfail plus package integration300s timeout,0skips. Installed15 subtests passed bothABI before timeout; later isolation/control cases unfinished. Providerfree Debug277/277 Release277/277 shared279/279. ASC-only ASan each46/50 with4mathfail. All30strict checks, Doxygen97headers1911members0warnings,format25Cpp/coverage/publicsurface/Markdown pass. No full success or normalized mode claim. Unchanged300s package replay prepared in fresh separate workspaces, waits for compiler load to fall.

Completion audit `p05-tridiagonal-integration-v11-01/completion-audit-01/audit.json` SHA256 `2b357813249acd33b7ac4487a7ebaca4c80ad16b08ca5075438cf0a266eede03` binds every command artifact and856 source files to frozen tree142997ad4a4bffdf612d0a0171e9da7391a05206, and preserves complete full/sanitizer/installed logs without invoking CTest again. The fresh replay will have separate records; original full failures are permanent evidence.


## Current root integration completed

Current frozen tree5dd66163 six18/18 lanes pass (all-ASC Debug,Release,ASan bothABI),0skips; all53CPUCore/Dense/provider TUs rebuilt. Exact112/168 unique profiles per scalar, all actual native INFO0. Format4/coverage/publicsurface pass. Six historical strict records remain valid through byte-identical actual compiler-read TU/headers/generated ABI config. Foreign archives unsanitized. No full-suite or pivot short-write completion claim.

Frozen tree `5dd66163f10e34fdd72e19da35c79370575775cb`, archive SHA256 `46d0f26a92f04dddfae0d383d966854b9b1df2199ab8d15ed56a341ce7e1b018`, mapping `ae514d2343cd688ab8f3880e1dd7e8b0c2e72decd73555e44f835c42d7cd6451`. Audit `p04-lu-info-integration-01/completion-audit-01/audit.json` SHA256 `b5dcd01f46262406469cf9581db80ddfdcb638fb6281fa3e6e4d6304712ff39f` binds21 actual commands, all six compiled source closures and complete emitted mode sets. Existing V12 full runs are older INFO bytes and are never relabeled as these tests.


## V12 completed integration evidence

V12 exact frozen current-at-c8ae2f2 product: full LP64/trueILP64 each634/648, exactly10required Band plus4GT mathfail,0skips. Providerfree Debug277/277 Release277/277 shared279/279; installed16eachABI pass including256Bandworkflows/2048calls/256stale rejects. All-ASC-C++ ASan each80/90 with10Bandmathfail,0skips; foreign archives unsanitized. All72strict checks, Doxygen102headers1969members0warnings,format58Cpp/coverage/publicsurface/Markdown pass. Complete original command/source/rawlog audit retained. Later originalLU and expertINFO fixes are separate snapshots and are not verified by these older source results. No full routine/mode/platform/shared-provider/XBLAS/provenance or mathematical gate closure claim.

Completion audit `p05-band-expert-integration-v12-01/completion-audit-01/audit.json` SHA256 `11503bf22178a23b627b9e4e0349a4dab3b00e66fb3a6b178600f827ddb7af45` independently verifies99 actual command records, 914 frozen source files, complete installed/full/sanitizer logs and actual Band INFO profile counts. Required reference denominator remains2113 with250in_progress/1863not_started; native20 remain separately implemented_unverified and fullyverified0.


## Current composed integration completed

Current originalLU8+expert16 INFO integration: six22/22 Debug/Release/ASC-only ASan lanes pass,0skips. All53CPUCore/Dense/provider TUs rebuilt eachlane; exact312/468expert modes/scalar and preserved112/168originalINFO profiles pass. Format5/coverage/publicsurface pass. Historical6strict records apply through byte-identical actual compiler-read changedTU/headers/generatedABIconfig; no freshstrict or fullproject claim. Foreign archives/runtimes unsanitized; output-pivot correction and full mathematical/mode/platform gates remain separate.

Frozen tree `e8d719945740a6640fa963a37c68b73c64b9f0ae`, archive SHA256 `7f75537dafded1f9ae2b8e002e50f41285abed84c6ae73dac126d99acda10211`, mapping `d566272179926c1ae9c67dabccc0cfd37acca14b021cd31b4c471c868d64c4da`. Completion audit `p04-lu-expert-info-integration-01/completion-audit-01/audit.json` SHA256 `e6da4a3267fad30efe4ab36e19bdaabf64eff43da37e2f67fb2b4f4527e50dee` verifies21 executed records,921 exact source files, every compiled CPU source and actual returned/native mode profiles. Root product files match the frozen source.


## V13 composed integration checkpoint

GB20 V13 full02 LP64/ILP64 each720/752, exactly30 required numerical failures plus two omitted-source-inventory failures, zero skips. Provider-free Debug280/282, Release280/282, shared282/284 retain the same two inventory failures. Final04 adds only five explicit source entries; the common source-only2/2 replay passes and its original commands match all five full lanes. Final03 formats only the new consumer and updates its mapping hashes: four fresh current-library and two fresh relocated-installed1/1 consumer tests pass. Full02 package passes17 installed families perABI with unchanged300s timeout. All58 primary CPU Core/Dense/provider TUs rebuilt in each full and ASC-only sanitizer lane; ASan each75/91 retains exactly16 GB math failures, no sanitizer diagnostics or skips. All34 changed-TU strict checks plus two final-consumer strict checks, format29, coverage, public surface, Markdown and Doxygen108headers/2036members/zero warnings pass. Exact consumer/object/provider closures audited. Original01 ABI-registration,02 format/source-policy and audit-helper failures remain preserved. No full04 suite, foreign sanitizer, normalized full-mode, platform, XBLAS or mathematical gate closure claim;270 Reference rows remain in_progress/1843 not_started of2113 required; native20 separately implemented_unverified; zero fullyverified.

Final product tree `eeb249415cbe7c8d0b0127f8af290f386bca1356`, archive SHA256
`15ff5e4717103d77eaf9ce6967f2338f410fd682242f2a7abf11bf1754f0bbfd`, mapping
`442e2f8c421b9ec5fd6149eaa23981b4abb3f01ba5e03fb7c6c747cadb3e4474`. Completion audit `p05-general-band-expert-integration-v13-04/completion-audit-01/audit.json`
SHA256 `99914ff87c875d8fdac08359ab600ce0787fdad273fa2c678df7ab972d67f32e` binds the executed records, primary production objects,
final public consumers and unchanged actual provider prefixes.
The audit first included a separately rebuilt subproject consumer in
its primary-library uniqueness check; that failed attempt remains
recorded. The final audit checks the exact primary library target
object directories, still requiring all58 production sources.


## Current composed root checkpoint

Root two45/45 fresh Release lanes include38 affected numerical/fault/count controls,4 placement tests, package and2 policy tests. Four exact-input Debug/ASC-only sanitizer root replays38/38 reuse sealed current binaries. Fresh root production58 TUs per ABI; agent six58-TU lanes and24 fresh strict remain separately identified. All17 installed families per ABI pass. No full-project suite, foreign sanitizer, full mode/platform/shared-provider/XBLAS or required mathematical closure claim. Source-bound profile audit matches472272 main modes,321648 main native calls,47232 native warm calls and41472 reuse calls across six root executions. Format15/coverage/publicsurface pass. Initial build-record directory collision was rejected before compilation and preserved; corrected runner reused successful configures. Denominator2113/270 partial Reference/1843 not_started, native20 separate, zero fullyverified;30 required mathematical failures remain open.

Frozen product tree `b5168234598803a41158c7ecf613e73af1ceb0e9`, archive SHA256
`b2ed985dac1bbc3078ba38dfc547b91449e1bc19bcbb99b2d117fbf254f3c879`, mapping
`91defdbf15f2d7df6cbcb630ccc8b1e352ba80b5fe0e2cc0ae76b7bbefbb829c`. Completion audit
`p04-lu-aux-integration-01/completion-audit-01/audit.json`, SHA256
`9f7356bcd29dd8c25fc673624fddcdc1fc3085f496fcadaa5be9bea202bc6dc0`. Root package executes all17 installed
families perABI with the existing300s timeout. All prior failed-source
controls remain in the original INFO02/pivot01 handoffs; no old or unexecuted
configuration is relabeled current.


## Root P02 printing-mode checkpoint

P02 modes/rank0 five-file delta integrated. Fresh GNU11 Debug/Release/shared and Clang19 ASan+UBSan each6/6; Debug libc diagnostic6/6; six fresh isolated relocated installed Dense/Sparse consumers each3/3:48 current passes,zero skips. All23 Core/Dense/Sparse production TUs rebuilt in each of four lanes (92 objects);21 Random/Utilities install-prerequisite objects have no Random runtime credit. Format3 passes; prior strict2 rehashed and source-identical. Independent literal mode endpoints cover four real/complex scalars, general/fixed/scientific1/32 and custom C++ locale (48 Dense/144 Sparse cases); 417 exact rank0 budgets and288 sink boundaries,12 Dense/22 Sparse empty fixtures and9/27 live-object scratch aliases. Full modes/platforms/read-count crossproducts remain open. Preserved initial global no-exceptions build failure at unchanged Random seed catch is not closed by printing passes. No LAPACK/native routine credit;270 Reference partial/1843 not_started of2113 required,20 native implemented_unverified,zero fully verified.

Audit SHA256 `9318cb2fbf4167626ed55c46c8b05caad77647b04ab75d18f6fb66107e4272fc`.


## Root GBEQUB v14 checkpoint

Six fresh current ASC production lanes59 TUs each. Release54 tests each38pass16required mathfail; Debug43 each27pass16mathfail; Clang19 ASC-only diagnostic36 each20pass16mathfail, including current public consumer. All266 selected tests execute,170pass96fail0skip;14 strict and6 static checks pass. Both18-family installed suites pass. Two rejected public Clang configurations retain the unchanged GNU-only package gate. No full-project suite, Clang package, foreign sanitizer, shared-provider, full normalized modes/platforms/XBLAS or mathematical closure claim. Four actual GBEQUB rows remain in_progress:274 Reference partial,1839 not_started of2113 required;20 native implemented_unverified and zero fully verified. Required130 XBLAS-dependent routines remain missing and required.

Audit SHA256 `caadfc7ae84cb19fb042d9840abf67effa9158b285e15a86fad7f600173bd1b5`.


## Root P03 binary stream checkpoint

Fresh root Debug/Release/shared/ASC sanitizer each3/3;three fresh relocated installed-only Dense consumers each3/3 with exceptions disabled:21passes zero skips.76 fresh Core/Dense objects;33 Sparse/Random/Utilities install prerequisites have no runtime credit. Initial installed harness missing-header build failure retained and fixed in new harness/directory. Prior exact-source strict/three diagnostic lanes rehashed, no full P03/platform/allocation-mode closure. Independent72-byte i16 fixture:75 reads and145 writes (220 workflows per lane), every failing boundary,3-byte short chunks, whole-file EOF failure before commit, exact reports/native diagnostics, whole destination/padding rollback, guarded staging/metadata/scratch, and failed-cursor nonreplay. Other scalars/formats/modes/platforms and complete P03 acceptance remain separate.

Audit SHA256 `b3838127365dc9c7a6c3e048dd31aa558192f9ac82bc86a7743f9306abf98d8f`.


## Full triangular bounded candidate acceptance

Six5/5 lanes:30 executed current tests zero failures/skips;6272 analytic workflows/1143040 assertions and7696 failure/preflight profiles/61072 assertions eachlane;1762 source-count checks and12 complete actual compiler-emission signatures perABI. One new triangular TU andeight test TUs perlane;59 prior ASC production TUs reused by exact identity. Four compiler/header modes and11-file format pass. Two INFO-zero diagnostic binaries fail1/1 each as required. Root registration, installed package, all memory/normalized numerical modes and platform gates remain open.

Candidate08 audit SHA256 `ac678d6895fd2d5e8f724494019eaa59e3d99ac39d49a37d5df2957a9c616d2b`. Root v15 full-suite/installed verification remains pending. All prior strict failures and both deliberate INFO-zero diagnostic failures are retained.

Root triangular v15 static checkpoint: corrected02 public surface, Doxygen110/110 headers and2068 documented members, Markdown and11-file format pass; fresh six-TU strict analysis passes both actual ABIs. Unchanged01 coverage passes; initial01 surface failure retained. Full01 suites remain executing; fresh02 Debug/header/package and ASC-only sanitizer checks are prepared, not passed. Added19 executed records; no new fully verified routine credit.

Root triangular full01: both815-test suites executed, each767 passes/48 failures/zero skips. Each retains46 required mathematical failures and2 public-surface failures from the incorrect ABI manifest prefix; corrected02 source differs only in that manifest. Both19-family relocated Release consumers pass. Original orchestration rejects the unexpected2 failures; no full-suite pass claimed. Corrected02 Debug and fresh source/installed surface replays remain pending. Added8 executed records.

Completed root triangular bounded integration: Twelve full-storage TRTRI/TRTI2/TRTRS routes registered;286 Reference in_progress/1827 not_started of2113 required,20 native implemented_unverified,zero fully verified. Full01 Release each767/815 pass with46 required mathematics plus2 old manifest failures andzero skips. Corrected02 Debug each24/24, ASC-only Clang19 ASan/UBSan each9/9, fresh surface replay02 each6/6 includingall4 fixture setups pass. All60 Core/Dense/provider TUs freshly rebuilt in eachof6 numerical lanes (360 objects), all4 Release/Debug relocated19-family packages pass;12 fresh strict checks and Doxygen110headers2068members pass. Foreign archives unsanitized. Initial surface-replay report-directory failures and the Debug runner18-versus24 fixture-count assertions preserved. Required130 missing XBLAS routines and full normalized/memory/extreme/platform/program gates remain open. Completion audit SHA256 `5455d0c0c00fa196b8d0b374298cb53388bf223799e798163938748cf14ee94e`.

Triangular v15 documentation reconciliation: three documentation/metadata files only;6 checks pass,1467 old artifacts/compiler inputs rehashed unchanged. Audit `d1b491a99cd3e0bd932ddf2f3abc2817778630538a8632f3bfe702086f5f886d`. No new numerical/full-suite claim.

Triangular edge acceptance: Six current tests pass, zero failures/skips:7952 profiles/62224 assertions perlane. 24 fresh test objects;60 prior production TUs perlane rehashed and reused. No new production build, foreign instrumentation, installed package or full-suite execution. 256 additive edge profiles preserve all prior7696 profiles byte-for-byte. Existing required mathematical/XBLAS and wider acceptance gates remain open. The new cases are64 real zero-diagonal TRTI2 source-fidelity profiles and192 live A/B-overlap query/execution rejections; neither implies mathematical invertibility for singular input. Fresh strict bothABIs, one-file format and coverage validation pass. Reference286partial/1827not_started of2113 and20native implemented_unverified remain unchanged. Audit `9a222d37e10396f521e826c3d7af771618dbc5182aa5ca5cae4737b83e33d55a`.

Triangular estimator candidate06 (unregistered TRCON/TRRFS8) is preserved with
audit `566fe87610e796155c698ea7cd6705a7e2c72f1f551fa5e0d26bae832451147a`. Six current12-test lanes each have4passes and8ordinary
required mathematical failures, zero skips;54 new adapter/test objects and
14 applicable strict checks are audited. The166 foreign closure objects and
16 actual native-emitted signatures are source-bound. Full current root/installed
integration is next; this scoped checkpoint promotes no routine to verified.


## Recovery and PPSVX boundary slice, 2026-09-10

The accepted native/I/O subset remains review-ready with20 verified native rows;
its product/test/workflow inputs are unchanged. Clean local `dcf8eb52` and the
external candidate18 survived; no candidate19 had started. The scoped validator
and native alias environment smoke pass. The latest bounded PPSVX result is
[the candidate21 review](packed-cholesky-expert-review.md): six actual ABI/mode
lanes pass faults and behavioral tests, while all four ordinary scalar math
tests per lane retain their candidate18 signatures. Strict2 and format pass.
Audit `f4437d76f079b358643b2c266847c8f3a8526bc2796094f4e8a55409fdb8e51b` binds the external source, command records and reused libraries.
No PPSVX registration or full numerical-family verification is granted.
The state pointer advances to candidate22 pre-native output-read observation.

## PPSVX candidate22 family checkpoint, 2026-09-10

Candidate22 completes the calibrated pre-native output observation in all six
prepared Linux/static configurations: real LP64 and true ILP64, each GNU11
Debug/Release and Clang19 ASC-only ASan/UBSan. Negative reads, supplied-factor
input reads, native writes, and permitted post-return complex preservation are
distinguished at foreign entry. Separate Clang local-N=0 load observation passes
both ABIs. All candidate18–21 assertions and failed attempts remain retained.

Real-provider concurrency passes all six configurations. GNU11 TSan passes in
both ABIs with ASC and consumer instrumentation; provider code is uninstrumented.
Relocated, dependency-isolated public PPSVX consumers pass against isolated
static candidate installs in both actual ABIs, including returned-factor reuse.
The pending integration patch, header ownership/digests, installed headers,
symbols, strict analysis, formatting and documentation checks are retained.
Shared-provider and Clang package admission remain blocked by existing policy.
No PPSVX product or registration change is applied to the feature worktree.

The four mathematical test processes still fail 176 composite assertions per
configuration: 44 distinct scalar/mode/value cases repeated over four layouts
and triangles. Their signatures match candidate21 and fresh staged integration.
The [numerical disposition](packed-cholesky-expert-disposition.md) identifies
five provider arithmetic causes: tiny condition and forward estimation,
maximum-value condition and refinement diagnostics, and subnormal equilibration.
The unchanged numerical gate requires an explicit provider/algorithm decision;
direct-call fidelity does not close it. The pending notice amendment is separate.

The [family review](packed-cholesky-expert-review.md) binds the completed external
candidate and 201 raw command records through audit
`6d4c244d7793a4e475179d4aa2c3e5438052abdc33c1154885b19f0504891a66`.
This command count is separate from historical normalized evidence totals.
PPSVX remains 0/4 root-registered and 0/4 fully verified. Reference counts remain
350 partial, 1,763 not started, zero complete or verified of 2,113 required.
The 20 verified native operations and accepted array-I/O subset retain
`SUBSET_REVIEW_READY_FULL_PROGRAM_INCOMPLETE` through unchanged feature inputs
and fresh provider-free installed regressions. No later summary tree is claimed
to have passed the earlier hosted checks. The next task is the recorded numerical
provider/algorithm decision, followed by the still-required admission gates.
