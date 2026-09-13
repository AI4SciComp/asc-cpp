# Dense native Matrix Market implementation review

This is a P10 implementation record, not a claim that the complete LAPACK
program or integrated P10 package has passed every gate.

## Scope and normative inputs

The Dense-owned implementation is
[matrix_market.h](../../include/asc/dense/matrix_market.h) and
[matrix_market.cc](../../src/dense/matrix_market.cc). It uses the shared
Core lexical/scalar helper and existing Core byte-source, byte-sink, File,
Status, checked-size and explicit-memory-resource boundaries. It does not
depend on Sparse, BLAS, Random, a LAPACK provider, SciPy or a new module.

The full runbook P10 and
[matrix-market-profile.md](../../docs/contracts/matrix-market-profile.md)
govern the implementation. The primary external format reference is the
[NIST Matrix Market format specification](https://math.nist.gov/MatrixMarket/formats.html),
read directly during implementation. The live
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
and existing repository compatibility rules govern source style.
Fixtures and expected matrices are independently derived; no MdeCpp
implementation, fixture, table or prose is used.

All ten valid native array field/symmetry classes are implemented: integer,
real and complex general, symmetric and skew-symmetric, plus complex
Hermitian. Array pattern and real/integer Hermitian are format errors.
Non-general matrices must be square. Coordinate input returns
`kUnsupported` without creating a Dense owner or performing conversion.
Dense-coordinate and Sparse-array conversions are optional conveniences
under the frozen profile and are not implemented here. Pattern policies,
coordinate duplicate assembly and sparse zero policies belong to the
independent native Sparse-coordinate implementation.

## Representation and publication

`DenseMatrixMarketReader::Prepare` validates the banner and dimensions using
caller scratch, retains source/report references and copies exactly two
extents and the limits. The move-only cursor allocates no metadata or
values. Preflight or owner-allocation failure leaves it reusable; a payload
attempt consumes it even on malformed input. Source progress cannot be
rolled back or resynchronized.

Array order is column-first independently of destination layout.
Symmetric/Hermitian arrays contain the lower triangle including each
diagonal once. Skew arrays contain only the strict lower triangle; Dense
diagonals are explicitly initialized to zero. Off-diagonal mirroring uses
identity, checked negation or conjugation exactly once. A non-real Hermitian
diagonal is rejected, not repaired.

Prepared and stream owner overloads accept an explicit destination scalar,
rank-two static/dynamic extents, host resource and left/right layout.
One budgeted candidate allocation is made, including the existing empty
owner allocation convention. Publication requires complete conversion,
mirroring, trailing comments/whitespace and EOF. Path loads additionally
require successful explicit close.

`ReadDenseMatrixMarketInto` requires a complete, disjoint, caller-owned
typed staging span. Target mapping, shape, accessible placement,
scratch/staging capacities and address ranges are validated before reading
values. Only a nonfailing scatter after EOF changes the target; padding
and all target bytes remain unchanged on every failure. Unsuccessful
parsing may change staging.

Writers validate the complete represented matrix before emitting any byte.
Exact equality, checked negation and conjugation validate compressed
symmetry, including omitted partners and diagonals. They traverse existing
host or pinned views directly without packing allocation. Device and
managed placements are rejected without transfer. Huge empty rectangles
skip the otherwise empty outer traversal.

Stream writes may leave an accepted prefix. Path saves require explicit
create/truncate intent and check flush and close; they promise neither
atomic replacement nor durability. Full-matrix validation precedes file
opening. Report-storage alias checks happen before any report reset.

## Scalar, resource and error boundaries

Destination scalars are the twelve native fixed-width integer, IEEE real
and complex types. Core parses integer magnitude exactly without a
floating-point intermediate, including exact integer-to-real conversion.
Real components round directly to the destination type. Complex fields
require complex destinations even when the imaginary component is zero.
Real/integer inputs to complex storage explicitly set a positive-zero
imaginary component. Nonfinite values and nonzero underflow are rejected.

Checked extents, products, triangular counts, full decoded bytes,
allocation count/bytes, staging, scratch, input/output, line and token
limits are independent. Existing invalid-limit diagnostics are preserved;
legal extreme limits exercise actual shape/product overflow checks.
There is no hidden map, vector, transfer, provider dispatch or coordinate
densification.

Borrowed source/sink diagnostic strings are intentionally not copied:
bounded codec errors preserve the code and native code under the Core codec
contract. Explicit resource failures preserve their full owned diagnostic
through the existing private failed-Result move helper. No public Result
API changed. File handle/path allocation remains a separate Core boundary.

## Candidate and evidence

The final immutable candidate is external `p10-dense-05/source`, based
on retained `p03-p04-d3675f3/source.tar` plus two new Core helper files and
four new Dense production/test files. Raw logs and builds remain under the
external program evidence root; none is a source-tree dependency.

The Dense test executable invokes fifteen test groups, not fifteen
CTests. It covers twelve native scalars, all valid field/symmetry classes,
exact conversion, malformed headers/records, every source-failure position,
truncation, comments/CRLF, a second banner, Hermitian/skew failure, empty and
huge-empty shapes, independent resource caps, rollback/padding, cursor
move/retry/consumption, placement/alias rejection, every sink-failure
position, long diagnostics, explicit path behavior, and 1,024 deterministic
bounded byte mutations.

Actual candidate-05 results:

- Debug `test-debug.log`: 1/1 CTest passed, zero skips.
- Release `test-release.log`: 1/1 CTest passed, zero skips.
- ASan/UBSan `test-asan.log`: 1/1 CTest passed, zero skips, using the
  separately instrumented existing Core/Dense base libraries.
- Candidate-04 `scipy-interop.log`: ten cases passed in both directions with
  SciPy 1.15.3 and NumPy 2.2.3. Independent explicit matrix oracles are used
  in the C++ fixture executable and external Python harness. Candidate-05
  changes only the main test by adding prepared-metadata/report alias cases;
  production, Core helper and interoperability executable sources are identical.
- Candidate-04 `tidy.log`: complete repository clang-tidy configuration
  passed for the Dense implementation, main test and interoperability test.
- Candidate-05 `tidy-test.log`: final test-only addition passed the same
  unchanged complete configuration. The final clang-format dry run and
  repository Markdown-link check also passed.
- Candidate-04 `doxygen-warnings.log`: strict Doxygen passed with an empty
  warning log; `header-alone.log` passed C++20, no-exception and strict-warning
  self-contained header compilation.

The global-new test probe observes C++ allocation entry points, not
arbitrary foreign/shared-runtime malloc calls. The codec call graph uses
the existing nonallocating scalar parser/formatter and synchronous borrowed
streams; explicit resources and File are accounted for separately.
Allocation tests include initial prepared read/write calls and long
source, sink and resource failures.

An additional external Linux/glibc diagnostic replaces the new-only test
probe with executable-symbol interposition of malloc, calloc, realloc,
memalign, aligned_alloc and posix_memalign. It forwards to the actual
glibc allocation primitives without recursive symbol lookup. Positive
controls observe both direct libc allocation and allocation through shared
libstdc++ operator new. The final `test-libc-final.log` passes 2/2 CTests:
the main Dense suite and an independent first-call scalar test. The latter
checks a 603-byte decimal token for f64/f32, maximum-f64 formatting, complex
minimum-subnormal input and overflow, reporting zero intercepted allocations.
This is exact-runtime evidence for these entry points, not a claim about
private hidden libc allocators or other platforms.

Exact source SHA-256 identities:

- Dense public header:
  `524ab033acfa9c38797d08b529180c09cb3d4a2db8e49f13130c834b99ebd8b7`.
- Dense production source:
  `3d5aff8945743cfef058c137585eca268ead5961996d59d8f6800de98139a418`.
- Final Dense main test:
  `cb52c97b965ab41a3d8ac0d1d3c5431e54411f598a8afa5b728170e9421a7f46`.
- Dense interoperability test:
  `a6ccbead4562693649eeefd83bb9e8f5b4fc3285c032e660858189977194911c`.
- Core helper header:
  `502db0a675457ea6b4e220b4ca5873bff066d16cac67af807233f52b35aa98ae`.
- Core helper source:
  `41e4aacbf7cab1e29ef9b64f1cfa675d4dbd1d83146a5e96c167cfd66702991e`.
- Base archive:
  `ce75f381e24019458a9c04468b6cc07ae3afb458af6dea2e0212f5a40a2db08e`.
- Final candidate-05 source archive:
  `5b63a49caf62c0191d1e2d677d28716b6f415863ff4b13d078772b5139a8dc88`.
- External C-allocation probe:
  `7a520f7481fae419b3189767b5192d75ae234ec72c25e19e5ecb8c1d6d508a38`.
- External scalar probe:
  `0f95845c98546ff47e283ef4f1e3957c432e915b95b7837cd2bb7533e055c82e`.
- Actual libc:
  `e01b1ce7be2987f3b8560e26d0df2623f9dd5cec17be923ae28a785bc0d32d50`.
- Actual libstdc++.so.6.0.30:
  `ff0825e113603c3866680d5d52216bc6d8eedf3a59f52a0aef67ff01994db128`.

Earlier failed diagnostics remain retained: expanded tests exposed the
new wrapper's long-resource Status copy and three incorrectly selected
overflow-test limits; Clang found an ambiguous test declaration and style
findings. These were corrected, not skipped. The initial live-file tidy
crash is retained; subsequent checks use immutable snapshots.

Integrated CMake, ABI/header inventories,
documentation coverage, installed consumers and component-isolation checks
are separate required parent-integration gates; these standalone checks
do not substitute for them. The parent integrator also owns central
evidence and independent Sparse-coordinate integration.
