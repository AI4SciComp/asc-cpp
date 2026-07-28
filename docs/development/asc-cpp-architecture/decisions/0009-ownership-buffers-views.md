# ADR 0009: move-only owners and element-const non-owning views

Status: Proposed at Architecture Checkpoint A

## Context

Historical types fused ownership, aliasing, views, synchronization, and
execution preference. Const owners could expose mutable aliases and manual
delete/mirroring complicated failure safety.

## Decision

- Core raw buffers are move-only, retain their memory resource/deleter, and
  release exactly once.
- Dense and sparse owners are move-only in v1.
- Deep copy is a named operation with explicit destination resource, execution
  context, synchronization, and failure result.
- Views are non-owning descriptors with element mutability in the element type,
  mapping/format metadata, accessible span, and memory space.
- A const owner yields only a const-element view. Mutable-to-const conversion
  is one-way.
- A captured view remains non-owning; owners outlive all view use and
  asynchronous completion.
- Resize/reallocation and sparse structural replacement invalidate views.
- External adoption requires an explicit space, resource/deleter, byte/span,
  alignment, and lifetime contract. A Boolean `own` flag is forbidden.
- Shared ownership is not a v1 default and requires a demonstrated need.
- Host dereference rejects non-host-accessible storage.

Dense owning mappings must be unique/exhaustive. Sparse finalized structural
arrays are immutable; values may be mutable under the view contract.

## Consequences

Copying becomes visible and may require caller changes. The type system
prevents shallow owner copying and const mutation but cannot prove that a
non-owning view has not dangled.

## Verification

Use compile traits, custom failure resources, exactly-once release counters,
partial construction rollback, move/clone tests, const-conversion negatives,
resize/structure invalidation, adopted-deleter lifetime, and ASan async/view
lifetime tests.
