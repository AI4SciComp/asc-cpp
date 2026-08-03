# ADR 0008: explicit memory resources, contexts, events, and no fallback

Status: Proposed at Architecture Checkpoint A

## Context

Historical global device/memory state, mirrored validity, implicit transfer,
and process-wide execution selection are unsafe for concurrency and optional
providers. Vendor libraries use explicit devices, streams, handles, workspace,
and errors.

## Decision

Core defines explicit:

- `MemorySpace`: host, pinned host, device, managed;
- device identity and accessibility;
- one-space memory resources and move-only raw buffers;
- copy/transfer operations;
- execution contexts with backend, device, queue/stream-neutral state,
  determinism, workspace policy, resources, and capability table;
- move-only completion events/operation handles.

A serial synchronous context is always available. Provider factories create
immutable context state; there is no mutable global current device, provider
registry, or default context in v1.

The requested context is authoritative. Unsupported and unavailable are
different statuses. No operation silently transfers, packs, allocates,
densifies, synchronizes, changes precision, narrows, or falls back. `AUTO` is
not a v1 backend.

Events retain provider completion state and provider-owned workspace, not user
arrays. User storage, external streams, caller workspace, and required context
state outlive completion. Event destruction does not device-wide synchronize.

Provider SDK types remain in compiled provider facets. Native interoperability,
if required, uses an explicit provider header and separate stability contract.

## Consequences

Call sites are more explicit and testable. Async lifetimes become caller
responsibility. Managed memory remains a distinct space and does not authorize
implicit migration.

## Verification

Instrument allocation/copy/sync, test independent contexts and all lifetime
orders, reject incompatible space before pointer access, compare capability
query with invocation, and require real-hardware event/copy/runtime evidence
before a GPU claim.
