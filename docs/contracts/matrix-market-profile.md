# ASC Matrix Market interchange profile

Status: frozen development format contract for `ASC-CPP-LAPACK-IO` P00/P10.
Implementation and interoperability evidence are tracked separately. This
profile uses the authoritative [NIST format description](https://math.nist.gov/MatrixMarket/formats.html),
retrieved 2026-09-07. ASC-specific limits and conversion policies below are
design choices, not claims that NIST prescribes them.

## Supported combinations

Only rank-two `matrix` objects are in scope. Native Dense interchange uses
`array`; native Sparse COO/CSR/CSC interchange uses `coordinate`, without
densification. Optional cross-representation reads require an explicitly
chosen destination and full decoded-size budget; they are not implied by
these required paths or by a filename extension.

| Representation / field | general | symmetric | skew-symmetric | hermitian |
| --- | --- | --- | --- | --- |
| array / integer | yes | yes | yes | invalid |
| array / real | yes | yes | yes | invalid |
| array / complex | yes | yes | yes | yes |
| array / pattern | invalid | invalid | invalid | invalid |
| coordinate / integer | yes | yes | yes | invalid |
| coordinate / real | yes | yes | yes | invalid |
| coordinate / complex | yes | yes | yes | yes |
| coordinate / pattern | yes | yes | invalid | invalid |

Symmetric, skew-symmetric and Hermitian forms require square dimensions.
Symmetric complex values mirror without conjugation. Hermitian values mirror
with conjugation and have exactly real diagonals (either sign of imaginary
zero is accepted). Skew-symmetric entries mirror by negation and the diagonal
is omitted, not encoded as explicit zero. A coordinate record on or above
the diagonal is invalid for skew-symmetric; above the diagonal is invalid
for symmetric/Hermitian. This prevents mirrored input from being counted
twice. Pattern skew/Hermitian have no value field with which to express the
required sign/conjugation semantics and are invalid combinations.

## Lexical and record grammar

Files are ASCII, with LF or CRLF records and no bare CR or NUL. The banner
must be the first record, with exact `%%MatrixMarket` and five whitespace-
separated fields:

```text
%%MatrixMarket matrix representation field symmetry
```

Readers accept ASCII case-insensitive object/representation/field/symmetry
words; writers emit the spelling in the compatibility table. Only spaces and
tabs separate fields and may precede/follow a record. Subsequent blank
records and whole-record comments whose first nonblank byte is `%` are
ignored before, between and after data records. Comments are not embedded in
value records. Each line is limited by `max_header_bytes`, including its line
ending; all bytes count against `max_input_bytes`. The last record may end
at EOF without a newline. There are no locale separators, ellipses or
executable expressions.

An array dimension record contains exactly two nonnegative decimal integers
`rows columns`. A coordinate dimension record adds a nonnegative record
count `rows columns records`. Dimensions, counts and structure bytes must fit
ASC and configured limits before any allocation. Zero dimensions/count are
accepted with their exact empty record count. No signs are accepted for
dimensions or coordinate indices.

Array records contain one integer/real token, or two real tokens for complex
real and imaginary components. Coordinate records contain one-based row and
column indices followed by zero tokens for pattern, one integer/real token,
or two real tokens for complex. Each record has exactly its required fields.
Integer and finite-real token syntax is that of
[ASC text](array-text-v1.md), with whitespace-separated complex components
instead of parentheses. For interoperability this profile accepts only
finite numeric input/output; NaN/infinity are rejected with a range status.
Each token is bounded by `max_token_bytes`.

Array general values are column-oriented: outer loop column, inner loop row.
Symmetric/Hermitian arrays contain lower triangle including diagonal in the
same column traversal: `(0,0),(1,0),...,(n-1,0),(1,1),...`. Their exact record
count is `n*(n+1)/2`. Skew-symmetric arrays contain strict lower triangle and
have `n*(n-1)/2` records; initialize the diagonal explicitly to zero. Compute
these formulas with checked arithmetic (divide an even factor first).
Coordinate data may arrive unsorted and has exactly the declared record
count. Validate one-based indices in `[1,extent]` before subtracting one.
EOF after comments/whitespace is mandatory for a single-object path read.
Extra data, fields or a second banner are errors before destination commit.

## Destination scalar and conversion

The caller chooses the exact destination type: Matrix Market contains no
f32/f64 precision code. Real and complex decimal components round directly to
the chosen IEEE component type, ties-to-even, rejecting nonfinite overflow
and nonzero underflow to zero. Complex fields require a complex destination;
imaginary components are never silently discarded. Real fields permit real
or complex destinations, with explicit positive-zero imaginary components.

Integer fields parse through checked integer magnitude, never through double.
Integer destinations require representability by sign/range. Real or complex
destinations require each input integer to be exactly representable in the
component type, tested before conversion; no large-integer rounding is
implicit. Real/complex fields do not implicitly convert into integer types.
Pattern requires an explicit unit-value policy selecting exact scalar one
(complex `1+0i`) or a separately implemented pattern-only destination.
The absence of a pattern policy is an error, not a default value.

## Assembly, duplicates, zeros and transactions

The default duplicate policy is reject and explicit-zero policy is preserve;
the API uses existing `DuplicatePolicy::kReject` and
`ExplicitZeroPolicy::kKeep`. A caller can explicitly request checked summation
or zero dropping. Account for at most twice the input coordinate count before
expansion; tighter exact counts can be obtained while parsing staging.

First validate lower-half/diagonal constraints; retain original record order;
group repeated source coordinates in that stable order. Reject duplicates or
sum left-to-right in destination scalar precision, checking each integer
addition and each floating/complex component result for overflow/nonfinite.
Then mirror each grouped off-diagonal entry exactly once. Negation must be
representable (in particular an unsigned or minimum signed integer can make
skew expansion fail). Drop exact zeros only after summation and mirroring if
requested. A complex zero requires both components zero; either signed zero
counts as zero. Preserve keeps explicitly stored zeros and their mirrors.

Canonicalize to the chosen destination kind using explicit builder/resource
or supplied sorting workspace. Report required scratch and sorting cost;
bounded in-place insertion sort has quadratic worst-case comparison cost and
must not be advertised as linear. Stable summation order is independent of
destination COO/CSR/CSC traversal order. Matrix Market never silently repairs
native ASC archive structure.

All resource, host-placement, source-progress and staged destination rollback
rules of [ASC text](array-text-v1.md) apply. A Dense coordinate destination
requires explicit full-size allocation and zero initialization; a Sparse
array destination requires explicit zero policy and conversion budget. These
optional conversions must not introduce a Dense/Sparse include/link edge.
Default readers publish only after complete parsing, expansion, checked
summation, canonicalization and applicable EOF checks. Sparse values-only
loads still compare every exact coordinate with the target before commit.

## Writing and independent fixtures

Writers emit canonical banners, decimal indices converted by checked +1,
and values with sufficient round-trip digits as in the native text contract.
They emit no comments by default. Every coordinate's declared count equals
the number of actual records. COO uses its existing dimension-zero-first
lexicographic traversal; CSR uses rows, CSC uses columns. Matrix Market
permits these different coordinate record orders. Values are never expanded
to dense storage for writing.

General output describes the actual matrix. Symmetry-compressed output must
validate the complete represented matrix, including omitted partners and
diagonals, with exact equality/conjugation/negation. It may instead accept a
separately documented typed structured descriptor interpreting one stored
triangle. An ordinary view plus a header flag alone does not authorize
discarding incompatible values. Sparse missing partners represent zero;
stored zero policy remains explicit. Writers include each diagonal once and
only the required lower off-diagonal records. The output byte budget and
sink/path failure/overwrite rules remain explicit.

With explicit zero preservation, symmetry-compressed Sparse output preserves
represented values rather than every asymmetric stored-zero pattern. An
upper-only stored zero produces a lower zero representative; re-expansion may
therefore add its stored mirror. A skew diagonal zero must be omitted. The
writer reports synthesized lower representatives and omitted diagonal zeros.
General output preserves stored entries subject to the explicit zero policy.
Native ASC archives remain the exact-structure archival format.

The independently derived
[`hermitian.mtx`](../../tests/array_io/fixtures/hermitian.mtx) represents rows
`(4,2-2i)` and `(2+2i,11)`. Its lower entry is `2+2i`; conjugation produces
the upper entry. Tests must additionally cover each valid field/symmetry
class, each invalid combination, integer exactness, duplicates, pattern
policy, comments/CRLF, empty sizes, malformed indices/counts, overflow and
staged rollback. An optional established external codec adds interoperability
evidence, but cannot replace independent fixtures or become a runtime
dependency.
