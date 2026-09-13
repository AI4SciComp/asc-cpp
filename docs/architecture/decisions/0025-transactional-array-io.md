# ADR 0025: bounded printing and transactional array persistence

Status: Accepted implementation direction under the user-approved
ASC-CPP-LAPACK-IO runbook, 2026-09-07. Wire fixtures and production execution
evidence are separate gates.

Core owns storage-neutral scalar/byte codecs. Dense and Sparse independently
own their array grammars and output traversal. There is no new I/O module,
shared array hierarchy, runtime-rank owner or Dense/Sparse sibling edge.
The established File API and filesystem usage remain compatible.

Printing is bounded, human-facing value output through an explicit sink and
caller scratch. It is independent of persistence. Dense traversal follows
logical coordinates; Sparse prints stored coordinates/values without
densification. Unsupported placement is rejected before value access.
Successful truncated previews visibly report truncation.

ASC text v1 and binary v1 have exact scalar identity, canonical Sparse
structure and explicit limits. Dense uses dimension-zero-fastest wire order;
COO uses dimension-zero-first tuple lexicographic structure order. Binary
encodes fields/scalar components and CRC explicitly, never native structs.
See the normative format contracts in docs/contracts.

New-owner loads publish only after complete validation. Existing Dense
loads require disjoint staging and a prevalidated nonfailing commit.
Sparse values-only loads compare exact canonical structure before mutation.
Source rollback is not promised; buffered over-read remains reader-owned.
Writes require caller sinks or explicit destructive-overwrite intent and
checked close, with no generic stream atomicity claim.

Matrix Market rank-two interchange has explicit field/symmetry, one-based
coordinate, duplicate, zero and pattern-value policies. It cannot silently
repair a malformed native archive.

Verification includes independent byte fixtures; every-byte truncation;
source/sink/resource failures; full destination/padding rollback; all scalar,
rank, layout and storage cases; installed isolation; bounded fuzz regression;
and no hidden allocation, transfer or densification.
