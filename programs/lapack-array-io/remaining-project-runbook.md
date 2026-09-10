# ASC-C++ — complete the remaining LAPACK and array-I/O project

**Master continuation, version 2.0 — 2026-09-10**
**Programme:** `ASC-CPP-LAPACK-IO`
**Repository:** `AI4SciComp/asc-cpp`
**Working branch:** `feature/lapack-array-io`; draft PR #47 targets `develop`.
**Purpose:** one continuing implementation assignment covering every remaining P00–P11 obligation, with bounded workspace use, incremental integration, and exact completion criteria.

> Resume the existing project. Do not restart it. Preserve the integrated robust PPSVX and accepted native/array-I/O work. Complete the remaining required work through dependency-ready, end-to-end implementation slices. Start with the selected PTTRF/PTTRS slice, bring P09 into the first implementation cycle, and perform P11 delivery continuously. A blocked numerical route must not monopolize the programme. A passing subset must not be called full completion.

## How to use this file

Place this file in the existing integration checkout as
`programs/lapack-array-io/remaining-project-runbook.md`, or provide it as an
attachment readable by the local Codex session. Do not overwrite a different
existing file. The same file is used for initial execution and later resumption;
there is no need for a new prompt after each routine or candidate.

Send the instruction below. It incorporates the previously unstarted workspace
housekeeping and P09/P11 scheduling tasks. Their separate prompt files are not
needed to execute this assignment.

### Instruction to send to Codex

Resume `ASC-CPP-LAPACK-IO` in the existing integration worktree and execute this
entire master continuation. This is an implementation assignment, not a request
for another plan or a single-routine handoff.

I authorize ordinary development, checked API additions within the approved
programme, tests, documentation, build/package integration, evidence mapping,
feature-branch commits, normal non-force pushes to the existing feature branch,
and evidence-based updates to draft PR #47. Continue through all unblocked
required tasks without requesting permission at every routine, review, or
candidate boundary. Apply the dependency-based scheduling and bounded-workspace
rules below. Preserve existing accepted contracts and all valuable work.

Do not repeat the robust PPSVX experiment or its completed integration. Do not
restart P00 or the completed native/array-I/O acceptance. Do not treat a pending
PPSVX Reference decision as a prerequisite for unrelated LAPACK families.

First reconcile the actual checkout and pending work once, make the finite
workspace-retirement proposal without deleting data, and map existing P10/P11
progress. Then implement and integrate the S/D/C/Z PTTRF/PTTRS slice or finish
its actual newer continuation. Start a ready P09 slice next, prioritizing
DSGESV/ZCGESV after checking their prerequisites. Continue the full ready queue,
including remaining P04–P08 coverage, all P09 requirements, platform admission,
and P11 final checks. Do not stop merely because one of these slices succeeds.

This instruction does not approve a third-party notice packet, dependency
upgrade, altered pinned provider, numerical-contract waiver, new default or
hidden fallback, unrelated architecture rewrite, destructive cleanup, merge,
tag, or release. Collect genuinely necessary owner decisions in one maintained
packet; continue independent work while those decisions are pending. Reuse
existing authorizations only within their recorded scope.

Use the live Google C++ and Python Style Guides with existing repository
compatibility decisions. Keep one active implementation writer, use the existing
build/evidence roots, and create no new sibling worktrees or top-level project
directories by default.

At a session boundary, save an exact resumable checkpoint in the existing
programme records. Do not equate that checkpoint with completion, and do not
promise background execution. On resumption, continue this same assignment from
the first unfinished actionable item. Finish with the full acceptance result or
an exact, evidence-based list of the remaining external decisions/resources—not
another generic instruction to continue.

---

## 1. Authority, source basis, and superseded instructions

This master continuation combines scheduling, workspace lifecycle, implementation,
and delivery. It does **not** replace the mathematical scope or silently edit
accepted API/format contracts.

Read, in order:

1. The actual root and applicable nested `AGENTS.md` instructions and the current
   user authorization.
2. The current programme decisions, source/provider locks, coverage manifests,
   accepted public contracts, and latest implementation-bound handoff.
3. This continuation for scheduling, workspace limits, ongoing execution, and
   completion decisions.
4. The existing `programs/lapack-array-io/runbook.md` for the full technical
   P00–P11 specification and its unchanged acceptance requirements.

The original runbook has SHA-256
`51c1cdf7e864bb38b849d7c5280fe34e92ff278da2653e6ee3c97c1cfb2923ae`
in the inspected state. Compare identities; do not overwrite a newer authorized
revision with this historical copy.

**Explicit scheduling amendment:** broad package dependencies remain final
completion dependencies. Independent subtasks may start when their own real
technical prerequisites are available. Record this distinction once in the
existing decisions/state machinery. Do not erase genuine dependencies.

This continuation supersedes earlier instructions that:

- Prohibit all new numerical families until PPSVX is fully accepted.
- Stop at a successful candidate and return only the next candidate number.
- Require a fresh worktree or complete source snapshot for every task.
- Treat preserving historical work as keeping every expanded build indefinitely.
- Defer all P11 work until every numerical family is complete.

It does **not** supersede provenance controls, numerical requirements, public
compatibility promises, or any explicit restriction on changing the pinned
Reference route. If a root instruction needs an amendment, propose that exact
amendment; do not quietly ignore or overwrite `AGENTS.md`.

Sections below marked as an execution rule are new instructions. Recorded
starting facts are drawn from the user handoff and the inspected PR/state;
proposed task ordering and diagnostic fixtures are not claims of existing code.

## 2. Starting checkpoint — preserve, then reconcile

The remotely inspected PR #47 and the supplied handoff agree on this baseline:

| Item | Recorded identity or status |
| --- | --- |
| Current local/remote HEAD in the user handoff; remote PR head independently read | `9032f622752d5014c673f08bc0a14a9ca1612989` |
| Hosted-tested product | `1e15d843d8968f659c393af85e6bdcf99af3345a` |
| Hosted-tested product tree | `ca291bf16f76ef78144f91caffc97bb84da442fa` |
| Recorded tested PR merge | `a8f7e30560c873ceb4dbdf7d6e70324ff168dd92` |
| `develop` base | `46412183b2ae86101b2361c52376a8db8efff264` |
| Integrated milestone | `PPSVX_ROBUST_INTEGRATED_EXPERIMENTAL_FULL_PROGRAM_INCOMPLETE` |
| Protected earlier milestone | `SUBSET_REVIEW_READY_FULL_PROGRAM_INCOMPLETE` |
| Native foundation | 20 callable/validator-verified operations within their recorded contracts |
| Reference inventory | 2,113 required rows; 350 partial registrations; 1,763 not started; zero strict complete/verified Reference rows |
| Reference PPSVX | 0/4 root-registered; retained numerical failures are not closed by robust success |
| Additional recorded limitations | 130 missing required XBLAS definitions; 78 historical required mathematical failures, with separate PPSVX counting units |
| P02, P03, P10 | `technical_status=verified_contract_scope`; overarching dependency/owner gates remain separately recorded |
| P09 / P11 top-level labels | `not_started` in the inspected state; substantial P11-related subset evidence already exists |
| Next selected numerical slice | S/D/C/Z PTTRF/PTTRS: eight existing inventory rows, implementation not started in this checkpoint |

The user reports that only the original owner-instruction file remains
untracked. Locate and preserve it; do not remove or automatically add it with
`git add -A`.

The source of the robust algorithm is now **maintained repository code**, not
merely an external candidate. Its opt-in switches are recorded as
`ASC_CPP_ENABLE_LAPACK=ON` and
`ASC_CPP_ENABLE_EXPERIMENTAL_ROBUST_PPSVX=ON`, with the robust option OFF by
default. Its initial admitted scope is Linux x86_64, GNU 11.4, static, actual
LP64 and global ILP64. Do not infer broader provider support from provider-free
Windows/macOS CI or from supplemental Clang consumer checks.

The current programme-record HEAD and tested product are different identities.
The reported unchanged-product comparison does not turn the two trees into the
same tree. Keep this distinction in subsequent reports.

The local handoff is expected at:

`/home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/ppsvx-robust-integration-01/final-handoff.json`

That is a discovery hint, not a path inspected remotely. Read the actual file,
its source references, and any newer checkpoint. Never reset to one of the
listed SHAs. Never replay a patch already included in the current ancestry.

## 3. Scope and completion: one programme, several independent verdicts

### 3.1 Required scope

Retain every original P00–P11 obligation. Complete the checked C++20 LAPACK
surface for the pinned required Reference CPU profile, including actual scalar
variants, expert drivers, computational and required auxiliary routines, legal
modes, structured storage, workspace, reports, tests, and installed use.

Retain the native foundation and complete any genuinely outstanding native
obligations. Preserve Dense/Sparse printing, ASC text/binary, and Matrix Market.
Complete their remaining integration/provenance obligations without rebuilding
accepted functionality.

A full native reimplementation of all LAPACK algorithms is **not** the original
requirement. Neither are new GPU LAPACK, general sparse direct/Krylov solvers,
PDE methods, neural models, HDF5, `.npy`, compression, or an agent framework.
Optional optimizations must not displace mandatory work.

### 3.2 Do not confuse these verdicts

| Verdict dimension | Evidence needed |
| --- | --- |
| Upstream requirement identified | Exact row exists in the pinned inventory. |
| ASC route integrated/callable | Public checked implementation exists in the maintained build, links, and is executed. |
| Contract/mode coverage complete | Every required branch/equivalence class and output/error contract has its required evidence. |
| Numerical verification | Independent mathematical requirements pass for that route and configuration. |
| Provider compatibility | Mapping to the identified provider is correct; this alone is not numerical verification. |
| Platform admission | Actual supported compiler/ABI/linkage/runtime tests pass for that profile. |
| Programme acceptance | All mandatory scope, numerical, platform, package, and owner/provenance gates are satisfied. |
| Merge/release authorization | Separate explicit authorization; never inferred from any technical verdict. |

A first-party robust algorithm can satisfy its own contract without satisfying
an unchanged Reference-provider row. Preserve the original full-profile result
as failed/pending while any required Reference numerical failure remains.

**Do not hide a logical conflict.** Keeping a provider byte-for-byte unchanged
while reproducing its known failures cannot also produce a passing verdict for
those same required cases on that provider. A future explicitly authorized
provider correction or route/profile decision may resolve that conflict; another
wrapper test cannot. Continue all independent implementation in the meantime.

### 3.3 Exact full completion

Declare the original project review-ready only after:

- Every required inventory row has a maintained executable checked path and all
  original mandatory route-specific numerical/contract gates are satisfied.
- All required P00–P11 outcomes and admitted full-profile platform obligations
  have exact, current evidence; no required unknown, stub, unimplemented row,
  unresolved failure, or missing dependency remains.
- Package consumers, documentation, isolation, provenance, and required owner
  decisions are complete.
- The implementation, generated outputs, tests, and evidence refer to a coherent
  frozen candidate; no unintegrated external implementation is counted.

A technical-ready result with owner approval pending is a useful intermediate
result, not full completion. A session checkpoint is not a completion verdict.

## 4. One-time workspace reconciliation, not another housekeeping programme

### 4.1 Keep the established roots

Expected roots under `/home/yicai/AI4SciComp` are:

| Path | Treatment |
| --- | --- |
| `asc-cpp` | Preserve the original checkout and any shared Git metadata. |
| `asc-cpp-lapack-array-io` | Reuse as the active integration worktree after checking its identity. |
| `asc-cpp-build` | Reuse existing compatible build configurations and installation scratch. |
| `asc-cpp-evidence` | Preserve unique source, patches, dependencies, reproductions, and acceptance evidence. |

No new sibling worktrees, clones, or top-level project directories by default.
Normal source directories and necessary isolated build/install subdirectories
inside the existing roots are allowed. A new public source family is not a new
workspace.

Use one active writer. Read-only review can be delegated when available, but do
not start multiple Codex writers in the same checkout or spawn new worktrees to
simulate parallelism. Cap build parallelism according to actual memory/load;
run timing-sensitive observers separately from competing builds.

### 4.2 Inspect once

Check current directory, Git top-level/common directory, branch, local/remote
refs, staged/unstaged/untracked changes, live writers, and available disk space.
Use `git worktree list --porcelain` and recursive disk usage to distinguish
worktrees, ordinary copies, build trees, and evidence. The directory-entry size
printed by `ls -l` is not recursive usage.

Bound inspection to known `asc-cpp*` paths, current handoffs, and their direct
references. Do not scan/hash the entire home directory or every historical
compiler object. Do not recompile the project merely to classify a folder.

Before implementing a family, inspect relevant existing branches/worktrees and
external patches for that family. Reuse reviewed useful changes; do not import
an entire sibling tree or start the same implementation again. Preserve any
unique work that is not selected.

### 4.3 Finish the previously unstarted housekeeping proposal

Write/update one file in the existing evidence root:
`lapack-array-io/workspace-cleanup-review.md`.

For each known workspace record actual kind, size, branch/HEAD, unique commits,
dirty/untracked/valuable ignored files, active/path dependencies, recommendation,
preservation method, and estimated reclaimable space. Classify KEEP,
INVESTIGATE, or RETIRE_AFTER_EXACT_APPROVAL.

A clean checkout does not prove its commits were integrated. An ancestor test
alone does not resolve squash integration; inspect the relevant patch when
needed. A Git bundle does not preserve uncommitted working-tree/index contents.

**Do not delete, move, prune, force-remove, expire reflogs, or garbage-collect
valuable existing material under this assignment without exact path approval.**
If approved later, preserve unique refs and non-Git contents, verify recovery,
then use Git's worktree-aware removal for registered clean worktrees. Never use
a deletion glob. Keep unresolved external implementations and required provider
prefixes live until their references are safely replaced.

Once this finite proposal is written, continue development. Lack of cleanup
approval is not a global blocker unless storage or an active-writer conflict
actually prevents safe work.

### 4.4 Stop future accumulation

Reuse configuration-specific build trees when source/toolchain/provider inputs
are compatible. Retain command outputs, failures, compact source identities,
patches, and milestone evidence—not a complete new expanded checkout per test.

For new disposable scratch, record ownership and disposal conditions at creation.
Reusing or clearing designated scratch must never overwrite the only retained
failure log, source, binary required for a claim, or handoff. Historical data
continues to require exact retirement approval.

Git history plus a compact manifest and the necessary archived inputs is the
preferred long-lived record. Do not introduce another general archive manager
or provenance database. Reuse the programme's existing mechanisms.

## 5. Reconcile state once and maintain an executable backlog

### 5.1 Use existing records as the authority

Read the coverage inventory, mapping, numerical disposition, source locks, and
existing validators. Run their supported status checks to identify actual
unfinished row/mode/profile obligations. Inspect existing `--help` and CMake
registrations rather than guessing command names.

Use current implementation/evidence to reconcile older labels. For example,
old native LU subtasks may say implemented-unverified although later native20
records verify the actual scope. Resolve this with evidence references, not
blindly trusting the newest timestamp or rewriting the historical report.

Keep P02/P03/P10's verified technical scopes intact. Map existing Doxygen,
installed-consumer, ABI, package, and relocation work into P11's relevant
subtasks. Mark ongoing P11 work in progress only with those mappings. Final P11
acceptance still depends on the complete programme.

### 5.2 Every remaining requirement needs a next executable action

Maintain one current backlog using the existing state format. Add only the
minimal compatible fields needed; update the existing validator and its tests
if the schema changes. Do not build a scheduler service.

Each actionable slice must identify:

`task ID; Pxx owner; exact upstream row IDs; actual prerequisite IDs; source/API
files; current route status; next action; required tests/modes; reuse candidates;
blocker ID when applicable; integrated revision; evidence references`.

Maintain separate current values for execution progress, numerical result,
profile admission, and owner decision. Keep immutable historical records
unchanged. A completed subtask need not remain falsely unstarted because its
package's final owner gate is pending.

All 2,113 upstream rows remain accounted for. A row can have multiple route
records, but it remains one upstream requirement. Four robust implementations,
24 public declarations, eight PT rows, test processes, profiles, and assertions
are distinct counting units. Do not add them together or invent a percentage of
project effort from them.

### 5.3 Small shared tooling, not repeated hand audits

Extend the existing validation/generation tools where they eliminate repeated
work: executable symbol probes, precise scalar signatures, finite mode records,
artifact references, capability tables, installed-example registration.

Generated declarations and test lists are not implementations or evidence.
Generators must emit readable code from reviewed routine-specific contracts,
not guess missing modes or create unsupported placeholders.

Reuse normalized shared contracts for genuinely identical validation classes,
and add the routine-specific exceptions explicitly. An alias test for POTRS
cannot automatically verify another routine with different live arguments.

## 6. Scheduling rule and first implementation cycle

### 6.1 Task readiness, not whole-package serial barriers

A task may start when its actual descriptors, integer/complex ABI, provider
symbols, permitted dependency, and test prerequisites are ready. It need not
wait for unrelated routines in a lower-numbered package.

A provider driver calling its own internal kernels does not require separate
ASC wrappers for each kernel before it can be exposed. Its actual linked
closure, ABI, resource, and execution requirements still need verification.
An ASC composition that calls another unfinished ASC API has a real dependency.

A blocked task records the exact missing prerequisite and releases the active
slot to another task. Do not use a stub to erase the dependency.

### 6.2 First cycle — perform this sequence

| Order | Deliverable |
| --- | --- |
| A | Reconcile current state/workspace once; write the finite cleanup proposal; map P10 and already executed P11 evidence. |
| B | Complete the selected S/D/C/Z PTTRF/PTTRS typed contract, implementation, tests, exports, and installed example. If newer local work exists, finish it instead of repeating it. |
| C | Start and deliver a dependency-ready P09 slice, first checking DSGESV/ZCGESV. If concretely blocked, select another ready mixed-precision or conversion/scaling family. |
| D | Apply P11 to both delivered slices: maintained source/tests, catalogue, documentation, installed consumers, scoped CI and truthful status promotion. This is part of B/C, not a later pause. |
| E | Continue remaining P04–P08 and P09 work from the complete inventory, prioritizing reusable partial implementations and high-dependency prerequisites. |

Do not postpone C until all of P05 is complete. If B encounters a genuine
provider limitation that cannot be remedied within existing authorization,
record its exact block and proceed to C. A normal implementation defect is work
to fix, not an excuse to abandon a slice.

### 6.3 Continue beyond the first cycle

Within a family, finish coherent implementation slices rather than hundreds of
partial wrappers. Across families, avoid starvation: after two delivered or
genuinely blocked slices in one numerical workstream, choose a ready task from
another unfinished workstream when one exists. A required shared prerequisite
can override that order, with a concise reason.

Prioritize, in order: shared correctness defects; almost-complete useful
implementations; prerequisites unlocking multiple required families; then
remaining leaf operations. Continue independent P09 work throughout, and P11
with every integrated change.

A session boundary is not a new project stage. Do not ask the user which routine
to do next when the ready backlog already answers that question.

### 6.4 Conditions for pausing

Pause a particular action for unavailable credentials/hardware, unapproved
materials, a genuinely unresolved numerical/provider policy, unsafe concurrent
work, or an actual resource failure. Continue other ready actions.

Pause the whole assignment only when all remaining ready paths have those
specific blockers, the user intervenes, or the execution environment ends the
session. Do not voluntarily finish after the first success. Do not claim that a
session can run forever or resume itself after shutdown.

## 7. First numerical slice: PTTRF/PTTRS, eight required rows

### 7.1 Fixed slice boundary

The initial row set is:

`SPTTRF, DPTTRF, CPTTRF, ZPTTRF, SPTTRS, DPTTRS, CPTTRS, ZPTTRS`.

Read all eight actual pinned sources/signatures and the existing ASC tridiagonal
contracts. The source baseline is Reference-LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Real and complex signatures are
not interchangeable. Do not change the provider version as part of this slice.

### 7.2 Factor representation

These positive-definite tridiagonal factors are unit-bidiagonal/diagonal
factorizations, not the packed square-root Cholesky representation used by
PPSVX. The real equation is `A = L D L^T`; the Hermitian equation is
`A = L D L^H`. The corresponding upper equation uses `U^T D U` or `U^H D U`.

The diagonal storage remains real for all four scalar families. Off-diagonal
storage is real for S/D and complex for C/Z. Factor storage records which unit
bidiagonal orientation is being represented; it must not imply a successful
factorization from arbitrary raw arrays.

PTTRF has no native UPLO argument. Complex PTTRS does have UPLO, with different
interpretations of E for upper/lower factors. Verify and implement the required
conjugation when interpreting the same Hermitian matrix in the other
orientation; do not pass identical complex lower E as upper E and claim the
same factor. Real PTTRS has a different native signature. Preserve all actual
precision-specific differences.

The C++ factor contract must distinguish input matrix diagonals from factored
D/E; borrowed views state lifetime and mutation, and an owning convenience form
uses the established resource policy. Reuse existing descriptor types when
correct; introduce only genuinely missing semantics.

### 7.3 Checked execution

Check N, RHS dimensions, real/complex scalar pairings, diagonal/off-diagonal
lengths, physical strides/backing spans, placement, aliasing, and provider/ABI
identity before any prohibited mutation or native call.

For N=0, the required off-diagonal length is zero; never compute an unsigned
`N-1`. Handle N=1 and NRHS=0 according to the actual contract, including what
metadata is still validated. Preserve input factors during solve and support
repeated multiple RHS.

Native routines do not acquire an LWORK argument just because ASC uses a query
protocol. Report zero native scratch where appropriate, and separately account
for explicit ASC layout conversion or staging. Preserve structure; do not
silently convert a tridiagonal problem to a dense one.

Preserve native INFO and its actual meaning. In PTTRF, a last-pivot failure has
a distinct documented partial-factor meaning. Do not certify such factors as
positive definite. PTTRS does not promise to discover every invalid supplied
factor as a positive INFO failure: distinguish checked ASC factor preconditions
from native status guarantees.

### 7.4 Finite verification

Freeze the required legal modes and reusable class mappings before expanding
fixtures. Cover all eight actual scalar routines and required layouts/ABIs.

Include independent factor reconstruction, known-solution RHS, repeated solves,
nontrivial complex conjugation, boundary sizes, padding, preflight rejection,
stale plans where applicable, mutation on failure, first/middle/last bad pivots,
valid raw-factor behavior, caller workspace, and installed public use. Preserve
existing numerical-domain requirements and justified extreme cases; do not
invent a universal finite-output guarantee for mathematically unrepresentable
results.

Useful original dyadic test inputs are given in Appendix A. Verify their
arithmetic independently before adopting them. Do not compute expected factors
by calling the function under test.

The structure should have linear factor storage and linear-time tridiagonal
factorization; solving scales with N times NRHS, excluding explicit interface
costs. Confirm the implementation and document its actual allocations/costs.
Do not make performance-speedup claims from complexity alone.

### 7.5 Slice exit

A contract document alone is not the exit. Integrate the actual typed APIs,
source lists, public headers, exports, tests, documentation, and an installed
factor-once/solve-twice example. Validate the eight rows with exact evidence.

Promote only rows that pass every required gate. If one route remains
numerically blocked, retain that failure, complete unaffected deliverables, and
advance the ready queue. Do not change 350 to 358 merely because eight names
were selected; derive actual distinct new registrations from the current map.

After this slice, start P09 as specified in Section 6 without asking for another
user instruction.

## 8. Complete the remaining P00–P11 obligations

The following is a work breakdown, not a replacement for the exact row inventory.
Names are navigation anchors; enumerate actual variants from pinned source.
Do not invent an S/D/C/Z counterpart just to make a symmetric table.

### 8.1 P00 — reconcile the remaining foundations and inventory

Preserve the established baseline, source locks, format specifications, initial
snapshots, and proven infrastructure. Close genuinely unfinished mapping,
classification, or validator work. Do not reacquire and rebuild unchanged
providers just because the project has a new continuation prompt.

Check that every upstream requirement has a stable ID, classification, source
identity, ASC owner, required profile, and legal-mode contract. Review any
inventory difference against the pinned source; do not shrink the denominator
to exclude difficult auxiliaries, missing XBLAS, or absent LAPACKE bindings.

Verify that excluded/internal/test/timing entries remain distinguishable from
required public/computational/auxiliary entries. Fix erroneous metadata with a
reviewable diff and tests. Do not label that correction as implementing routines.

Close the source/notice decision packet and its exact affected material mapping
when the owner supplies the decision. Until then record the affected gate, not
an invented approval. Existing permitted first-party/adapter work proceeds.

### 8.2 P01 — finish only missing shared contracts

Preserve complex owners, views, contexts, status/report conventions, and explicit
CPU/CUDA boundaries. Finish remaining structured representations and workspace,
pivot/permutation/logical/callback contracts only as needed by required families.

The shared layer must support exact scalar pairings, distinct input/output
capacities, checked integer products/cursors/conversions, native integer width,
complex ABI, selection ranges, factor provenance, and valid partial outputs.
Descriptor validation must use actual containing storage; a pointer cast is not
proof of capacity, object lifetime, or complex/native representation.

Keep allocation/resource ownership and operation workspace explicit. Queries
must obey the established metadata-only promise; a native workspace query that
can inspect data cannot silently implement an ASC metadata-only query. Use a
reviewed documented formula or a properly specified protocol instead.

Callbacks need real interoperability and state/lifetime/concurrency contracts.
Do not introduce unguarded global capture state or confuse C++ bool with a
provider Fortran LOGICAL representation. A shim still needs actual ABI evidence.

Do not redesign the six modules or introduce a mandatory external provider into
Core, Dense array I/O, Sparse, or Random.

### 8.3 P02/P03 — preserve completed printing and native array I/O

Reuse the maintained printers, text/binary codecs, independent readback, scalar/
rank/layout tests, parser hardening, transactional loads, cleanup tests, and
installed harness. Reconcile their evidence with P11 and rerun affected tests
when shared code changes.

Keep preview truncation separate from persistence; retain binary scalar coding,
endianness/version rules, text precision/locale contracts, exact integer parsing,
complex representation, resource limits, checked shape products, and placement
rejections. Do not quietly change the frozen wire format.

Preserve documented destination rollback without claiming source rewind, atomic
file replacement, or durability. Maintain combined primary/cleanup failure
semantics. Sparse consumers must not gain a Dense or LAPACK link dependency.

Do not expand accepted technical matrices merely to create more acceptance
work. A concrete untested mandatory contract or a regression is a valid task;
a larger Cartesian product with no new contract coverage is not a milestone.

### 8.4 P04 — close general LU and provider integration

Recover and finish existing partial LU interfaces before writing duplicates.
Complete all required general matrix factor/solve/inverse, condition,
equilibration, refinement, driver and expert entries, including the actual
variants of `getrf/getrf2/getf2`, `getrs`, `getri`, `gesv/gesvx`, `gecon`,
`gerfs`, `geequ/geequb`, and required supporting operations.

Separate native and provider routes. Factorization must support reusable solves;
mode-specific pivot orientation and transpose/conjugate choices remain explicit.
Check rectangular factorization separately from square solve/inverse operations.

Preserve exact native INFO, pivots, scaling, error estimates, and meaningful
partial results. Expert drivers are not equivalent to simple GESV. Refined
solutions, condition estimates, and factor reuse need independent tests as well
as direct-provider mapping checks.

Close row-level evidence for already implemented passing operations; do not
leave them unverified merely because PPSVX or a different family is blocked.
An incomplete mode on one row must remain explicit.

### 8.5 P05 — structured direct systems

After PTTRF/PTTRS, work through the exact remaining families, interleaving other
ready packages as Section 6 requires:

| Family | Required distinctions and acceptance |
| --- | --- |
| Positive-definite full matrices | Factorization/solve/inverse; blocked/unblocked variants; expert/refinement/equilibration/condition paths. Preserve native Cholesky and Random reproducibility. |
| Packed and band positive-definite | Native storage order, band width/leading dimensions, preserved factors, diagnostics, refinement, and no hidden densification. |
| Positive-definite tridiagonal | Real diagonal and real/complex off-diagonal; PTTRF/PTTRS, then required PT condition/refinement/drivers with their own contracts. |
| RFP and conversions | Parity, orientation, real/complex semantics, exact conversion round trips, and all inventoried RFP operations; RFP is not ordinary packed. |
| Pivoted semidefinite Cholesky | Numerical rank, permutation, stopping threshold, and reconstruction with rank-deficient examples. |
| Symmetric/Hermitian indefinite | Actual 1-by-1 and 2-by-2 pivot blocks, standard/rook/Aasen/two-stage variants and extra factor arrays; transpose differs from conjugate transpose. |
| General band and tridiagonal LU | Fill-in capacity, diagonal positions, extra superdiagonal storage, pivot encoding, reusable solves, and expert diagnostics. |
| Triangular full/packed/band | Solve/inverse/condition/refinement; unit diagonal and ignored entries must remain unread. |

Use the existing sibling implementations and reviews for band, expert,
indefinite, general-band, and tridiagonal work when they match current contracts.
Import only reviewed source/test deltas with provenance and integration checks.

For every family, reconstruct the documented factor equation, preserve partial
factor failures, exercise complex non-real fixtures, and verify numerical
reports. Do not substitute one algorithm variant for another under the upstream
variant name.

The integrated robust PPSVX remains opt-in experimental. Its maintenance and
any later required admission are bounded tasks, not a compulsory detour before
every other P05 row.

### 8.6 P06 — orthogonal/unitary factors and least squares

Preserve accepted native QR. Finish provider QR/LQ/QL/RQ/RZ, pivoted and positive-
diagonal variants, generalized QR/RQ, blocked/compact/tall-skinny representations,
Q generation and application, and every inventoried associated operation.

Representative anchors include `geqrf`, `geqr2`, `geqp3`, `geqrfp`, `gelqf`,
`geqlf`, `gerqf`, `tzrzf`, `ggqrf`, `ggrqf`, the actual `org/ung` and `orm/unm`
families, and block/tall-skinny variants such as `geqr/gelq/gemqr/gemlq`,
`geqrt/gelqt`, and `tpqrt/tplqt` where present.

Complete the actual least-squares drivers `gels`, `gelst`, `getsls`, `gelsy`,
`gelss`, `gelsd`, `gglse`, and `ggglm` and their relevant support. Preserve rank
thresholds, fixed pivot columns, overwritten RHS storage, minimum-norm
semantics, constrained rank assumptions, and actual integer/real workspaces.

Tests must distinguish full-rank, rank-deficient, square/tall/wide, consistent
and inconsistent systems. Verify orthogonality/unitarity, factor reconstruction,
Q application, residual optimality, and minimum-norm/nullspace conditions.
Do not demand zero residual on inconsistent least squares or use normal
equations as a silent substitute for an exact named rank-revealing driver.

Validate `max(m,n)` RHS capacities and all option-dependent output sizes before
execution. Reuse the existing least-squares, rank-revealing, and SVD-least-squares
worktrees only after scoped review.

### 8.7 P07 — SVD, GSVD, CSD and computational stages

Finish actual standard/selected/divide-and-conquer/Jacobi/QR-preconditioned SVD
variants, including applicable `gesvd`, `gesdd`, `gesvdx`, `gesvdq`, `gejsv`,
`gesvj`, reductions, bidiagonal problems, and factor formation/application.

Support each required values-only, economy/full, overwrite, selected index/value,
rectangular, and partial-convergence mode with exact output conventions. A right
factor returned as V^H must not be documented as V. Real/integer/complex workspace
requirements differ by routine and job.

Complete actual GSVD (`ggsvd3`, `ggsvp3`, `tgsja` and required compatibility
entries), CSD (`orcsd`, `uncsd` and inventoried variants), their rank/block
partitions, angles, factors, and documented computational routines.

Independently test reconstruction, orthogonality/unitarity, ordered nonnegative
singular values where specified, selection endpoints, repeated/clustered
subspaces, rectangular/rank-deficient/scaled inputs, and valid warning outputs.
For GSVD/CSD, use their exact factor equations rather than treating a stacked
ordinary SVD as an equivalent implementation.

Do not expose a single generic SVD wrapper and count all named upstream
algorithms as covered. Reuse ready existing P07 work instead of cloning it.

### 8.8 P08 — eigenvalues, Schur/QZ and matrix equations

Complete symmetric/Hermitian ordinary and generalized-definite problems with
full/selected spectra and full/band/packed/tridiagonal storage. Include required
reductions, factor reconstruction/application, and actual one/two-stage
variants. Respect documented modes that upstream genuinely does not support;
record source-backed inapplicability, not invented ASC support.

Complete nonsymmetric standard eigen/Schur, balancing/backtransforms,
eigenvectors, reordering, condition/separation diagnostics, and their actual
variants. Handle real conjugate pairs, valid selections, scaling, normalization,
left/right vectors, callbacks, and convergence/partial-output semantics.

Complete generalized eigen/QZ and the corresponding computational stages.
Preserve homogeneous alpha/beta pairs, infinite eigenvalues, singular pencils,
problem-dependent normalization, and real block structure. Validate homogeneous
residuals without dividing by beta zero.

Complete the inventoried Sylvester and generalized Sylvester kernels and
support, including actual `trsyl`, `trsyl3`, and `tgsyl`. Preserve returned
scale/sign/transpose conventions. Test the scaled equation; do not automatically
unscale a valid result into overflow. Use exact paired equations for generalized
kernels, not a guessed ordinary extension.

Acceptance includes nonzero normalized vectors, original-system residuals,
Schur/QZ reconstruction for both matrices, orthogonality or the correct
B-dependent metric, analytic diagonal/rotation/nonnormal examples, clustered
invariant subspaces, and real/injected failure reports. Do not require identical
valid eigenvector signs/phases/bases from different methods.

A generic Riccati, Lyapunov, or matrix-exponential framework is not a substitute
for required LAPACK rows and is outside this assignment unless already in the
approved inventory.

### 8.9 P09 — start now and finish all specialized/completeness work

Split P09 into runnable implementation tasks and final catalogue closure. Do
not make P09 start depend on completion of all P05–P08.

**Mixed-precision general solves first.** Inspect DSGESV and ZCGESV availability,
actual signatures, lower-precision work, iteration/status outputs, residual
criteria, and upstream-defined fallback. Implement their checked ASC routes,
then DSPOSV/ZCPOSV or another dependency-ready mixed-precision family. Use the
actual inventory, not fabricated four-precision variants.

A mixed-precision driver must really execute the prescribed mixed-precision
algorithm and expose its iteration/fallback semantics. Always executing a
working-precision solve is not that driver. Required tests include successful
refinement, documented fallback causes, input conversion range, loss of lower-
precision definiteness where applicable, singular/nonpositive cases, multiple
RHS, and independently checked solution/report behavior.

**DMD.** Complete actual `gedmd`/`gedmdq` variants and their required modes,
rank/scale selection, snapshots, eigen/mode outputs, residuals, and workspace.
Use known linear maps, non-square snapshots, exact rank deficiency, and
appropriate invariant subspace checks. The upstream provider may internally
use SVD/eigen stages; that does not require every corresponding ASC wrapper to
be complete first when the checked provider call is otherwise ready.

**Extra precision.** Complete required `svxx`/`rfsx` and related entries according
to actual source. Map all 130 currently absent required XBLAS definitions to
exact build/symbol/algorithm requirements. Obtain a concrete approved dependency
or approved genuine equivalent; never use ordinary BLAS under an extra-precision
name. Section 10 defines the decision and integration process.

**Conversions, norms, scaling, auxiliary/compatibility operations.** Complete all
required safe conversion/scaling/norm routines, blocked/two-stage alternatives,
expert computational helpers, and compatibility entries identified by the
inventory. Handle their actual precision/workspace/output conventions and
branch-specific exceptional cases.

**Exhaustive closure.** For each still-incomplete row, identify the missing
contract, body, symbol/shim, legal mode, test class, documentation, installed
linkage, or policy decision. Complete it or keep its exact block visible. Probe
actual installed ASC and provider symbols. Regenerate/check the unchanged
inventory. No placeholder body, empty test, or blanket auxiliary exclusion
passes closure.

### 8.10 P10 — Matrix Market reconciliation, not a rewrite

The inspected state records the bounded technical scope as verified. Map its
current fixtures, mode matrix, public contracts, installed examples, and
provenance to P10/P11. Repair only actual missing mandatory cells or regressions.

Preserve valid array/coordinate, field and symmetry combinations, explicit
pattern policy, duplicate/zero handling, exact integer conversions, 1-based
indices, column-oriented dense wire order, resource bounds, and transactional
publication. Optional cross-representation conveniences must not silently
become new mandatory work or create Dense/Sparse dependencies.

Reconcile separate owner/final-package gates truthfully. Do not leave an old
label suggesting that the parser has not been implemented, and do not mark all
programme acceptance complete because Matrix Market passed.

### 8.11 P11 — ongoing integration and final delivery

Begin/continue P11 now. Each numerical slice must deliver maintained source,
headers, contracts, exports, meaningful examples, generator updates, and tests
through the real installed package. Existing P11-related evidence is reusable
only at its exact scope/input identity.

Complete the small explicit convenience layer on the expert APIs: owning or
borrowed factors, repeated solves, least-squares results, spectral results, and
explicit Q formation where appropriate. Reuse established resource and lifetime
conventions. Do not create one redundant class hierarchy per routine or an
implicit matrix-property/backslash dispatcher.

Complete developer/user guides for providers/ABIs, basic and reused solves,
complex arrays, rank/minimum-norm decisions, SVD/eigen conventions, diagnostics,
workspace/packing, formats, Matrix Market, migration, and unsupported profiles.
Keep a readable generated capability catalogue tied to truthful row evidence.

Preserve the five original installed workflows: read–factor–solve–report–print–
save–reload; complex Hermitian solve/eigen; rectangular least squares/SVD; Sparse
archive/preview; and corrupt-file/singular-system failures. Add the selected
tridiagonal and mixed-precision examples without recreating an application
framework. Examples must use public installed headers and exported targets.

Run component isolation, installed relocation, no-exception/header/ODR checks,
static/shared and compiler profiles where required/admitted, missing-component
rejection, competing-provider resolution, and C++-only use of a prebuilt provider.
Do not bundle foreign runtimes contrary to the approved external-provider model.
Instead prove documented external runtime discovery for the selected model.

Final P11 closure is Section 15. P11 being in progress is not permission to call
the full Reference profile complete.

## 9. Reusable definition of done for every numerical slice

Use this lifecycle for each ready slice. Do not create a separate candidate
programme for each bullet.

### 9.1 Read and freeze the local contract

Read the actual pinned source/interface and relevant existing ASC contract.
Identify exact scalar signatures, option interactions, mathematical output,
algorithm identity, storage/backing spans, workspace, mutation, allocation,
placement, concurrency, provider identity, and report behavior.

Review the transitive native closure to the depth needed for integer admission,
allocation/termination promises, global-state assumptions and uncommon ABI
features. Reuse unchanged validated components. Do not repeat a complete source
proof for every scalar when an already checked equivalence is applicable; do not
assume such equivalence when signatures or code differ.

Freeze finite legal-mode equivalence classes and required tests. Add genuinely
newly discovered requirements with the exact reason; do not keep increasing
profile counts just to demonstrate activity.

### 9.2 Implement the maintained path

Write actual checked APIs and bodies in the appropriate existing owner. Normal
first-party wrappers and algorithms already within the approved programme do
not require another owner approval per routine. Novel provider changes, altered
numerical guarantees, hidden fallbacks, or a new scope require Section 10's
specific decision instead.

Validate all preconditions before forbidden writes/native entry. Handle exact
and partial aliases, caller scratch alignment/capacity/object lifetime, unused
operands, layout conversions, scalar/integer width, and empty operations using
existing mechanisms. Avoid hidden allocation or device movement.

Preserve real/complex precision and index/INFO conventions. Do not advertise
native error-handler behavior as a recoverable ASC failure if an unchecked
input can terminate inside the provider. Do not mutate process-global provider
handlers as a shortcut.

### 9.3 Test mathematical and interface behavior independently

Use both direct-provider comparisons for mapping and independent mathematics
for correctness. Tests must distinguish successful calls from meaningful
solutions and distinguish backward error, forward error, and estimates.

Use valid storage for hostile descriptor tests, meaningful nonzero examples,
actual factor/eigen/SVD identities, analytic special cases, and justified
precision/dimension/conditioning-aware tolerances. Preserve existing numerical
assertions; a correction requires a demonstrated oracle defect, not convenience.

Real numerical failure tests are required where feasible. Synthetic callbacks
or provider injection can test rare report paths but grant no algorithmic
accuracy, real convergence, or provider-thread-safety credit.

### 9.4 Integrate and verify the candidate

Update source lists, exports, public-header ownership/digests, generated
metadata, validators and consumers atomically. Add maintained tests and an
installed use path. A passing external archive is not an integrated capability.

Run targeted numerical/interface tests, both required actual ABIs, affected
regressions, applicable sanitizer/concurrency tests, strict style, documentation,
ABI/package checks, and new-mode installed consumers. Schedule current hosted
validation under the existing CI budget/permissions without duplicating runs
unnecessarily.

Promote row status through the existing validator only after exact evidence
supports it. Keep route failures and unsupported platforms separate. A native
row is not held back by an unrelated Reference failure.

### 9.5 Continue

Make a coherent feature commit and authorized ordinary push. Update the single
current backlog/handoff and draft PR with scoped results. Then select the next
ready slice. A successful intermediate commit is not the end of this assignment.

## 10. Resolve genuine blockers without another global stall

### 10.1 One consolidated owner-decision packet

Maintain a concise current decision section/file under the existing programme
records. Do not create a new approval prompt after every candidate. Each
open decision must identify exact materials or contract, affected row/profile
IDs, evidence, recommended action, alternatives, and what remains blocked.

Separate at least these decision classes:

| Decision | What may continue without it | What it cannot silently authorize |
| --- | --- | --- |
| Existing source-derived metadata/notice packet | Permitted native, adapter, I/O, test and external-dependency development | Applying a pending notice as approved; bundling libraries/runtimes; copying upstream/MdeCpp implementation |
| Missing XBLAS dependency or equivalent | All independent routines, source/build/symbol analysis and preparation permitted by policy | Unreviewed import, substitution of ordinary precision, dependency pin changes |
| Known numerical-provider limitations | Unaffected mode/route/family work, validated first-party capability, exact remediation design | Waived requirements; silently patched pinned provider; Reference credit for robust output |
| Wider provider profile admission | Existing admitted profiles and portability implementation/testing | Calling unexecuted Windows/macOS/shared/Clang-provider lanes verified |
| Workspace retirement | All safe work in existing roots | Removing exact paths before their approval |
| Merge/publication | Review-ready feature implementation | Merging, tags, releases, registry upload, protection/permission changes |

Do not ask whether to continue routine development. Ask only for these genuine
non-resolvable decisions when necessary, and continue independent work rather
than stopping the whole programme to wait.

The known notice packet is the amended manifest
`380a8792f8c23263439ac79e8c02bb1959089b674c1221c6397e9bc26585992c`,
with proposed notice
`9ebeaf30a78e77f5fa4a4ba94ed66c68bdb92acd1dfdc8b1456e0c114ffbed41`.
Recheck the actual current binding. New bound material changes need an explicit
amendment; do not keep requesting approval for a stale hash or imply that
technical test success constitutes approval.

### 10.2 Reference mathematical failures

Group the 78 historical failing gates and separate PPSVX failures by actual
root cause and affected route. Preserve their original test IDs, cases,
tolerances, failure signatures, provider identity, and raw results. Do not add
176 assertions to 78 test processes as if they were comparable quantities.

Use the existing five-cause PPSVX disposition rather than rediscovering it.
For each failure, distinguish an ASC defect, an independently proved oracle
error, unrepresentable mathematical output, and provider arithmetic violating a
still-required property. Repair ASC defects. Correct only demonstrated oracle
errors with review and retained before/after evidence.

For a provider limitation, prepare one concrete remediation recommendation with
an exact algorithm/provider identity and the unchanged required tests. Assess
available approved routes first. Do not launch another large robust family
experiment automatically. The integrated robust PPSVX is preserved, but does
not by itself authorize a replacement Reference provider or new default.

Until a route/profile decision is explicitly recorded, keep the corresponding
strict Reference numerical gates failed. Valid partial implementation can be
reported as such without claiming numerical completion. If an alternative
profile is later authorized, keep its result separate from the original profile
and preserve all original test results; do not rename the original requirement
out of existence.

### 10.3 XBLAS and absent definitions

Do not merely re-run the same missing-symbol check. Produce the exact affected
row set, source/build conditions, required helper symbols, precisions and ABI
constraints. Verify whether absence is in the source distribution, selected
build graph, missing dependency, or ASC binding—not all missing definitions
have the same remedy.

Locate authoritative source, version, license materials, and checksums for a
candidate dependency under the repository's existing policy. Prepare its exact
external build/probe plan and required accuracy tests. Obtain any mandated
material approval before adoption/import. An approved first-party equivalent
must preserve the actual extra-precision contract; ordinary BLAS or wider input
types alone are not proof of equivalence.

Once authorized, integrate external preparation reproducibly, build both actual
ABIs, prove all required symbols and numerical behavior, and complete the
affected ASC routes. Dependency success alone does not verify their wrappers.

### 10.4 Unchanged blockers do not consume repeated cycles

A blocked record is reconsidered when its implementation, source identity,
contract, available provider/toolchain, owner decision, or evidence changes.
Without such a trigger, select another ready task. Keep a compact exact
reproduction rather than repeatedly launching a broad suite solely to observe
the same failure.

Do not treat a difficult but actionable implementation as an external blocker.
The reason must name the missing authority/resource or the precise unresolved
contract, and demonstrate why an already permitted solution is unavailable.

## 11. Complete provider/platform admission as its own workstream

### 11.1 Preserve known-good configurations

Keep the pinned provider commit, actual LP64 and global ILP64 preparations, and
supported GNU/Linux/static scope. A `typedef` change is not an ILP64 provider
build. Verify provider BLAS/LAPACK/LAPACKE-or-shim integer agreement, character
arguments, complex representation, logicals, symbol conventions, and runtimes.

Do not make every default install discover LAPACK or Fortran. Existing
provider-free/native/Sparse/Random consumers stay independent. The robust option
stays OFF by default and retains its explicit first-party reporting, regardless
of the provider context used for admission.

### 11.2 Admit additional configurations with evidence

Reconcile the original required platform/linkage matrix with current guarded
availability. Plan missing shared, full Clang/provider, macOS and Windows
admission as explicit tasks, not as a requirement to reopen unrelated accepted
provider-free results.

For each required new configuration, deliver the appropriate bridge/export/
runtime changes; check exact provider identities and linkage; build, install,
relocate, and execute real consumers; then run its mathematical, interface,
concurrency, and applicable instrumentation gates. Prove static/shared export
and runtime discovery instead of copying missing DLLs ad hoc or disabling
provider guards prematurely.

Only enable a previously rejected profile after its actual admission checks
pass. If an available toolchain needs a different compiler interoperability
boundary, develop and verify that boundary; do not pretend the GNU ABI is
portable by renaming types. An unavailable runner or incompatible required
provider remains a named platform blocker, not a passing test.

A supplement that compiles a Clang C++ consumer against a GNU-built library
proves that supplement only. It is not full Clang-built provider admission.
Do not claim WSL Linux testing as native Windows MSVC testing.

### 11.3 Oracles and numerical environment

Validate the precision and exponent range of independent numerical oracles on
each platform. Do not assume `long double` has the same properties everywhere.
Retain approved scoped oracle exceptions and their checks; use independent
analytic/exact or approved test-only high-precision methods when needed. Do not
remove the independent check because one compiler lacks the old oracle range.

Preserve the documented rounding/subnormal/floating-point environment. Do not
switch to fast math, change process-wide exception/rounding modes, or silently
flush subnormals to make a numerical failure disappear.

Provider/runtime code is not instrumented merely because ASC or the consumer
is. Record ASC-only ASan/UBSan/TSan, actual foreign instrumentation, and
uninstrumented components separately. Reuse a previously justified process-
local test workaround only with the same scoped explanation; do not disable
host security settings globally.

## 12. Verification that closes requirements without consuming the programme

### 12.1 Three test levels

| Level | When | What it establishes |
| --- | --- | --- |
| Local development checks | After relevant edits | Focused routine/branch/compile checks on current inputs; rapid defect feedback. |
| Slice integration checks | For a coherent implementation slice | Its complete finite contracts, mathematical cases, actual required ABI executions, affected shared regressions, installed use, and relevant style/docs/package gates. |
| Programme/profile checks | At genuine integrated milestones and final closure | Full selection, catalogue closure, cross-component/platform/provenance result on one candidate. |

This staging changes when checks run, not which requirements are mandatory.
Do not re-run all hosted matrices after every prose edit or tiny private test
iteration. Do run fresh affected checks when code, tests, contracts, generators,
compiler flags, source/provider identities, or linked dependencies change.

Do not rerun the whole project solely to create a new SHA in a report. Freeze a
product candidate, test it, and record final evidence separately.

### 12.2 Shared finite test classes

Maintain reusable tests for descriptor validation, modes/flags, integer ranges,
empty/scalar shapes, layout/padding, aliasing, metadata-only queries, exact
workspace minimum/alignment, stale plans, mutation and error publication,
resource observation, and public installed consumption. Bind each class to
actual applicable row/mode inputs; do not award coverage from a test filename.

Use branch-specific equivalence classes where independently reviewed. Pairwise
flag tests alone are not proof for arbitrary interacting options. Invalid
combinations are rejected and not counted as missing legal modes.

Cover ignored-input and output-only read promises with calibrated mechanisms
appropriate to the implementation. Include deliberate violations to prove an
observer detects them. Record platform/optimization/instrumentation limits;
never claim ASan proves all forbidden reads or that a mock proves native
concurrency.

Finite sizes should hit structural boundaries and independent mathematics,
including representative larger inputs where the algorithm has blocked paths.
Do not equate millions of repetitions of the same scalar branch with coverage
of a new algorithmic branch.

### 12.3 Numerical checks by family

Use reconstruction and known-solution residuals for factorizations/solves,
residual optimality and nullspace/minimum norm for least squares, reconstruction
and orthogonal subspaces for SVD, normalized nontrivial eigenvectors and correct
metrics for spectral problems, and returned-scale equations for matrix kernels.

Condition and forward-error estimates must be checked according to their exact
contracts and justified analytic fixtures, not universal guarantees they do not
provide. Preserve the existing extreme-value requirements and demonstrate any
claimed invalid-oracle correction. Do not compare two uses of the same broken
provider and call that independent verification.

Normalize residuals with appropriate scale-aware denominators and a stated
zero-denominator rule. Use the original inputs when an in-place driver overwrites
A/B. Keep real versus complex absolute-value conventions explicit. Prevent the
oracle itself from overflow/underflow; an overflowing verification formula is
not evidence of an algorithm failure or success.

### 12.4 Actual selection and result accounting

Enumerate the selected CTest IDs and compare selection with required IDs before
crediting a run. Empty selection, missing executables/tools, unexpected skips,
ignored return codes, or truncated logs are not passes. Use supported version-
appropriate CTest/JUnit facilities; discover exact target/test names from the
checkout instead of inventing them.

Retain raw stdout/stderr and exit status of configure/build/test/analysis, even
when a pre-CTest stage fails. Capture the actual pipeline's failing command;
`tee` success must not mask a test failure.

A selected legacy numerical suite may correctly exit nonzero because required
failures remain. Record that result as failed. Do not remove those tests from a
full profile, invert them with `WILL_FAIL`, disable them, or relabel them as
passing compatibility checks. An explicitly scoped subset can pass while the
full profile remains failed, and reports must display both results.

### 12.5 Timeouts, replays, and resources

Preserve timed-out/failed attempts. Diagnose whether a timeout is contention,
a genuine hang, or another cause. An identical binary passing later is a
supplemental replay result, not evidence that the first invocation passed.

Serialize resource-sensitive observers and limit overlapping builds. Do not
raise test timeouts or weaken checks merely to obtain a green result. A justified
infrastructure change must be explicit and reviewed; original failures remain.
Do not retry indefinitely without a new hypothesis or changed resource condition.

### 12.6 Current CI and prior evidence

Use the existing provider-free and robust/provider preparation workflows as
starting infrastructure. Add reusable required family jobs or selectors, not
one workflow per candidate. Keep job permissions at their existing necessary
scope; do not change protections, secrets, runner ownership, or billing.

A green workflow with the provider disabled grants no Reference numerical or
ABI coverage. An upstream provider suite grants no ASC wrapper credit. Match
head/merge/tree, source lock, configurations, actual executed IDs, and skipped
counts before reusing results.

Report pending/cancelled/unavailable hosted jobs honestly. Do not promise to
monitor them after the Codex session ends. Continue independent local tasks
while jobs are legitimately running; preserve their IDs for the next session.

## 13. P11 maintained delivery and user-facing usability

### 13.1 Product integration, not permanent external candidates

Ready source belongs in the integration branch with its maintained build,
exports, tests and examples. Keep external material only for historical evidence,
approval-bound dependencies, or work deliberately not yet admitted.

Before integrating recovered work, inspect the exact diff and current common
base. Reject unrelated unapproved entries from combined patches. Run the
required integration checks on the actual result. Do not cherry-pick or apply a
whole family workspace merely because its historical tests passed.

An experimental route can be integrated only under its explicit accepted
experimental contract and support guard. Do not mark it stable/full because it
is now callable. Do not leave a passing ordinary approved feature external
solely because another family's numerical decision is pending.

### 13.2 Coherent public API and documentation

Document operation, scalar pairing, factor meaning, required shapes/backing,
ignored fields, layout, mutation, workspace/allocation, memory placement,
lifetimes, diagnostic validity, factor authorization, concurrency, precision,
provider/algorithm identity, and support constraints on every public declaration.

Keep headers self-contained. Put ordinary non-template implementation in source
files; do not expose private helper types as a supported API. Use established
export/visibility macros and generators. Update public inventories and consumers
atomically, not just a checksum that hides missing declarations.

Produce a concise capability table and useful workflow guides generated from
current normalized state. Distinguish integrated, callable-partial, numerically
verified, experimental, blocked and unavailable; do not hide qualifications in
a massive historical log.

The existing robust PPSVX complexity/diagnostic qualifications remain intact.
Do not redesign its basis-vector estimators or expand its test domain solely to
produce another milestone. Performance optimization is not the blocker for
unrelated unimplemented LAPACK capabilities.

### 13.3 Installed examples and packaging

Reuse existing installed harnesses. All new examples use public headers and
`find_package` with intended exported targets. Include a prefix containing
spaces, relocation, controlled package discovery, and operation without access
to checkout/private headers. Use an isolated consumer environment rather than
renaming/removing the active source checkout to test hidden-source behavior.

Maintain provider-free Dense/Sparse/Random consumers and explicit missing-provider
rejection. A prebuilt provider consumer should not need a Fortran compiler merely
to link if that is the accepted package contract; it still needs its actual
runtime libraries. Prove these dependencies rather than inferring them from a
successful build on a developer machine with everything installed.

Check C++20, no-exception/header self-containment, multi-TU ODR, static/shared
visibility, missing components, ABI integers, actual complex calling conventions,
competing BLAS/LAPACK libraries, no implicit download, and no accidental source-
tree runtime dependency. Maintain the approved external-provider distribution
model; package tests do not authorize bundling.

Complete all original end-to-end applications and keep their independent
mathematical checks. Do not supply only a demo that prints a success banner.

### 13.4 Full-scope availability versus optional improvements

Keep optimized MKL/OpenBLAS/other providers, new CUDA LAPACK, and full native
coverage distinct from the required Reference CPU profile. Preserving their
existing supported behavior is mandatory; adding every optional provider or GPU
operation is not a prerequisite for this programme's original scope.

Mandatory platform gaps remain gaps. Do not silently reduce the original
required profile to Linux because Linux is presently admitted. A scoped Linux
feature is useful, but it is not full cross-platform acceptance.

## 14. Compact evidence, restart safety, and review discipline

### 14.1 One current execution record

Use existing state, blockers, verification summary and handoff records. Add
one compact current continuation record only if none serves that role. Store
raw output beneath the existing evidence root with stable configuration/slice
locations. Do not create a new top-level directory or full programme copy.

For each actual run retain timestamp/timezone, source/product/tree and relevant
diff hashes, provider/toolchain/ABI/configuration, exact command and working
directory, exit status, selected/executed/passed/failed/skipped IDs/counts,
covered requirements, and log/artifact references. Do not dump credentials or
unrelated environment values. Public summaries use sanitized relative paths;
local absolute paths belong in local records.

Do not expand every assertion into duplicated source-controlled JSON when
existing finite test classes and JUnit output express the requirement. Keep
existing records readable and compatible. A new evidence format is not a
required scientific-computing feature.

### 14.2 Checkpoint before a session boundary

Record exact active worktree/branch, HEAD/upstream, relevant dirty/untracked work,
current slice, completed code/tests, last command result, interrupted command,
required scratch paths, source/provider locks, pending CI IDs, blocker decisions,
and the next executable action. Inspect scripts before replaying them; checkpoint
text is not permission to evaluate arbitrary shell strings from metadata.

Use coherent commits when safe. Never commit user-owned unrelated files just
to claim a clean worktree. If changes cannot yet be committed, preserve the exact
patch and untracked contents through the established mechanism without claiming
that the patch contains everything automatically.

After Windows/WSL restart, recheck actual processes and outputs, identify which
command completed, and rerun only interrupted or invalidated work. Retain the
same master continuation. Do not recreate all earlier candidate directories.

### 14.3 Product identity and summary-only commits

Freeze the product candidate before integration gates. Product inputs include
source, headers, tests, generators, CMake/workflows, dependencies, flags and
accepted contracts—not just `.cc` files.

A summary-only commit may follow with its own Markdown/schema/link checks and
an exact unchanged-product comparison. Do not claim whole-tree CI on that later
commit unless actually executed. Do not trigger an endless cycle by amending a
report solely to name the latest SHA and then insisting on a fresh full audit
of the amended report.

### 14.4 Review findings and progress reports

Use focused review on actual source, numerical semantics, interface/resource
behavior, and integration. Fix actionable defects before the slice is accepted.
Independent review is useful when genuinely performed; do not describe a
self-check as independent or cross-model verification.

Report progress by integrated capabilities, row/mode gates closed, real blockers
resolved, and runnable examples. Candidate numbers, profile totals, assertions,
new directories, or generated lines are not the primary completion measures.

Keep the current PR summary short enough to show the actual product, support
matrix, remaining failures and next queue. Link detailed historical records;
do not replace the current status with pages of repeated old narratives.

## 15. Final gates and truthful exit states

### 15.1 Full inventory and build closure

Use the existing strict validators to verify every required row and actual
scalar signature, legal mode/storage class, workspace/report rule, exported
implementation, installed linkage and evidence reference. Regenerate the frozen
inventory canonically and review any difference. No denominator shrink, missing
symbol disguised by a system library, declaration-only route, or unconditional
unsupported body can pass.

Complete all remaining P00/P01/P04–P09 obligations. Reconcile P02/P03/P10's
technical scopes and owner gates. Complete P11 convenience/workflow,
documentation, package, platform and final evidence requirements. Keep optional
new-provider/GPU/native-expansion goals separate.

### 15.2 Full test and package closure

Run all required numerical and contract groups, existing parser/fuzz regressions,
sanitizer/static checks, package/relocation/header/ODR/ABI tests, component
isolation, required hosted profiles, and provenance/architecture validators on
the frozen final candidate. Preserve real nonzero results. An intentional
unsupported configuration needs its correct rejection test, not false runtime
coverage.

Check total and exact required test selection; unexplained disappearance of
cases or sudden test-count reductions must fail acceptance. Repair actual
regressions. Rerun affected tests after subsequent product changes.

Generate a readable final catalogue showing native, unchanged Reference,
experimental first-party and platform-specific results separately. Include
unresolved original-provider failures even if an approved alternative passes.

### 15.3 Allowed final outcomes

Use existing verdict names when suitable. The following names define the
required distinctions, not a demand to rebuild the status system:

| Outcome | Required meaning |
| --- | --- |
| `PROJECT_REVIEW_READY_FULL_SCOPE_VERIFIED` | Every mandatory original programme/profile gate and required owner/provenance decision is satisfied; ready for review, not automatic merge/release. |
| `TECHNICAL_WORK_COMPLETE_OWNER_DECISIONS_PENDING` | All executable mandatory implementation/validation work is complete, with only specifically enumerated owner decisions preventing full acceptance. Do not use if routines remain not started or actionable. |
| `ALL_UNBLOCKED_WORK_COMPLETE_FULL_SCOPE_BLOCKED` | No remaining ready required action exists; every unfinished row/profile maps to a precise unresolved external decision/resource and its blocked dependents. This is explicitly incomplete. |
| `SESSION_CHECKPOINT_CONTINUE_REQUIRED` | Work remains actionable but the execution session ended. Preserve the exact next action; do not call the programme finished. |

The integrated robust PPSVX and native/I/O milestone verdicts remain scoped
milestones. They are not the final outcome for this master continuation.

### 15.4 Required final report

Return one concise report containing:

- Exact local/remote/product/merge/tree identities and any uncommitted content.
- Before/after counts with consistent row/route units and a full P00–P11 status
  table separating implementation, numerical, platform and owner gates.
- Newly integrated public functionality and commands for maintained installed
  examples, taken from the actual checkout.
- Tests actually run, failures and supplementary replays, missing tools/runners,
  skipped tests, and exact support/instrumentation scope.
- Remaining decision packet entries, their affected rows, recommended actions,
  and the precise ready queue or reason none remains.
- Workspace count/usage changes, cleanup proposals and any separately approved
  actions; no hidden new workspace collection.
- A single resumable handoff location. No promise to continue in the background.

Do not finish with “next: review another candidate” when required ready work
remains in this assignment. Do not manufacture a completion date or guarantee a
single unlimited Codex session will close every externally blocked requirement.

## 16. Source layout, style, and permission boundaries

Use C++20 and the actual repository `.clang-format`, `.clang-tidy`, lint and
build rules. Reference the **live** Google C++ and Python guides; check them at
the start of this continuation or when a material rule is in doubt. Keep one
retrieval record rather than downloading a new copy for every routine.

The supplied `Google C++ Style Guide.html` and `Google Python Style Guide.zip`
are local references. Locate them only in provided/known project paths. If live
access fails, state that the snapshot is being used; do not call an unverified
snapshot current.

Preserve accepted compatibility decisions such as D003 and existing public
naming/types. Do not introduce an unrelated rename of camelCase, concepts,
filesystem-based APIs, or exception handling to follow a newly read rule. Apply
current guides to new code within those accepted constraints. Prefer readable
ordinary generated code to elaborate templates/macros.

Python tooling uses the supported Python version, safe parsing, clear interfaces,
useful typing/docstrings, checked subprocess results, and explicit error exits.
Do not add Python as a runtime dependency of installed C++ consumers. Do not
execute untrusted metadata or commands fetched from a document.

This task permits routine feature engineering and ordinary feature-branch
synchronization. It does not permit force-push, reset/clean of valuable work,
permission changes, credential disclosure, changing default branches, committing
provider binaries/runtime archives, merging PR #47, publishing, or moving
feature development onto `main` or `release/0.9.0`. Preserve the root policy that
release preparation is a separate activity.

---

## Appendix A. Independently derived PT starting fixtures

These are new suggested fixtures, not repository test results. They supplement,
not replace, the existing mathematical requirements. Confirm the arithmetic in
the test/oracle tooling before freezing expectations.

Take a three-by-three unit lower bidiagonal L with subdiagonal `ell0, ell1` and
positive diagonal factor `D = diag(2, 3, 5)`. Then

`A = L D L^H`,

with diagonal `[2, 3 + 2*|ell0|^2, 5 + 3*|ell1|^2]` and subdiagonal
`[2*ell0, 3*ell1]`. The real case uses transpose in place of conjugate transpose.

**Real example:** `ell0 = 1/4`, `ell1 = -1/2` gives

```text
A diagonal    = [2, 25/8, 23/4]
A subdiagonal = [1/2, -3/2]
Factor D      = [2, 3, 5]
Factor E      = [1/4, -1/2]  # lower interpretation
```

**Complex example:** `ell0 = 1/4 + i/8`, `ell1 = -1/2 + i/4` gives

```text
A diagonal    = [2, 101/32, 95/16]
A subdiagonal = [1/2 + i/4, -3/2 + 3*i/4]
Factor D      = [2, 3, 5]
Factor E      = [1/4 + i/8, -1/2 + i/4]  # lower interpretation
```

The upper entries of A are the conjugates of its lower entries. An upper unit
bidiagonal representation of the same factorization uses the conjugate
superdiagonal. This makes an incorrect UPLO/conjugation mapping observable.

Use two independently specified known solution columns, form B directly from
the original tridiagonal A, factor once, solve, and solve again with different
RHS. Check original-input preservation, factor reconstruction, and residuals.
Use non-real solution values in at least one complex test.

Use separate diagonal matrices with a zero/negative first, middle, or last
pivot to exercise documented factorization failure; keep N=0, N=1, NRHS=0,
strided RHS, and real/complex type-pairing tests distinct. Do not infer numerical
accuracy from a test containing only a zero matrix or zero solution.

## Appendix B. Same-file resumption instruction

After a genuine session interruption, use this instruction with this same file:

> Resume the master continuation at
> `programs/lapack-array-io/remaining-project-runbook.md`. Read the latest actual
> handoff and worktree once, preserve all work, identify completed/interrupted
> commands, and continue the first unfinished actionable requirement. Do not
> restart P00, housekeeping, native/I/O acceptance, or robust PPSVX integration.
> Keep one writer and existing roots. Continue the dependency-ready queue,
> including P09 and incremental P11, until full completion, an actual session
> limit, or all remaining work is specifically externally blocked. Retain the
> original numerical/provenance gates and return exact scoped results.

No automatic scheduling, permanent agent service, recursive self-invocation,
new workflow platform, or fresh worktree is required for resumption.

## Appendix C. Source register and inspection limits

The preparation of this continuation read the current PR metadata, root
instructions, current state excerpt, the existing full technical runbook, and
pinned PT source documentation. It did **not** inspect the user's local
filesystem, replay raw local evidence, or independently run project tests.
Recorded CI/test counts are attributed to the PR/user handoff, not reproduced
results from preparation of this document.

### Project sources

- **[S1] Current PR #47:** https://github.com/AI4SciComp/asc-cpp/pull/47
  — inspected at feature head `9032f622752d5014c673f08bc0a14a9ca1612989` on
  2026-09-10. Supports the integrated milestone, current identity, reported
  evidence, remaining route/platform decisions, and selected PT slice.
- **[S2] Current state:** https://github.com/AI4SciComp/asc-cpp/blob/9032f622752d5014c673f08bc0a14a9ca1612989/programs/lapack-array-io/state.json
  — inspected initial programme/package records. Older subordinate labels must
  be reconciled with later evidence rather than blindly copied.
- **[S3] Root instructions:** https://github.com/AI4SciComp/asc-cpp/blob/9032f622752d5014c673f08bc0a14a9ca1612989/AGENTS.md
  — C++20, six modules, compatibility, tests, out-of-source builds, and release
  boundaries.
- **[S4] Original full technical runbook:** https://github.com/AI4SciComp/asc-cpp/blob/9032f622752d5014c673f08bc0a14a9ca1612989/programs/lapack-array-io/runbook.md
  — full scope retained. The supplied local original has the SHA-256 recorded
  in Section 1. This continuation explicitly amends scheduling/lifecycle, not
  the frozen numerical and format requirements.
- **[S5] Adopted robust public contract:** https://github.com/AI4SciComp/asc-cpp/blob/9032f622752d5014c673f08bc0a14a9ca1612989/docs/contracts/robust-ppsvx.md
  — reference discovered in the current PR; Codex must read the actual current
  file before changing that capability. It was not independently re-audited
  during preparation of this continuation.
- **[S6] User-supplied prior continuation files:**
  `asc-cpp-workspace-consolidation-codex.md` and
  `asc-cpp-p09-p11-scheduling-amendment.md` — their pending scheduling and
  housekeeping instructions are incorporated here; separate execution is not
  required. The user's current handoff states that those tasks were not started.

### Pinned numerical sources

- **[S7] DPTTRF:** https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/dpttrf.f
  — LDL^T storage, no UPLO argument, and positive-INFO factorization semantics.
- **[S8] ZPTTRS:** https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zpttrs.f
  — real D, complex E/B, and mode-specific upper/lower factor interpretation.
  These inspected examples do not replace reading all eight actual signatures.

### Live guides and tool references

- **[S9] Google C++ Style Guide:** https://google.github.io/styleguide/cppguide.html
- **[S10] Google Python Style Guide:** https://google.github.io/styleguide/pyguide.html
- **[S11] Git worktree:** https://git-scm.com/docs/git-worktree
- **[S12] Git bundle:** https://git-scm.com/docs/git-bundle
- **[S13] CTest:** https://cmake.org/cmake/help/latest/manual/ctest.1.html

The live guide/tool pages were accessed during preparation on 2026-09-10.
Use locally installed tool capabilities and the repository's supported versions;
reading newer documentation does not authorize a toolchain or C++-standard
upgrade. Proposed scheduling, completion logic and fixtures in this document
are new instructions/reasoning, distinct from these source-derived facts.
