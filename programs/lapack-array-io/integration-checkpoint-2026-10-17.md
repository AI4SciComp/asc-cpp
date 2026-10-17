# Development integration and continuation — 2026-10-17

The maintainer requested integration while preserving unfinished work and its
plan. The destination is local `develop`, as specified by repository governance.
This checkpoint preserves the feature history and open work without promoting
unverified routines or changing the release support contract.

## Branch and release boundaries

- Integration starts from `46412183b2ae86101b2361c52376a8db8efff264`, the local
  `main`, `v0.9.0` and recorded `origin/develop` baseline.
- `feature/lapack-array-io` contains 155 subsequent commits through numerical
  checkpoint `df91a449037b5f16945f6aa6bf3602c3481970f7`. Preserve this ancestry in
  the merge; do not replace it with a source-only copy or a squash.
- The existing `asc-cpp-lapack-array-io` directory becomes the local `develop`
  checkout. Its name is retained because historical evidence uses that path.
- `main`, the immutable release tag, `release/0.9.0`, and the release worktree's
  uncommitted changes are not part of this integration.
- `ASC_CPP_ENABLE_LAPACK` and experimental RobustPpsvx remain opt-in. Project
  version metadata is unchanged; this merge is not a new 0.9.0 publication.
- Historical no-merge statements are superseded only for this authorized local
  development integration. Provider decisions, numerical acceptance, security,
  platform admission, release review, tagging and publication remain separate.
- Remote refs and PR #47 were not refreshed as part of this local checkpoint.
  A remote integration must first reconcile the actual current remote state.

## Preserved implementation state

The maintained [state](state.json), [coverage ledger](
../../docs/contracts/lapack-coverage.yaml) and [family backlog](
remaining-reference-backlog.json) retain the same requirements and credit:

| Item | Count |
| --- | ---: |
| Required Reference routines | 2,113 |
| Callable Reference routines awaiting full verification | 330 |
| Reference routines in progress | 296 |
| Reference routines not started | 1,487 |
| Fully verified Reference routines | 0 |
| Verified native routines | 20 / 20 |

The accepted finite native/array-I/O subset remains documented in the
[stabilization review](stabilization-review.md). A local default-build regression
check cannot substitute for the complete Reference-provider profile matrix.
Known required numerical failures remain failures; no tests or gates are waived.

## Provider metadata draft

Nine previously untracked source/header/test files are preserved byte-for-byte
in [provider-metadata.patch](drafts/provider-metadata.patch). The accompanying
[manifest](drafts/provider-metadata-manifest.json) records their original paths,
sizes and SHA-256 identities. Applying the patch in an empty temporary directory
and comparing every restored file verified preservation before relocation.

These files were never registered, built or credited. They remain outside the
active source and public include trees so unfinished declarations are not
installed as supported APIs. Existing native prerequisite observations are
retained in `state.json`; they do not validate the drafted adapters.

To resume from the repository root on a dedicated continuation branch:

```sh
git apply --check programs/lapack-array-io/drafts/provider-metadata.patch
git apply programs/lapack-array-io/drafts/provider-metadata.patch
```

The check must succeed against the current tree. If those paths later acquire
implementations, reconcile the draft with them rather than overwriting files.
The patch is a recovery artifact, not an executable build step.

## Remaining execution plan

1. Resume `P09.required.ilaprec`, followed by `P09.required.ilatrans`. Finish the
   six ILAPREC, ILATRANS, ILAUPLO, ILADIAG, CHLA_TRANSTYPE and ILAVER adapters;
   complete their fault/validation tests and installed consumer.
2. Register source, public headers, test targets, documentation and ABI/header
   baselines together only when the draft is ready. Validate both actual integer
   ABIs and the required installed profiles before assigning implementation or
   verification credit.
3. Continue the maintained family backlog under the original P00–P11 scope.
   Review actual prerequisites before each family; do not restart completed
   packed work from older next-task prose.
4. Retain the [blocker records](blockers.md), mathematical failures, missing
   XBLAS definitions, [owner decisions](owner-decisions.md), provenance review,
   required platform checks and remote/security acceptance work. Integration
   into `develop` resolves none of these obligations.
5. Complete the full candidate and release gates before any supported full
   Reference claim or publication. The local preservation merge is an earlier
   administrative integration, not completion of the final release milestone.

The following original plans and maintainer instructions remain preserved:

- [Original program runbook](runbook.md).
- [Master continuation](remaining-project-runbook.md).
- [Full completion plan dated 2026-09-13](completion-plan-2026-09-13.md).
- [Remaining-project instructions](asc-cpp-complete-remaining-project-codex.md).
- [Resume instructions](../../asc-cpp-resume-all-remaining-codex.md).
- [Robust PPSVX owner instruction](../../asc-cpp-robust-ppsvx-owner-instruction.md).

Use the current state and this checkpoint for branch, draft location and next
task. Older dates, counts, running-process labels and sandbox constraints remain
historical evidence, not assertions about the current environment.

## Local recovery material

The external directory `../workspace/reports/lapack-integration-20261017`
contains a verified complete Git bundle, index/worktree patches, verified
archives of all 357 outstanding files from the release, integration and
placement worktrees, and the draft-restoration audit. Keep it until an
independent recovery copy is confirmed. Cleanup hash inventories alone are not
source backups.

The remaining placement worktree is preserved separately. Fourteen historical
experiment worktree paths were already absent when this integration started;
their Git registrations do not prove preservation of their former untracked
files. Reconcile archived/evidence copies, especially historical SVD work,
before treating those experiments as fully integrated or deleting their refs.
