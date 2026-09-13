# Remaining project completion plan — 2026-09-13

Finish the current packed-indefinite work, establish a reliable path from
implemented code to verified capability, and execute every remaining required
family through that path. Resolve numerical/provider and extra-precision
decisions early. Carry platform admission and P11 delivery alongside numerical
development so they cannot become an unexamined final backlog.

This is a planning artifact requested after the project status review. It does
not change implementation states, accepted contracts, provider identities or
authorization. The technical requirements remain the [original runbook](runbook.md)
and the [master continuation](remaining-project-runbook.md). Actual scheduling
uses current code, [state](state.json), [coverage](../../docs/contracts/lapack-coverage.yaml)
and the [568-family backlog](remaining-reference-backlog.json), rather than
historical next-task prose.

**Completion target**

All original P00–P11 requirements are implemented, independently verified,
documented and usable through installed public components. Every required
Reference row and mode has passing evidence for its required profiles. Native
LU/Cholesky/Householder QR and accepted printing/text/binary/Matrix Market
functionality retain their verified contracts. Required owner/provenance
decisions are recorded. The result is implementation complete and review-ready.

Merge, tagging and publication are separate subsequent gates. Release
preparation occurs only on `release/0.9.0`, after compatibility/version review;
this feature plan does not assume the expanded API can simply be published as
the existing 0.9.0 candidate. Full native LAPACK, optimized BLAS, new GPU LAPACK,
new file formats and general sparse solvers are outside the required program.
CUDA remains experimental without its exact-commit real-NVIDIA release gate.

**Baseline and accounting**

The audited feature HEAD is `bfc95f44ff1e6ace5dfa39e05a75ba52b98203e2`.
PR #47 is an open draft targeting `develop`. The working tree contains 11
modified tracked files and 20 untracked files, including six packed SPTRF/HPTRF
producers. Preserve this work and the existing user instruction files.

The maintained backlog regenerates identically from the current inventory,
mapping and XBLAS closure. All 2,113 required rows appear exactly once. Current
generator ownership gives the following planning totals:

| Package | Family tasks | Required rows | Callable, unverified | In progress | Not started |
| --- | ---: | ---: | ---: | ---: | ---: |
| P04 | 13 | 52 | 44 | 0 | 8 |
| P05 | 119 | 550 | 184 | 232 | 134 |
| P06 | 81 | 324 | 0 | 44 | 280 |
| P07 | 44 | 132 | 0 | 0 | 132 |
| P08 | 86 | 332 | 0 | 4 | 328 |
| P09 | 225 | 723 | 20 | 16 | 687 |
| Total | 568 | 2,113 | 248 | 296 | 1,569 |

There are 288 reviewed contracts and zero fully verified Reference rows.
Reviewed contracts overlap implementation states. Native verification remains
20/20. Ledger state does not prove that no unregistered or external code exists:
the current packed producers and historical SVD work need reconciliation before
starting new implementations. The 130 dependency-blocked definitions are part
of these totals, not an additional 130 routines.

The status audit established these immediate facts:

- Committed-checkout coverage validation passes. The dirty tree fails artifact
  hash validation; nine changed integration/artifact paths have stale bindings.
- Provider-free push and PR CI each pass 19/19 jobs on the pushed candidate.
  These jobs do not establish dirty-tree or full provider acceptance.
- Fresh packed Release selections in actual LP64 and true ILP64 each execute
  43 tests: 31 pass, 12 fail, zero skips. Six required range tests and six native
  endpoint tests fail per ABI. Prior ASC sanitizer evidence has the same scope
  limitation: foreign Fortran/BLAS internals are uninstrumented.
- All 142 tooling unit tests and `git diff --check` pass.
- Selected LAPACK CI and the separate CodeQL PR gate fail. That PR gate reports
  four critical and five high security alerts requiring triage; they are not
  nine independently confirmed vulnerabilities. The push default analysis job
  also failed, while its two LAPACK analysis jobs passed.

Raw status evidence is under the existing external evidence root at
`status-audit-20260913-_0qpd4h8/`. This baseline must be compared with actual Git
and live checks when execution begins.

**Milestones and dependencies**

| Milestone | Work | Depends on | Exit evidence |
| --- | --- | --- | --- |
| M0 | Reconcile the current tree and execution ledger | Current baseline | Coherent artifact bindings, exact task ownership/dependencies, passing integrity/tooling checks |
| M1 | Stabilize packed producers and triage security findings | M0; preparation can overlap | Complete packed engineering matrix, reviewed security findings and fixes, explicit remaining numerical failures |
| M2 | Resolve provider/numerical, XBLAS and notice decisions | Start during M0/M1 | Concrete approved disposition and implemented, tested resolution for each affected requirement |
| M3 | Complete P04–P09 families in dependency order | Actual subtask prerequisites; M2 only for affected rows | All 2,113 checked routes and legal modes implemented; independent gates closed per row |
| M4 | Complete provider/platform admission | Start during M1; representative implemented routes | Every required compiler/ABI/linkage/runtime profile admitted with executed evidence |
| M5 | Finish P00/P01/P02/P03/P10 acceptance and P11 delivery | Continuous during M1–M4 | Installed workflows, docs, isolation, provenance and accepted-subset regressions complete |
| M6 | Freeze and verify the complete candidate | M0–M5 | Full-profile configure, strict coverage and all required tests pass on the frozen candidate |
| M7 | Review, integration and release handoff | M6 plus applicable authorization | Reviewed integration candidate; separately verified release gates before publication |

M2, M4 and M5 are ongoing workstreams, not reasons to stop all family work.
Use one active implementation writer in the existing worktree. With one writer,
interleave these workstreams at family checkpoints. Whole-package dependencies
remain final acceptance dependencies; a subtask can start when its actual
inputs and contracts are available.

**M0 — make the next checkpoint reliable**

1. Reconcile actual HEAD, staged/unstaged/untracked files, current handoff,
   provider prefixes, active builds and free space. Preserve the dirty packed
   work before modifying it. Reuse the existing workspace retirement review;
   update only changed facts. Cleanup is not a prerequisite unless resources
   actually prevent work. Any destructive retirement remains a separate exact
   path decision.
2. Reconcile `active_task`, `next_task`, `current_delivery`,
   `pending_verifications`, the opening of `blockers.md`, and current owner
   decisions. Retain dated history explicitly. The current task is
   `P05.required.hptrf`; old PTTRF/PPSVX starting instructions must not restart
   completed work.
3. Audit each changed artifact binding against the actual change. Prepare a
   reviewed extension using existing evidence tools and schema2 identities.
   Update generators, mappings, evidence consumers and explicit ABI/header
   baselines together where affected. Preserve historical implementation and
   execution identities; do not make old logs appear to test new source by
   replacing hashes. Keep failed executions outside the passing evidence ledger.
4. Add real producer/consumer, descriptor, workspace and algorithm prerequisites
   to the maintained task machinery. Its current generic P01/P04 prerequisites
   are not an executable dependency graph. Keep stable routine IDs. Review
   package attribution: `pstrf`/`pstf2` are currently owned by P09 but belong to
   the P05 semidefinite work; `tgsja` is currently P08 but is part of P07 GSVD.
   Reconcile generator, consumers and task references atomically. The table
   above describes the current grouping and will change after that review;
   the total required inventory must not change.
5. Give every family a next action, actual dependency IDs, source/reuse pointers,
   missing modes/profiles, acceptance evidence, and any blocker/dependent rows.
   Reconcile all 35 entries currently in the state blocker list against the
   family reviews and owner packet; some are resolved historical engineering
   defects, while newer failures are recorded outside that list. Do not treat
   the list as a current count of independent bugs.

M0 passes when the current tree's coverage integrity and backlog checks pass,
the current dirty work is fully accounted for, and no unresolved dependency is
hidden behind a generic next-action string. This gives no numerical promotion.

**M1 — finish the current slice and address security findings**

For the six SSPTRF/DSPTRF/CSPTRF/ZSPTRF/CHPTRF/ZHPTRF producers, retain the
[packed contract and fault findings](indefinite-packed-review.md). Execute the
recorded Debug LP64 selection, then Debug ILP64. Add independent-call concurrency
coverage with immutable shared plans/provider and distinct mutable operands,
workspace and reports. Complete matching shared, sanitizer/TSan, installed
consumer, ABI/export, header, strict analysis and documentation checks. Reuse
existing verified Release evidence through explicit unchanged-input checks;
rerun affected profiles after code changes.

Review all required range and endpoint failures through M2. Engineering
containment, provider-byte fidelity and numerical acceptance remain distinct.
A slice can be an explicitly incomplete feature checkpoint while its genuine
provider blocker remains open; it cannot be marked verified or used to imply
full provider admission. Packed consumer contract work can proceed once factor
provenance and storage contracts are stable, with inherited blockers recorded;
consumer verification still requires its producer prerequisites.

Triage CodeQL using exact current branch/PR analyses and code flows. Start with
critical/high alerts, then correctness warnings; batch informational style
notes separately. Determine whether each finding is a real defect, duplicate,
test-only behavior or a demonstrable false positive. Fix confirmed defects and
add focused regression evidence. Record source-based findings without bulk
dismissal or suppression. Diagnose the failed Analyze/upload job separately
from the security alert gate. Successful analyzer execution alone is not alert
closure. The P11 security gate must pass under existing policy before M6.

**M2 — decisions that are on the completion path**

Use the existing [owner packet](owner-decisions.md), not a new approval queue.
Prepare concrete technical results and exact affected material before requesting
any decision. Ordinary diagnosis, test-oracle correction and first-party bug
fixes remain engineering work. The following decisions are not prerequisites
for unrelated family development.

| Decision | Preparation and recommendation | Closure |
| --- | --- | --- |
| Provider numerical and native-boundary failures | Consolidate minimal reproductions by arithmetic/source cause and affected consumers. Distinguish adapter bugs, invalid oracle assumptions, provider defects, ABI/runtime defects and genuinely unrepresentable outputs. Recommend the smallest reviewed correction that preserves required semantics, in an explicitly identified provider revision/profile if a provider change is needed. | Approved scope, exact source/build identity, unchanged applicable mathematical tests passing, affected ABI/platform/consumer regressions passing, updated provenance and profile evidence |
| Extra-precision provider | Reuse the [130-row dependency closure](xblas-dependency-closure.json): 54 rows reach 28 external XBLAS helpers; 76 need only existing provider boundaries but are excluded by current source selection. Review the exact retained XBLAS candidate, license and C-integer/true-ILP64 bridge strategy. Recommend a separately attested external preparation, preserving existing provider prefixes. | Approved source-selection/dependency strategy; all 130 definitions and required ASC routes built, probed, independently tested and admitted in both ABIs |
| Redistribution notice | Revalidate the bound materials in the [review packet](redistribution-review-packet.md), including actual newly proposed runtime/dependency contents. Present the exact notice diff and distribution inventory. | Explicit required owner/reviewer decision; accepted notice/provenance and package contents agree |
| Platform resources or interoperability | First attempt preparation using available approved toolchains/runners. Identify a concrete missing runner, compiler, bridge or runtime; do not infer blanket Windows/macOS support from Linux. | Actual resource available and full profile evidence passing, or an explicit unresolved full-completion blocker |

Group shared arithmetic causes such as reciprocal-before-multiplication,
scaled inverse estimation, residual weighting, complex division, and compiler
power evaluation. Keep independent causes such as LATBS term omission,
SGEDMDQ mode selection, workspace-query rounding, GELSD/GELSS outputs and native
endpoint writes distinct. A shared cause record must enumerate every affected
routine/mode; passing one scalar reproducer does not close all consumers.

Correct an oracle only after deriving the actual mathematical contract and
showing why the old predicate is invalid. Retain the original failure and all
valid input classes. Do not loosen tolerances, delete required finite cases,
flush subnormals, enable fast math, or classify failures as expected successes.

The unchanged pinned provider cannot both reproduce a known required failure
and pass that requirement. A separately named first-party alternative can be
useful, but it does not verify the unchanged Reference row. If the owner keeps
the original provider and rejects every conforming corrective route, full
original-scope completion remains blocked; complete all independent work and
report that conflict explicitly. Approval of this plan is not approval of a
specific future provider patch, notice or contract change.

**M3 — exhaust the numerical backlog**

Execute every entry in the maintained 568-family backlog, including required
auxiliaries and compatibility entries. Family names below give order and
mathematical grouping; the exact inventory supplies actual scalar variants.
Before implementing an apparently unstarted family, compare existing source,
reviews and relevant historical sibling patches for reusable work. Import only
reviewed necessary changes into the existing integration worktree.

| Package | Remaining work and order | Independent acceptance |
| --- | --- | --- |
| P04 | Close the 44 callable LU routes' missing modes/profile evidence; implement the eight `getc2`/`gesc2` rows and required helper contracts; complete provider context, symbol and ABI boundaries. Start evidence closure with an existing family whose required mathematical tests pass, to establish one complete Reference acceptance path. | LU reconstruction, original-matrix residuals, repeated/transposed solves, scaling, condition/refinement diagnostics, partial factors and installed use |
| P05 | Complete packed producers, then `hptrs`, `hptri`, `hpcon`, `hprfs`, `hpsv`, `hpsvx` and their actual symmetric counterparts. Close existing full/band/tridiagonal/triangular, classic/rook/RK/Aasen/two-stage and positive-definite gaps. Add RFP conversions and algorithms, pivoted semidefinite Cholesky, and all remaining required structured variants. Route extra-precision items through M2/P09. | Variant-specific factor equations and pivot blocks, selected-triangle/ignored-diagonal rules, solves/inverse/condition/refinement, singular/rank cases, packed/band/RFP boundaries |
| P06 | Reconcile existing QR and least-squares code; complete reflector generation/application, QR/LQ/QL/RQ/RZ, generalized factors, pivoted/positive-diagonal and compact/blocked/tall-skinny/short-wide forms. Complete ordinary, rank-revealing, SVD-based and constrained least squares. | Reconstruction, orthogonality/unitarity, Q application, tall/wide/rank-deficient systems, residual optimality and known-nullspace minimum norm |
| P07 | Reuse existing SVD work; implement/close bidiagonal reductions and kernels, all required SVD variants and selection/overwrite modes, then GSVD and CSD with their actual supporting stages. Coordinate dependencies on P06 reflectors and P08-related kernels explicitly. | Reconstruction, ordered/nonnegative values as specified, orthogonality, invariant subspaces for clustered values, exact GSVD/CSD factor/block equations |
| P08 | Deliver required reductions/reconstruction for symmetric/Hermitian ordinary and generalized eigenproblems across full/band/packed/tridiagonal and staged forms. Then complete nonsymmetric eigen/Schur, balancing/reordering/condition, generalized eigen/QZ and ordinary/generalized matrix equations. Develop callback/LOGICAL bridges before dependent sorting drivers. | Eigen/invariant-subspace residuals and normalization, selected-spectrum boundaries, real conjugate pairs, homogeneous `beta*A*v-alpha*B*v`, Schur/QZ equations, scaled coupled equations and concurrent callbacks |
| P09 | Close mixed general/positive and DMD/DMDQ work already present; retain upstream-defined refinement/fallback reporting. Complete conversions, norms/scaling, permutations, expert auxiliaries, deprecated compatibility rows and source-selected staged alternatives. Build and bind the 130 extra-precision definitions after M2. Audit every remaining and excluded inventory row. | Precision/mode-specific equations, rank/dynamics fixtures, actual mixed-precision branches, extra-precision accuracy, symbol/build closure and exhaustive row accounting |

Prefer prerequisites that unlock several families: shared reflector contracts,
structured conversions, checked selection callbacks, exact workspace rules and
portable test oracles. Preserve the explicit six-module architecture. Do not
introduce a generic dispatch rewrite merely to accelerate binding generation.

After each coherent family checkpoint, select the next ready action from the
actual dependency graph. Pair numerical development with any newly ready
platform, evidence or delivery task. An unchanged provider failure is retained
and linked to its dependents; it is not repeatedly reproduced while ready work
is neglected. A consumer may reuse stable contracts from an incomplete
producer, but neither obtains verified credit until all required gates pass.

For the immediate packed work, `hptrs`, `hptri` and `hpcon` require the stable
producer/factor contract; refinement requires its solve path; the basic driver
requires factorization and solve; the expert driver additionally requires
condition/refinement behavior. Implement the necessary helper/source
dependencies as discovered. Do not serialize a basic driver behind an unrelated
inverse solely because of the order in the table.

**M4 — finish provider and platform admission early**

Use the compiler/configuration requirements in the
[support matrix](../../docs/support-matrix.md) and the provider admission
requirements in the master continuation. Maintain exact compiler/runtime,
integer ABI, linkage, architecture and test-oracle identities for each cell.

| Profile | Remaining admission work |
| --- | --- |
| Linux GNU | Reuse actual LP64/true-ILP64 static and shared preparations; fill missing Debug/Release and family-specific evidence. Verify foreign runtime isolation and provider identity in installed use. |
| Linux Clang | Complete a Clang-built ASC provider facet against the explicitly attested numerical backend in both required integer ABIs and static/shared configurations. Validate interoperability and oracle precision; a supplemental Clang consumer alone is insufficient. |
| Windows x64 / MSVC | Implement/prove the compatible numerical ABI or private shim, exports, CRT/Fortran/runtime discovery and DLL relocation; run Debug/Release static/shared and required ABI cases on native Windows. |
| macOS arm64 / AppleClang | Prove provider/complex/INTEGER/LOGICAL/character interoperability, dylib/rpath relocation and numerical-oracle behavior; run the required Debug/Release static/shared and ABI cases. |
| Instrumentation and analysis | Complete applicable ASan+UBSan, standalone LSan, bounded TSan, strict format/tidy and CodeQL gates. State which ASC, provider and runtime components are instrumented. |

For each new profile, first prove a representative LU, structured, complex,
workspace and callback path as applicable; then run every required family/mode
on that profile before granting full-profile credit. Keep guards on unavailable
profiles until admission evidence passes. Separate provider acquisition from a
C++-only consumer build; consumers must not require a Fortran compiler.

Strict no-allocation claims require observation of the real foreign path as
well as ASC. Calibrate ignored-read and mutation observers with deliberate
violations and appropriate platform-specific methods. Check `long double`
precision/range before using it as an oracle; use independently justified
analytic/exact or approved test-only higher-precision methods where needed.

**M5 — reconcile all other packages and deliver usable results**

| Package | Remaining obligation | Completion evidence |
| --- | --- | --- |
| P00 | Preserve the baseline and pins; finish reviewed classification/mode mapping, true dependency graph, exclusions and source/provenance decisions. Reconcile generated inventory canonically. | Every required row accounted for once; every exclusion justified; generators/validators and source identities agree |
| P01 | Finish only missing structured/factor, workspace, pivot, permutation, logical and callback contracts needed by M3/M4. Preserve complex storage lifecycle and CPU/CUDA isolation. | Overflow, span, object-lifetime, alias, placement, metadata-only query, partial-report, concurrency and component checks |
| P02 | Preserve accepted Dense/Sparse bounded printing and format/precision semantics. Close only genuinely missing integration/owner obligations. | Existing finite acceptance matrix remains covered on the final relevant inputs; required regressions pass |
| P03 | Preserve accepted transactional text/binary codecs, file lifecycle, limits and independent interoperability fixtures. Retain parser/fuzz regressions. | Final relevant profiles pass rollback, encoding/CRC, resource/stream/close, rounding and malformed-input requirements |
| P10 | Preserve accepted Matrix Market combinations and independent fixtures; finish remaining integration/provenance reconciliation. | Ordering, indices, symmetry, duplicates/pattern, bounded parsing and Dense/Sparse isolation requirements remain passing |
| P11 | Complete convenience workflows, public documentation/catalogue, ABI/header inventories, packaging, examples, security, provenance and exact execution records. | All below-listed deliverables execute through installed public targets and all final gates pass |

For every delivered numerical family, maintain useful Doxygen for its equations,
types, shapes/storage, ignored inputs, workspace/allocation, aliasing, lifetimes,
placement, mutation/partial outputs, reports and concurrency/provider limits.
Keep a small consistent factor/result convenience layer for repeated solves,
least squares and spectral workflows. Do not duplicate every expert routine
with a second owning abstraction.

Finish and run these installed applications with private/source/build headers
unavailable:

1. Read, factor once, solve multiple RHS, report residuals, print, save and
   reload through both native and Reference LU.
2. Complex Hermitian Matrix Market input, explicit solve/eigen operations,
   correct real eigenvalues and meaningful complex outputs.
3. Rectangular least squares and SVD with rank, minimum-norm and selected-mode
   diagnostics, including a valid nonzero residual for inconsistent data.
4. COO/CSR/CSC archive and bounded preview without Dense linkage or densification.
5. Corrupt-input rollback and singular-solver reports without fabricated success.

For package acceptance, install and relocate into a prefix containing spaces;
hide source/build trees and disable package registries. Test static/shared,
header self-containment, no-exception and multi-TU ODR behavior, exports and
patch-line ABI compatibility, requested-missing components, and C++-only
consumers with LAPACK/Fortran/CUDA discovery unavailable when unrequested.
Audit dynamic dependencies and symbol resolution with another BLAS/LAPACK
present. Keep provider-free `ASC::cpp`, Dense, Sparse and Random independent.

Preserve BLAS/Random behavior and reproducibility. Retain their coverage,
Joe–Kuo data/license and MdeCpp provenance checks. Make no CPU BLAS performance
claims. Review explicit ABI baselines under [ABI policy](../../abi/README.md);
the existing inspection tooling does not automatically approve baseline updates.

**Definition of done for each family**

Each row remains incomplete until all applicable items are evidenced:

1. Pinned source and actual scalar signatures, legal modes/storage classes,
   mathematical equations, native ABI, allocation and INFO/partial-output
   semantics are reviewed. No invented precision counterparts or unsupported
   upstream job modes enter the capability claim.
2. Maintained public declarations, checked implementations, native shims where
   necessary, build/export registration, documentation and installed use exist.
   All validation required before mutation/call occurs in that order.
3. Independent mathematical checks and direct native fidelity are separately
   recorded. Cover ordinary and required extreme cases, empty/scalar/blocked
   boundaries, singular/nonconvergent/partial outcomes, all required option
   interactions, exact minimum workspace, staleness, aliasing and output limits.
4. Real-ABI tests exercise the actual wrapper in LP64 and true ILP64. Failure
   injection proves containment, not numerical correctness. Concurrency,
   allocation, memory observations and required platform/linkage profiles pass.
5. The configured CTest selection matches executed/JUnit IDs. Zero tests,
   unexpected skips, missing tools, nonzero required exits and unexplained
   disappearing tests fail acceptance. Retain failures and raw logs externally.
6. Schema2 evidence binds exact source/provider/build/test inputs to reviewed
   row/mode/class assignments. Partial observations stay partial. Coverage,
   backlog, capability catalogue, ABI/header baselines and docs agree.

Use three verification levels: focused tests after relevant edits; complete
finite family and affected regression checks at integration; whole-program
matrices at real integrated milestones and final freeze. Reuse unaffected
evidence only through explicit dependency/input comparisons. This reduces
duplicate execution without reducing required coverage.

**M6/M7 — final acceptance and release boundary**

Freeze the final implementation candidate and provider/configuration identities.
Run the full-profile configure with
`ASC_CPP_LAPACK_REQUIRE_FULL_PROFILE=ON`, and the coverage validator with
`--require-full`. Full-profile support must actually build and execute; its
current intentional rejection is a development safeguard, not final success.

Require all of the following before declaring the original project complete:

- All 2,113 required Reference rows and their required mode/profile classes are
  verified; zero unimplemented, unsupported, untested or numerically failed
  required rows remain. Every required definition links to the intended provider.
- All P00–P11 requirements and owner/provenance decisions are reconciled with
  exact evidence. Native20 and accepted array-I/O scopes retain their meaning.
- Every required full CTest selection passes with zero unexpected skips;
  numerical, parser/fuzz, instrumentation, static analysis/security, architecture,
  BLAS/Random/provenance, headers/ABI, package/relocation and strict docs gates
  pass on their required platforms.
- CI evidence identifies the actual tested checkout/tree and PR merge candidate
  where relevant. A run/head label alone is not an independently attested tree.
  Relevant changes after freeze trigger affected revalidation.
- The final readable catalogue and evidence index report native, Reference,
  experimental first-party and platform results separately and truthfully.

Then prepare the concrete integration review. Any merge and subsequent release
actions follow existing authorization and [release process](../../docs/release-process.md).
On `release/0.9.0`, reconcile the proposed feature/API scope with the version/ABI
policy before preparing artifacts. Verify the exact ASCCMake identity from
[installation](../../docs/installation.md), repository security/protection and
publication environment gates, exact-release-commit hosted results, and the
downloaded release assets' independent consumers. Publication does not inherit
approval from this planning request or a passing feature branch.

If an unavoidable decision/resource still blocks required rows, state exactly
which rows and dependents remain incomplete. Do not call a checkpoint, all
unblocked work completed, or technical work awaiting owner decisions full
project completion.

**First execution queue**

| Order | Concrete next action | Deliverable |
| --- | --- | --- |
| 1 | Revalidate this baseline and preserve the packed dirty diff; reconcile state and artifact bindings | Passing current-tree integrity check and exact restart record |
| 2 | Triage the nine reported critical/high CodeQL alerts and the failed Analyze job | Reviewed finding list, focused fixes/evidence for confirmed defects, remaining exact causes |
| 3 | Consolidate the active provider failure causes and revalidate the existing XBLAS/notice packets | Concrete decisions ready for the responsible owner; unaffected queue remains executable |
| 4 | Run packed Debug LP64 then ILP64; add independent-call concurrency and complete remaining packed engineering profiles | Coherent packed producer checkpoint with all failures preserved |
| 5 | Close one existing mathematically passing LU family's complete evidence, while starting representative missing platform admission | Demonstrated path to a verified Reference family and exposed platform blockers |
| 6 | Execute the packed consumer dependency chain where ready; if blocked, take the next ready P04/P06/P09 prerequisite | Next tested family with docs, installed use and evidence; no repeated generic planning |
| 7 | Continue M3 with M4/M5 at each checkpoint, then run M6 | Exhausted required backlog and exact full acceptance result |

The recorded packed command is:

```sh
python3 -B /home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/master-continuation-20260910-01/continuation-20260912-01/packed-indefinite-prerequisite-01/containment-recovery-01/run_checks.py debug lp64 01
```

Check that its attempt directory is still unused before invoking it; preserve
existing attempts. The script accepts `debug ilp64 <unused-attempt>` for the
second actual ABI. Keep raw build/test logs and generated verification outputs
inside the established external evidence roots.

At every checkpoint, record completed row/mode classes, remaining classes,
exact failures/skips, changed inputs, pending decisions and one executable next
action in existing state/handoff records. Use dependency readiness and verified
deliverables to measure progress. Do not assign unsupported calendar estimates
or derive project-completion percentages from routine counts alone.


**Execution checkpoint — 2026-09-14**

The packed producer and solve engineering slices have complete local sixteen-
profile matrices, installed public consumers and scoped P11 checks. Required
numerical failures remain. The separate header-order repair passes both hosted
CI runs 19/19. Current counts are 260 callable-unverified Reference rows,
300 reviewed contracts and zero verified Reference rows; the full 2,113
denominator and Native20 remain unchanged. Broader platform admission is open.

The implementation queue advances to six SPTRI/HPTRI packed inverses. Exact
sources, emitted ABIs and guarded native prerequisites are complete; checked
APIs, full tests, installation and evidence registration are next. Use live
state and family reviews instead of replaying the original producer command.
Existing provider/security/XBLAS/notice decisions remain open while independent
required implementation continues. This is not whole-project completion.
