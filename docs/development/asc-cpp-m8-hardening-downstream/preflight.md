# Milestone 8 Preflight

Status: Pass

Date: 2026-07-27

## Approval and branch

```text
approval:  Milestone 8, no corrections
branch:    feature/asc-cpp-m8-hardening-downstream
HEAD:      33b261ea33616a6395c4ad3b20646093103344f7
version:   unreleased 0.9.0 candidate
```

The branch was created locally from the M7 feature branch without committing,
stashing, resetting, or discarding the cumulative M0--M7 working tree.

## Repository state

The cumulative restart and pre-existing deletions remain intentionally
uncommitted over the unchanged historical baseline. M8 must not treat the
complete dirty-tree status as M8-owned. No remote/history action is authorized.

The sibling repositories are read-only for this milestone:

```text
asc-cmake HEAD: 8a7dcbad3a97267cce59810aff24de800a3497a7
asc-cmake state: clean main
asc-xde HEAD:   abcb29b51f22f40afd7f174707b7ccf83c32d4bf
asc-xde state:  clean skeletal main
```

## Approved implementation basis

- ASCCMake 0.1.0 is available at
  `/home/yicai/AI4SciComp/asc-cmake/build/test-debug`.
- The roadmap and ADR 0018 name M8 hardening, `0.9.x`, a full matrix, and an
  asc-xde trial.
- M7 reached local Checkpoint B with all existing product targets implemented.
- GCC 11.4, Clang 19, CMake 4.1.2, CUDA 12.9.86, Compute Sanitizer, and an
  NVIDIA compute-8.6 device are locally available.
- The actual asc-xde repository has no build system or production sources, so
  its trial is a committed asc-cpp-owned isolated downstream fixture; the
  sibling repository remains unchanged.

## Dependency and scope gate

No new product dependency or product target is approved. System inspection
tools may be used for local review but do not become build/install
dependencies. Any unavailable tool produces a truthful skipped evidence row.

No product C++ API change is planned. A proven defect may be corrected only by
the lead after independent evidence and review. DLPack, AD, distributed
adapters, new providers, legacy compatibility, 1.0 cleanup, publication, and
other-repository writes are excluded.
