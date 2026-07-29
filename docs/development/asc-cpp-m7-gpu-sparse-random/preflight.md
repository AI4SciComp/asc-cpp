# Milestone 7 Preflight

Status: Passed before implementation

Date: 2026-07-28

## Authority read

The lead read repository `main:AGENTS.md`, the architecture blueprint, ADRs
0001--0018, dependency/capability manifests, backend matrix, roadmap,
repository/provenance audits, implementation/testing/CI/ASCCMake guidance, the
Milestone 6 Publication Checkpoint B report, and the owner-supplied runbook.

The approved Milestone 7 contract, ownership, dependency, and provenance
records retained on the M8 handoff branch were read as design authority only.
No M8 implementation or prohibited historical MdeCpp/deleted asc-cpp source
was inspected.

No material design decision remains unresolved. The four target closures,
sparse/random operation bounds, bit contracts, ownership/lifetime rules,
external dependencies, and explicit exclusions are fixed by the approved M7
record and accepted ADRs.

## Repository and predecessor audit

Commands:

```sh
git fetch --prune origin
git branch --show-current
git rev-parse HEAD main origin/main \
  feature/asc-cpp-m6-gpu-core-dense \
  feature/asc-cpp-m7-gpu-sparse-random
git status --porcelain=v1
git diff --cached --name-only
git diff --name-only
git ls-files --others --exclude-standard
git worktree list --porcelain
```

Result: pass. Before and after the safe branch switch, `HEAD`, `main`,
`origin/main`, M6, and M7 were exactly
`33b261ea33616a6395c4ad3b20646093103344f7`. There is one worktree. The exact
M6 checkpoint state was unchanged: 459 porcelain entries, 334 staged paths,
38 tracked-unstaged paths, and 265 untracked files.

No reset, clean, stash, path checkout, commit, or deletion occurred.

## Dependencies and environment

ASCCMake is clean/current `main`, exact tag `v0.1.0`, commit
`8a7dcbad3a97267cce59810aff24de800a3497a7`. MdeCpp remains current `main`
at `f6294e9079262682ce63ae7ff2d8a643e658bf5d`; its pre-existing modified
`Makefile` remains untouched.

```text
CMake:                 4.1.2
GCC:                   11.4.0
Clang:                 19.0.0
CUDA compiler/toolkit: 12.9.86
driver:                576.83
GPU:                   NVIDIA GeForce RTX 3060 Laptop GPU
memory:                6144 MiB
compute capability:    8.6
```

This inventory establishes that local validation can proceed. It does not by
itself assign a provider evidence label.

## Approved owner amendment

During implementation, independent verification identified that a raw
pointer-plus-count Random CUDA API could not prove the destination span using
the approved CUDA Runtime dependency. The owner approved the existing Core
`MutableMemoryView` plus explicit word count on 2026-07-28. This makes the
declared capacity observable and safely testable without an allocation
registry, CUDA Driver API, new dependency, or later-milestone Core change.
