# Milestone 8 Preflight

Status: Pass with preserved stale-publication boundary

Date: 2026-07-28

## Approval and branch

```text
approval:  Milestone 8, no corrections
branch:    feature/asc-cpp-m8-hardening-downstream-r2
HEAD:      33b261ea33616a6395c4ad3b20646093103344f7
version:   unreleased 0.9.0 candidate
```

The revision branch was created locally from the approved M7 feature worktree
without committing, stashing, resetting, restoring, or discarding the
cumulative M0--M7 state.

## Main and predecessor state

After `git fetch --prune origin`, `main` and `origin/main` are synchronized at
`33b261ea33616a6395c4ad3b20646093103344f7`. The M7 feature branch and
revision branch begin at the same commit. One worktree exists.

The required predecessor is the intentionally cumulative, unpublished M7
Publication Checkpoint B worktree, not a clean commit. It remains exactly at:

```text
porcelain entries:       484
staged paths:            334
tracked-unstaged paths:   38
untracked files:         334
```

Those counts, the M7 contract/reviews, and the M7 checkpoint are the
preservation boundary. M8 must not treat the complete dirty status as M8-owned.

## Existing stale M8 branch and pull request

The local and remote branch
`feature/asc-cpp-m8-hardening-downstream` both point to
`d611aa876576ab949c2c213977f628d2844539ba`; open pull request 1 targets
`main`. That branch contains an earlier M8 attempt with the now-rejected raw
pointer Random CUDA signature. It predates the approved M7
`MutableMemoryView + word_count` correction.

The old branch and pull request contain preserved unique work and are not
modified or deleted. This revision uses a distinct branch so no force-push,
history rewrite, unsafe checkout, or predecessor loss is required. Exact
remote reconciliation is deferred to a separately authorized publication
step.

## Governing material read

The lead read the current owner prompt, repository `main:AGENTS.md`, the
complete 2,262-line asc-cpp runbook, architecture blueprint, ADRs 0001--0018,
released ASCCMake binding, dependency and capability manifests, backend
matrix, roadmap, implementation/testing/CI strategy, repository/provenance
audits, and the complete M7 Publication Checkpoint B.

No material product-design choice remains unresolved. The roadmap and ADR 0018
fix M8 as hardening-only, `0.9.x`, unchanged product/dependency surface, full
locally available matrix, and an asc-xde-shaped downstream trial.

## Repository and tool state

Sibling repositories are read-only:

```text
asc-cmake:
  main/origin/main 8a7dcbad3a97267cce59810aff24de800a3497a7
  exact release v0.1.0
  clean, one worktree
asc-xde:
  main/origin/main abcb29b51f22f40afd7f174707b7ccf83c32d4bf
  clean skeletal repository, one worktree
MdeCpp:
  main/origin/main f6294e9079262682ce63ae7ff2d8a643e658bf5d
  unrelated modified Makefile preserved, one worktree
```

Available local evidence infrastructure:

```text
CMake/CTest 4.1.2
GNU Make 4.3
GCC 11.4.0
Clang/clang-format 19.0.0
CUDA/nvcc 12.9.86
Compute Sanitizer 2025.2.1.0
NVIDIA GeForce RTX 3060 Laptop GPU
driver 576.83, 6144 MiB, compute capability 8.6
Ninja unavailable
```

GitHub authentication is available. Pull request 1 is the only open asc-cpp
pull request; no open issue exists. This milestone performs no GitHub
mutation.

## Dependency, provenance, and scope gates

No new product dependency or product target is approved. System inspection
tools may be used for local review but do not become build/install
dependencies. An unavailable tool produces a truthful skipped row.

No product C++ API change is planned. A proven defect may be corrected only by
the lead after independent evidence and review. DLPack, AD, distributed
adapters, new providers, legacy compatibility, 1.0 cleanup, publication, and
other-repository writes are excluded.

The previous M8 branch may inform hardening requirements, but every reused idea
must be re-reviewed against the current M7 API, manifests, package graph,
provenance boundary, and independent verification design.
