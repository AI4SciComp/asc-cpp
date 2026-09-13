# ASC bounded array display, version 1

Status: frozen development printer contract for `ASC-CPP-LAPACK-IO` P00/P02.
Display is human-facing and is not a persistence grammar. Native text and
binary archives have separate complete-value contracts.

## API and limits

`PrintArray` takes a supported Dense owner/const view or finalized Sparse
COO/CSR/CSC owner/view, an explicit `ByteSink`, options, caller-provided
bounded byte scratch and a mandatory `ArrayPrintReport`. It returns `Status`.
No global stdout selection, expression evaluation, hidden allocation,
transfer, packing, provider selection or synchronization occurs.

`ArrayPrintOptions` has these defaults: `max_elements=64`, `max_rows=8`,
`max_columns=8`, `max_slices=4`, `max_output_bytes=16384`, `precision=6`,
`float_format=general`, `edge_preview=true`, `show_metadata=true`.
Sizes are counts except output bytes. Zero size limits mean zero, never
unlimited. Precision must be in `[1,32]`; invalid enum values or unrepresentable
limits fail before output. General precision counts significant digits;
fixed/scientific precision counts digits following the decimal point.
Report fields `values_displayed`, `output_bytes`, `truncated` are initialized
to zero/false before validation and retain completed progress on failure.
`output_bytes` counts bytes actually accepted by the sink, including a short
write before failure; values count fully emitted value tokens.

## Value spelling and layout

All output is ASCII with LF and no locale effects. Numeric integers include
character-sized values, never glyphs. General output follows the finite
formatting rule in [ASC text](array-text-v1.md) at the selected precision.
Fixed/scientific keep the requested fractional digits; scientific uses
lowercase `e`, exponent sign and at least two exponent digits. Sign of zero
survives, while infinities and NaN use `inf`, `-inf`, `nan`. Complex values
are `(real,imag)` with no space inside the pair. NaN payloads are not printed.

The optional metadata line is
`dense scalar=f64 shape=(2,3) count=6` or
`csr scalar=i32 shape=(2,3) stored=2`, followed by LF. Shape tuples contain
comma-separated decimal extents with no spaces, with `()` for rank zero.
Kind/scalar codes are the native format codes. Unsupported native wire
scalar representations fail rather than mislabel values.

Dense rank zero emits its scalar token plus LF. Rank one uses `[1, 2, 3]`
plus LF. Rank two uses conventional rows, irrespective of physical layout:

```text
[[1, 2, 3],
 [4, 5, 6]]
```

Higher rank uses axes 0 and 1 as displayed rows and columns. Fixed slice
coordinates are labeled `slice axes=(0,1) fixed=(2:0,3:1)` and LF; list
dimensions 2 upward in order. Slice enumeration is dimension-2-fastest.
Each selected slice uses the rank-two form, including its final LF. A zero
extent causes `empty shape=(...)` plus LF and no element reads; this shape
line remains present even when metadata is suppressed.

Sparse emits one coordinate/value line per selected stored entry:
`(0,2) = 7`, or `() = 0` at rank zero. Coordinates are zero-based. COO
traversal is dimension-zero-first lexicographic; CSR traverses rows then
inner columns; CSC traverses columns then inner rows. Stored zeros are
displayed; absent entries are never materialized. An empty Sparse owner
emits `[]` plus LF after optional metadata.

## Preview selection and visible truncation

For an axis or sequence of length n with limit m, if n<=m select all. When
n>m, edge preview selects the first `ceil(m/2)` and final `floor(m/2)` indices
in ascending order without overlap; prefix preview selects the first m.
Ellipsis `...` appears at each omitted run, including an omitted tail in
prefix mode. Rank-one values use `max_elements`. Rank-two/higher rank apply
row, column and slice limits independently, then emit selected values in
display order until the total `max_elements` limit is reached. Sparse applies
`max_elements` to stored traversal; row/column/slice limits do not suppress
Sparse coordinate entries. No selected numeric token is read more than once
unless an explicitly documented bounded column-width pass is selected.
The default has no alignment pass and no padding between numeric tokens.

Any omitted value/row/column/slice makes `truncated=true`. Every successful
truncated result ends with the separate line `... (truncated)` and LF in
addition to any in-bracket ellipses. Zero preview limits on a nonempty array
therefore still produce a visible truncated result without reading values.
Do not label an empty array truncated merely because its extent is zero.

The printer reserves enough byte budget for balanced delimiters, newlines
and the truncation marker before emitting a preview token. If a next token
would exceed the budget, finish a balanced truncated preview and its marker.
If the budget cannot hold even the applicable header/shape label, minimum
balanced preview and required marker, return a size/limit failure. It may
already have emitted a prefix, but cannot return success as though a complete
display had been produced. A token exceeding supplied scratch capacity is a
resource failure; arbitrary-size temporary strings are not permitted.

## Cost, placement and failure

Dense traversal costs rank metadata plus selected values; a bounded preview
must not scan the whole tensor or its padding to determine widths. For
compressed Sparse entries, locate outer coordinates using offset binary
search or a bounded traversal selected explicitly. Random selected positions
can cost O(log(outer_extent)) each; do not promise O(displayed entries) for
all compressed storage. In particular, huge empty row ranges should not be
scanned linearly just to display a few entries.

Dense host/pinned-host placement permitted by direct view access is supported;
Sparse requires host placement. Device/managed/unsupported placement is
rejected before value reads. Callers own sink, backing arrays and scratch for
the entire synchronous call and serialize concurrent writes to the same
sink. Scratch must not overlap any input value or structural storage.

Use incremental encoding and `WriteAll` short-write behavior; a zero-progress
sink is failure. A sink may retain a prefix on any failure and cannot be
rolled back generically. A borrowed file/stdout-handle adapter, if introduced,
is Core-owned and never closes the borrowed handle. Optional formatting into
an owned string is not required and cannot hide unchecked allocation in a
no-exceptions build. `operator<<` is not part of the required API.

At the new array-printer stream boundary, a failing caller `ByteSink` retains
its `ErrorCode` and `native_code`, but its owning message/provider strings are
discarded. Copying arbitrary callback diagnostics would introduce hidden
allocations; the report retains the bounded accepted-byte progress instead.
The caller's callback may itself allocate; the printer does not promise to
control that implementation. This policy does not modify existing Core
`Status`, `Result`, `WriteAll` or `File` semantics.
