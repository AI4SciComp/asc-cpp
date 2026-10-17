# ASC array text, version 1

Status: frozen development wire contract for `ASC-CPP-LAPACK-IO` P00.
This document specifies bytes and required behavior; it does not assert that
the C++ readers or writers have been implemented or verified. New fields or
changed interpretations require a new format version and new fixtures.

## Ownership and scope

Dense and Sparse each interpret their own array schema. Core owns only the
storage-neutral source/sink, scalar codec and bounded token machinery. The
format needs neither LAPACK nor a Dense/Sparse sibling dependency. It records
logical values and declared storage kind, excluding padding, strides,
capacity, placement, resource identity, view relationships and factor state.

The exact wire scalar identities are below. Integers are base-ten numbers,
including character-sized integers; there are no character glyph values.
Native aliases map by signedness and exact number of value bits, excluding
padding bits: signed 8/16/32/64-bit types map to `i8/i16/i32/i64`, and unsigned
types to the corresponding `u` codes. Plain `char` follows its actual
signedness and width. `float` and `double` require IEEE binary32 and binary64
respectively. Only `std::complex<float>` and `std::complex<double>` map to
complex codes. Bool, extended precision, unsupported representations and
arbitrary user types are rejected; storage eligibility alone is insufficient.

| Code | Value |
| --- | --- |
| i8, u8 | signed, unsigned 8-bit integer |
| i16, u16 | signed, unsigned 16-bit integer |
| i32, u32 | signed, unsigned 32-bit integer |
| i64, u64 | signed, unsigned 64-bit integer |
| f32, f64 | IEEE binary32, binary64 real |
| c64, c128 | two binary32, two binary64 components; suffix is total bits |

Default native reads require identical wire scalar, kind, compile-time rank
and static extents. A reader never silently converts a scalar, storage kind or
rank. A separately named checked conversion is outside v1 required APIs.

## Grammar

All bytes are ASCII. Literal keywords are case-sensitive. The writer uses
LF; the reader accepts LF or CRLF independently at every line boundary. A
bare CR, NUL, byte-order mark, non-ASCII byte, tab, comment, empty record,
leading/trailing space or additional field is invalid within a frame. `SP`
below is exactly one ASCII space. `EOL` is LF or CRLF, and every record,
including `end`, requires an EOL. Repetitions are parameterized by validated
metadata; no record may contain extra tokens.

```text
frame       = "ASCARRAY 1" EOL
              "kind " kind EOL
              "scalar " scalar EOL
              "rank " unsigned EOL
              "shape" { SP unsigned }rank EOL
              "order " order EOL
              "count " unsigned EOL payload "end" EOL
kind        = "dense" | "coo" | "csr" | "csc"
order       = "dim0" | "coo" | "csr" | "csc"
scalar      = "i8" | "u8" | "i16" | "u16" | "i32" | "u32"
            | "i64" | "u64" | "f32" | "f64" | "c64" | "c128"
unsigned    = digit { digit }
integer     = [ "-" | "+" ] unsigned
finite-real = [ "-" | "+" ] ( unsigned [ "." [ unsigned ] ]
              | "." unsigned ) [ ( "e" | "E" ) integer ]
real        = finite-real | "inf" | "-inf" | "nan"
complex     = "(" real "," real ")"
coordinate  = "(" [ unsigned { "," unsigned }rank-1 ] ")" EOL
value-line  = ( integer | real | complex ) EOL
dense-data  = "data" EOL { value-line }count
coo-data    = "coordinates" EOL { coordinate }count
              "values" EOL { value-line }count
csr-data    = "offsets" EOL { unsigned EOL }rows+1
              "indices" EOL { unsigned EOL }count
              "values" EOL { value-line }count
csc-data    = "offsets" EOL { unsigned EOL }columns+1
              "indices" EOL { unsigned EOL }count
              "values" EOL { value-line }count
```

The payload production must match `kind`; `order` must be `dim0` for Dense
and exactly the kind name otherwise. A coordinate has exactly `rank`
components. Rank-zero coordinates are the explicit record `()`. The selected
scalar controls the value grammar; a real token is invalid for an integer,
and a real value cannot stand in for a complex pair. Unsigned scalar input
may have `+` but never `-`, even `-0`. Metadata has no sign. Leading zeros are
accepted, but canonical integer and metadata output has none except `0`.
Canonical integer output has no leading plus sign or negative zero.

Finite decimal input is correctly rounded to the declared component type
using round-to-nearest, ties-to-even. Nonzero overflow and underflow to zero
are range errors, not alternate spellings of infinity or zero. Subnormal
values are supported. Signed zero is preserved, including `-0` and `-0.0`.
Explicit infinities and `nan` are accepted only for floating components.
Text NaNs have no payload/sign identity promise. Hex floats, `infinity`,
`+inf`, `-nan`, and implementation-specific NaN syntax are invalid.

Canonical floating output uses locale-independent general decimal formatting
with 9 significant digits for binary32 and 17 for binary64, dropping trailing
fractional zeros and a vacated decimal point. Fixed notation is used when the
rounded decimal exponent is in `[-4, precision)`; otherwise scientific
notation uses lowercase `e`, an explicit exponent sign, and at least two
exponent digits. A zero is `0` or `-0`; specials are the three tokens above.
Complex output is `(real,imag)` with no internal spaces. All finite values
must round-trip exactly to their declared component type. Implementations
must verify their bounded `to_chars`/`from_chars` support; locale-sensitive
fallbacks are not allowed.

## Shape and canonical structure

All extents and structure integers must fit the nonnegative portion of ASC's
signed 64-bit index/extent domain, and all resource limits below. Rank must
fit `rank_t`. Validate every extent even when another extent is zero. A
shape containing zero has logical size zero; an empty shape has logical size
one. Otherwise its checked product must fit `extent_t` and the logical-size
budget. Dense count equals logical size. Sparse count is at most logical
size, preserves every explicitly stored zero, and never contains duplicates.

Dense values use dimension-zero-fastest logical order: for `(e0,e1,e2)`,
indices advance `(0,0,0),(1,0,0),...,(e0-1,0,0),(0,1,0),...`. This does not
depend on memory layout. Rank-zero Dense has exactly one value.

COO coordinates are ordered by the actual Sparse comparator in
[`coordinate.h`](../../include/asc/sparse/coordinate.h): compare dimension 0
first, then dimension 1, through dimension `rank-1`; the first unequal
component decides the order. Each tuple must be strictly greater than the
preceding tuple. For rank two this is row-major coordinate order, distinct
from Dense wire value order. Every coordinate component is less than its
extent. Rank-zero COO consequently has either zero or one stored entry.

CSR and CSC require rank two. CSR has `rows+1` offsets, then column indices
in row order. CSC has `columns+1` offsets, then row indices in column order.
Offsets start at zero, end at count and are nondecreasing within `[0,count]`.
Within each segment inner indices are strictly increasing and less than the
inner extent. Even zero stored count has the full outer-offset sequence.
Native input failing these invariants is rejected, never sorted, summed or
otherwise repaired. Matrix Market canonicalization is a separate contract.

## Required resource limits

`ArrayIoLimits` uses byte units except for explicitly named counts. Defaults
are deliberately bounded ASC policy, and can be raised explicitly subject to
checked arithmetic and representable host capacities.

| Field | Default | Meaning |
| --- | ---: | --- |
| max_input_bytes | 67,108,864 | bytes consumed for one frame, checksum included |
| max_output_bytes | 67,108,864 | bytes accepted by a writer, checksum included |
| max_header_bytes | 65,536 | through the `count` line, or binary header end |
| max_token_bytes | 256 | one numeric token; complex pair including punctuation |
| max_rank | 32 | metadata rank, before any rank-sized allocation |
| max_extent | 2,147,483,647 | each extent, including empty shapes |
| max_logical_elements | 16,777,216 | checked logical size |
| max_stored_elements | 16,777,216 | Sparse count |
| max_structure_bytes | 67,108,864 | decoded 64-bit structural arrays |
| max_decoded_bytes | 67,108,864 | structure plus typed value storage |
| max_staging_bytes | 67,108,864 | simultaneously live staging capacity |
| max_allocation_bytes | 134,217,728 | sum of requested allocation bytes per operation |
| max_allocations | 16 | total successful allocation requests per operation |
| max_scratch_bytes | 65,536 | supplied parser/metadata/sorting scratch capacity |

Zero caps are legal restrictive limits, not unlimited sentinels. Negative or
unrepresentable option values are invalid. Arithmetic must be checked before
allocation or access. Tokens and aggregate coordinate/header records must be
processed incrementally: a high-rank tuple does not get an unchecked
rank-sized stack buffer. Metadata buffers, parser scratch, staging, candidate
owners and Sparse builders all count against their applicable budgets. A
caller resource must serve all operation-owned memory and outlive returned
owners. No allocation failure is reported recoverable unless the actual
allocation path returns a failure status. Ordinary no-exceptions builds must
not use unchecked growing strings or containers for such allocations.

## Streaming, transactions and paths

ByteSource short reads are normal and zero progress is EOF. A framed reader
consumes exactly through the `end` line ending. If it buffers beyond the
frame, it owns and retains those bytes for its next frame; they must not be
discarded. Metadata preparation retains the parser/cursor state so a later
payload read does not expect the header again. After a malformed or failed
frame, the reader is failed until explicit reset onto a known frame boundary;
there is no automatic resynchronization or source rollback.

A path operation reads one object and then requires EOF after any ASCII
space, tab, LF or CRLF trailing whitespace. A second frame, bare CR or other
trailing byte is invalid. Trailing-file validation is part of the transaction.
A streaming read stops at its own frame; it does not test the next frame.
Failure reports retain consumed-byte count, section and a bounded diagnostic.
New array stream adapters preserve a failing `ByteSource`/`ByteSink`'s
`ErrorCode` and `native_code`, but discard owning message/provider strings
before existing helpers could copy them. Bounded section/progress and
transaction results remain available; unbounded callback text is not retained.
This closes diagnostic-copy allocation at these new borrowed stream
boundaries, not allocation inside user callbacks or existing owner-resource
creation/`File` paths. Existing Core `WriteAll`, `Status`, `Result` and `File`
behavior is unchanged.
For a nonseekable `ByteSource`, whole-file EOF validation requires one spare
byte of `max_input_bytes` budget before each probe, including the final probe
that returns zero bytes. An exhausted cap fails before the source is called:
otherwise a trailing byte could be consumed beyond the limit without a way to
put it back. Zero-byte EOF probes do not increment consumed-byte progress.
Thus a complete framed read can succeed at the exact cap, while a whole-file
read needs at least one spare byte after its permitted trailing whitespace.

New-owner loading validates type/rank/static extents before values allocation;
candidate resources are released on any error, and no partial owner is
published. Existing Dense destinations require caller-supplied disjoint value
staging and parse scratch. Validate shape, mapping uniqueness, reachable span,
placement, all capacities and aliasing before the commit. Parse and validate
the full frame into staging, then perform a prevalidated nonfailing host
copy/scatter. Every failure before commit leaves every destination value and
padding byte unchanged. Repeated fallible `At()` calls during commit do not
meet this requirement.

Sparse structure loading returns a new owner. Values-only loading into an
existing Sparse object validates exact kind, shape, and every coordinate or
offset/index against immutable destination structure, stages values, then
commits through prevalidated writes. Matching hashes cannot replace exact
comparison. No finalized structural buffer is modified.

Dense value operations permit host and pinned-host placement where the Dense
view contract permits direct host access. Sparse value/structure operations
require host placement. Device, managed or otherwise unsupported placement
is rejected before reading values or beginning destination mutation. There
are no implicit transfers, expression evaluations or synchronizations.

Writes use an explicit caller sink and bounded scratch, retry short writes
through `WriteAll`, and fail on zero progress. The sink may retain a prefix on
failure; there is no sink transaction. The production headers use explicit
`WriteDenseArrayText`/`WriteDenseArrayBinary` and
`WriteSparseArrayText`/`WriteSparseArrayBinary`. Prepared readers feed
`ReadDenseArray`/`ReadSparseArray` or staged `Into` operations. Source wrappers
have explicit `Text`/`Binary` suffixes; path conveniences use `Load`/`Save`.
Only declarations in installed owning-component headers are supported API.

Path wrappers compose the existing Core `File`. `File::OpenWrite` opens `wb`
and truncates; a writer must require explicit destructive overwrite intent.
There is no default overwrite, racy exists-then-open no-clobber promise,
implicit atomic replacement or crash-durability claim. Success requires
checked write, flush and explicit close. Preserve the primary error if
cleanup also fails. Exclusive create and atomic replace are separate optional
Core operations requiring their own platform tests.

## Independent fixtures

[`dense-f64.asc`](../../tests/array_io/fixtures/dense-f64.asc) represents
conventional rows `[1,2,3]` and `[4,5,6]` with wire values `1,4,2,5,3,6`.
[`coo-rank-zero.asc`](../../tests/array_io/fixtures/coo-rank-zero.asc)
preserves one explicitly stored rank-zero integer zero. The independent
Python fixture codec is development tooling, not an installed runtime parser
or proof of C++ implementation coverage.
