# ADR 0006: storage-independent core I/O and owner-specific formats

Status: Proposed at Architecture Checkpoint A

## Context

Historical numerical I/O wrote native representations and sometimes used
source-tree binary data. The restart needs explicit ownership, portable
encoding, and no implicit device transfer.

## Decision

Core owns:

- RAII local file handles;
- byte/text source and sink contracts;
- `ReadSome`, `ReadExact`, `WriteSome`, and `WriteAll` semantics;
- EOF, short operation, permission, path, size, and overflow errors;
- fixed-width little-endian integer and IEC 60559 float encoding helpers;
- magic, schema version, length, and endianness validation.

Dense owns dense shape/layout/value formats. Sparse owns format/index/structure/
value formats. Random owns engine/state formats. Each schema documents version,
limits, compatibility, and checksums where generated data is involved.

Serialization never infers native object layout and never transfers device
data. Callers explicitly copy to host or use a later provider-specific I/O
facet. Utilities may own concrete configuration-file parsing but not general
numerical serialization. General logging is not a v1 core responsibility.

## Consequences

Portable schemas add encoding work and may be slower than raw dumps. This is
preferred to nonportable or provenance-opaque files. Serialized-state
compatibility is separate from library ABI.

## Verification

Test short reads/writes, EOF, permission/failure injection, endian simulation,
unknown version, malicious sizes/overflow, truncation, round trips, no device
copy, and golden files generated from documented schemas.
