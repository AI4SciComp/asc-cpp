# Matrix Market interchange

Matrix Market is a rank-two interchange format, separate from bounded previews
and exact ASC text/binary archives. Dense owns `array` interchange; Sparse owns
`coordinate` interchange for COO, CSR and CSC. Neither imports the other storage
component, LAPACK, CUDA or Random. Core supplies shared bounded parsing and scalar
formatting. The normative [ASC profile](contracts/matrix-market-profile.md)
defines accepted combinations and exact lexical/conversion rules.

## Choose a destination and explicit storage

Include `asc/dense/matrix_market.h` and link `ASC::dense`, or include
`asc/sparse/matrix_market.h` and link `ASC::sparse`. The owning `Read*MatrixMarket`
functions require a memory resource and caller byte scratch. Prepared readers
separate header inspection from payload allocation. `Read*MatrixMarketInto`
requires disjoint typed staging and publishes only after the entire object,
including trailing comments and EOF, has been validated. Sparse values-only
reads require exactly the destination's canonical coordinates; they never
replace its structure or densify it.

`Load*MatrixMarket` and `Save*MatrixMarket` are filesystem conveniences. Saving
requires an explicit `ArrayFileOverwrite::kTruncate`; an existing file can be
truncated before a later failure. Byte sinks can retain partial output on I/O
failure. Successful saves check flush and close, not atomic replacement or
crash durability. No filename extension selects a codec automatically.

`ArrayIoLimits` independently bounds input/output bytes, per-line and token
bytes, shape, stored counts, resource storage, scratch and staging. EOF probing
requires one spare input-budget byte beyond the file's actual bytes. Scratch
and complete reachable source, destination, report and staging spans must obey
the public disjointness and lifetime contracts. No bounded codec operation
silently obtains replacement workspace or transfers device data.

Dense payload preflight/allocation rejection leaves its prepared cursor usable;
an attempted payload consumes it, even on failure. Any Sparse payload attempt
consumes its cursor, including preflight rejection. Destination rollback does
not rewind a source, clear staging, or make a consumed cursor reusable.

## Numeric and structural interpretation

Matrix Market does not encode destination precision. The caller chooses the
exact scalar type. Integer input is range-checked without first converting to
double; integer-to-floating conversion requires exact representability. Complex
input is never silently projected onto real values. Only finite numbers are
accepted. Nonzero underflow to zero and nonfinite overflow are errors.

Symmetric/Hermitian data contain the lower triangle; Hermitian expansion
conjugates off-diagonal values and requires real diagonals. Skew-symmetric data
omit the diagonal and negate off-diagonal values. Writers check the complete
represented matrix before emitting structured output. General array order is
column-oriented, independent of the Dense view's physical layout and padding.

Sparse defaults reject duplicate coordinates and preserve explicit zeros.
`SparseMatrixMarketReadOptions` can explicitly request checked summation in
original record order, followed by one symmetry expansion and optional zero
dropping. Its stable insertion sort has quadratic worst-case comparison cost;
`max_sort_comparisons` defaults to 16,777,216 and zero means no comparisons.
Resource and supplied-staging paths both account for expansion and canonical
assembly. Pattern input requires an explicit unit-value policy. Pattern output
requires every retained value to equal exact one.

General Sparse output preserves stored structure subject to the explicit zero
policy. Symmetry compression preserves represented values, not an arbitrary
asymmetric stored-zero pattern: an upper-only zero needs a lower representative,
and skew diagonal zeros are necessarily omitted. Counters report these
projections. Use native ASC archives when exact storage structure is required.

## Examples and verification scope

The standalone [Dense example](../examples/dense_matrix_market/README.md) checks
Hermitian values, both layouts, file interchange and malformed-payload rollback.
The [Sparse example](../examples/sparse_matrix_market/README.md) independently
checks COO/CSR/CSC structures and values, explicit file output and staged
rollback. Each can configure with only its owning component and C++ enabled.

Tests include independent fixtures, every valid native representation/field/
symmetry class, invalid combinations, exact integers, short/failing streams,
resource limits, malformed input, allocation interception and rollback.
External SciPy checks are additional development evidence, not a runtime or
ordinary-build dependency. Cross-representation reads (Dense coordinate and
Sparse array) are optional and remain unsupported; they are not inferred from
the native paths. Exact tested source/toolchain identities and outstanding
platform/integration gates are recorded in the program verification records.
