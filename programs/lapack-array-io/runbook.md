# Codex execution runbook: full LAPACK capability and array I/O for asc-cpp

**Program:** `ASC-CPP-LAPACK-IO`  
**Runbook version:** 1.0 — 2026-09-07  
**Target:** `AI4SciComp/asc-cpp`  
**Endpoint:** implemented, tested, documented, installable changes on feature branches, ready for review. Publication and merging are separate authorized actions.

> **Execution directive:** This is an implementation task, not a request for another plan. Read the repository instructions, establish the evidence baseline, and implement P00–P11. Continue through every unblocked work package. Do not stop after scaffolding, a provider discovery check, a few factorizations, array printing, or a list of future issues. Preserve unfinished work and an exact restart point when the execution session ends. Never label unfinished capability complete.

## Contents

1. Mission, scope, and interpretation
2. Execution protocol, authority, and persistent state
3. Architecture, naming, and compatibility
4. P00 — baseline, upstream inventory, and frozen contracts
5. P01 — scalar, descriptor, workspace, ABI, and result foundations
6. P02 — Dense and Sparse printing
7. P03 — native text and binary array I/O
8. P04 — Reference-LAPACK adapter and native LU
9. P05 — structured direct systems and Cholesky
10. P06 — orthogonal factors and least squares
11. P07 — singular values, GSVD, and cosine-sine decomposition
12. P08 — eigenproblems, Schur/QZ, and matrix equations
13. P09 — specialized routines and completeness closure
14. P10 — Matrix Market interchange
15. P11 — integration, documentation, packaging, and final evidence
16. Cross-cutting numerical verification
17. Coverage manifests and tooling requirements
18. CI and reproducible command protocol
19. Reviews, commits, blockers, and final handoff
20. Final acceptance checklist
21. Source register

---

## 1. Mission, scope, and interpretation

### 1.1 Required outcome

Deliver both approved tracks:

**Track A — Full, version-defined LAPACK capability.** Add a checked C++20 ASC interface for the public driver and computational capabilities of Reference-LAPACK 3.12.1, with all actual applicable precisions, documented modes, structured storage forms, diagnostics, and necessary expert auxiliary operations. Provide the complete CPU capability through an explicitly enabled, pinned reference provider. Independently implement the foundational native LU, Cholesky, and Householder QR functionality. Preserve a genuinely provider-free default installation.

**Track B — Array usability and persistence.** Add value printing and portable reading/writing for Dense and Sparse owners/views: bounded human-readable display, self-describing ASC text, versioned ASC binary, and Matrix Market interchange. Reading must have documented transaction, resource, type, and source-cursor semantics. Printing must show values, not only metadata.

These tracks share scalar, memory, failure, and packaging contracts but remain separately testable. Array I/O must not depend on LAPACK.

### 1.2 The approved implementation strategy is binding

Do not silently replace the approved strategy with any of these:

- A complete native rewrite required before any advanced operation is available.
- Mandatory LAPACK/Eigen/Fortran for every ASC user.
- An Eigen-only subset advertised as full LAPACK.
- A generic `void*` escape hatch or public raw Fortran calls standing in for checked ASC operations.
- Only LU, Cholesky, QR, and one SVD/eigen driver.
- Dense-only printing or a pretty-printer presented as a persistence format.

Maintain separate reporting for **specification mapping**, **callable ASC capability**, **verified reference-provider coverage**, and **verified native coverage**. Completeness of one does not imply completeness of another.

All P00–P11 packages are required. Optional improvements are explicitly marked; they cannot displace required work. A full-native rewrite, optimized-provider rollout, full GPU LAPACK, generic sparse direct/Krylov solvers, PDE algorithms, `.npy/.npz`, HDF5, MATLAB files, compression, and general CSV are not required by this program. Preserve existing CUDA behavior and test isolation; do not introduce new GPU claims.

### 1.3 Source-derived baseline versus new design

This runbook operationalizes `asc-cpp-lapack-and-array-io-plan.md`, approved in the conversation. The program scope and P00–P11 IDs come from that plan. Detailed command protocols, example manifest schemas, wire-format defaults, fixture designs, and review checklists below are **new implementation instructions**, not claims that the repository already contains those facilities.

Verified reference identities:

| Item | Identity / observation |
| --- | --- |
| Inspected `asc-cpp/main` | `46412183b2ae86101b2361c52376a8db8efff264` |
| Historical MdeCpp behavioral catalogue | `f6294e9079262682ce63ae7ff2d8a643e658bf5d` |
| Reference-LAPACK specification | `v3.12.1`, released 2025-01-08; still identified by Netlib's release page when checked 2026-09-07 |
| LAPACK annotated tag object | `5ebe92156143a341ab7b14bf76560d30093cfc54` |
| LAPACK peeled source commit | `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` |
| Existing ASCCMake pin in installation documentation | version `0.1.0`, commit `8a7dcbad3a97267cce59810aff24de800a3497a7` |
| Existing ASCCMake source archive hash | `67765391bef06c6c9a1a0c43e934d0db7a9c52876e66e0cf644042eeb0c2a5c9` |

Sources: [S01–S06]. A pinned source identity is not build or test evidence. No new ASC implementation or numerical test execution is supplied by this runbook.

**Observed metadata trap:** the inspected LAPACK 3.12.1 commit's top-level CMake file declares patch version `0`. Preserve the exact source commit as authority and record such discrepancies. Do not reject or silently relabel the pinned tree solely from that CMake string; do not assume `ILAVER` or a vendor's version string independently proves the source identity. [S05]

### 1.4 What “full” means

The denominator is the reviewed inventory generated from the pinned upstream source and documentation, not a hand-written list of popular routines and not only `lapacke.h`.

Include every actual public driver and computational operation, actual S/D/C/Z instances, specifically named mixed-precision entries, documented job choices, and required storage/auxiliary capabilities. Account explicitly for all other discovered source routines: expert-callable auxiliary, internal dependency, optional extra-precision, deprecated compatibility, test-only, timing/build support, or absent/inapplicable.

An auxiliary designation does not automatically remove a useful public expert operation. Every exclusion needs source evidence and a concrete reason. Optional upstream build dependencies do not make an existing required routine mathematically inapplicable. A missing LAPACKE binding is an adapter task, not an exclusion.

The endpoint is ASC semantic capability, not exporting the original Fortran ABI or copying all helper symbols into the public `asc` namespace. Every required operation must nevertheless have a real, typed, checked, documented, executable ASC path. No successful full-profile report may contain pending required rows.

---

## 2. Execution protocol, authority, and persistent state

### 2.1 Read before editing

Read the actual checkout's root and relevant nested `AGENTS.md` files, `CONTRIBUTING.md`, `CMakeLists.txt`, `CMakePresets.json`, installation and extension guidance, public API inventories, accepted architecture decisions, module contracts, provenance records, and affected tests. Identify which documents are historical proposals rather than current implementation evidence.

Preserve existing instructions. Do not overwrite `AGENTS.md` with this runbook, dilute safety checks, or add a blanket waiver to make a test pass. New decisions must document this program's narrowly scoped changes.

Use the live Google C++ and Python Style Guides, with retrieval date and identity recorded in the source ledger. The uploaded Google guide files are local references, not proof that their contents are the latest. If live access fails, record the failure and use the local snapshot without calling it current. Use the checked-in formatter/tidy configuration and preserve accepted repository exceptions. [S11–S12]

Examples of scope-sensitive style decisions: preserve the existing `std::filesystem`-based File API even though the Google guide disallows that header; do not break it as an unrelated cleanup. Preserve established public customization contracts rather than deleting concepts during complex-scalar work. Record these limited compatibility decisions.

### 2.2 Worktree and branch safety

Use a feature branch based on the current target integration base. Suggested branch: `feature/lapack-array-io`. If one already exists, inspect and resume it instead of replacing it. All branch/path names below are defaults, not permissions to delete collisions.

Inspect before changing anything:

```bash
git rev-parse --show-toplevel
git status --short
git branch --show-current
git log -5 --oneline
```

Fetch and inspect the current `origin/main` when permitted. Compare it with the inspected commit; incorporate newer work through an explicit baseline delta record. Do not reset the repository to the historical commit. Do not work directly on `main` or on the `release/0.9.0` maintenance branch. This is feature development, not the historical release-hardening task.

For unrelated dirty work, use a new worktree from the approved base or preserve it in place with a carefully separated patch. Do not stash, reset, clean, force-checkout, overwrite, or amend user work automatically. If an existing worktree is clearly the active program, resume its state.

Local changes, builds, tests, and reviewable local commits are the default execution scope. Push only feature branches and create/update draft PRs when the user has authorized remote writes and tools actually support them. Do not merge PRs, create release tags, publish packages, alter branch protection, grant permissions, change secrets, delete repositories, or modify unrelated ASC repositories.

### 2.3 Persistent program state

Create or reuse a small durable directory, proposed as:

```text
docs/development/lapack-array-io/
  README.md
  state.json
  decisions.md
  blockers.md
  baseline.md
  api-notes.md
  verification-summary.md
```

Keep the source runbook in `docs/development/lapack-array-io/runbook.md` if repository policy permits; otherwise keep a stable local copy and record its hash/path. These are development records, not extra installed public components. Preserve existing release-artifact exclusions.

Store raw logs, coverage measurements, generated test output, source archives, dependency builds, and installed prefixes **outside** the source tree. Suggested evidence root: a dedicated sibling directory named `asc-cpp-evidence/lapack-array-io`. Resolve its actual absolute path once and record it. Do not depend on a ChatGPT `/mnt/data` path in the user's environment.

`state.json` must include:

- Program ID, runbook version/hash, target repository, initial and current base commits.
- Active worktree/branch, last implementation commit, worktree-diff identity when dirty.
- Required P00–P11 states, subtask IDs, dependencies, active task, exact next action.
- Source/manifest hashes, provider identity, toolchain/configuration identity.
- Evidence index paths and applicable test outcomes.
- Unresolved blockers, decisions needing actual external approval, and independent work still possible.
- Implemented versus verified counts; keep unimplemented and untested distinct.

Use states `not_started`, `in_progress`, `implemented_unverified`, `verified`, `blocked`. A blocked task is not completed. Do not add elapsed-time estimates or unsupported progress percentages.

### 2.4 Per-session execution loop

1. Read repository instructions, program state, blockers, and current diff.
2. Reconcile the state with real files and commits; do not trust stale green labels.
3. Select the next dependency-satisfied task, favoring the first incomplete required milestone.
4. Read its exact contracts and current upstream routine documentation.
5. Implement the smallest complete, testable vertical slice with tests and useful documentation.
6. Run its positive, failure, numerical, and compatibility checks. Fix discovered failures.
7. Conduct a focused self-review; use an independent reviewer/subagent only when available, and identify what actually ran.
8. Update coverage and evidence from actual test results, not from intentions or comments.
9. Commit a coherent change when appropriate; update the restart point and continue.

Do not end a session merely because one package has passed. Continue while useful unblocked work and execution capacity remain. At an unavoidable session boundary, preserve an exact checkpoint and report the next command/task. Do not promise unattended future work.

Ordinary design/test gates are internal execution gates, not repeated requests for user confirmation. If a repository policy genuinely requires owner approval of a new public component or license route, record the pending decision and continue unaffected work; never falsely mark a human approval as obtained.

### 2.5 Parallel work, only when supported

If parallel agents/worktrees are available, use one integration owner and bounded task owners for: inventory/contracts, scalar/descriptor foundations, array printing/I/O, LAPACK bindings, numerical tests, and package/docs review. Freeze shared API contracts before parallel edits. Give each owner exact file ownership and input/output artifacts.

One owner merges manifest edits and shared-header changes. No two agents write the same worktree or rewrite shared schema concurrently. A subagent report is not a substitute for the integrator running the tests. Without parallel facilities, execute the same roles sequentially.

---

## 3. Architecture, naming, and compatibility

### 3.1 Preserve six modules

Keep `core`, `utilities`, `expression`, `dense`, `sparse`, and `random`. Preserve random-owned Dense/Sparse integration facets. No new Array, Linalg, Math, shared Backend, or general I/O module.

| Responsibility | Owning component |
| --- | --- |
| Storage-neutral byte sources/sinks, file handling, bounded scalar codecs | Core |
| Dense owners/views, LAPACK API, factorization algorithms, Dense print/file formats | Dense |
| Sparse formats/builders/views, Sparse print/file formats | Sparse |
| Storage-neutral expression customization | Expression |
| Configuration/command-line syntax, timers | Utilities |
| Existing engines, distributions, samplers and their reproducibility | Random |
| External LAPACK adapter and its external dependencies | Optional Dense-owned provider facet |

Preserve forbidden edges: Dense must not include/link Sparse, Utilities, or Random; Sparse must not include/link Dense, Utilities, or Random; Expression and base Random must not acquire storage dependencies. Mixed interchange is owned by the explicitly chosen destination or a caller composition; never create a sibling dependency to share a parser.

Shared scalar tokenization can be Core-owned if genuinely storage independent. Dense and Sparse each own array grammar interpretation. Minor duplication is preferable to a forbidden dependency or a premature type-erased array hierarchy.

### 3.2 Proposed public file organization

Adapt to actual repository conventions; proposed paths are:

```text
include/asc/dense/lapack.h
include/asc/dense/lapack/{types,workspace,report,factor_view}.h
include/asc/dense/lapack/{general,positive_definite,indefinite,structured}.h
include/asc/dense/lapack/{orthogonal,least_squares,svd,eigen,schur}.h
include/asc/dense/lapack/{generalized,matrix_equations,specialized}.h
include/asc/dense/providers/lapack.h
include/asc/dense/{print,io,matrix_market}.h
include/asc/sparse/{print,io,matrix_market}.h
```

Keep provider-only headers out of common umbrella headers. Keep lengthy implementations in `.cc` files or clearly marked owning-module internal headers as required by templates. Do not create one unreviewable header containing every routine.

All public declarations stay directly in `namespace asc`. Internal namespaces contain `internal`; do not introduce `asc::detail` or a new nested public `asc::lapack` hierarchy contrary to the established flat namespace.

Prefer stable expert names such as `Getrf`, `Potrf`, `Geqrf` using overloads with the actual scalar contracts. Explicit S/D/C/Z spelling is acceptable only when needed to disambiguate a real interface difference and documented in the mapping. Do not use macros to generate an opaque public API; a reviewed generator may emit ordinary declarations.

### 3.3 Component and provider model

Proposed new component: `dense_lapack`, imported target `ASC::dense_lapack`. It is a Dense-owned facet, not a seventh module. Record the addition in an ADR, package capabilities, dependency graph, target inventory, install/relocation tests, and documentation.

The base `ASC::dense` contains provider-neutral types/contracts and native implementations, without vendor headers, link dependencies, or dynamic discovery. The provider facet exposes an explicit provider/context object or overloads. Selection must not depend on link order or a mutable process-wide registration table.

Preserve `ASC::cpp` as the provider-free aggregate. Linking `ASC::cpp` must not silently select or require LAPACK. A provider-specific context is mandatory for provider-backed dispatch; a native call must not silently fall through to the external provider.

### 3.4 Public compatibility boundaries

Before changing a public declaration, identify affected source, ABI, numeric, serialized-format, and provider contracts. Adding support for complex storage must not invalidate existing real code or cause unrelated overload ambiguities.

Do not move existing BLAS functions to a new header/namespace or rename them again. LAPACK functions belong under Dense LAPACK, not misleadingly under BLAS. Avoid changes to existing enum numeric values, SONAME, release promises, or Random sequence definitions. Any necessary compatibility change is an explicit decision and migration note, not an incidental refactor.

Do not assign a new published release version or rewrite the old release's evidence to describe unverified features. Place change notes under the repository's unreleased/development convention and leave publication to the separate release process.

---

## 4. P00 — baseline, upstream inventory, and frozen contracts

**Depends on:** none.  
**Exit:** trustworthy baseline, immutable upstream source, complete classification, executable coverage validators, and specified array formats. No claim of implemented LAPACK coverage yet.

### 4.1 Establish the baseline

Record source commit, dirty diff hash, compiler/standard-library identity, CMake version, generator, architecture, build type, exception policy, enabled components, dependency identities, and actual available tools. Do not log tokens or dump the environment.

Read the current installation document and use its ASCCMake pin. The historical pin above is an expected value to compare, not a reason to downgrade a newer accepted dependency. A missing public dependency is a specific build blocker; do not invent private tokens or silently replace it with a stub package.

Use verified current presets, adapting only to the actual checked-in configuration. At the inspected baseline these commands exist:

```bash
cmake --list-presets
cmake --preset test-debug
cmake --build --preset test-debug --parallel 2
ctest --preset test-debug --no-tests=error --output-on-failure
cmake --preset test-release
cmake --build --preset test-release --parallel 2
ctest --preset test-release --no-tests=error --output-on-failure
```

Capture each command, exit status, and output outside the source tree. If a prerequisite prevents a build, record the failed configure/build and its reason. Do not call a skipped baseline green. Continue source/contract/I/O work that can be verified independently. Never conceal baseline failures by disabling tests.

Save a pre-change snapshot of the public header/target contracts and of relevant Random Dense test outputs. This is regression evidence, not permission to update expected values mechanically.

### 4.2 Obtain and pin Reference-LAPACK

Use the exact commit above. Retrieve it directly from the authoritative upstream, not through MdeCpp. Resolve the annotated tag to the commit and verify both identities. Record whether the tag is signed; an unsigned tag must not be described as cryptographically authenticated.

Use a fresh external source directory. Inspect an existing directory rather than overwriting it. A possible bootstrap sequence in a new directory is:

```bash
git init "$LAPACK_SOURCE"
git -C "$LAPACK_SOURCE" remote add origin \
  https://github.com/Reference-LAPACK/lapack.git
git -C "$LAPACK_SOURCE" fetch --depth=1 origin \
  6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca
git -C "$LAPACK_SOURCE" checkout --detach FETCH_HEAD
git -C "$LAPACK_SOURCE" rev-parse HEAD
```

`LAPACK_SOURCE` must already be set to the chosen new absolute directory. Confirm the resulting commit is exactly the pinned commit; abort dependency use on mismatch. If the server disallows a SHA fetch, fetch the version tag, peel it, and perform the same comparison. Do not fall back to the moving default branch.

Create a dependency lock containing upstream URL, source commit/tree, tag/tag-object identity, archive URL and hash when archives are used, local tree verification method, license and notice paths/hashes, build options, and patches. A checksum computed after a download records identity; it does not alone authenticate the first download. Do not invent hashes. Normal package consumers must not contact the network.

Inspect `SRC`, `DEPRECATED`, build lists, auxiliary dependencies, LAPACKE headers/sources, and optional XBLAS paths. Read the exact license before importing/distributing any upstream material. Keep original code, notices, provenance, and modifications distinguishable from ASC-authored code. MdeCpp implementation, tests, tables, and prose remain excluded by the existing provenance rule.

### 4.3 Generate the upstream inventory

Implement a deterministic repository tool, reusing existing Python tooling conventions. It accepts the verified local source root, pinned commit, and output path. It must not execute upstream arbitrary code or depend on a network service to classify files.

Use multiple independent discovery inputs:

1. Fortran procedure declarations, handling fixed/free form, continuation lines, and multiple procedures per file.
2. Pinned source/build file lists and conditional compilation/build options.
3. Documentation groups, routine argument contracts, and precision-specific exceptions.
4. LAPACKE declarations as a cross-check, not the sole denominator.
5. Built/exported symbols later in P04/P09 as another cross-check, not the sole definition of public semantics.

Do not assume every file stem is one routine or every routine has four precision variants. Generate an unresolved-classification report for disagreements. Retain absent documentation placeholders as evidence of absence, not callable entries. Avoid counting C wrappers, `_work` wrappers, and Fortran implementations as three distinct numerical capabilities.

Required inventory fields: stable ID, routine, source path/hash, precision/input/output types, family, public/internal classification, optional build conditions, deprecated status, declaration signature, documentation reference, and discovered interface routes.

Do not silently omit `DEPRECATED`, optional source lists, DMD, named blocked/two-stage variants, or callable expert auxiliary routines. Optional/timing/test-only classifications must be reviewed and justified.

### 4.4 Create the reviewed ASC mapping

Proposed contract artifacts:

```text
docs/contracts/lapack-upstream-inventory.json
docs/contracts/lapack-coverage.yaml
docs/contracts/array-io-coverage.yaml
docs/contracts/lapack-provider-lock.json
docs/contracts/array-text-v1.md
docs/contracts/array-binary-v1.md
docs/contracts/matrix-market-profile.md
```

Use existing equivalent paths if present. Do not duplicate competing truth sources.

For each required numerical routine record its ASC entry point, scalar signature, legal option rules, descriptor types, workspace requirements, mutation and alias contracts, integer/pivot encoding, numerical outcomes, native/provider routes, test IDs, documentation, and status. Reject rows claiming verified without executable test evidence tied to a configuration and implementation identity.

Build a validator that fails on missing inventory entries, duplicate IDs, absent required mappings, unresolved classification, illegal status transitions, malformed evidence links, contradictory capability claims, or required operations counted as inapplicable solely because implementation is missing.

### 4.5 Record program decisions

Create appropriately numbered ADRs according to repository policy for:

- Full capability versus native coverage and exact upstream denominator.
- Dense-owned LAPACK facet and explicit provider dispatch.
- Complex Dense storage, factor descriptors, workspace/result/pivot contracts.
- Transactional array I/O, formats, and printer resource limits.
- Provider ABI, callbacks, no-hidden-allocation guarantees, and supported configurations.

Do not label unapproved ADRs accepted if actual owner approval is required. The user approved the program direction, not arbitrary future licenses or architectural expansion. Use the defaults in this runbook to implement allowed work, and preserve genuine approval blockers explicitly.

### 4.6 P00 acceptance

The baseline report identifies actual successes and failures. The source lock contains verified identities. The inventory is reproducible and all discovered routines are classified. Full-profile requirements are frozen without a guessed routine count. Coverage validators have independent tests proving they reject missing rows and false evidence. Array formats have complete normative grammars/encodings rather than only illustrative examples. A resumable state file is present. Continue to P01; do not stop with these documents alone.

---

## 5. P01 — scalar, descriptor, workspace, ABI, and result foundations

**Depends on:** P00.  
**Exit:** tested reusable foundations; no vendor dependency in base Dense; existing real and Random behavior preserved.

### 5.1 Extend Dense owners to complex storage safely

At the inspected baseline, `DenseElement` excludes complex types while Sparse explicitly supports the two standard complex precisions. Verify the current source before changing it. Add support for `std::complex<float>` and `std::complex<double>` without accepting arbitrary nontrivial objects or conflating storage eligibility with algorithm eligibility. [S07]

Audit all owner lifecycle paths, not only the concept declaration:

- Required alignment, checked byte count, memory resource lifetime, ownership transfer, and destruction.
- `Create`, `CreateUninitialized`, view construction, const conversion, cloning, moving, and discard-resize.
- Starting element lifetimes in raw buffers under C++20; do not rely on C++23 `std::start_lifetime_as` or a C++23-only complex guarantee.
- Whether supported standard-library complex specializations satisfy the exact copy/destruction traits required by the current buffer implementation. Test them on each required compiler/standard-library combination.
- Typed value initialization to zero; do not assume zeroing raw bytes establishes all necessary object lifetimes.
- Explicit copies of complex storage without changes to real-array semantics.
- Host-only versus provider memory creation. Do not make an unsupported device construction work by touching device pointers on the host.

If current C++20 storage machinery cannot safely support complex objects on a required platform, implement the minimal valid lifecycle path rather than weakening the trait check or requiring C++23. Preserve the explicit memory model and document capabilities of uninitialized/device arrays separately.

Audit expression scalar categories and writable adapters. Complex sum and algebraic evaluation may be supported with explicit tests; ordered complex `ReduceMin`/`ReduceMax` must remain unavailable unless an independently named ordering policy is later approved. Do not accidentally instantiate `<`, `std::isfinite(complex)`, real-only random fills, or real-only numerical predicates on complex values. Keep conjugation and magnitude operations explicit.

Tests include complex owners of rank 0/1/2/3, zero extents, static/mixed/dynamic extents, layout-left/right, noncontiguous views, move/clone/resize, failure-injecting allocation, const correctness, and preservation of existing real tests. Add negative compile tests for bool, arbitrary classes, invalid operations, and unsupported scalar conversions.

### 5.2 Reuse descriptors and add only genuinely distinct storage contracts

Audit existing BLAS descriptors before introducing replacements. Common full-matrix and vector semantics should be reused or adapted without allocation. Never reinterpret a descriptor whose storage contract is different.

Required additional vocabulary includes:

| Storage | Required distinction |
| --- | --- |
| General full matrix | logical dimensions, leading dimension, row/column layout, backing span, memory placement |
| General band input versus LU factor band | factor storage includes fill-in rows; cannot use the ordinary BLAS band capacity |
| SPD/Hermitian band | selected triangle and diagonal-row convention differ from general-band LU |
| General tridiagonal | separate subdiagonal/diagonal/superdiagonal; LU factors can include a second superdiagonal |
| Hermitian positive-definite tridiagonal | real diagonal with possibly complex off-diagonal; do not force all buffers to one type |
| Bidiagonal | real diagonal/off-diagonal and orientation as specified by the routine |
| Packed symmetric/Hermitian/triangular | exact selected-triangle ordering and capacity |
| Rectangular-full-packed (RFP) | orientation, parity, triangle, and transposed/conjugated storage rules |
| Householder factors | packed matrix, tau/block reflector representation, dimensions, and originating family |
| Indefinite factors | factor layout, block/pivot encoding, triangle, and algorithm variant |

Every public descriptor validates checked dimensions, products/sums, leading dimensions, alignment, full reachable backing span, placement, uniqueness for writable mappings, and lifetime requirements. Document whether empty descriptors permit null pointers; provider adapters still supply valid dummy storage if the foreign interface requires it.

Preserve existing negative-increment BLAS behavior but do not invent negative strides for general Dense views. Arbitrarily strided LAPACK input uses an explicit checked pack/unpack path with caller storage. Padding and ignored triangles are not mathematical input.

### 5.3 Workspace query and execution contract

Design typed query results with separate minimum and preferred requirements for scalar, underlying-real, integer, logical, pivot-conversion, layout-conversion, provider-complex conversion, and bounded scratch buffers. Include alignment and total-byte limits. Distinguish entries from bytes.

Each plan/query is bound to routine, scalar signature, shape, options, provider identity/version/build, and integer ABI. A stale plan after any of these change must be rejected or revalidated before mutation. Caller-owned workspace must be large enough and disjoint from forbidden inputs/outputs and other simultaneously live workspace regions.

Do not impose a universal `lwork=-1` rule: some routines have fixed requirements, multiple query arrays, or option-dependent formulas. For query paths requiring actual data/addresses, document the needed read-only operands and guarantee the query does not overwrite user data. Supply valid scratch/query buffers, not pointers to unrelated or undersized objects.

Foreign floating-valued query results require finite, nonnegative, representable and correctly rounded-up capacity checks. Validate all arithmetic before converting to ASC byte counts or a provider integer. Test precision-boundary cases synthetically without attempting huge allocations.

Low-level execution does not allocate, pack, transfer, synchronize, change precision, or select another provider implicitly. Allocating convenience factories must accept an explicit resource and expose which allocations occur. Classify actual provider-internal allocation separately; never claim a whole-call guarantee merely because the ASC wrapper itself does not allocate.

**Dispatch design checkpoint:** Freeze one explicit Dense-owned dispatch design before generating hundreds of bindings. A suitable default is an owning/borrowed Dense LAPACK context carrying the existing execution context plus immutable typed operation tables and provider state. The base library implements checked front ends and native tables; the optional facet constructs the reference-provider context. Tables are injected explicitly, not populated by static registration or link-order discovery. A missing entry in a native-only context returns a truthful unsupported-capability status; a missing required entry in the final reference context fails full-profile closure. Keep scalar/mixed-signature calls typed and keep foreign handles private. An equivalent explicit provider-overload design is acceptable if frozen once, without duplicating inconsistent validators or leaving ordinary base consumers with unresolved optional symbols. Test context/provider lifetime and configuration identity as part of P01/P04.

### 5.4 Numerical reports that survive failure

Preserve existing `Status`/`Result` conventions. Prefer `Status` plus a mandatory small caller-supplied report for operations whose numerical diagnostics must survive a non-OK return. An equivalent structured return is acceptable only if non-success reports remain accessible and callers cannot mistake execution success for numerical success. Do not change Core `Result<T>` globally just for LAPACK.

Initialize reports deterministically before validation. Use `called_provider=false` and an optional native-info field when no native call occurred; do not fabricate `INFO=0`. Include routine/provider identity, outcome category, raw signed native `INFO`, translated diagnostic index only when meaningful, and output-validity state.

Numerical outcomes include success, accuracy warning, singularity, non-positive-definiteness, numerical rank decision, nonconvergence, and partial result. Rank deficiency can be a successful least-squares outcome; do not globally classify it as an error. Positive `INFO` has routine-specific meaning. Negative `INFO` from an already checked wrapper can indicate an adapter defect or provider mismatch and must be visible, including the native argument position. [S09]

Keep larger outputs in typed caller-owned buffers: rank, singular values, selected eigenvalue count, reciprocal condition estimates, equilibration, normwise/componentwise error bounds, support intervals, convergence information, and returned scaling. Flags identify fields actually computed. Do not manufacture diagnostics a driver does not provide.

Structural validation errors must leave numerical destinations unchanged. Numerical failure after a valid computation begins can leave only the documented partial output; distinguish these guarantees. Preserve outputs explicitly documented usable on warnings.

### 5.5 Pivots, permutations, logicals, and integer interfaces

Use checked ASC 64-bit dimensions/indices at the boundary. Never reinterpret 64-bit arrays as 32-bit foreign integers. Allocate conversion arrays through supplied workspace, range-check all inputs before execution, and validate native outputs before widening/publishing.

Preserve the exact raw pivot encoding in a typed descriptor tagged with the factorization family and variant. Low-level LAPACK pivots remain signed, convention-tagged, one-based values; this is a documented foreign-encoding payload, not ordinary ASC coordinate storage. Expose a separately named conversion to zero-based sequential swaps/final permutations where valid.

A swap sequence is not a final permutation. Negative paired pivot entries for 2-by-2 indefinite blocks are not ordinary negative indices. Rook/Aasen/block variants can have different encodings and extra arrays. Do not normalize all pivot arrays with `p-1`, `abs(p)-1`, or a generic permutation converter. [S10]

Validate incoming factor metadata, pivot lengths, family/triangle, and all index bounds before solving. Do not accept a Cholesky or unrelated QR buffer as LU simply because dimensions agree. If factors may be borrowed, document that mutation or expiration invalidates all consumers. A convenience factor object cannot be reused as successful after failed factorization.

Test LP64 and a supported true ILP64 route separately. A suffixed `_64` interface and a globally changed integer ABI are not interchangeable. Match provider headers, libraries, BLAS, logical widths, character calling conventions, and runtime libraries. Testing an ASC cast helper alone is not evidence of an ILP64 provider.

### 5.6 Complex and spectral contracts

Specify actual scalar signatures, including complex matrices with real singular/eigenvalue/diagonal work buffers. Preserve `transpose` versus `conjugate transpose`; complex symmetric is not Hermitian.

Low-level SVD follows the upstream right-factor convention, often `V^H`, not an undocumented V. Real nonsymmetric eigenvectors can use conjugate-pair encoding; provide an explicit checked expansion into complex output rather than pretending the raw real columns are independent real eigenvectors.

Generalized eigenproblems retain homogeneous `(alpha, beta)` outputs, including zero beta and indeterminate pairs. Never divide automatically. Schur/QZ APIs preserve their exact factorization conventions and ordering reports. Sylvester outputs retain the returned scale and describe the scaled equation.

Callbacks must be stateless/reentrant function pointers with documented finite-call lifetime or a carefully reviewed explicit-context bridge. No static global capture, mutable singleton, or thread-local hidden capture stack. Do not call a Fortran callback with a guessed C signature. Routines accepting native callbacks need a verified private interoperability route. A separate compute-then-reorder convenience path is not evidence that a named expert driver's exact interface is implemented.

### 5.7 P01 acceptance

Complete scalar lifecycle/compile tests; descriptor overflow/alias/shape tests; workspace query/staleness tests; report failure-access tests; raw pivot round-trip tests; and header/component isolation checks. Existing BLAS, Sparse and Random contracts remain intact. Document all new public declarations before expanding routine bindings.

---

## 6. P02 — Dense and Sparse printing

**Depends on:** P00 display contract and P01 scalar foundations.  
**Exit:** actual usable host-side value printing, independently of LAPACK or file serialization.

### 6.1 Required API behavior

Implement `PrintArray` overloads for supported Dense owners/const views and finalized Sparse COO/CSR/CSC owners/views. Use explicit `ByteSink`, options, and bounded scratch storage. Return `Status` and a small report containing values displayed, output bytes, and whether display was truncated.

Do not select stdout globally. Add a small explicit non-owning standard-output/file-handle sink adapter only if needed; it must not close borrowed handles and must propagate write failures. Core owns any storage-neutral handle adapter. Do not add a new File class.

An allocating `FormatArray` convenience is optional. If included, expose its resource and maximum result size; do not claim recoverable allocation failure around unchecked `std::string` growth in a no-exceptions build. `operator<<` is optional and must not be the only interface. Leave it out unless exception-mask and stream-state semantics are properly resolved.

### 6.2 Display semantics

Dense formatting:

- Rank 0: print the scalar with optional type/shape summary; logical scalar count remains one.
- Rank 1: a readable vector.
- Rank 2: conventional displayed rows and columns independent of physical layout.
- Higher rank: label each displayed two-dimensional slice with fixed coordinates; state which axes vary.
- Zero extents: identify the empty shape and print no element reads.
- Noncontiguous valid views: use logical indexing; never dump padding.
- Integers: numeric decimal values, including byte-sized integer types rather than glyphs.
- Real/complex values: explicit general/fixed/scientific precision, signed zero/nonfinite policy, and unambiguous complex display.

Sparse formatting shows kind, shape, stored count, and coordinate/value entries. A stored zero is shown as stored; an absent entry is not silently materialized. No dense expansion for a convenient matrix-looking display. Optional dense-looking output would need an explicitly chosen destination and limit and is outside the required printer.

Specify `max_elements`, `max_rows`, `max_columns`, `max_slices`, `max_output_bytes`, precision limits and edge-preview behavior. All are bounded by default. Choose deterministic defaults in P00; recommended preview defaults are 64 values, 8 rows, 8 columns, 4 slices, and 16 KiB output. They are ASC design choices, not external format standards.

A successful truncated preview must visibly indicate truncation. If the byte budget cannot accommodate even the header/marker, return a size/limit failure rather than claiming a complete display. Sinks can contain a prefix after failure; no stream rollback guarantee.

### 6.3 Algorithm and cost requirements

Use incremental encoding through a caller-provided fixed/bounded buffer and `WriteAll`. Do not construct a full intermediate string or scan an entire large tensor to determine column widths for a small preview. Width measurement is restricted to displayed elements.

For Dense previews, work should scale with rank metadata plus selected/displayed values, not total logical array size. For compressed Sparse previews, finding stored coordinates can require outer-offset traversal or binary search; document this honestly. Do not scan all rows to print a few selected entries when an offset search suffices. Do not promise O(displayed entries) for all compressed formats without evidence.

Reject device/managed or otherwise unsupported value placement before reading any values; optional metadata-only display is separately named and does not count as value printing. No implicit copy, evaluation of arbitrary lazy expressions, provider selection, or synchronization.

### 6.4 Tests

Independent expected strings for tiny rank-0/1/2/3 arrays; row/column layout equivalence; complex/integer/NaN/infinity/signed-zero formatting; empty views; strided views with poisoned padding; huge-shape small previews; sparse stored zeros; all truncation boundaries; invalid options; token exceeding scratch capacity; short writes; zero-progress sink; failure after every output chunk; disallowed placement; caller-resource/allocation instrumentation.

Validate output length and number of element reads. A test using a billion-sized logical shape with only a tiny valid preview must not dereference outside the actual valid backing span; use legal descriptors or an instrumented adapter, never fake storage capacity.

Add a compiled installed-package example using an explicit sink. It must print values and handle status errors without throwing.

---

## 7. P03 — native text and binary array I/O

**Depends on:** P00 format contracts and P01.  
**Exit:** Dense and Sparse load/save with complete data, reliable rollback, bounded parsing, and portable encodings. P02 display is not a substitute.

### 7.1 API operations and allocation boundaries

Implement distinct operations for text versus binary, Dense versus Sparse ownership, and new-owner loading versus staged loading into existing storage. Suggested names include `ReadDenseArray`, `WriteDenseArray`, `ReadSparseArray`, `WriteSparseArray`, and explicit binary counterparts. Freeze final names consistently; do not create ambiguous overloads that guess a format from a filename.

A metadata-reading facility must support inspecting kind/scalar/rank/shape without creating a new dynamic-rank array hierarchy. A prepared reader retains its input position or documents replay needs; do not consume the header and then call a full reader expecting it again. Compile-time rank and static extents are checked against the file before allocating values.

New-owner loading accepts a caller memory resource and input limits. Read and validate into temporary storage owned by the candidate result, then publish the owner only after the entire frame and required checks pass.

`ReadInto` for an existing Dense view requires explicit disjoint staging and parse scratch. Validate target mapping/placement and all capacities before the final commit. During commit, use a prevalidated nonfailing host-copy/scatter path; repeated fallible `At()` calls after some writes would not establish the promised transaction.

Sparse structural input produces a new owner/builder. Existing finalized offsets/indices remain immutable. Values-only loading stages values and verifies **exact** kind/shape/coordinate pattern before committing. A checksum match alone is not proof of identical structure. Prevalidate every destination write.

An explicitly named partial/destructive streaming reader is optional, not the default. Report exactly which elements/bytes were consumed/written on failure. Source rollback is never promised for an unseekable stream.

### 7.2 Normative text-format default

The following is an original **proposed v1 wire specification** to freeze in P00. It is not an existing ASC file format. If an already accepted format is discovered, reconcile through an explicit decision before committing a second format. Once v1 fixtures exist, do not change bytes silently.

Use ASCII keywords and locale-independent numeric tokens. Writer emits LF; reader accepts LF/CRLF as documented. Canonical keywords are case-sensitive. No comments, ellipses, locale separators, implicit type coercion, or arbitrary executable expressions.

Dense example representing conventional rows `[1, 2, 3]` and `[4, 5, 6]`:

```text
ASCARRAY 1
kind dense
scalar f64
rank 2
shape 2 3
order dim0
count 6
data
1
4
2
5
3
6
end
```

Header fields occur exactly once and in the declared order. Dense values are dimension-zero-fastest. `count` must equal the checked product of extents; rank-zero has one value, any zero extent has zero values. `shape` has exactly `rank` extents, including no extent tokens for rank zero. Extra header keys are an error in v1 rather than silently ignored.

Scalar codes: `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `f32`, `f64`, `c64`, `c128`; complex suffix is **total bits**. Bool and implementation-specific extended precision are not v1 scalars. Document how accepted native arithmetic aliases map to exact wire widths; plain character values are numeric, never text glyphs.

Integer tokens are decimal with checked range; reject negative unsigned input and out-of-range magnitude before arithmetic overflows. Canonical real output uses sufficient significant digits for finite round-trip. Accept finite decimal/exponent tokens and the explicitly spelled `inf`, `-inf`, `nan`; no NaN payload promise in text. Preserve signed zero. Complex values use `(real,imag)` with both component tokens governed by the underlying real format. Specify whitespace placement and token-length limits in the grammar.

Sparse v1 uses `kind coo`, `csr`, or `csc`, `count` for stored entries, and an explicit native structure order. Freeze the canonical coordinate ordering against the actual current Sparse contract, with a written comparator; do not write merely “sorted.” Native archives preserve stored zeros and do not contain duplicate finalized coordinates.

Proposed payload sections:

- COO: `coordinates`, exactly `count` tuples of `rank` zero-based indices; `values`, exactly `count` scalar values; `end`.
- CSR: `offsets`, exactly `rows+1` offsets; `indices`, exactly `count` column indices; `values`, exactly `count` values; `end`.
- CSC: corresponding `columns+1` offsets and row indices.

Use `order coo`, `order csr`, or `order csc` in the header and specify each ordering fully in the normative document. For rank-zero COO, encode each empty coordinate tuple explicitly as `()` and permit only the stored counts supported by the existing Sparse rank-zero contract. Do not create a new Sparse rank-zero semantic just for serialization.

Offsets are zero-based, begin at zero, end at stored count, and are nondecreasing. Inner indices are in range and strictly ordered within each compressed segment. Structure mismatch, duplicate coordinates, wrong count, and noncanonical native input are errors. Matrix Market's duplicate-summing policy is separate and must not silently repair a native archive.

Default readers require exact scalar identity. Named checked conversion may be added, but never silently narrow, drop an imaginary part, alter integer precision, or round a large integer through double. Native readers preserve the declared storage kind; cross-kind conversion is a separately requested operation.

Single-object path reads consume the full file and reject trailing non-whitespace. A framed source reader consumes exactly through `end` and its defined line ending; buffered over-read belongs to the same reader object, not discarded bytes from the next frame.

### 7.3 Normative binary-format default

Use a portable v1 envelope and encode each scalar component explicitly, not a C++ struct or raw array span. The following concrete layout is a new ASC design default; finalize it through the P00 contract and independent fixtures.

| Offset | Width | Field |
| --- | --- | --- |
| 0 | 8 bytes | ASCII magic `ASCARRB` followed by LF |
| 8 | u16 | major version = 1 |
| 10 | u16 | minor version = 0 |
| 12 | u8 | kind: 1 dense, 2 COO, 3 CSR, 4 CSC |
| 13 | u8 | scalar code from the mapping below |
| 14 | u16 | flags, must be zero for v1 |
| 16 | u32 | rank |
| 20 | u32 | reserved, must be zero |
| 24 | u64 | count: logical values for Dense, stored values for Sparse |
| 32 | u64 | number of structure integers in payload |
| 40 | u64 | total payload byte length |
| 48 | u64 | total header length, exactly `56 + 8 * rank` |
| 56 | `8 * rank` bytes | u64 extents |
| header end | payload length | structure integers followed by scalar values |
| payload end | u32 | checksum of all preceding frame bytes |

All multibyte values use little-endian encoding. Scalar codes 1–12 map in order to `i8,u8,i16,u16,i32,u32,i64,u64,f32,f64,c64,c128`. Signed integers use specified fixed-width two's-complement wire encoding. Floats use IEEE binary32/binary64 bits; complex numbers are real component followed by imaginary component. Check source scalar compatibility rather than assuming every host arithmetic type fits the wire model.

Dense structure count is zero and values use dimension-zero-fastest order. COO structure count is `count * rank`, followed by coordinate tuples and then values. CSR count is `(rows + 1) + count`, containing offsets then inner indices; CSC uses `(columns + 1) + count`. Structure integers use u64 on wire but must fit ASC's signed index/extent domain before allocation or access. Rank and kind constraints are validated. The payload length must exactly equal checked structure bytes plus checked scalar bytes.

Define the checksum explicitly as the reflected CRC-32 recurrence with polynomial `0xedb88320`, initial state `0xffffffff`, and final XOR `0xffffffff`; serialize the resulting u32 little-endian. The recurrence updates all header and payload bytes in stream order and excludes the checksum field. Test the independent check value `0xcbf43926` for bytes `123456789`. This is accidental-corruption detection, **not** authentication or tamper protection. Do not rely on a checksum to make an unsafe size or index trustworthy.

Reusing Core endian primitives is required where their semantics fit. Preserve exact finite/signed-zero and quiet-NaN component bits for supported IEEE paths; use bit-preserving copies without arithmetic. Audit complex component access and object initialization. Do not promise signaling-NaN handling without actual toolchain evidence; explicitly document that boundary rather than silently claiming universal bit identity.

The binary format preserves logical values and declared Sparse structure, not original padding, capacity, physical strides, device placement, resource identity, or view relationships. A Dense reader chooses a valid destination layout explicitly. Do not serialize a provider factor object as an ordinary matrix while implying pivots and interpretation survive; factor-state persistence is outside this format unless separately specified.

### 7.4 Parser implementation and limits

Use bounded, incremental tokenization over `ByteSource`/`ReadSome` and byte encoding over `ByteSink`/`WriteAll`. Avoid unbounded `getline`, whole-file concatenation, locale-sensitive parsing, regex backtracking, or stream extraction with uncontrolled allocation. Reuse a tested Core token codec where possible; keep array schemas owned by Dense/Sparse.

Check floating `from_chars`/`to_chars` support on the minimum compiler/standard library. A missing implementation requires a reviewed bounded locale-independent fallback or an explicit toolchain blocker, not silently substituting locale-sensitive `strtod`. Do not add a mandatory parsing library without a scoped dependency/provenance decision.

Limits are explicit and validated: input bytes, header bytes, token length, rank, each extent, extent product, stored count, structure bytes, decoded bytes, staging bytes, total allocations and parser scratch. Check before allocation and before multiplication. Memory resources must be respected for temporary owners/builders as well as final arrays. Resource exhaustion is a recoverable status only when the actual allocation path supports it.

Metadata parsing for runtime rank uses a bounded supplied metadata buffer or explicit resource allocation within the budget. Never allocate a rank-sized vector from an unchecked 32-bit header value. Do not allow `rank=0` or a zero extent to bypass checks on enormous malicious remaining metadata.

Short reads are normal; zero bytes means EOF under the existing synchronous source contract. Short writes must be retried through the existing helper; a zero-progress sink must not cause an infinite loop. Track input offset/section and bounded contextual diagnostics without dumping the entire input.

### 7.5 File-path convenience and overwrite behavior

Compose existing Core `File` with the stream APIs. Inspect `OpenWrite`'s actual truncation semantics before using it in a convenience wrapper. The wrapper must require explicit overwrite intent; a check-then-open pattern is not an atomic no-clobber guarantee.

If supporting “fail if exists,” implement/test an actual exclusive-create OS operation in Core, not a racy `exists()` check. If supporting atomic replace, implement a separate same-directory temporary-write/checked-close/rename path with cleanup and platform tests. Both are optional unless made part of the frozen path-API contract. A required minimal safe writer can require an already-open caller sink and an explicitly destructive overwrite wrapper.

Propagate write, flush, and close failures. A destructor cannot report a close error, so successful save requires an explicit checked close where applicable. Preserve the primary error and include cleanup diagnostics without converting failure into success. Flush/close success is not a crash-durability guarantee. No generic sink can promise atomic replacement.

### 7.6 P03 tests and examples

Round-trip Dense and Sparse over every supported wire scalar, rank/storage combination, layouts, noncontiguous views, empty shapes, stored zeros, and extreme representable numeric values. Validate an independent encoder/decoder or hand-derived byte fixtures, not only two sides of the same implementation.

For tiny fixtures, truncate input at every byte boundary; corrupt each header field; use wrong magic/version/flags/rank/count/scalar/payload/checksum; insert excess tokens/trailing frames; test overflow, overlong tokens, invalid compressed offsets, duplicate/out-of-range coordinates, and numerical narrowing attempts.

Use fail-after-N source/sink/resource adapters. For every failed default `ReadInto`, compare all destination values and padding with the original byte image. For owner loading, verify all temporary allocations are released and no partial owner is returned. Test two adjacent frames through the same buffered reader, including a failed first frame; never assume a failed stream can recover without a defined resynchronization protocol.

Require installed examples for text round-trip, binary Dense/CSR round-trip, and a staged read rejection preserving its destination. Add bounded parser fuzz targets and retain minimized independent regression cases. Do not mark P03 verified with a single real 2-by-2 round-trip.

---

## 8. P04 — Reference-LAPACK adapter and native LU

**Depends on:** P01; the end-to-end I/O example also requires P02/P03.  
**Exit:** a working explicitly selected CPU provider, a tested native LU path, and a compiled installed consumer. This is not yet full LAPACK completion.

### 8.1 Bring up the pinned provider externally

The first provider spike may use the ordinary 32-bit-integer, reference-BLAS, LAPACKE-enabled build. These are verified upstream option names, not ASC options. Resolve compiler/generator choices before running:

```bash
cmake -S "$LAPACK_SOURCE" -B "$LAPACK_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$LAPACK_PREFIX" \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DLAPACKE=ON \
  -DUSE_OPTIMIZED_BLAS=OFF \
  -DUSE_OPTIMIZED_LAPACK=OFF \
  -DBUILD_INDEX64=OFF \
  -DBUILD_INDEX64_EXT_API=OFF \
  -DUSE_XBLAS=OFF
cmake --build "$LAPACK_BUILD" --config Release --parallel 2
ctest --test-dir "$LAPACK_BUILD" -C Release \
  --no-tests=error --output-on-failure
cmake --install "$LAPACK_BUILD" --config Release
```

Set all three directories to separate external locations. Inspect configure output/cache for ignored options or unintended external BLAS/LAPACK selection. This spike explicitly disables XBLAS and is **not** evidence for optional extra-precision rows or the final full profile. Enable and pin all required optional dependencies during P09. [S05]

For ILP64, use a separate build/install prefix and the actual supported upstream index64 mechanism; verify every linked dependency matches. Do not mix headers/libraries from the index32 and index64 prefixes. Record whether the configuration uses a separate ILP64 library or suffixed extended entry points.

Building an optional provider from source may require C/Fortran toolchains. Base ASC and a C++ consumer of a prebuilt provider must not suddenly require a Fortran compiler. Missing compilers are dependency-build blockers, not permission to alter base language requirements or call a prebuilt unverified binary complete.

### 8.2 Implement the optional ASC facet

Add the proposed `ASC::dense_lapack` export using the existing component mechanisms. Base modules and tests must still configure with provider discovery disabled. Add a positive provider consumer, a negative unavailable-provider consumer, and a provider-free consumer while LAPACK is deliberately undiscoverable.

Proposed ASC configuration names, **to create and document before use**:

- `ASC_CPP_ENABLE_LAPACK=OFF` by default.
- `ASC_CPP_LAPACK_ROOT` for an explicitly prepared provider prefix.
- `ASC_CPP_LAPACK_INTEGER_BITS=32` or `64`.
- `ASC_CPP_LAPACK_REQUIRE_FULL_PROFILE=OFF` for incremental development and `ON` for the full-profile gate.

Reuse equivalent existing options instead of creating duplicates. A development subset configuration must expose an explicit incomplete capability report. Required final testing sets the full-profile requirement ON. The full requirement must fail on missing required symbols or invalid configuration; it must not remove the rows from its denominator.

Use CMake package targets where reliable, and implement missing dependency discovery locally to the facet. `FindLAPACK` alone does not prove LAPACKE headers, every routine, the correct ABI, or exact source identity. Match minimum CMake 3.25 features; do not unconditionally use newer `BLA_THREAD` behavior. [S13]

Do not introduce process-global provider registration, mutable algorithm selection, or global thread-count changes inside an operation. A provider/context owns its state; concurrent calls and required external serialization are explicit. No fixed global log/error stream.

### 8.3 Build a real interoperability boundary

Prefer audited **column-major** LAPACKE middle-level work interfaces where they implement the required operation without hidden adapter allocation. Never route a row-major user matrix through an allocating LAPACKE transpose branch while advertising explicit workspace. Pack row-major/strided input into caller-provided storage when needed and unpack only defined output regions. [S09]

For routines missing from LAPACKE, implement a private checked shim using a verified interoperability method, preferably provider-built Fortran `bind(C)` wrappers when applicable. Do not guess symbol mangling, hidden character-length arguments, Fortran complex function returns, logical representations, or callbacks. Keep all foreign types private.

Complex data may use a verified compatible route or explicit conversion into supplied provider-format buffers. Matching `sizeof` alone is insufficient. Audit alignment, object aliasing/lifetime, real/imag order, and compiler interoperability. Do not reinterpret native provider structs as C++ complex objects merely because tests happen to pass on one compiler.

Fortran logical arrays are not `std::vector<bool>` or arbitrary C++ bool storage. Convert through correctly typed workspace. Keep code for character arguments and integer/logical widths confined to the provider boundary.

Perform preflight validation before any native call. Confirm the provider error path cannot print to global streams or terminate the application for an ordinary rejected ASC input. Do not install a process-wide `XERBLA` override. If a private provider build needs scoped support-hook adaptation, document the patch/provenance and verify its behavior. Negative-call tests must prove the ASC layer returns before calling the provider when validation fails.

Audit actual provider/internal allocation and hidden packing. A strict no-allocation route must pass instrumentation for both ASC and the provider path. If a required upstream routine allocates internally, implement a conforming audited route or record an explicit limitation and ADR; never silently relax the global contract or claim full strict-profile completion. Any permitted resource-aware non-strict path is separately named and reported.

Capture every wrapper's raw `INFO`; map negative argument positions correctly because LAPACKE has an extra layout argument. Detect integer conversion failures before call, output sentinel/width corruption after call, and unexpected native failures without losing diagnostics.

### 8.4 Native LU, not copied from MdeCpp

Implement independent provider-free partial-pivoting LU for actual supported LAPACK numerical scalars, including complex. Keep a stable packed factor representation and a documented pivot convention. Derive it from the mathematical specification and approved primary descriptions, not MdeCpp source/test fixtures.

Required native functionality: rectangular factorization, square reusable solve, multiple RHS, transpose/conjugate-transpose solve, and documented singular behavior. The expert API must map to the intended upstream semantics. Do not substitute a small absolute tolerance for exact-zero-pivot semantics unless it is a separately named ASC policy.

Handle every pivot, including the last; handle empty shapes before accessing a last element. Pivot row swaps must update previously computed multipliers consistently. Test a nontrivial swap sequence, not only an identity pivot vector. Complex pivot selection must follow the documented reference convention when exact expert semantics require it; a different acceptable pivot policy needs explicit naming rather than an undocumented substitution.

A solve does not refactorize. Native execution uses caller storage and existing BLAS primitives where their numerical/memory contracts fit. Do not implement solve as `inverse(A) * B`. Do not hide an intermediate vector allocation.

Keep raw factorization results and higher-level successful-factor objects distinct. Do not construct a usable convenience solve object from a singular factorization; numerical reports and documented raw partial factors remain available.

### 8.5 LU-family provider coverage

Complete the actual `getrf`, `getrf2`, `getf2`, `getrs`, `getri`, `gesv`, `gesvx`, `gecon`, `gerfs`, `geequ`, and `geequb` instances present in the inventory, plus the family's required helpers. Extra-precision variants are tracked for P09 rather than silently omitted.

Do not merely wire one double routine and mark S/D/C/Z complete. Check each distinct input/output signature, legal transpose mode, factor reuse, multiple RHS, storage layout, workspace query, and non-success output validity. Inverse routines are supported because they are in scope, but examples favor solves for solving systems.

### 8.6 P04 vertical slice and tests

Produce an installed-package example that reads A/B, preserves originals, factors once, solves two RHS, computes a scaled residual, prints X and diagnostics, writes X, reads it back, and verifies values. Run native and external provider routes explicitly. Handle status/report failures at each step.

Also test provider unavailable, wrong integer width, missing symbol, partial prefix, stale workspace plan, forbidden alias, undersized backing span, device placement, singular/empty/scalar/tall/wide matrices, extreme scaling, and failed native status mapping. Use a small fake provider only for adapter failure injection; it does not count toward numerical coverage.

Freeze component/ABI tests and the provider capability report before expanding families. Continue to P05–P09 even after the first successful demo.

---

## 9. P05 — structured direct systems and Cholesky

**Depends on:** P04.  
**Exit:** complete inventoried structured-system paths with diagnostics, storage contracts, and reusable factors.

### 9.1 Native Cholesky and reusable solves

Implement independent lower/upper Cholesky and solves for real SPD and complex Hermitian positive-definite matrices. Document which triangle is referenced and how the diagonal is interpreted. Do not read an unused triangle to perform a blanket symmetry scan. A named full-matrix validation convenience can be separate from LAPACK-compatible selected-triangle semantics.

Preserve the existing Random Dense preparation path exactly. Prefer leaving it unchanged initially. Share a new kernel only if its operation order, finite/symmetry checks, failure mutation, and sampling outputs are proven unchanged under the existing numerical contract. Do not add a Random dependency to Dense or rewrite Random expected vectors.

Native Cholesky tests include upper/lower factors, reuse/multiple RHS, complex conjugation, tiny/huge scaling, non-positive-definite input, zero/scalar sizes, ignored triangle sentinels, alias/workspace failures, and all relevant Random regressions.

### 9.2 Required provider families

Expand the exact inventory across:

- Positive-definite full, packed, banded, RFP and tridiagonal factorizations/solves/inverses/drivers, condition estimation, equilibration and refinement.
- Pivoted positive-semidefinite Cholesky (`pstrf` and actual computational counterparts), numerical rank and pivot reporting.
- Real/complex symmetric and complex Hermitian indefinite systems, including 1-by-1/2-by-2 blocks, packed forms, standard/rook/Aasen/two-stage variants present upstream.
- General banded and tridiagonal LU, fill-in storage, factor reuse, expert-driver diagnostics.
- Triangular full/packed/banded solve, inverse, condition and refinement operations.
- RFP conversions and associated operations, without masquerading as ordinary packed storage.

Representative stems are `potrf`, `potrf2`, `potf2`, `potrs`, `potri`, `posv`, `posvx`, `pocon`, `porfs`, `poequ`, `poequb`; actual `pp*`, `pb*`, `pt*`, `pf*`; `sytrf`, `hetrf`, `sytrs`, `hetrs`, `sysv`, `hesv` and their actual variants; `gbtrf`, `gbtrs`, `gbsv`, `gbsvx`, `gttrf`, `gttrs`, `gtsv`, `gtsvx`; `trtrs`, `trtri`, `trcon`, `trrfs`, and actual packed/band counterparts. Stems are navigation aids, not generated proof of applicability. [S08]

### 9.3 Numerical/storage traps to prevent

Never replace indefinite factorization with unpivoted LDLT and claim the same routine. Preserve the variant-specific pivot/block encoding and all extra factor arrays. Complex symmetric routines use transpose, while Hermitian routines use conjugate transpose.

Band LU needs factor fill-in capacity and changed diagonal positioning. Preserve extra superdiagonal factors of general tridiagonal LU. Hermitian tridiagonal diagonals can be real even with complex off-diagonals. Unit triangular diagonals are not read. Avoid reading padding, the unstored triangle, or overwritten input regions that a driver does not require.

Do not densify a structured matrix implicitly. Explicit conversions are allowed only through a named destination/workspace path, and do not establish that the required named structured algorithm is implemented. Respect any upstream-defined internal algorithm choices within a driver and report them where exposed.

### 9.4 Family acceptance

Reconstruct every factor family using its actual documented equation. Check solve residuals, inverse products where applicable, condition estimates against analytic diagonal examples, and refinement accuracy/report semantics. Test scaling, pivot-block reconstruction, singularity in last pivots, rank-deficient semidefinite input, wrong triangle, RFP parity/orientation, and retained outputs on warnings.

Every family must have a zero-size, scalar, layout, precision, workspace and malformed-descriptor path. Confirm no hidden conversion or allocation. Update installed consumers and coverage rows only after these checks pass.

---

## 10. P06 — orthogonal factors and least squares

**Depends on:** P04 and relevant P01 descriptors.  
**Exit:** complete orthogonal/unitary and least-squares operation families, plus native basic Householder QR.

### 10.1 Native Householder QR

Implement packed Householder factors with explicit tau and supplied scratch. Use numerically safe norm/scaling formulas, including the zero-vector and complex phase cases. Avoid naive squaring that overflows for representable inputs. Forming full/economy Q and applying Q or its transpose/conjugate transpose are explicit operations, not automatic allocations.

Support real and complex required native scalar cases, tall/square/empty factorization, and the rectangular cases claimed by the expert QR API. A native full-rank tall least-squares composition is appropriate; do not extend that claim to rank-deficient or wide minimum-norm problems without the corresponding method. Provider routes must cover the complete least-squares profile regardless of native scope.

### 10.2 Full provider scope

Complete actual QR/LQ/QL/RQ/RZ and generalized QR/RQ families; pivoted QR; compact/blocked reflector forms; positive-diagonal QR where supplied; tall-skinny/short-wide algorithms; Q generation/application from left/right with each actual operation choice.

Include `geqrf`, `geqr2`, `geqp3`, `geqrfp`, `gelqf`, `geqlf`, `gerqf`, `tzrzf`, `ggqrf`, `ggrqf`, actual `org*`/`ung*` and `orm*`/`unm*` paths, and inventoried `geqr`, `gelq`, `gemqr`, `gemlq`, `geqrt`, `gelqt`, `tpqrt`, `tplqt`, `gemqrt`, `tp*` or other block/tall-skinny variants. Do not invent absent type counterparts.

For least squares, implement actual `gels`, `gelst`, `getsls`, `gelsy`, `gelss`, `gelsd`, `gglse`, and `ggglm` routes. Preserve rank thresholds, minimum-norm semantics, mixed input/output work types, pivot-column inputs, residual storage, constrained assumptions and reports. [S08]

### 10.3 Important contracts

A and B storage for a least-squares driver may require capacity beyond the input equation dimensions, including `max(m,n)` rows. Validate the entire documented overwritten/output capacity before calling the provider. Keep the original right-hand side separately when residual verification needs it.

Do not solve rank-deficient problems by normal equations or claim that a successful unpivoted QR provides a rank certificate. Do not silently substitute one algorithm-specific driver for another. A convenience “least squares” policy may select a documented method, but exact expert names remain exact.

Reflector application must distinguish full versus reduced factors and rectangular Q dimensions. Preserve fixed-column pivoting semantics, valid job combinations, legal side/transpose choices, and ignored-output contracts. Invalid option combinations must fail before mutation.

### 10.4 Acceptance

Check reconstruction, orthogonality/unitarity, and Q application without forming Q. Use finite-rank fixtures with known nullspaces to verify minimum norm. Test tall/wide/full-rank/rank-deficient cases, multiple RHS, rank thresholds bracketing known singular values, and constrained-system residuals with documented rank assumptions.

Compare least-squares residual optimality (`A^H r` near zero) in addition to residual size. A nonzero residual can be the correct solution. Test that the minimum-norm solution is orthogonal to the known nullspace. Keep condition-sensitive tolerances and avoid asserting a unique factor basis when sign/phase freedom exists.

---

## 11. P07 — singular values, GSVD, and cosine-sine decomposition

**Depends on:** P06 and relevant provider support.  
**Exit:** all inventoried singular-value and related decomposition variants, not only one SVD driver.

### 11.1 Required implementation

Cover actual `gesvd`, `gesdd`, `gesvdx`, `gesvdq`, `gejsv`, `gesvj`, bidiagonal reductions and subproblem routines (`gebrd`, `bdsqr`, `bdsdc`, `bdsvdx` where present), and required reflector application. Expand real/complex applicability from the source.

Support values-only, full/economy factors, documented overwrite modes, selection by index/value, rectangular matrices, and all valid job combinations. Respect distinct real/complex workspace and integer/logical requirements. Return the documented right factor and explicit dimensions.

Include GSVD `ggsvd3`, `ggsvp3`, `tgsja`, and actual legacy compatibility entries; include `orcsd`, `uncsd` and their actual variants. Preserve separate matrix dimensions, rank partitions, generalized singular-value pairs, block conventions, angle outputs, and returned factors. Do not reduce GSVD to an ordinary SVD of a stacked matrix while claiming upstream equivalence. [S08]

### 11.2 Verification

For SVD, test `A ≈ U diag(s) V^H`, nonnegative ordered singular values under the driver's documented order, orthogonality/unitarity, and selected-value bounds. Preserve documented partial outputs on nonconvergence. Use square/tall/wide, all-zero, exact-low-rank, repeated/clustered singular values, and strongly scaled examples.

Compare subspaces rather than elementwise singular vectors when values repeat or cluster. Phase/sign normalization belongs only to a documented convenience representation, not silent changes to expert outputs.

For GSVD/CSD, verify the **exact pinned routine's** factor equations, rank/block relationships, orthogonality, and angle/sine/cosine identities. Build tiny independent examples with known orthogonal/unitary rotations. Do not assume all variants return identical factor shapes or normalization.

Test every option-dependent output size, overwrite alias rule, minimum workspace and selected-spectrum endpoint. An SVD callback or stub returning zero singular values is not an implementation even if a reconstruction test uses an all-zero matrix.

---

## 12. P08 — eigenproblems, Schur/QZ, and matrix equations

**Depends on:** P04 and the necessary structured/orthogonal foundations.  
**Exit:** complete standard/generalized spectral families with correct output interpretations and scaling.

### 12.1 Symmetric/Hermitian eigenproblems

Implement full/selected spectrum, ordinary/generalized definite problems, full/band/packed/tridiagonal storage, and actual one-stage/two-stage variants. Include driver, reduction, reconstruction/application, and necessary expert computational routines. Representative anchors are actual `syev*`, `heev*`, `sygv*`, `hegv*`, `sbev*`, `hbev*`, `spev*`, `hpev*`, `sbgv*`, `hbgv*`, `spgv*`, `hpgv*`, and `ste*` families. [S08]

Respect selection interval conventions and index base conversions, eigenvector support indices, absolute tolerances, generalized problem type, and definite B assumptions. Some returned eigenvectors are normalized in a B-related metric, not Euclidean norm; verify the documented equation and normalization for each problem type.

Do not infer that every advertised job value is implemented by an upstream two-stage driver. Preserve documented availability and record unsupported upstream modes as such with source evidence, not as new ASC omissions or fabricated support.

### 12.2 Nonsymmetric standard eigen/Schur

Cover actual `geev`, `geevx`, `gees`, `geesx`, `gebal`, `gebak`, `gehrd`, `hseqr`, `hsein`, `trevc*`, `trexc`, `trsen`, `trsna`, and relevant newer/auxiliary entries. Preserve left/right eigenvectors, balancing, condition estimates, real quasi-triangular blocks, Schur vectors, ordering and convergence diagnostics.

Verify real conjugate-pair encodings before optional complex expansion. Distinguish a real Schur 2-by-2 block from two unrelated diagonal eigenvalues. Do not order eigenvalues by an undocumented rule or compare unstable orderings across providers.

Implement validated selection callbacks with the P01/P04 ABI constraints. Test complex-pair selection and driver sorting/roundoff failure outcomes. No hidden global capture state. Independent concurrent callbacks must not exchange state.

### 12.3 Generalized eigen/QZ

Cover actual `ggev`, `ggev3`, `ggevx`, `gges`, `gges3`, `ggesx`, `ggbal`, `ggbak`, `gghrd`, `gghd3`, `hgeqz`, `tgexc`, `tgsen`, `tgevc`, `tgsna` and required computational paths.

Preserve homogeneous eigenvalues. Verify `beta A v - alpha B v` rather than forming `alpha/beta`. Explicitly handle beta zero, clustered eigenvalues, singular pencils, and valid partial convergence. Indeterminate alpha/beta pairs must not be normalized into fabricated finite values.

For QZ/Schur factors, verify both original matrices and the actual conjugation/transposition convention. Preserve condition and separation estimates, reordering selections, and appropriate left/right normalization.

### 12.4 Matrix-equation kernels

Implement actual `trsyl`, `trsyl3`, `tgsyl` and inventoried supporting operations. Retain scale, sign, transpose choices, problem dimensions and native warning semantics. A returned X may solve a scaled equation; verify the equation with the returned scale.

For an ordinary Sylvester call, the equation is of the form `op(A) X + sign * X op(B) = scale * C`. For generalized calls, use the two coupled equations from the pinned routine, not a guessed extension. Unscaling is a separate checked operation that may overflow and is not mandatory for a valid scaled result.

Do not add a generic Lyapunov/Riccati/matrix-exponential framework under the name “full LAPACK.” Those are separately approved ASC compositions unless an exact required upstream routine supports them.

### 12.5 Acceptance

Use diagonal/rotation/triangular examples, repeated and clustered spectra, nonnormal matrices, generalized problems with positive-definite and singular B where appropriate, and analytically constructed Schur/QZ decompositions. Check residuals, invariant subspaces, orthogonality or problem-specific metrics, and homogeneous eigenvalue consistency.

Test callbacks, pair handling, illegal selection ranges, ignored triangles, interval endpoints, failure-report validity, complex/real outputs, and scaled Sylvester warnings. A residual may be small for a wrong zero eigenvector; check vector norms and nontriviality. Compare projectors/subspaces for repeated eigenspaces instead of demanding the same basis.

---

## 13. P09 — specialized routines and completeness closure

**Depends on:** P05–P08; dependency discovery starts in P00, not here.  
**Exit:** no unimplemented required operation remains in the frozen full reference-CPU profile.

### 13.1 Implement the less familiar required families

Complete DMD (`gedmd`, `gedmdq` and actual precision variants), mixed-precision systems such as actual `dsgesv`, `dsposv`, `zcgesv`, `zcposv`, optional extra-precision drivers/refinement (`*svxx`, `*rfsx` where present), named blocked/two-stage alternatives, precision conversions, safe norms/scaling, expert auxiliaries, and inventoried compatibility entries.

DMD must preserve exact rank-selection, scaling, job options, eigen/mode outputs and residual conventions of the pinned routines. Test known linear dynamics, exact rank deficiency, and nonsquare snapshot sets. Do not replace it with a separately invented DMD implementation under an existing expert routine name.

Mixed-precision routines can have an **upstream-defined** refinement/fallback behavior. Preserve and report it; this differs from an unauthorized hidden ASC fallback. Do not change a driver's working precision to double everywhere and call it a mixed-precision implementation. Test successful refinement, fallback/status reporting, and ill-conditioned examples under appropriate assumptions.

### 13.2 Close optional dependency gaps without changing the denominator

The pinned upstream CMake exposes `USE_XBLAS`; identify precisely which required routines depend on it. Obtain an authoritative source/version with recorded redistribution terms and checksum, or implement an approved genuine equivalent helper route. Build and probe every affected symbol. A system library named `xblas` without source/build identity is not sufficient evidence. [S05]

Never compile an extra-precision signature against ordinary BLAS and pretend its accuracy contract is unchanged. A missing or legally unresolved dependency remains a blocker for affected rows and full-profile completion. Continue independent rows and state the exact dependency/action needed. Do not mark those rows not applicable, optional-to-ASC, or verified by a stub.

Inspect all required routines for allocations, unhandled callbacks, Fortran-only interfaces, arithmetic assumptions, runtime-global state, and algorithm-specific diagnostics. Earlier wrappers' patterns may not fit these cases.

### 13.3 Exhaustive closure procedure

Regenerate the inventory from the unchanged pinned source and compare it byte-for-byte/canonically with the frozen version. Review every difference; the denominator cannot shrink because of a generator regression.

For each required row still not verified, complete its declaration, checked descriptor/option contract, workspace path, provider binding, precise `INFO`/output semantics, tests and documentation. Check every distinct actual scalar signature, not merely a family count.

For each excluded row, verify the reason against source/build/docs. Keep test generators, timings, internal support and compatibility classifications explicit. A callable public auxiliary used by expert users cannot be buried as internal simply to clear the report.

Run link probes against the installed provider and ASC library. Missing LAPACKE bindings receive real shims. Check that generated code emits executable bodies rather than declarations or unconditional unsupported returns. Test coverage must invoke the actual wrapper and record the precise routine/mode exercised.

Optional optimized providers and new CUDA routines are **not** prerequisites for closing the required reference CPU profile; their existing unsupported rows remain clearly separate. Full-native coverage is also separate from required capability.

---

## 14. P10 — Matrix Market interchange

**Depends on:** P03. Can proceed alongside P05–P09.  
**Exit:** documented standard rank-two interchange in both owning modules with independent fixtures and strict resource control.

### 14.1 Format combinations

Implement actual Matrix Market `matrix array` and `matrix coordinate` representations; applicable real/integer/complex fields, and coordinate pattern fields; general/symmetric/skew-symmetric/Hermitian forms where valid. Matrix Market's dense array ordering is column-oriented and coordinate indices are one-based. [S14]

Freeze a compatibility table that distinguishes supported valid combinations from combinations forbidden by the format. `array pattern` is not valid. A real symmetric matrix and a complex Hermitian matrix have different mirroring rules. Structured symmetry requires square dimensions. Do not make an invalid combination “work” by silently discarding its header.

The native array format's scalar precision code does not exist in Matrix Market; use the caller's explicit destination type and documented exactness/conversion policy. Avoid converting integer tokens through floating point. Pattern files have no values: require an explicit unit-value policy or a pattern-only destination; never invent random or uninitialized values.

### 14.2 Reading algorithm

1. Read/validate banner, object kind, storage kind, field and symmetry. Handle the standard comment and whitespace rules, including bounded line lengths.
2. Parse dimensions and stored count with checked sign/magnitude/range arithmetic. Compute maximum decoded/expanded storage under the resource budget before allocating.
3. Read exactly the required records or array values. Convert coordinate indices to zero-based only after checking their legal one-based range.
4. Enforce the format's stored-half/diagonal rules. Mirror off-diagonal values exactly once, with identity, negation or conjugation as appropriate. Do not double mirrored diagonal values.
5. Apply the caller's explicit duplicate policy (`reject` by default, or deterministic checked summation) and explicit-zero policy (`preserve` by default).
6. Canonicalize in a builder or explicit staging workspace; report required scratch and sorting cost. Never claim allocation-free canonicalization while allocating a hidden map/vector.
7. Validate the full object and trailing-file policy, then publish or transactionally commit.

Hermitian diagonals must obey the documented real-valued constraint; skew-symmetric diagonals follow the standard zero/omission rule. Do not silently repair malformed diagonal values. Duplicate handling and symmetry expansion order must be documented so repeated mirrored entries are not counted twice unexpectedly.

Dense destination of a coordinate file is an **explicit** choice with a checked full-size budget and explicit initialization/scattering. Sparse destination of an array file is likewise explicit with a defined zero policy. These optional cross-representation conveniences must not create Dense/Sparse sibling dependencies. Required native Dense-array and Sparse-coordinate paths are independent.

### 14.3 Writing algorithm

Write a valid banner, dimensions/count and exactly the standard required data order. Count emitted records consistently with symmetry and explicit zeros. For symmetry-compressed output, either validate the complete represented matrix or accept a correctly typed structured descriptor whose contract makes the assumption explicit; do not label arbitrary data Hermitian based on one triangle without a documented interpretation.

Dense array output is column-major on wire regardless of memory layout. Coordinate indices become one-based through checked arithmetic. A native CSR/CSC object can be traversed as coordinates without densification. Any sorting/conversion requirement is explicit. Use sufficient digits for the chosen numeric interchange guarantee.

### 14.4 Required independent examples and tests

A tiny Hermitian coordinate fixture:

```text
%%MatrixMarket matrix coordinate complex hermitian
% Independent ASC interoperability fixture.
2 2 3
1 1 4 0
2 1 2 2
2 2 11 0
```

It represents conventional matrix rows `(4, 2-2i)` and `(2+2i, 11)`. Derive expected values independently; do not generate the expectation with the reader under test.

Also test real/integer/complex general array files; rectangular coordinate files; symmetric and skew-symmetric lower-half data; pattern policies; duplicates; zero stored count; dimension overflow; one-based zero/out-of-range indices; bad symmetry/field combinations; comments; CRLF; count mismatch; truncated records; trailing garbage; resource failures; and destination rollback.

Use an independent established reader/writer in an optional test environment for additional interoperability evidence. It must not become a runtime dependency, and a missing external test tool is reported, not falsely passed. Hand-derived format fixtures remain mandatory.

---

## 15. P11 — integration, documentation, packaging, and final evidence

**Depends on:** all required P02–P10 outcomes and cross-cutting verification.  
**Exit:** review-ready implementation with truthful full-profile evidence, or an explicitly incomplete handoff with precise remaining blockers. Never relabel incomplete as done.

### 15.1 Finish usable convenience APIs

Build a coherent small convenience layer on the expert operations: named factor creation, repeated solves, least-squares results, SVD/eigen results and optional explicit Q formation. Factories accept a resource; borrowed factor views state lifetimes; numerical status and meaningful diagnostics remain visible.

Do not wrap every expert routine in a second redundant allocation-heavy class. A few consistent user workflows plus the complete expert surface are preferable. Do not add automatic matrix-property detection/backslash dispatch unless separately approved. Explicit method and provider selection remain the default.

Generic array read/print/save remains usable with only Dense or Sparse linked. Scalar metadata inspection does not force a LAPACK context. Installed examples must use only public headers and exported targets, never build-tree/private includes.

### 15.2 Documentation deliverables

Write useful Doxygen comments for every new public declaration: mathematical operation, actual scalar types, shape/storage constraints, ignored inputs, output layout, aliasing, workspace/allocations, memory placement, lifetime, mutation on failure, report semantics, precision/reproducibility, concurrency and provider availability.

Generate a readable capability catalogue from the checked manifest, showing required versus implemented/verified native/provider support. Include exact upstream reference identities. Do not say “full LAPACK” in a release/support heading before the full-profile gate passes.

Add guides for basic/reused solves, least-squares/rank decisions, SVD/eigen output conventions, complex arrays, printing, text/binary schemas, Matrix Market, explicit packing, provider setup, failure handling, and supported configuration limits. Explain that a low residual alone is not a forward-accuracy certificate.

Provide migration notes for new complex-owner support and new public targets. Preserve existing BLAS/Random documentation and compatibility promises. Update public-header/target lists through their existing generators and validators, not by manually editing only checksums.

### 15.3 Package tests

Test provider-free and provider-enabled builds, static and shared libraries, Debug and Release, minimum and current supported compilers, header self-containment, multi-TU ODR, no-exception behavior, isolated components and missing components.

Install, relocate into a prefix with spaces, hide the source/build trees, disable package registries, and build/run consumers using `find_package`. Test a C++-only consumer of a prebuilt provider with no Fortran compiler available. Correctly ship/find required foreign runtime libraries; compilation without execution is insufficient.

The provider's BLAS and LAPACK integer ABI must match. Do not allow unrelated system libraries to satisfy missing symbols accidentally. Audit link maps/dynamic dependencies and duplicate symbols, including an environment with another BLAS/LAPACK present. Do not change process-global resolution to mask a broken package.

Base `ASC::cpp`, `ASC::dense`, `ASC::sparse`, and Random consumers must still work when LAPACK and CUDA discovery are unavailable. A requested unavailable provider component fails clearly; an unrequested one is not discovered.

### 15.4 End-to-end acceptance applications

Compile/run at least these installed applications:

1. **Read–factor–solve–report–print–save–reload:** native and reference LU, multiple RHS, known residual and round-trip.
2. **Complex Hermitian solve/eigen workflow:** complex Dense owner creation, Matrix Market import, explicit provider, real eigenvalues and meaningful complex output.
3. **Rectangular least squares/SVD:** rank and minimum-norm/selected-mode diagnostics, without claiming zero residual for inconsistent data.
4. **Sparse archive/preview workflow:** COO/CSR/CSC round-trip and bounded coordinate/value output without Dense linkage or densification.
5. **Failure workflow:** corrupt file leaves an existing target unchanged; singular solver exposes numerical failure/report without fabricated solution success.

Examples can share small support helpers but must not depend on MdeCpp or test-private APIs. Their purpose is demonstration and integration evidence, not a new application framework.

### 15.5 Final profile checks

Run all complete-program validators in strict mode, full numerical groups, parser/fuzz regression tests, sanitizers/static analysis, package tests, documentation checks and provenance/architecture checks. Confirm required tests were selected and actually executed; zero tests, unexpected skips, ignored failures and missing tools are not passes.

The final reference profile must include every required upstream row, actual scalar signature and documented option/storage class. Report platform-specific evidence separately. A Linux-only run must not claim Windows/macOS verification. Configure appropriate hosted CI when authorized and available; lack of access stays a specific evidence blocker.

Freeze the final code commit and verification configuration. Recheck any code changes made after verification. Evidence-only summary commits may reference the tested implementation tree, but do not claim tests ran on a new implementation identity without checking.

---

## 16. Cross-cutting numerical verification

### 16.1 Independent checks, not circular comparisons

A wrapper compared with a direct call to the same provider proves argument mapping, not independent algorithm correctness. Combine such checks with independently derived matrices and mathematical residuals, normalization, reconstruction, subspace and failure-contract tests.

Reuse the existing deterministic testing framework. New tests must not couple Dense to Random solely to generate fixtures; use explicit small formulas or local deterministic test-only generators with independently documented seeds. Tests for Sparse remain component-isolated.

Preserve inputs before destructive operations where residuals need them. The preservation allocation is explicit test setup, not hidden production behavior. Validate all returned dimensions and finite outputs where promised before computing numerical residuals.

### 16.2 Normalized metrics

Define a consistent norm and scaling policy per test. Use safe scaled norm computations to avoid making the verifier overflow when the algorithm does not.

- Linear solve: `||A X - B|| / (||A|| ||X|| + ||B||)`.
- LU: reconstruction using the actual pivot convention, not an assumed permutation direction.
- Cholesky: reconstruction of the selected Hermitian/symmetric matrix and factor diagonal validity.
- QR/SVD: reconstruction plus orthogonality/unitarity of appropriate full/economy factors.
- Least squares: residual optimality `A^H(A X - B)` and minimum-norm/nullspace conditions where required.
- Eigenpairs: `||A v - lambda v||` relative to matrix/vector/eigenvalue scales and vector nontriviality.
- Generalized eigenpairs: `||beta A v - alpha B v||` with scale-aware normalization and no division by beta.
- Schur/QZ: both factor reconstruction and orthogonal/unitary transformations.
- Sylvester: the returned **scaled** equation, including all operation/sign choices.

For a zero denominator, return metric zero only when the numerator is also exactly zero; otherwise treat it as failure/infinite error. Do not divide by zero or use an arbitrary additive one that hides zero-problem failures.

Tolerances depend on scalar epsilon, dimensions, algorithm and known conditioning. Record the reason for each bound. Do not use one `1e-6` tolerance for all precisions and matrix sizes. Do not loosen tolerances merely to match a broken wrapper. Backward error and forward error are distinct.

For repeated/clustered eigenvalues or singular values, compare invariant subspaces/projectors and unordered spectra as appropriate. Do not demand identical signs, phases, pivots, or bases across valid algorithm implementations unless the particular tested contract promises them.

### 16.3 Independent starting fixtures

Use these mathematical examples as independent starting points, then extend adversarial coverage. They are original runbook fixtures, not copied tests.

**LU with nontrivial pivoting and two RHS:**

```text
A = [[0,  2,  1],
     [1, -2, -3],
     [2,  3,  1]]
X = [[ 1,  2],
     [-1,  0],
     [ 2, -1]]
B = [[ 0, -1],
     [-3,  5],
     [ 1,  3]]
```

Verify `A X = B` independently before freezing expectations. Test factor reuse, transpose and conjugate variants with separately formed right-hand sides. Singular example: `[[1,2],[2,4]]`; scalar singular example: `[[0]]`.

**SPD factor:** use

```text
L = [[ 2, 0, 0],
     [ 1, 3, 0],
     [-1, 2, 2]]
A = [[ 4,  2, -2],
     [ 2, 10,  5],
     [-2,  5,  9]]
```

**Hermitian positive-definite factor:** `L = [[2,0],[1+i,3]]` gives `A = [[4,2-2i],[2+2i,11]]`. **Indefinite 2-by-2 block:** `[[0,1],[1,0]]` exercises behavior that unpivoted positive-definite logic cannot handle.

**Inconsistent full-rank least squares:** `A = [[1,0],[0,1],[1,1]]`, `b = [3,0,0]`, exact minimizer `[2,-1]`, residual `b-Ax = [1,1,-1]`. **Wide minimum norm:** `A = [[1,0,1],[0,1,0]]`, `b = [2,3]`, minimizer `[1,3,1]`.

**Repeated singular values:** diagonal `(4,4,0)` with explicit orthogonal/unitary left/right rotations. **Complex pair:** `[[0,-2],[2,0]]` has eigenvalues `±2i`. **Infinite generalized eigenvalue:** `A = diag(2,3)`, `B = diag(1,0)`; verify homogeneous pairs rather than dividing by zero.

**Sylvester:** with `A=diag(1,2)`, `B=diag(3,5)`, and `X=[[1,-1],[2,0.5]]`, use `C=[[4,-6],[10,3.5]]` for `A X + X B = C`. Add strongly scaled cases and test the returned scale without requiring a particular implementation to choose scaling when it does not need to.

**DMD:** use independently specified full-rank snapshots satisfying a known diagonal linear map, then rank-deficient variants. Exact expected modes are interpreted up to normalization and repeated-spectrum ambiguity.

### 16.4 Error and memory evidence

For every operation family, test invalid flags, dimensions, leading dimensions, selection ranges, scalar types, integer overflow, backing-span capacity, aliasing, workspace length/alignment, provider/placement mismatch and stale plans. Structural failure must not mutate destinations or invoke the provider.

Use red zones, sentinels and instrumented resources to detect padding writes, ignored-operand reads, hidden packing/allocation, implicit device copies and unnecessary synchronization. Use valid backing spans; forged descriptors are not a safe test harness.

Exercise real numerical failure where feasible: singularity, non-positive-definiteness, known rank deficiency and warnings. Use a mock provider to test rare nonconvergence/report translation only when a deterministic real example is impractical, and label it as injection evidence rather than a real numerical convergence test.

Native paths receive sanitizer coverage. Provider code should be instrumented when supported; clearly distinguish ASC-only instrumentation from an instrumented foreign library. A foreign toolchain incompatibility is a coverage limitation, not permission to call all provider memory behavior verified.

---

## 17. Coverage manifests and tooling requirements

### 17.1 Separate inventory, implementation, and evidence

Use three independent layers:

1. **Upstream inventory:** generated from the frozen upstream source; source-derived requirements, not current implementation status.
2. **ASC mapping/contracts:** reviewed design linking each required operation to an API, layouts/options, implementation route and tests.
3. **Execution evidence:** produced by actual test runs against a specific implementation/provider/configuration.

A generator cannot award itself `verified` because it emitted code or a test filename. Do not populate a test ID with a nonexistent or nonexecuted test. Changing code, a source lock, a relevant contract, or a provider build invalidates affected evidence until rechecked.

### 17.2 Illustrative LAPACK mapping row

The following is a schema example, **not** a completed row or an existing API guarantee:

```yaml
schema_version: 1
program: ASC-CPP-LAPACK-IO
specification:
  version: 3.12.1
  commit: 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca
routines:
  - id: lapack.dgetrf
    upstream_routine: dgetrf
    classification: public_computational
    required_profiles: [reference_cpu_full]
    source_paths: [SRC/dgetrf.f]
    family: general_lu
    scalar_signature:
      matrix: f64
      pivots: signed_provider_integer_encoding
    asc_operation: Getrf
    contract_ref: lapack/general-lu
    legal_modes_ref: lapack/general-lu-modes
    workspace_ref: lapack/general-lu-workspace
    mutation_ref: lapack/general-lu-mutation
    native_info_ref: lapack/general-lu-info
    implementations:
      native: {state: not_started}
      reference_cpu: {state: not_started}
    required_test_classes:
      - validation
      - reconstruction
      - factor_reuse
      - layouts
      - workspace
      - numerical_failure
      - installed_consumer
    evidence_ids: []
```

Actual source hashes, docs, declaration/signature identity, precision exceptions, descriptor fields, provider-lock reference, ABI configurations, test IDs, and legal mode constraints must be present in production records. Store detailed mode requirements in normalized subordinate records rather than opaque prose that a checker cannot validate.

Represent finite legal mode combinations or reviewed equivalence classes explicitly, including interactions among JOB flags. An equivalence class requires a reason and representative tests covering all relevant branches; pairwise flag testing is not automatically sufficient. Numerical sizes can use boundary/representative cases; the requirement is not an impossible enumeration of all matrix entries or sizes.

Track unimplemented, implemented-unverified, verified, blocked and excluded states distinctly. Preserve excluded entries in the source inventory with evidence. Raw Fortran `INFO`, ASC translated status and callback/logical representation are separate fields.

### 17.3 Array I/O coverage dimensions

Each required behavior has an ID and a test mapping over applicable:

- Dense versus COO/CSR/CSC; owning load versus view/values-only transactional load.
- Print versus text versus binary versus Matrix Market.
- Scalar codes and relevant special values.
- Rank zero, rank one, rank two, higher rank, mixed/static extents and zero extents.
- Physical layout, noncontiguous views and padding.
- Resource limits, exact counts, duplicate/zero policies, type conversion and corrupted input.
- Source/sink failures, source progress, destination rollback, and unsupported placement.
- Installed components and external-format compatibility.

Do not take the Cartesian product of invalid combinations: Matrix Market is rank two, CSR/CSC are rank two, pattern is coordinate-only, and LAPACK numeric scalar restrictions differ from I/O. Each not-applicable combination needs a rule tied to the format/storage contract.

### 17.4 Tooling deliverables

Create or extend deterministic tools with tested `--help` and nonzero failure exits. Proposed capabilities:

- Generate/check the pinned LAPACK inventory without network access.
- Validate source-lock consistency and immutable inventory denominator.
- Validate coverage schemas, legal mode rules, evidence references and required-profile closure.
- Generate binding declarations, documented tables and test registration only from reviewed contracts.
- Detect stale generated files and reproduce outputs in a clean checkout.
- Validate array format fixtures using an independent simple reference codec.
- Build an evidence index and a readable final status summary.

Prefer ordinary readable generated code over template or macro metaprogramming. Every generated file identifies its generator, input hash and regeneration command. Golden expectations must not be computed by the same function under test.

Python tooling follows the live Google Python guide: explicit responsibilities, documented public functions, type hints where useful, structured diagnostics, safe parsing and checked subprocess exits. Reuse the repository's supported Python version and approved development dependencies. YAML parsing uses a safe approved parser or a documented JSON-compatible representation, not unsafe object construction or ad hoc executable YAML. Keep Python out of ordinary runtime library use and installed C++ consumers.

Do not download dependencies, run shell text from metadata, or execute arbitrary generators during ordinary builds. Explicit bootstrap/source-update steps can acquire approved artifacts; regular validation works offline.

### 17.5 Evidence record

A machine-readable evidence entry contains: unique ID, timestamp with timezone, implementation commit/tree and dirty-diff hash, manifest/source-lock hashes, provider build identity, compiler/standard-library/OS/architecture, integer ABI, configuration, exact command/working directory, exit code, selected/executed/passed/failed/skipped test counts, covered row/mode IDs, and log/artifact paths plus hashes.

Do not record access tokens, credentials, unrelated environment variables, or private absolute paths in public artifacts. Maintain a local absolute-path index and sanitized shareable summaries. Test-count changes are reviewed; a sudden drop must fail closure checks instead of improving the pass rate.

---

## 18. CI and reproducible command protocol

### 18.1 Existing commands versus commands to create

The baseline presets cited in P00 and below were inspected in the pinned repository. New LAPACK presets/tools in this section are **required implementation deliverables**, not facilities presumed to exist today. Codex must create and validate them before invoking them. If equivalent repository commands already exist, reuse them and record the mapping.

Use bounded parallelism appropriate to the machine. Missing tools are reported as missing evidence; do not auto-install arbitrary global packages or change the system environment without the appropriate authorization.

### 18.2 Preserve baseline lanes

At the inspected baseline, the relevant configure/build/test presets include:

```text
test-debug
test-release
test-shared
test-release-shared
test-asan-ubsan
test-lsan
test-tsan
install-test
docs
```

For example, after dependencies are prepared:

```bash
cmake --preset test-release-shared
cmake --build --preset test-release-shared --parallel 2
ctest --preset test-release-shared --no-tests=error --output-on-failure

cmake --preset test-asan-ubsan
cmake --build --preset test-asan-ubsan --parallel 2
ctest --preset test-asan-ubsan --no-tests=error --output-on-failure

cmake --preset docs
cmake --build --preset docs --parallel 2
```

The existing `docs` build preset targets `asc_cpp_docs_check`. Preserve strict documentation behavior. Existing sanitizer presets intentionally select certain labels; document their scope and supplement uncovered package/provider tests rather than pretending one lane covers everything. Do not delete existing exclusions or checks blindly. [S03]

### 18.3 New required provider lanes

Add separate provider-enabled native integer32 and supported integer64 configurations, at least Release static/shared plus Debug correctness coverage where the toolchain supports it. Suggested preset names are `test-lapack-reference` and `test-lapack-reference-shared`; choose clear unique names for ILP64 and strict full-profile configurations.

The provider configuration must identify the exact prepared prefix/lock, disable unintended alternative vendors, and expose the resulting source/build fingerprint. Do not embed one developer's absolute path in version-controlled presets. Use documented user presets/cache inputs for local paths.

A strict final run uses the newly implemented full-profile option and then runs the complete provider test selection, not only smoke tests. Ensure CTest registration covers every required row/mode-equivalence class and actually executes it. Use an evidence validator to detect absent registrations, filtered-out required tests and skips.

### 18.4 Reproducible logging

Use shell failure propagation; when piping logs through `tee`, enable `pipefail`. Preserve both command failures and logging failures. Do not wrap required tests in `|| true` or `continue-on-error` and later call the lane green.

For a supported Bash environment, a per-command pattern is:

```bash
set -euo pipefail
cmake --preset test-release 2>&1 | tee "$EVIDENCE_DIR/configure-release.log"
cmake --build --preset test-release --parallel 2 2>&1 \
  | tee "$EVIDENCE_DIR/build-release.log"
ctest --preset test-release --no-tests=error --output-on-failure 2>&1 \
  | tee "$EVIDENCE_DIR/test-release.log"
```

Set/create `EVIDENCE_DIR` outside the checkout first. A failed command stops that script; Codex records and fixes it or records the exact blocker, then continues other legitimate work. Do not reuse log filenames across distinct configurations/commits without a uniquely versioned parent directory.

Use equivalent checked PowerShell behavior on Windows rather than assuming Bash is available. Test the logging/evidence tool itself, including nonzero compiler/test/logging exits.

### 18.5 Required CI matrix and boundaries

Retain the current repository support matrix. Required evidence includes Linux GCC/Clang, Windows MSVC, and macOS AppleClang for the supported base component configurations. Add supported reference-provider toolchain combinations explicitly; do not mix incompatible Windows C/Fortran runtimes or assume a Linux shared library is portable.

Header/self-containment/no-exception, isolated component builds, installed relocated consumers, format/tidy, sanitizer/concurrency, provenance, API inventory, exact reference-profile, malformed-input and documentation checks are all relevant. Use the repository's current accepted tool versions rather than inventing “latest” names.

CI must not silently download a moving provider or use unpinned action tags where repository policy requires pinned actions. Restrict permissions. Do not make secrets available to untrusted fork code or change workflow permissions to bypass a failed check. No network is needed in installed consumer tests after dependency preparation.

Real NVIDIA verification is required only for actual new or changed GPU claims. Preserve experimental CUDA isolation and compile/regression checks when available; absence of a GPU is not a reason to stop CPU LAPACK/I/O work, and CPU tests are not proof of GPU runtime correctness.

---

## 19. Reviews, commits, blockers, and final handoff

### 19.1 Reviewable change sequence

Use P00–P11 as stable program IDs, not fabricated GitHub issue numbers. Break large packages into coherent subcommits/sub-PRs, for example P01 scalar lifecycle before P01 workspace ABI. Maintain an integration branch when necessary so dependent code can be tested together.

A practical sequence is:

```text
P00 inventory/locks/contracts
  -> P01 scalar + descriptor + report foundations
  -> P02 printing and P03 native I/O
  -> P04 provider/ABI + LU + integrated example
  -> P05 structured systems
  -> P06 orthogonal/least squares
  -> P07 SVD/GSVD/CSD
  -> P08 eigen/Schur/QZ/matrix equations
  -> P09 remaining required routines and optional dependencies
  -> P10 Matrix Market (can be earlier in parallel after P03)
  -> P11 full integration and evidence
```

A provider ABI spike can start alongside printing after the foundations. Do not leave array reading until the numerical catalogue is complete. Use actual diffs and state to avoid duplicate work.

Before committing, inspect the diff, run relevant tests, check generated drift and documentation, and record outstanding evidence. Commits are not inherently proof of verification. Do not fabricate issue/PR URLs or claim a push/merge that did not happen. Local commits and a patch bundle are valid handoff artifacts when remote writing is unavailable.

### 19.2 Focused reviews

For each package, perform a contract review, a numerical/format review, and a failure/ownership review. For P04/P09, add ABI/provenance review. For P03/P10, add malformed-input/resource review. For P11, add installed-package/no-hidden-dependency review.

Ask independent reviewers, when available, to derive expectations from the frozen contract rather than copy the implementer's tests. Request concrete findings with file/line references, severity, reproducer and required evidence. Without an independent reviewer, label the check as self-review; do not invent an “independent audit passed” claim.

Review checklist:

- Can a user call the promised operation through a typed public ASC API?
- Are scalar/job/storage variants actually implemented rather than declared?
- Is validation complete before any disallowed mutation or provider call?
- Are diagnostics and partial-result semantics preserved?
- Are allocation, packing, conversion, transfer and synchronization visible?
- Are lifetimes, constness and factor provenance coherent?
- Is the test oracle independent enough to detect the target bug?
- Do base-only and relocated installed consumers still work?
- Are the source lock, license/notices and documentation truthful?

### 19.3 Blocker record and anti-loop rule

For each blocker record: ID; affected package/rows; observed failure; exact command; logs; cause; at least one concrete attempted resolution; actions requiring unavailable access/tools/approval; independent tasks that can proceed; and the evidence that would close it.

Do not repeatedly check the same missing dependency/release with no changed conditions. After establishing the blocker, pursue a permitted source/prebuilt preparation route or continue unaffected work. Do not skip a required routine to avoid an inconvenient dependency. Do not invent a substitute package with the same name/version.

An environmental blocker may prevent verification but not source implementation. Distinguish `implemented_unverified` from `unimplemented`. A genuine unresolved license/ownership question may block an import but not independent implementation from authorized specifications. Keep the distinction explicit.

### 19.4 Resume after interruption

At the next session, read state and actual git status, verify dependency/manifest hashes, inspect unfinished diffs and rerun the last critical test if identity changed. Continue the next incomplete package; do not rebuild the whole design or reset to P00 unnecessarily.

If a prior state says verified but lacks exact logs/test counts/configuration, downgrade to implemented-unverified until checked. Preserve previously tested unaffected code, but do not carry forward whole-program success across relevant changes automatically.

If the entire program cannot be completed in a session, finish the current safe vertical slice, record an exact restart point, and report the remaining required rows. Do not call an interrupted implementation “full LAPACK” merely because the source plan includes every routine.

### 19.5 Final report contents

Provide a concise human summary and a machine-readable evidence index:

- Repository/base/final implementation identities, worktree/branch and actual PRs if any.
- Completed P00–P11 packages and remaining work, without ambiguous percentages.
- Frozen upstream version/commit/build and exact required routine denominator.
- Counts for mapped/callable/verified reference rows, verified native rows, blocked/unimplemented/untested rows, plus justified exclusions.
- Array printing/text/binary/Matrix Market behavior actually implemented and verified.
- Exact commands/configurations/tests executed, failures/skips/missing platform evidence.
- Public API/target additions, docs/examples, installation instructions, and compatibility decisions.
- Source/provenance/license changes and unchanged Random/BLAS guarantees.
- Remaining manual steps, only where actually required; no promise of future background work.

If every required development gate passes, say **implementation complete and review-ready**. Do not say released, published, merged, full-native, fully GPU-supported, or cross-platform-verified without the corresponding separate evidence and authorization.

---

## 20. Final acceptance checklist

All unchecked required items remain open. This checklist supplements, not replaces, the routine/mode manifests.

### Scope and correctness

- [ ] P00–P11 are all reconciled with real code/evidence, not merely written plans.
- [ ] The immutable upstream inventory includes all actual required public families and precision/mode variants.
- [ ] Missing LAPACKE entries have implemented reviewed shims or remain visible blockers.
- [ ] Required optional extra-precision dependencies and specialized/compatibility routines are not silently excluded.
- [ ] Native LU/Cholesky/Householder QR functionality is implemented and separately verified.
- [ ] Complete reference-provider capability is not mislabeled as full native C++ LAPACK.

### Contracts and compatibility

- [ ] C++20 and six-module dependency isolation are preserved.
- [ ] Complex Dense owners have valid tested lifecycle semantics on required platforms.
- [ ] Raw pivot/block encodings, integer/logical/complex ABI and `INFO` interpretation are correct.
- [ ] Workspace, explicit packing, output capacities, lifetimes and alias contracts are tested.
- [ ] Reports remain available on numerical failure and do not imply invalid factors are usable.
- [ ] No unapproved silent provider/algorithm/precision fallback exists.
- [ ] Existing BLAS and Random reproducibility tests retain their meaning and pass where verified.

### Array features

- [ ] Dense/Sparse printers output actual values with bounded work and visible truncation.
- [ ] Text/binary codecs have complete frozen schemas and independent fixtures.
- [ ] Reads into existing storage satisfy the staged transaction guarantee.
- [ ] Source consumption, failed writes, flush/close and overwrite behavior are documented and tested.
- [ ] Sparse shape/structure/stored-zero semantics and finalized immutability are preserved.
- [ ] No hidden densification, device transfer, padding serialization or unbounded parsing occurs.
- [ ] Matrix Market ordering/index/symmetry/duplicate/pattern rules pass independent tests.

### Delivery evidence

- [ ] Every required capability/mode-equivalence class has actual passing evidence or blocks full completion.
- [ ] Numerical checks, malformed-input tests, memory instrumentation and relevant sanitizers pass.
- [ ] Strict no-allocation claims include the actual foreign provider path.
- [ ] Installed/relocated examples run through public targets with the correct runtime dependencies.
- [ ] Provider-free C++-only consumers work without LAPACK/Fortran/CUDA discovery.
- [ ] Documentation, generated files, provenance, notices and API inventories are consistent.
- [ ] Platform/toolchain claims match actual execution; no skip, missing tool or unexecuted CI config is counted as a pass.
- [ ] Final code/evidence identities and an accurate handoff/resume state are recorded.

---

## 21. Source register

URLs are retained as reproducible references, not instructions to trust moving content during builds. Repository observations and the approved plan are distinct from the new design decisions in this runbook. Retrieve authoritative routine-level specifications during implementation and add their immutable identities to the contract ledger.

**[S00] Approved input plan.** `asc-cpp-lapack-and-array-io-plan.md`, supplied in this conversation, dated 2026-09-07. The two-track scope, provider/native distinction and P00–P11 packages are inherited from this plan. The preceding source-stage assessment is `MdeCpp-to-asc-cpp-feature-migration-plan.md`. These are planning artifacts, not test evidence.

**[S01] Target branch identity and public boundary, checked 2026-09-07.**

```text
https://api.github.com/repos/AI4SciComp/asc-cpp/branches/main
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/README.md
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/abi/public-headers.sha256
```

**[S02] Repository instructions, architectural boundaries and provenance.**

```text
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/AGENTS.md
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/docs/architecture/overview.md
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/docs/provenance/mdecpp-disposition.yaml
```

**[S03] Existing dependency and preset commands, inspected 2026-09-07.**

```text
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/docs/installation.md
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/CMakePresets.json
```

**[S04] Reference-LAPACK release identity, checked 2026-09-07.**

```text
https://www.netlib.org/lapack/release_notes.html
https://api.github.com/repos/Reference-LAPACK/lapack/git/ref/tags/v3.12.1
https://api.github.com/repos/Reference-LAPACK/lapack/git/tags/5ebe92156143a341ab7b14bf76560d30093cfc54
```

**[S05] Exact provider build configuration.** This is the source for the inspected index64, LAPACKE, optimized-provider and XBLAS option names and the patch-version metadata discrepancy.

```text
https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/CMakeLists.txt
```

**[S06] Existing Core I/O and owner-specific serialization boundary.**

```text
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/include/asc/core/io.h
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/docs/architecture/decisions/0006-io-and-serialization-boundary.md
```

**[S07] Scalar ownership and current numerical compatibility.**

```text
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/include/asc/dense/view.h
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/include/asc/dense/array.h
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/include/asc/sparse/coordinate.h
https://github.com/AI4SciComp/asc-cpp/blob/46412183b2ae86101b2361c52376a8db8efff264/docs/api-compatibility.md
```

**[S08] Official LAPACK routine catalogue and scope.** Family requirements above operationalize the approved plan [S00]; the live catalogue is a navigation/cross-check source. The final inventory must use the pinned source and routine-level documentation, not blindly trust this page's moving contents.

```text
https://www.netlib.org/lapack/explore-html/topics.html
https://www.netlib.org/lapack/faq.html
```

**[S09] LAPACKE interface conventions.** Historical interface documentation, useful for workspace, layout, integer/logical/complex and native `INFO` distinctions; not a complete contemporary inventory.

```text
https://www.netlib.org/lapack/lapacke.html
```

**[S10] Example of nontrivial indefinite pivot encoding.** Read the exact source counterpart for each other factorization variant.

```text
https://www.netlib.org/lapack/explore-html/d8/d0e/group__hetrf_ga431b081d6c9c48af82ec003a7d3070ff.html
```

**[S11] Google C++ Style Guide, live page checked 2026-09-07.** The user also supplied `Google C++ Style Guide.html`. Record snapshot identities when executing, and preserve accepted repository compatibility exceptions.

```text
https://google.github.io/styleguide/cppguide.html
```

**[S12] Google Python Style Guide, live page checked 2026-09-07.** The user also supplied `Google Python Style Guide.zip`; Python tooling is development-only.

```text
https://google.github.io/styleguide/pyguide.html
```

**[S13] CMake minimum-version LAPACK discovery reference.** Check discovery and imported targets without assuming LAPACKE/full routine coverage. Use the current documentation only with version guards for newer features.

```text
https://cmake.org/cmake/help/v3.25/module/FindLAPACK.html
https://cmake.org/cmake/help/latest/module/FindLAPACK.html
```

**[S14] NIST Matrix Market format specification.**

```text
https://math.nist.gov/MatrixMarket/formats.html
```

**[S15] Official Codex guidance for repository instructions and durable execution plans.** This runbook uses explicit readable state and checkpoints; it does not assume particular optional multi-agent tools or CLI flags.

```text
https://developers.openai.com/codex/guides/agents-md
https://developers.openai.com/cookbook/articles/codex_exec_plans
```

---

**Final instruction to Codex:** Implement the full approved program. Keep the base library small in dependency scope, not small in promised capability. Make every claim traceable to a real API, a real implementation and an executed verification record. Continue through the remaining required work instead of stopping at the first successful demonstration.
