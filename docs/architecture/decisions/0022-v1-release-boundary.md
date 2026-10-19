# ADR 0022: Version 1.0 release boundary

- Status: Accepted
- Date: 2026-10-19
- Owners: ASCCpp release approvers

## Decision

The owner-authorized 1.0.0 release retains the six-module C++20 architecture,
public declarations, numerical semantics, Random sequence versions, and
explicit CPU/CUDA boundary of 0.9.0, with the documented migration fixes.

Compiled libraries use VERSION 1.0.0 and SOVERSION 1.0. Package compatibility
remains SameMinorVersion. The documented public contract is preserved within
1.0.x patches; another minor or major requires explicit downstream review.
No cross-toolchain ABI compatibility is promised. Applications upgrading from
0.9.x must rebuild all translation units and shared-library boundaries.

The 0.9.0 ELF observations remain historical observations for SONAME 0.9 and
must not be selected as 1.0 baselines. New toolchain/version combinations
produce full symbol/SONAME observations without claiming an unmeasured digest.
Public-header hashes, ownership, component contracts, exported-symbol checks,
and exact-tag platform/package tests continue to apply.

CUDA remains experimental until the complete trusted exact-commit NVIDIA gate
passes. CPU Dense/Sparse BLAS remain correctness references without performance
claims. Publication requires the existing protected exact-tag release pipeline.

## Relationship to earlier decisions

This decision advances the owner-approved release boundary in ADR 0018 and
supersedes the 0.9.0 version/SONAME values in ADR 0021 for the 1.0.x line.
Earlier version evidence remains historical; it is not relabeled as new evidence.
