# ADR 0021: Version and SONAME policy

- Status: Accepted
- Date: 2026-08-03
- Owners: ASCCpp release approvers

## Context

ASCCpp is releasing version `0.9.0`. The project does not promise binary
compatibility across different `0.x` minor versions, but patch releases within
the `0.9.x` line must remain consumable without silently changing the loader
identity.

## Decision

Every compiled ASCCpp library has CMake `VERSION 0.9.0` and `SOVERSION 0.9`.
Interface-only component targets have neither property. The package version
file retains `SameMinorVersion` compatibility.

Changing the minor version may change the SONAME after an explicit ABI review.
Patch releases must preserve the `0.9` ABI or document and test an approved
exception before publication. No compatibility is promised across `0.x`
minor versions.

## Consequences

- Shared objects expose a nonzero, pre-1.0 loader identity.
- ABI baselines and installed consumers verify the chosen policy.
- The policy does not imply source or binary compatibility with `0.10.x`.
