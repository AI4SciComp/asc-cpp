# ASC array binary, version 1.0

Status: frozen development wire contract for `ASC-CPP-LAPACK-IO` P00.
This specifies a new format, not an existing implemented capability. The
[text contract](array-text-v1.md) governs scalar identity, shape/canonical
Sparse structure, resource defaults, placement, ownership, staging, paths
and source failure semantics unless this document explicitly differs.

## Envelope

Every integer field is unsigned little-endian unless stated otherwise. The
header is field-encoded, never a native C++ structure image. Exactly one
payload and one checksum follow the header; there are no alignment gaps.

| Offset | Width | Meaning |
| ---: | ---: | --- |
| 0 | 8 | ASCII `ASCARRB` followed by LF, bytes `41 53 43 41 52 52 42 0a` |
| 8 | 2 | major = 1 |
| 10 | 2 | minor = 0 |
| 12 | 1 | kind: 1 Dense, 2 COO, 3 CSR, 4 CSC |
| 13 | 1 | scalar code from table below |
| 14 | 2 | flags = 0 |
| 16 | 4 | rank |
| 20 | 4 | reserved = 0 |
| 24 | 8 | count: Dense logical size or Sparse stored entries |
| 32 | 8 | structure integer count |
| 40 | 8 | total payload bytes |
| 48 | 8 | header bytes = `56 + 8*rank` exactly |
| 56 | `8*rank` | extents in dimension order, each u64 |
| header end | payload bytes | structure integers, then values |
| payload end | 4 | CRC-32 of all preceding frame bytes, little-endian |

Reject unknown versions (including nonzero minor), kind/scalar codes,
nonzero flags/reserved fields, inconsistent header/payload lengths, and
truncated or extra content according to the source framing rules. Check rank
and all bounded arithmetic before allocating or requesting the extents.
Metadata does not become trusted merely because its CRC matches.

| Code | Scalar | Bytes per value |
| ---: | --- | ---: |
| 1 | i8 | 1 |
| 2 | u8 | 1 |
| 3 | i16 | 2 |
| 4 | u16 | 2 |
| 5 | i32 | 4 |
| 6 | u32 | 4 |
| 7 | i64 | 8 |
| 8 | u64 | 8 |
| 9 | f32 | 4 |
| 10 | f64 | 8 |
| 11 | c64 | 8 |
| 12 | c128 | 16 |

Signed scalar integers have fixed-width two's-complement encoding. An i8
value of -1 is `ff`, and an i16 value of -2 is `fe ff`. Unsigned values use
ordinary base-two magnitude. Real values are exact IEEE binary32/binary64
bits, least-significant byte first. Complex values are a real component
followed immediately by an imaginary component, each independently encoded;
no assumption about native complex interleaving/padding is permitted.

## Payload and bounds

All structure integers are u64 on wire and must fit ASC's signed 64-bit
index/extent domain. Dense has no structure integers and stores logical
values in dimension-zero-fastest order. COO stores `count*rank` coordinate
components as consecutive tuples, then count values. Rank-zero COO has no
coordinate bytes but still permits only zero or one stored value.

CSR stores `rows+1` outer offsets followed by count column indices and then
count values; its structure count is `(rows+1)+count`. CSC substitutes
`columns+1` offsets and row indices. Both require rank two. Structure has
exactly the canonical ordering and invariants of the text format. Stored
zeros, including signed floating zeros, survive unchanged.

The payload byte count must equal
`8*structure_integer_count + scalar_width*count`, computed with checked
arithmetic and all resource budgets before allocations or value access.
The whole frame length is `header_bytes + payload_bytes + 4`, likewise
checked. Even empty values require complete metadata and any required
compressed offsets. There is no implementation-defined native padding.

## Checksum

The checksum is reflected CRC-32 with polynomial `0xedb88320`, initial
state `0xffffffff`, and final XOR `0xffffffff`. For each byte in stream
order, XOR the byte into the low eight state bits, then perform eight
updates: if the low bit is one, replace state with `(state >> 1) XOR
0xedb88320`; otherwise replace it with `state >> 1`. Keep 32 state bits.
Apply the final XOR once after the last payload byte. The checksum field
itself is excluded. The check value for ASCII `123456789` is `0xcbf43926`
and its serialized bytes are `26 39 f4 cb`.

This detects accidental corruption; it does not authenticate the sender.
Readers validate both envelope/structure and checksum before publishing or
committing a destination. Core little-endian scalar primitives must be reused
where their actual semantics match these fields. CRC validation is not a
reason to postpone resource limits or structural safety checks.

## Bit identity and framing

Supported IEEE paths preserve all finite bits, infinities, signed zeros and
quiet-NaN component bits using bit-preserving copies without arithmetic. Test
host scalar representation and C++20 complex component access/lifecycle
explicitly. Signaling-NaN transport is outside the v1 C++ support guarantee:
platform loading, component extraction or construction can quiet a signaling
NaN. A tool that manipulates opaque scalar bytes can retain more bits than
the C++ scalar support claim; this does not extend that claim.

A framed reader consumes exactly header, payload and four checksum bytes.
Adjacent binary frames are allowed through the same reader. Metadata-only
preparation retains its cursor and bounded metadata storage. Failed readers
require explicit reset at a known boundary, with no implicit resynchronizing
scan. A path read requires exact EOF immediately after its checksum; binary
has no ignorable trailing whitespace. These differ from text path reads.

All default Dense `Into` and Sparse values-only loads use the text
transaction guarantee, including checksum and path-EOF validation before
the nonfailing commit. File writes require explicit overwrite intent and
checked close as documented there.

The hand-derived fixture
[`dense-i16.hex`](../../tests/array_io/fixtures/dense-i16.hex) encodes Dense
shape `(2)` and values `-2, 513`, with payload `fe ff 01 02`. The development
reference codec checks exact bytes and CRC independently of production C++.
