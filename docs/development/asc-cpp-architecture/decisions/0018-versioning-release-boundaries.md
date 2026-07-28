# ADR 0018: milestone-gated pre-1.0 compatibility and release evidence

Status: Proposed at Architecture Checkpoint A

## Context

The repository is a clean restart with no API. Stability claims must grow with
implemented components and provider evidence rather than historical version
claims.

## Decision

- Use semantic versioning.
- Milestones and releases are separate approvals.
- Milestone 0 has no library release and exports no production target.
- Earliest lines are: core `0.1.x`; independent foundations `0.2.x`; dense
  `0.3.x`; sparse `0.4.x`; random facets `0.5.x`; core/dense CUDA `0.6.x`;
  sparse/random CUDA `0.7.x`; hardening candidate `0.9.x`.
- During 0.x, minor versions may break source/ABI with migration notes. Patch
  versions preserve documented public contracts.
- 0.x CMake package compatibility is `SameMinorVersion`.
- Component availability is versioned; absent components are not stubbed.
- ABI, source, numerical reproducibility, provider determinism, file schema,
  and serialized random state are distinct compatibility claims.
- A provider appears in release notes only with configure, compile,
  real-hardware runtime, and parity evidence appropriate to the claim.
- v1.0 requires the complete six-module/facet package, downstream asc-xde
  trial, support matrix, provenance/security/API/ABI/performance review, and
  explicit owner approval.
- Never move a published tag. Fixes use new patch releases.

No compatibility facade for the deleted `array`/`linalg` API is planned.

## Consequences

Downstream users must pin pre-1.0 minors. Early releases can remain honest and
narrow. Serialized data may require migration even when ABI is compatible.

## Verification

Check versions/config files, component manifests, API/ABI reports, schema
goldens, release notes with exact environments/counts/skips, source archives,
installed consumers, and explicit publication authorization.
