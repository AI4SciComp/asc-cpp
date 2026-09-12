# P05 classic Bunch–Kaufman slice

The historical candidate record below is preserved. Current integration work
continues in [the INFO correction review](indefinite-info-review.md); its
existing workspace and fresh evidence supersede the old candidate workspace
as the location for ongoing development.

Status: bounded implementation frozen with the local verification below;
parent integration and owner-review gates remain open. This slice owns actual S/D/C/Z
SYTRF/SYTF2/SYTRS and C/Z HETRF/HETF2/HETRS: 18 required source rows, not
the whole indefinite family or P05/P09/P11 closure.

## Isolation and inputs

Worktree: sibling `../asc-cpp-p05-indefinite`, branch
`feature/lapack-p05-indefinite`, base
`3d5909d6c26ba2d274749106bda448e2280b8330`. Root's dirty index is excluded.
The previous expert slice remains frozen. Only new optional provider headers,
sources/private helpers, tests and this review are owned here; no central
registration, foundation, ABI, coverage, commit or remote write occurs here.

The external evidence root is sibling `../asc-cpp-evidence/lapack-array-io`.
Reference source commit is `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`,
with source-input manifest
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`
and required inventory SHA-256
`1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7`.
The immutable denominator remains 2113.

The exact provider-build identities are
LP64 `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`
and true ILP64
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
Their external attestation records are
`provider-lp64-attestation-01.json` and
`provider-ilp64-attestation-01.json`; prefix libraries are read-only inputs.
Both are the audited GNU 11/Linux static development subset with deprecated
sources enabled and XBLAS disabled, not a full-reference-profile claim.
The bounded external harness links unchanged frozen ASC base/provider-facet
archives from `p04-lu-v4-01/build-<abi>` and compiles this slice separately;
that is not a claim that the parent integrated tree was built here.

Live [Google C++](https://google.github.io/styleguide/cppguide.html) and
[Python](https://google.github.io/styleguide/pyguide.html) guides were retrieved
2026-09-07. External `p05-indefinite-01/google-cppguide.html` has SHA-256
`f681e8c1b71ed5f2420555a28b7e7120f46914cfa126e9d8ec5e6e9851512caf`;
`google-pyguide.html` has SHA-256
`9b02fa0d1aa05bfc8a4b5a95d3594665124f7a0414c82a69359f4a0b2f65e1c0`.
Existing repository compatibility exceptions and Clang 18 policy are retained.

## Source decisions

- Classic Bunch–Kaufman signed one-based pivots are preserved as paired
  1-by-1/2-by-2 block metadata. They are not a permutation or LU swap list.
  Rook/Aasen/packed variants are not accepted under this type.
- Symmetric complex uses transpose and actual complex diagonals. Hermitian
  original input uses real diagonal components; HETF2 rank-two updates and
  HETRF panel copies require explicit selected-component caller packing even
  for column-major original input. Completed selected outputs are published.
- HETRS consumes actual block-factor coefficients: its 2-by-2 path reads
  complete complex diagonal values, unlike its real-diagonal 1-by-1 path.
  Factor-consuming packing must not reuse original-input normalization.
- The pinned S/DSYTRF introductory upper-factor prose says U^T D U. The
  source transformation details, SYTF2 and SYTRS implement U D U^T; independent
  block reconstruction must verify the latter equation rather than copy the
  inconsistent sentence.
- SYTRF/HETRF preferred workspace is source-derived NB=64, with minimum one
  active scalar entry. Source SROUNDUP and provider-INTEGER cursor/loop
  arithmetic are checked independently of ASC byte counts and backing spans.

### Missing installed prototypes

The pinned installed `lapack.h` has TRF/TRS but omits S/D/C/Z SYTF2 and
C/Z HETF2. GNU Fortran 11.4.0 `-fc-prototypes-external -fsyntax-only`
successfully emitted all six from the exact source for ordinary INTEGER and
the actual true-ILP64 `-fdefault-integer-8` build. External
`p05-indefinite-01/prototype-<routine>-<abi>.h` files preserve those emissions.
They require `int*` versus `long*`, one trailing `size_t` CHARACTER length,
and the same ordinary underscore symbol names. The private C++ facade asserts
those exact integer and complex types, not only representation sizes.

Each emitted header was included alongside the private declaration in a
direct ABI probe linked to its matching attested provider. All six actual
routines passed both triangles with genuine backed 2-by-2 paired-negative
pivots and a 1-by-1 singular positive-INFO case: 24 calls per ABI. CHARACTER,
dimensions, leading dimensions, full-width INFO/pivots and scalar storage have
sentinels. This uses the previously audited GNU/libstdc++ complex boundary;
it adds no portable-Fortran claim or complex-return/callback signature.
A provisional, uncompiled bind(C) draft was moved outside the repository to
`p05-indefinite-01/unused-bindc-draft.F90`; it is not a build dependency.

### Source INTEGER and workspace arithmetic

The actual factorization calls need `n < INTEGER_MAX` for terminal loop and
`K+KSTEP` expressions. For `n > 2`, strided pivot search/panel copies require
`1 + (n-1)*foreign_lda <= INTEGER_MAX`. The length-one IAMAX path returns
before its stride increment, and unblocked swaps have at most `n-2` entries;
there is no invented `n*lda` rejection for the valid order-one/two cases.
TRF additionally evaluates `64*n` even at minimum workspace. Its float
SROUNDUP initial REAL-to-INTEGER conversion and possible upward-epsilon
rounding are separately bounded below the exact power-of-two integer limit.
D/Z retain the maximum of raw integer workspace and representable returned
real integer; a downward-rounded query never reduces mandatory capacity.

SYTRF's actual ILAENV(ISPEC=2) minimum useful block size is eight; HETRF's is
two. Reduced-workspace tests cross each exact transition at order 67.
The panel's `LDW=n`, at-most-64-column row cursors are bounded by the same
`64*n` constraint (the signed integer limit has 63 remainder modulo 64).
Solves separately require `1 + nrhs*foreign_ldb <= INTEGER_MAX` for actual
SWAP/SCAL/GEMV/GER/LACGV row-vector cursors. A factor matrix's unused stride
is not substituted into that bound. Empty wrappers do not enter foreign
loops. A single column uses the equivalent effective leading dimension equal
to its row count while retaining its full ASC leading dimension in the key;
real backed order-one descriptors with `INT64_MAX` ASC stride are exercised.

Private foreign pivots correctly use `kInteger`, whose entry size follows
the actual provider ABI. The initial diagnostic incorrectly chose
ASC-index-sized `kPivotConversion`; the foundation rejected all six LP64
paths before foreign entry. Those failed logs remain external. No foundation
contract was changed to accommodate this mistake.

### Provenance and mutation limits

The optional factor view validates current square shape, pivot count and full
signed pairing/directional bounds, current triangle/symmetry and matching
scalar/source-routine/provider success metadata. Reports cannot authenticate
buffer contents or prove that a caller-supplied triangle/shape belonged to a
historical operation. Common call origin and unchanged borrowed buffers are
explicit caller preconditions, not inferred or cryptographically verified.
Positive INFO publishes completed selected factorization data and raw pivots
but never authorizes successful factor reuse. Actual NaN-pivot positive INFO
is distinguished from an exact-zero singular pivot. Unexpected INFO, invalid
provider pivot encodings or unexpected WORK output mark results unusable;
row-major conversion output and public pivots are not then published.

### Allocation evidence scope

The final source-conditioned static closure in
`p05-indefinite-05/source-closure-<abi>.json` records exact archive/source
hashes for 76 reached members per ABI. The only unconditioned external imports
are `memcmp`, `memcpy` and `memset`. XERBLA error I/O is excluded only by the
recorded complete scalar/shape/workspace preflight; ILAENV branches are limited
to the audited classic ISPEC=1/2 queries. This is a bounded call-chain audit,
not an allocation proof for all archive symbols or the full denominator.

The ordinary harness observes global C++ new and ELF `--wrap` calls from
linked ASC/static provider objects. A separate Linux/glibc Release harness
also interposes the process-visible libc allocators, with executed positive
controls for direct libc and shared-libstdc++ allocation. Instrumentation
includes initial and repeated wrapper entries; private runtime allocators or
unrelated provider routes are not silently covered by that statement.

## Verification and next action

The new bounded implementation exists, with independent reverse
Schur-complement reconstruction and residual/known-solution solve oracles.
There are 92 factor cases and 552 reuse/solve cases for each of six actual
scalar/symmetry classes per ABI/configuration, including empty-operation
checks; 80 factor and 320 solve cases per class actually enter the provider.
They cover order 0/1/2/7/67,
both triangles, independent factor/RHS layouts, minimum/preferred/reduced
workspace, and small/large power-of-two scales (S/C exponents +/-100; D/Z
+/-800). Padding, unused-triangle NaNs and original Hermitian imaginary
diagonals have independent preservation/read-semantics checks.

Additional tests cover actual first/last zero-pivot INFO, actual selected NaN
INFO, malformed paired pivots, stale plans, workspace role/size/alignment and
overlap, live metadata/report alias, pinned operands and nonempty scratch,
zero-byte unused pinned scratch, representable wide strides, evaluated-zero
block divisors and injected provider defects. Synthetic fault injection never
counts as successful numerical evidence. Pure arithmetic boundary tests use
ordinary integers, never invented huge backing spans.

Final `p05-indefinite-05/source.tar` has SHA-256
`34d714056a0bd22844353e320263ef5d1886be12e017b64d9560aeeccf5e2993`.
The separate `implementation-files.json` binds all 14 code/header/test files;
this final review is hashed independently because its evidence summary is
written after the immutable verification snapshot.

Both LP64 and true ILP64 passed Debug, ASan+UBSan Debug, and Release with
dynamic libc interposition: six configurations, each 15/15 CTest cases,
90 total, with zero failed/disabled/skipped tests in the saved JUnit XML.
Sanitizers instrument this ASC slice and its tests; the attested Fortran
provider and unchanged ASC dependency archives were not rebuilt with
sanitizers. Provider buffer guards and the source-derived integer audit are
separate evidence, not a claim of instrumented Fortran internals.
Complete Clang 18 policy checks passed all seven new translation units;
no checker was disabled for the final result. Strict Doxygen passed with
43 public functions documented, standalone repeated-include C++20
`-fno-exceptions` compilation passed without vendor include paths, formatting
passed all 14 code files, and the Markdown checker passed 101 source files.
Both provider attestations were reverified read-only against the frozen
inventory/lock and exact external prefixes.

Failed intermediate diagnostics remain external: the initial wrong workspace
role; ordinary include/enum/status/test-structure warnings; an exploratory
style-only tool crash; premature compilation-database lookup; and intentional
duplicate/probe-only forced-include declarations rejected by strict include
policy. The final ABI test directly includes the untouched emissions under
test-only symbol renames, checks exact function-type equality, undefines those
renames, and executes real provider symbols. It passes without suppression or
removing the compiler-emitted ABI comparison. No failed or zero-test run is
credited as a completed gate.

No central manifest, ABI inventory or registered capability is updated here.
Parent integration must import the final exact frozen files, verify the new
prototype/facet linkage on the integrated tree, and run installed C++-only
isolation, ABI/symbol/header oracles and full regression gates. Other classic
indefinite routines, rook/Aasen/packed/band variants, expert drivers,
reconstruction utilities and the rest of P05/P09/P11 remain outside this
18-row slice, with the unchanged 2113-row full-program denominator.
