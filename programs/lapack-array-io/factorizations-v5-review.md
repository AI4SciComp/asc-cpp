# Native and reference factorization integration v5

This is an in-progress integration, not full P05/P06 or reference-profile
completion. All implementation remains on `feature/lapack-array-io` based
on develop, with the original dirty release checkout preserved. The previous
product commit is `3d5909d6c26ba2d274749106bda448e2280b8330`, pushed to
[draft PR 47](https://github.com/AI4SciComp/asc-cpp/pull/47). Its tested product
tree is `eebeb3a3632e66f9d07d77dc3f522cb88dc404f1`; program-state-only
changes distinguish that tree from the commit. None of its 315-provider or
271-provider-free results is attributed to newer v5 source.

## Integration scope

- Native S/D/C/Z POTRF/POTRS and GEQRF, plus separately named native Q
  formation/application conveniences. Twelve new native rows make twenty
  implemented-unverified native rows, not ORG/UNG/ORM/UNM native credit.
- Reference S/D/C/Z POTRF/POTRF2/POTF2/POTRS/POTRI/POSV (24 rows),
  GEQRF/GEQR2 and real ORGQR/ORMQR or complex UNGQR/UNMQR (16 rows), and
  LASWP/LAQGE (eight rows). Together with the existing 44 LU rows this is
  92 partial reference rows against the unchanged 2,113-row requirement.
- Source-derived original LU integer/cursor/query guards, root-reviewed
  reference Cholesky original-ASC-stride correction, and explicit nonempty
  workspace/operand admission across the older provider routes.
- Atomic five-header ownership/self-containment/package registrations and
  the partial mapping profile `incremental-factorizations-v5`. The public
  inventory now contains 79 headers. No private helper becomes installed API.
- Provider-free public native factorization example, independently installed
  using only Dense; three additional public-only provider consumers for
  Cholesky, QR and LU helpers. Existing LU consumers remain present.

Frozen family reviews retain the exact code, provider, external archive,
raw log, numerical and bounded sanitizer/allocation identities:
[native Cholesky](native-cholesky-review.md),
[native QR](native-qr-review.md),
[reference Cholesky](reference-cholesky-review.md),
[reference QR](reference-qr-review.md),
[LU helpers](lu-helpers-review.md),
[LU integer correction](lu-integer-review.md), and
[provider placement](provider-placement-review.md).
The root read the actual headers, source, private helpers, tests and full
applicable runbook requirements before registration; generated declarations
are not credited as execution evidence.

## Placement integration amendment

The imported placement candidate archive is
`provider-placement-7XFHFr5a/source-candidate-02.tar`, SHA256
`b749747709a1cbd6a337e1cebbcf9c7835f99317d3c88bee880aad8ba67b78e3`.
That exact candidate passes 35/35 affected CTests in Debug, Release and scoped
ASan/UBSan for each actual LP64/true-ILP64 provider, with zero skips. Its old
workspace and separate old-pivot controls fail all four scalar processes for
each ABI. Full numerical family tests remain in those lanes.

Root additionally retained the pre-existing explicit Host/Pinned predicate
at both writable-pivot sites, together with the new context admission check.
This defensive amendment changes those two source hashes, not the current
public Serial context behavior. The owner's exact candidate evidence is
retained separately; the amended source requires the new combined run.
The neutral workspace validator and unused zero-byte compatibility are
unchanged. Each existing old route's nonempty supplied regions, including
unused roles, must now pass the explicit context before packing or entry.

## Installed-consumer diagnostics

The external runner `factorizations-v5-consumer-diagnostic.sh` builds the new
consumer source and its matching new adapter against the exact v4 base
Dense/Core/provider-construction libraries and real pinned providers. This is
a direct compilation/runtime diagnostic, not a relocated installed package or
combined-placement test. Each consumer executes all four scalar variants.

- Cholesky: all three factor algorithms, both triangles and independent A/B
  layouts, known selected factor, widened residual, repeated factor reuse,
  selected inverse product, preserved padding and failed-last-minor INFO=2
  for all three factors and POSV.
- QR: both factor algorithms, all factor/Q/C layouts, actual generation and
  left/right N/real-T/complex-C application. Widened reconstruction,
  orthogonality and independent explicit-Q products; immutable factors/tau,
  padding and actual-query versus formula-only reporting.
- LASWP/LAQGE: forward/reverse sequential swaps with nontrivial permutation,
  all four scaling modes, both layouts, exact pivots/scales/padding and
  genuine foreign calls without invented INFO.

Attempt 01 and attempt 02 compile and run all three consumers successfully
for both ABIs. Attempt 01 strict lint fails five findings: three nodiscard
accessors, a nested conditional, and a false-positive transposed-index call.
Attempt 02 retains the last transposed-index lint finding. The code uses
explicit mathematical mirrored indices and a named factor-query helper;
no lint check or numerical assertion is suppressed. Attempt03 catches a QR
size-to-double arithmetic conversion; attempt04 passes Cholesky/QR strict
checks and catches the helper's missing direct LapackFactorFamily header.
All four attempts pass the six numerical processes. The first frozen archive
`p05-p06-factorizations-v5-01/source.tar`, SHA256
`5e613cec806ac931f463827552fd289256aa0497d0295512cf0405f72615201a`,
tree `4d8e103beb92506e214e7f2f0aff9a47973af2e3`, is retained without
combined-run credit. Candidate02 adds the direct type include and refreshed
artifact hashes before any full build. Final lint/runtime and relocated
package runs are still pending at that point. Attempt05 subsequently passes
all six direct numerical processes and all three strict Clang18 checks with
exit zero. Raw logs retain unique
`logs/v5-installed-{family}-{abi}-{build,run}-{attempt}.log` and
`logs/v5-installed-{family}-tidy-{attempt}.log` names under the evidence root.

## Remaining work and exact next task

Candidate02 is frozen at Git tree
`a97f793c140b514717a8c9438b4f84ac2c56f81e`, archive SHA256
`dd4469842a0ab9cb846e237b5e0dabf304e1c7b7054855133a81a823fe48a496`,
mapping SHA256
`a58be7ec2d1ec1a4a5c9545abf976d5a1e90a58e366aea2fe17d7bd1c9eb651a`.
The archive, immutable source, metadata, five full build/test lanes and strict
documentation runner are in `p05-p06-factorizations-v5-02` under the evidence
root. The partial validator passes 3,551 discovered/2,113 required/92 reference
in-progress/2,021 not-started/20 native implemented-unverified, zero verified.

Candidate02 full LP64 and true ILP64 each finish 343 passed/1 failed/344
executed; provider-free Debug and Release each finish 276 passed/1 failed/277,
and shared Release finishes 278 passed/1 failed/279. All five runs have zero
skips and fail the same architecture dependency-manifest test: its exact
capability-name and status consumer still describes the prior LU-only scope.
The correction adds the new native capability, renames the incremental
reference capability and checks their explicitly scoped runtime-tested status.
The exact-set, ownership, dependency and status checks remain active; no test
is weakened or excluded. Full fresh-source reruns are required for candidate04.
Both actual-ABI relocated provider packages pass all five consumers, including
the two existing LU and three new factorization/helper consumers, in the failed
full candidate02 runs. These scoped passes do not make the full runs pass.
Candidate02's independently rebuilt native Clang19 ASan/UBSan suite passes
5/5 with zero skips; strict Clang18 checks of the root-amended pivot sources
and changed-file formatting also pass. System libraries are not instrumented.

Candidate02's Doxygen command fails because included-by graphs for result.h
and status.h require 52 and 53 nodes, beyond the configured maximum50.
The raw warning log and failed command record are retained. The correction
raises graph capacity to256 so those graphs are actually emitted; it does
not disable graphs, reduce depth, suppress warnings or change the zero-warning
coverage gate. A separate frozen documentation revision will be checked;
numerical libraries/tests remain unchanged from candidate02. Documentation
revision03, tree `ace0cf3c11c95fb3384c1627b7e1217a0de5fb31`, archive SHA256
`7bf52da3685f1d7e5e9cf726d5ff4cf307ed0f5b8e5c2e84ba42ecb18df63228`,
passes Doxygen with 79/79 headers, 1,573 documented public members and zero
warnings, plus all81 Markdown files. Both larger graph SVGs are present.
Only Doxyfile graph capacity/comments differ outside program records from
candidate02. These documentation results are not numerical runs of revision03.

Independent P05 expert and band-positive-definite implementations and P06
least-squares run in separately linked worktrees, never simultaneous writers
to this tree. They are not in this v5 scope until imported and verified.
Remaining structured, indefinite, orthogonal/rank-revealing, least-squares,
SVD/GSVD/CSD, spectral/matrix-equation, specialized, mixed/extra-precision
and deprecated routines remain required, as do P02/P03/P10 exact mode
evidence and P11 platforms/docs/license gates. No package or full profile is
marked complete. GEEQUB, GERFS, GESVX and LAQGE mathematical limitations and
owner/redistribution approvals remain explicit in [blockers.md](blockers.md).

## Completed candidate04 command checkpoint

The full corrected rerun uses tree
`f021fb84e345c355affe65d8c06119c14a18e594`, archive SHA256
`af1a759135aea53d8b9fed7c999cd876198674c69bbc5abb2d66e7a82f658155`,
with the same mapping `a58be7ec2d1ec1a4a5c9545abf976d5a1e90a58e366aea2fe17d7bd1c9eb651a`.
Only the graph configuration and exact capability-name/status consumer differ
outside program records from candidate02. All five configure/build commands
pass. Full LP64 and true ILP64 each pass344/344; provider-free Debug and
Release each pass277/277; provider-free shared Release passes279/279. Every
run has zero failed and zero skipped tests. Both relocated provider packages
pass all five public-only consumers. Documentation on this same candidate
passes79/79 headers,1573 public members and zero warnings; Markdown81 passes.
Raw command/JUnit/source identities remain external in
`p05-p06-factorizations-v5-04`, indexed in
[verification-checkpoints.json](verification-checkpoints.json).

This is actual command evidence, not complete mode/contract closure. A later
rank-revealing source audit identified an additional INTEGER row-cursor gap in
the pinned LARF1F/LARFB call graph, including existing reference QR and imported
least-squares. Root confirmed it, recorded REFLECTOR-ROW-CURSOR, and started
separate uncommitted guards/regressions for v6. Those changes and the imported
20 Cholesky expert plus12 least-squares rows are not in candidate04 or this
v5 code checkpoint. The v5 staged source remains exact; future fixes require
their own source identities, family and full integration verification.
