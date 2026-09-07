# Native Cholesky implementation and evidence

This bounded P05 slice implements provider-free full-matrix POTRF/POTRS for
float, double, complex float and complex double. It does not complete P05's
Reference-LAPACK structured, packed, banded, RFP, tridiagonal, indefinite,
pivoted-semidefinite or expert-driver families.

## API and numerical semantics

[cholesky.h](../../include/asc/dense/lapack/cholesky.h) adds eight explicit
overloads in the existing flat ASC namespace. POTRF takes an execution
context, selected triangle, checked mutable square matrix and mandatory
report. POTRS takes the existing successful borrowed
`LapackCholeskyFactorView`, checked RHS and mandatory report. There is no
new factor owner, global registration, optional-provider link, implicit
packing, transfer or allocation.

The independent arithmetic uses selected-triangle access to the equation
`A=L*L^H` or `A=U^H*U`, with real transpose as applicable. Complex
diagonal imaginary components are ignored on factorization input and
replaced with positive zero. No symmetry/finiteness scan of the unselected
triangle is performed. Success is not a condition or finiteness certificate.
The first nonpositive or NaN real Schur pivot causes a numerical stop, with
its unsquared real value stored in the diagonal, an exact zero-based index
and documented partial factors. No tolerance substitutes for positivity.
Later unprocessed selected entries retain their original values.

Factor reuse performs two substitutions without refactorization. Its
preflight checks native provenance, shape, placement, overlapping complete
reachable ranges and positive finite real-valued factor diagonals before
modifying a nonempty RHS. Diagonal failure leaves RHS unchanged. Empty
operations do not read numeric entries. Factors and padding remain intact.

Both operations require exactly zero scratch and use constant auxiliary
storage. POTRF arithmetic is cubic; POTRS is quadratic per RHS. No
performance claim is made. Reports are deterministically initialized before
ordinary validation; an alias with the report object itself is rejected
without resetting that overlapping object. Raw foreign INFO stays absent
and called_provider stays false. Native provider identity is the existing
default native identity, not an attested Reference-LAPACK build.

The actual serial context accepts only `MemorySpace::kHost`. The initial
test incorrectly expected pinned-host acceptance; the retained failure was
resolved by following Core's existing serial-access contract and native LU
behavior. Pinned, device and managed placements are rejected. No Core
execution or Random implementation was changed.

## Independent verification

The test uses explicit real and Hermitian coefficient tables and
independent lower-factor and solution tables. The real fixture is the
runbook's three-by-three SPD example. The complex lower factor is
`[[2,0,0],[1+i,3,0],[-1+2i,2-i,2]]`; its independently specified Hermitian
matrix has rows `[4,2-2i,-2-4i]`, `[2+2i,11,7]`, `[-2+4i,7,14]`.
No MdeCpp source, fixture or expected vector is used.

Actual tests record 48 reconstructions and 192 solve residuals across all
four scalars, both layouts, both triangles, multiple RHS layouts/counts and
three scale levels. Wider complex arithmetic computes row-sum norms without
naive squaring. Reconstruction tolerance is 256 scalar epsilons; residual
tolerance is 512 epsilons for these small, moderately conditioned fixtures.
Zero denominators cannot hide nonzero errors.

Scalar tests reach minimum subnormal and large finite magnitudes.
Additional checks cover empty orders/RHS, exact first/last failed pivots,
zero/negative/NaN diagonals, provider/family mismatch, rectangular matrices,
RHS shape/alias/placement rejection, and deliberately invalidated borrowed
factor diagonals. Unselected triangle and complex input diagonal imaginary
components contain NaN sentinels. Floating-component bit comparisons retain
NaN and signed-zero distinctions when checking untouched storage.

The final frozen external candidate is `p05-native-cholesky-03/source`,
based on `p03-p04-d3675f3/source.tar` with only the three new implementation
and test files overlaid. Available completed results:

- Candidate-03 Debug, Release and ASan/UBSan each pass 1/1 CTest, zero skips.
  Every run prints the actual 48/192 numerical-case counts.
- Candidate-03 full clang-tidy configuration passes both production and
  tests without user-code warnings. The one local enum-cast annotation is
  an intentional invalid-option test, matching existing repository tests;
  the malformed-option check itself executes and must reject.
- Candidate-02 strict Doxygen passes with an empty warning log; the public
  header is byte-identical in candidate-03.
- Candidate-03 Linux/glibc C-allocation interposition passes the complete
  suite, with positive controls observing direct libc and shared libstdc++
  allocation. Self-contained C++20/no-exception strict-warning header
  compilation, clang-format dry run and Markdown-link checks also pass.

The regular tests observe C++ global-new calls. The external Linux
diagnostic additionally observes malloc/calloc/realloc/memalign/aligned_alloc/
posix_memalign by executable-symbol interposition, including first
factorization and solve scopes and numerical-failure scopes. This is
exact-runtime evidence for those entry points, not a universal claim about
private libc allocators or other operating systems.

## Frozen identities and remaining integration gates

SHA-256 identities:

- Public header:
  `0e4a7332790ab0598252c9b7e40437891dbb0256ed2f18589fda19d88c0a2b24`.
- Native production:
  `3321c6c0469632d47eb43ea9dcbd2b06251c6259a9491530148f2e8434f81080`.
- Test:
  `73f4a66a4f41d16251eaff70cedead7d0d096e6881d5017cf4348519408ddd9f`.
- Candidate-03 source archive:
  `ff74917274c454a555cff6d0ff78408334fda954c9849f6c4c4d374ce7e5a736`.
- Base source archive:
  `ce75f381e24019458a9c04468b6cc07ae3afb458af6dea2e0212f5a40a2db08e`.

Logs, standalone CMake configuration and builds are external program
evidence, not source-tree artifacts. CMake/ABI/header registration,
integrated installed consumers, component isolation, full Random
regressions on the integrated revision and central native coverage evidence
remain parent-integration gates. No Reference-provider row is credited by
this implementation or its tests.
