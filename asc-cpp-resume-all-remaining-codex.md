# ASC-C++: resume and complete the remaining programme

**Recovery instruction — 2026-09-13**\
**Programme:** `ASC-CPP-LAPACK-IO`\
**Repository:** `AI4SciComp/asc-cpp`\
**Existing integration worktree:** `/home/yicai/AI4SciComp/asc-cpp-lapack-array-io`\
**Branch:** `feature/lapack-array-io`; draft PR #47 targets `develop`.

This entire document is the instruction for the new Codex session. Execute the
existing remaining-project assignment, rather than writing another plan or
restarting its original first family. Use durable local state to recover work
from the interrupted session. Continue implementing, testing, documenting and
integrating the remaining dependency-ready requirements.

## 1. Objective and authority

Resume the full P00–P11 programme under
`programs/lapack-array-io/remaining-project-runbook.md` and its underlying
`programs/lapack-array-io/runbook.md`. Those files remain the detailed technical
specification. This instruction clarifies recovery and continuing execution;
it does not reduce their required scope.

Ordinary development, checked API additions within the approved programme,
maintained tests, documentation, build/export/package integration, truthful
coverage updates, feature-branch commits, normal non-force pushes to the existing
feature branch, and evidence-based updates to draft PR #47 are authorized.
Do not ask for another task instruction after each completed family, candidate,
contract review or test batch. Continue the ready queue in this session.

Keep the runtime's sandbox, command approvals, network policy and repository
instructions in effect. Ordinary task authorization is not permission to bypass
a tool restriction or a denied command. Request a narrowly scoped permission
through the normal mechanism when genuinely necessary.

The following remain outside this authorization unless a later explicit owner
decision exists: destructive cleanup; deleting worktrees or branches; force-push
or history rewriting; adopting or changing a third-party notice packet; changing
or patching the pinned provider; importing/bundling dependencies or runtimes;
waiving mathematical requirements; creating a silent fallback; expanding an
experimental algorithm's adoption beyond its approval; merge, tag or release.
Do not manufacture an owner decision from successful tests or from this request
to finish the project. Collect pending decisions and continue independent work.

## 2. Current remote evidence is a discovery hint, not a reset target

Preparation of this instruction read PR #47 and the latest recovery record at:

- Feature HEAD: `cd0d9aad74211d9f770ee90cdd948ef77a0592c1`.
- The newest recovery section records RK condition estimators `SYCON_3/HECON_3`
  as integrated with required numerical failures still open.
- RK producers and `SYTRS_3/HETRS_3` solves are also already integrated.
- The recorded next task is `P05.required.hetri_3`, including the `TRI_3X`
  inverse dependencies, followed by the RK drivers.
- The recovery record reports 2,113 required Reference rows: 194
  implemented-unverified, 296 in progress, 1,623 not started, and zero fully
  verified; 234 routine contracts are reviewed. These are different counting
  units, not completion percentages or a required result to reproduce.
- The native20/array-I/O subset and the separate experimental robust PPSVX
  integration remain protected prior milestones.

The PR body still contains an older GERFS checkpoint and older counts. Do not
prefer that narrative over source-bound newer records. The local checkout may
be newer than every SHA above. Never reset, rebase or check out an old revision
merely to match this document. No local filesystem or test execution was
inspected while preparing it.

In particular, do not restart PTTRF/PTTRS, PPSVX, or the initial P09 slice from an
old prompt. Locate the actual unfinished action. An interrupted later RK inverse
or other family takes precedence over the remote next-task hint.

## 3. Recover once, preserving all work

First read the applicable root/nested `AGENTS.md`, this instruction, the newest
recovery section, current programme state, current task record, source/provider
locks, and the relevant remaining-runbook sections. Read large histories only
where they are needed to resolve an actual ambiguity.

Inspect the real branch, HEAD, worktree registration, index, staged/unstaged
changes, untracked user files, relevant recent commits, and remote tracking
state. Confirm the repository identity. Network failure during a read must not
be misreported as missing remote work. A local/remote difference alone is not
permission to overwrite either side.

Check for a still-active Codex writer, build, CTest process or evidence runner
using this worktree/build directory. Correlate a process with its working
directory, command, start time and current log, not just a PID or name. Do not
start a competing writer, duplicate a live test, kill processes by wildcard, or
remove locks blindly. If another live writer owns the workspace, stop conflicting
writes and report that concrete conflict; read-only inspection may continue.

Preserve pre-existing staged, unstaged, untracked and valuable ignored files.
In particular, preserve user instruction files and unfinished numerical work.
Do not automatically stash them, run `git clean`, use a hard reset, apply an old
patch again, or stage everything with `git add -A`. Record a compact baseline
and, where needed, a small patch/file inventory in the existing evidence area.
Do not copy the entire repository or all build directories for this recovery.

Use these maintained files when they exist:

- `programs/lapack-array-io/recovery-status.md`
- `programs/lapack-array-io/state.json`
- `programs/lapack-array-io/remaining-reference-backlog.json`
- `programs/lapack-array-io/owner-decisions.md`
- `docs/contracts/lapack-coverage.yaml` and the existing evidence records
- `programs/lapack-array-io/remaining-project-runbook.md`
- The current family's review and the exact local handoff referenced by it.

A recorded success is reusable only for its actual source, test, provider,
configuration and execution identity. For an interrupted command, inspect its
exit status and complete logs/artifacts before deciding whether to rerun it.
A partial log, successful configure, or available executable is not proof that
its tests completed. Do not assume the transport error rolled back filesystem
changes or that every child command necessarily stopped.

Once state is coherent, proceed to the unfinished implementation/verification
action. Recovery is not a new full repository audit. Only missing, interrupted,
failed or invalidated evidence needs fresh execution.

## 4. Use the live backlog; do not serialize every package behind one blocker

Keep one active implementation writer and one current numerical delivery slice.
Choose work from actual dependencies, not from the oldest task pointer in this
chat. Whole-package dependencies constrain final acceptance; they do not block
an independent subtask whose actual prerequisites are available.

Priority order:

1. Finish the meaningful interrupted action in the current family. Integrate
   owned changes and close its actionable checks rather than discarding them.
2. Continue the current dependency chain. At the remotely inspected checkpoint,
   that means RK inverses, their `TRI_3X` requirements, and RK drivers. Check the
   pinned inventory for every actual scalar variant and exact routine name.
3. Deliver other ready P04–P09 families and existing partial implementations.
   Start missing functionality as well as closing existing evidence gaps. Do not
   replace implementation indefinitely with review-only or metadata-only work.
4. Perform P11 integration, installed examples, documentation and evidence
   mapping with each slice; do not defer all delivery until the end.
5. Revisit a blocked task when its dependency, resource or authorization changes.
   Otherwise continue other required work and keep its blocker visible.

P09 and P11 have already started. Do not automatically repeat mixed-precision,
DMD or auxiliary work merely because the original runbook once labelled them
not started. P02/P03/P10 have an accepted bounded technical scope; reconcile
remaining requirements without reimplementing accepted printing, file formats
or Matrix Market.

The full remaining scope stays in force:

| Package | Continue the genuinely outstanding obligations |
| --- | --- |
| P00 | Inventory, row/evidence consistency, actual decisions and scope reconciliation. |
| P01 | Missing common storage, descriptors, workspace/report/ABI contracts required by later operations. |
| P02–P03 | Unclosed printing/I/O requirements and affected regressions only; preserve accepted functionality. |
| P04 | General systems, LU, experts, diagnostics and provider integration. |
| P05 | Structured systems and all inventoried factorizations, solves, inverses, condition/refinement/driver variants. |
| P06 | Orthogonal/unitary factorizations, applications, rank-revealing, ordinary/generalized/constrained least squares. |
| P07 | Required SVD, GSVD, CSD and computational variants. |
| P08 | Standard/generalized eigenproblems, Schur/QZ, reordering, condition estimates and matrix equations. |
| P09 | Missing mixed/extra-precision, DMD, conversions, auxiliaries, compatibility and exhaustive catalogue work. |
| P10 | Remaining Matrix Market package/acceptance obligations, not an I/O restart. |
| P11 | Continuous integration/docs/package/examples; actual provider/platform admission; final full-profile evidence. |

This table is an index, not an exclusion list. Derive exact rows from the pinned
inventory and existing runbooks. Do not invent routines from naming patterns,
introduce stub implementations to clear counts, or generate declarations and
call them completed capabilities. Do not expand into general sparse solvers,
new GPU algorithms, a new agent framework, or a wholesale native LAPACK rewrite.

## 5. Deliver each slice as maintained executable code

For each selected family, follow this loop without stopping at its first
successful substep:

**Resolve its exact contract → implement/repair → run targeted tests → integrate
public build/export surface → run required installed and profile checks → map
truthful evidence → checkpoint/commit → choose the next ready task.**

Read exact pinned signatures and relevant implementation semantics: scalar
pairings, storage and factor provenance, conjugation/transpose rules, ignored
inputs, workspace sizes/units, integer bounds, native status, mutation on
failure and output validity. Reuse existing reviewed infrastructure. A missing
contract is a concrete prerequisite, not a reason for an open-ended review.

Implement the checked public entry points, real execution path, build sources,
public headers, exports, useful Doxygen and public example/consumer. Keep
caller-owned workspace, checked arithmetic, actual object lifetimes, explicit
packing/placement and existing error/report contracts. Preserve C++20, the six
owning modules, existing BLAS/Random semantics and CPU/CUDA isolation.

Use independent reconstruction, residual, known-solution, orthogonality,
subspace or other appropriate mathematical checks alongside provider-fidelity
checks. Exercise required legal options, real/complex variants, empty cases,
failure paths, relevant alias/workspace guards and actual ABIs. Reuse existing
finite test requirements; do not invent a much larger acceptance matrix after
every passing checkpoint unless a concrete defect or missing contract requires
it. Record any justified additional requirement explicitly.

Integrate usable, policy-permitted code into the existing feature branch rather
than leaving every family permanently in an external candidate. An engineering-
complete but numerically blocked route may only receive its existing permitted
incomplete status; it must not be promoted to fully verified or supported beyond
its evidence. Where integration itself needs an owner decision, preserve the
patch, identify that exact decision, and continue independent work.

Before a commit, inspect the staged diff and include only the intended owned
changes. Preserve unrelated pre-existing index entries and dirty user files.
Do not commit another task's work or historical evidence wholesale. Push normal
feature commits and update the existing draft when permitted; do not merge it.

## 6. Numerical, dependency and security blockers

Preserve the pinned Reference provider and its required mathematical failures.
First-party robust PPSVX remains separate, explicitly selected and experimental.
Its success is not verification of the corresponding Reference routine. Its
specific authorization is not permission to create or adopt new robust routes
for every other family.

For a newly encountered failure, classify it with sufficient evidence as an ASC
implementation defect, an objectively incorrect test/oracle, a provider behavior
that fails an unchanged project requirement, or an environmental/resource issue.
Repair ordinary ASC defects and demonstrate any justified oracle correction
without removing valid inputs or weakening the mathematical requirement.

For an already established provider cause, link affected rows to the existing
cause and reuse valid reproductions. Do not repeatedly run an unchanged broad
sweep merely to rediscover it. Preserve failure visibility and execute it when
an affected regression or mandatory final profile requires it. Never disable,
turn into a passing expected-failure, loosen, or exclude a mandatory gate to
obtain a green summary.

Keep one current `owner-decisions.md` packet. Consolidate related provider
causes and their affected rows rather than requesting the same decision for
every scalar or wrapper. Propose exact remediation choices, scope and evidence;
do not record their approval. A source-preserved provider cannot simultaneously
retain a reproduced failure and pass that same numerical requirement.

Preserve the extra-precision/XBLAS source-selection, ABI, numerical-environment
and notice decisions. Inspect the current dependency closure rather than
assuming every missing definition has the same cause. Continue independent
rows while affected rows wait. Do not change library selection, pointer widths,
precision or the floating-point environment silently.

Inspect current CodeQL **alerts** as well as analysis-run status. A successful
analysis job does not close an outstanding security finding. Repair an in-scope
finding with a regression and preserved evidence; do not suppress or dismiss it
merely to close a gate. Inspect it before importing affected code. Do not infer
severity, exploitability or closure from the recovery record alone.

## 7. Verification without another endless stabilization campaign

Use the repository's actual scripts, presets and installed tool versions. Inspect
script inputs, outputs and side effects before executing historical continuation
commands. Do not invent flags, provider targets, CMake commands or test IDs.

During editing run the smallest meaningful relevant checks. At slice delivery,
run its required ABI, runtime, sanitizer, concurrency, package, documentation
and static-analysis checks. At integration boundaries run affected regressions.
The final full programme still requires all mandatory full-profile checks.
Nothing here lowers the established acceptance criteria.

Reuse compatible out-of-source builds and exact-input evidence. If an input
changes, invalidate only the dependent evidence where the existing policy
supports that determination. Do not label reused tests as freshly run or reuse
old whole-tree CI for new product code. Record the feature SHA, merge/test tree,
provider/ABI and selected test identities separately.

Keep exit codes, actual selected/executed test IDs, failed cases and unexpected
skips. Zero selected tests and unavailable tools are not passes. Pipeline logging
must not mask a failing build or test. A task with only known required numerical
failures remains failed in that dimension; it can still have passing engineering
results and permit independent development.

Do not saturate the machine with expensive builds while timing-sensitive
observers run. Run observers with the established controls and limits. Preserve
failed/timeout attempts and distinguish a later unchanged-binary replay from an
original passing invocation. Do not retry indefinitely or change limits to hide
a defect. A transport error does not by itself diagnose proxy or network cause.

Wider Windows/macOS/compiler/provider/linkage admission needs actual evidence.
Provider-free CI is not provider admission. A missing runner or permission is a
specific blocker, not proof of failure or success. Do not change public support
guards before the required admission evidence and authorization are present.

## 8. Bounded workspace and durable checkpoints

Use the existing source worktree, `/home/yicai/AI4SciComp/asc-cpp-build` and
`/home/yicai/AI4SciComp/asc-cpp-evidence` roots. Reuse the current compatible task
build/evidence locations. No new sibling worktrees, full clones, top-level
project directories, or full source snapshots per test iteration. Keep build
products outside source. Do not move active paths just to tidy the layout.

Keep valuable existing worktrees/evidence intact. If the housekeeping proposal
already exists, do not repeat it. Unapproved retirement is a pending owner action,
not a prerequisite to numerical work unless actual storage prevents execution.
Keep small new test fixtures and executable test tooling in the repository, not
permanently dependent on `/home/yicai/.../asc-cpp-evidence` paths.

Use the current programme state/recovery record and the existing latest-handoff
location. Add a small field or missing handoff only if needed; do not introduce
a second workflow framework or a new directory for each checkpoint.

Before a costly command and after a meaningful edit/test/commit boundary, record:

- Current worktree, branch, HEAD and uncommitted source identity.
- Active task, exact next action and existing build/evidence paths.
- Command/cwd/configuration and source/provider inputs; omit credentials.
- State: planned, running, passed, failed, interrupted or unknown.
- Actual exit code and log/result locations when known.
- Completed tests and the input scope in which they remain reusable.
- Remaining engineering work and linked approval/resource/numerical blockers.

Update the current checkpoint in a recoverable way and keep old raw failures,
without generating another large narrative history. A missing final chat reply
must not be the only place the next action was stored. Do not claim the command
completed before its exit and artifacts were checked.

Continue while the current session is available. Do not invoke Codex recursively,
install a service, launch unattended restart loops, or claim background work.
After a genuine session interruption the user can rerun the same launch command;
this instruction recovers from disk rather than depending on the broken remote
compaction history. This does not repair or guarantee a network connection.

## 9. Honest progress and stopping conditions

After recovery give a short statement of the recovered active task and next
executable action, then execute it. After a delivered family, report only its
new callable functionality, commit, scoped tests, blockers and next task while
continuing. Do not stop with only another implementation plan when authorized,
actionable work remains.

Maintain separate current counts for required rows, not-started rows, partial
rows, callable-unverified rows, fully verified Reference rows, native operations
and separately named first-party algorithms. Recompute using existing validators;
do not copy stale PR numbers. Declarations, scalar routines, reviewed contracts,
mode cases, test processes and assertions are different units. Report progress
in integrated capabilities as well as tests, not raw lines added or audit count.

Stop only for one of these outcomes:

1. **Full programme review readiness:** all mandatory rows and P00–P11 numerical,
   contract, platform, package, documentation and owner/provenance gates pass.
   Merging and releasing still need their separate authority.
2. **Remaining work specifically blocked:** no authorized ready task exists;
   every remaining task has a concrete decision/resource/dependency blocker and
   a precise unblocking action. This is not programme completion.
3. **Session/resource/interruption checkpoint:** preserve the exact unfinished
   action and local state when execution genuinely cannot continue. This is not
   a claim that the rest of the backlog is externally blocked.

Do not stop just because an owner decision is pending for one family. Do not
promise that one CLI invocation can survive disconnection or finish unlimited
work. Do not say the entire programme is done because the selected subset passed.

At a genuine stop give a compact final report containing: outcome; actual HEAD
and dirty-state preservation; newly integrated capabilities; scoped passing and
failing gates; current counts; blockers; and one exact resumption action using
the same existing task records. No new single-family prompt should be required.

## 10. Style and source references

Use the current Google guides and the repository's accepted compatibility rules:

- C++: https://google.github.io/styleguide/cppguide.html
- Python: https://google.github.io/styleguide/pyguide.html

The uploaded Google C++ HTML and Python ZIP are reference copies when actually
available locally; do not invent a local path or copy ChatGPT sandbox paths into
build scripts. Fetch the live guides when permitted, retain checked-in
format/tidy settings and accepted exceptions, and do not change the C++ standard
or public API solely because a guide evolves. A failed guide fetch is not
permission for an unrelated style rewrite.

Preparation read these project sources at the pinned discovery revision; the
new session must inspect the current local versions:

- PR #47: https://github.com/AI4SciComp/asc-cpp/pull/47
- Current recovery record: https://github.com/AI4SciComp/asc-cpp/blob/cd0d9aad74211d9f770ee90cdd948ef77a0592c1/programs/lapack-array-io/recovery-status.md
- Existing complete continuation: https://github.com/AI4SciComp/asc-cpp/blob/cd0d9aad74211d9f770ee90cdd948ef77a0592c1/programs/lapack-array-io/remaining-project-runbook.md
- Original technical specification: `programs/lapack-array-io/runbook.md`.

CLI invocation options were checked against the official command reference:
https://developers.openai.com/codex/cli/reference/

Begin with state recovery now, then continue the entire remaining ready queue.
