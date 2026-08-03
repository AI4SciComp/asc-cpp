# ASCCpp 0.9.0 release plan

## Identities

- Audited and live base: `402cbac35334bb2a20e7e6afa7214efb8fad1c8f`
- Snapshot: annotated tag `snapshot/v0.9.0-candidate-20260802`
- Integration branch: `develop`, created from the base without unique work
- Implementation branch: `release/0.9.0`, created locally from the base
- Planned release tag: `v0.9.0` (not created)

## Supported profile

Version 0.9.0 is a portable C++20 provider-free reference release. CPU Core,
Utilities, Expression, Dense, Sparse, Random, and Random Dense/Sparse storage
facets are supported on platforms that pass the exact release matrix. CPU
Dense BLAS and Sparse BLAS are correctness-reference implementations, not
performance-optimized providers.

CUDA sources and package components are retained but experimental. They are
outside the supported matrix unless the full trusted real-NVIDIA gate passes
on the exact release commit.

Compiled libraries use `VERSION 0.9.0` and `SOVERSION 0.9`; see ADR 0021.

## External publication gates

The release may become public only after all of the following are verified:

- `AI4SciComp/asc-cmake` is public and its immutable `v0.1.0` tag still peels
  to `8a7dcbad3a97267cce59810aff24de800a3497a7`;
- the pinned source download still has SHA-256
  `67765391bef06c6c9a1a0c43e934d0db7a9c52876e66e0cf644042eeb0c2a5c9`;
- a private vulnerability-reporting route and required branch/tag protection
  are configured and verified; and
- the `release-publication` GitHub environment requires an approving owner;
- the release draft's downloaded assets pass the independent consumer job;
- every required hosted job passes on the exact proposed release commit.

Until then, release artifacts are internal validation artifacts and must not be
published as a public supported release.

## Phase sequence

1. Correct contributor, governance, security, support, CUDA, and ABI policy.
2. Relocate durable contracts/ADRs/provenance and archive temporary traces.
3. Add strict public Doxygen documentation and installed-package examples.
4. Convert status/version wording and generated contracts to release state.
5. Add secret-free CI, complete platform/linkage jobs, and release automation.
6. Run local validation and stop at Gate B before committing or pushing the
   implementation branch.

No roadmap feature, numerical change, public API redesign, merge, release tag,
draft release, or publication is part of this plan before its later gate.
