# Verification summary

The complete program is not verified. The following exact checkpoints are
kept distinct from newer uncommitted integration work and from full-profile
routine/mode closure. No package-wide success is inferred from a scoped run.

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
records. The draft describes that checkpoint, not newer uncommitted v7 work.

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
Retained PBCON lower-complex scaled condition, GELSD zero-RHS/wide-minimum
process-safety and single-precision tree-storage, and GELSS wide-path returned
right-vector gates are listed in [blockers.md](blockers.md).

Root-owned P08 ordinary TRSYL4 is implemented separately and not in those162
registered rows. A scoped direct snapshot passes4240 nonempty numerical cases
per actual ABI, including real Schur blocks, complex conjugation, explicit
scaling and warning outcomes with allocation audits. Initial9-test contract
selections fail4 each; corrected snapshot `p08-sylvester-contracts-g73W72tm`
passes9/9 and strict5 translation units per ABI. A fresh13-test alias snapshot
is running under `p08-sylvester-alias-MX9v4Gek`; no pass is assumed. This is neither
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
