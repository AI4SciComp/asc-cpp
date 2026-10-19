# ASCCpp 1.0.0 release plan

- Preparation branch: `release/1.0.0`; `main` remains the release-only branch.
- Integration branch: `develop`.
- Version tag: `v1.0.0`, immutable once created.
- Release authorization: owner request on 2026-10-19.
- Historical 0.9.0 records and the annotated
  `snapshot/v0.9.0-candidate-20260802` tag remain historical evidence.

## Scope

Retain the complete six-module provider-free C++20 reference package and
Random Dense/Sparse facets. Include the migration compatibility and test fixes;
change no public signatures, numerical algorithms, or Random sequence versions.
CUDA remains experimental. Shared libraries use VERSION 1.0.0 and SOVERSION 1.0;
package compatibility remains SameMinorVersion.

## Publication gates

The release may become public only after all of the following are verified:

- The exact-tag CI compiler/platform/linkage matrix, sanitizers, static
  analysis, strict documentation, contracts, installed examples, and downstream
  trials pass without unexpected skips.
- CodeQL succeeds on the same commit.
- The pinned ASCCMake 0.1.0 archive is publicly downloadable and checksum valid.
- Private vulnerability reporting and branch/tag protection are enabled.
- Immutable source, CPU static/shared packages, documentation, SBOM, checksums,
  and attestations are generated and independently consumed from the draft.
- The owner-authorized publication workflow verifies the unchanged draft assets
  through the protected release-publication environment.

Use a new patch version for later fixes; never replace a published tag or asset.
