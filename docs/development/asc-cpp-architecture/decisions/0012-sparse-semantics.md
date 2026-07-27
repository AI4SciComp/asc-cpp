# ADR 0012: coordinate and compressed sparse invariants

Status: Proposed at Architecture Checkpoint A

## Context

Historical sparse storage combined assembly, finalization, compressed formats,
views, mutation, conversion, and dense-order expression behavior. It used
shared dense/array storage and often normalized through COO.

## Decision

Sparse independently owns:

- a compile-time-rank coordinate builder and finalized coordinate owner/view;
- rank-two CSR and CSC owners/views;
- conversion, sparse expression evaluation, and sparse algebra.

Canonical rules:

- zero-based signed 64-bit ASC indices by default;
- builders may accept unsorted entries and duplicates;
- finalization requires explicit `DuplicatePolicy` and
  `ExplicitZeroPolicy`; neither has a silent default;
- canonical traversal is lexicographically sorted and unique;
- duplicate summation, when requested, follows a documented stable order;
- finalized structure is immutable; values may be mutable;
- empty CSR/CSC owns `outer_extent + 1` zero offsets;
- structure replacement invalidates views;
- conversion is named, allocation/workspace-bearing, and context-taking;
- no routine hidden COO round-trip.

General-rank compressed formats are not advertised. BSR, SELL, distributed
formats, and arbitrary structural mutation are deferred.

Every expression operation declares its sparsity effect. Sparse evaluation
rejects a densifying expression and never creates a dense temporary/result.
Structure-changing work requires an explicit builder/destination and workspace.

## Consequences

Callers must select duplicate/zero semantics and result storage. Structural
immutability simplifies provider validation and concurrency. Floating duplicate
summation remains order-sensitive but reproducible under the documented order.

## Verification

Test invalid offsets/indices/base/width, unsorted/duplicate/zero/NaN policies,
empty invariants, canonical order, COO/CSR/CSC round trips, view invalidation,
allocation complexity, and no-densification under strict resources.
