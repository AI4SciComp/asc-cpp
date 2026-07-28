# Milestone 6 Preflight

Status: Passed before implementation

Date: 2026-07-28

## Authority read

The lead read repository `main:AGENTS.md`, the architecture blueprint, ADRs
0001--0018, dependency/capability manifests, backend matrix, roadmap,
repository/provenance audits, testing/CI/ASCCMake guidance, the Milestone 5
Publication Checkpoint B report, and the owner-supplied runbook.

The retained approved Milestone 6 contract/ownership records on the current
roadmap's M8 handoff branch were read as design authority only; no M7/M8 or
historical MdeCpp/deleted asc-cpp implementation source was inspected.

No material design decision remains unresolved. The exact public/provider
surface, package closure, execution/lifetime semantics, cuBLAS subset, and
exclusions are fixed by the approved M6 record and ADRs 0002, 0004, 0007--0009,
0011, 0013, 0017, and 0018.

## Repository and predecessor audit

Commands:

```sh
git fetch --prune origin
git branch --show-current
git rev-parse HEAD main origin/main \
  feature/asc-cpp-m5-random-storage-generation \
  feature/asc-cpp-m6-gpu-core-dense
git status --porcelain=v1
git diff --cached --name-only
git diff --name-only
git ls-files --others --exclude-standard
git worktree list --porcelain
```

Result: pass. Before branch switch, `HEAD`, `main`, `origin/main`, M5, and M6
were exactly `33b261ea33616a6395c4ad3b20646093103344f7`. There was one
worktree. The exact M5 checkpoint state was unchanged: 437 porcelain entries,
334 staged paths, 34 tracked unstaged paths, and 207 untracked files.

The switch to `feature/asc-cpp-m6-gpu-core-dense` preserved those counts. No
reset, clean, stash, path checkout, commit, or deletion occurred.

## Build dependencies and external references

ASCCMake is clean/current `main`, exact tag `v0.1.0`, commit
`8a7dcbad3a97267cce59810aff24de800a3497a7`. MdeCpp remains current `main`
at `f6294e9079262682ce63ae7ff2d8a643e658bf5d`; its pre-existing modified
`Makefile` remains untouched.

## CUDA inventory

```text
CUDA compiler/toolkit: 12.9.86
driver: 576.83
GPU: NVIDIA GeForce RTX 3060 Laptop GPU
memory: 6144 MiB
compute capability: 8.6
runtime/cuBLAS libraries: visible
```

This inventory only establishes that required validation can proceed. It is
not itself configure-tested, compile-tested, runtime-tested, or parity-tested
ASCCpp evidence.

