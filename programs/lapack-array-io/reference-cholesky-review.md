# Bounded reference Cholesky review

This record covers exactly S/D/C/Z POTRF, POTRF2, POTF2, POTRS, POTRI and POSV
through the explicit Dense-owned reference facet. It is not completion of P05,
the full reference profile, owner review or license approval. The frozen
2,113-routine required denominator is unchanged. Native Cholesky has separate
implementation and scoped verification; its normalized contract-bound verified
count is still zero until integration and evidence closure.

## Implementation and frozen identity

The public API is in
[lapack_cholesky.h](../../include/asc/dense/providers/lapack_cholesky.h).
Each operation has four execution overloads and corresponding nonmutating
packing-plan queries. The implementation calls all 24 actual pinned routines;
declarations are not counted as execution.

| ASC-authored file | SHA256 |
| --- | --- |
| include/asc/dense/providers/lapack_cholesky.h | 7e73daeec70ad1d577995aa6dbdae799db1044d2b64448f30927e1f2815c75e9 |
| src/dense/lapack/reference_cholesky.cc | eb750be9dd659581dcf15c3763f7c03eb53004a55c3298d8826556eff3531bb0 |
| src/dense/lapack/internal_cholesky_limits.h | 22d13baed3999f9dc94902532042552b4d6bf4daba98a5a2453caf685ed5b70f |
| tests/dense_lapack/reference_cholesky_test.cc | ec7d46def9cd33aa8fdc7142c3088f7942eb48ca7419360c6f6700d07a463255 |
| tests/dense_lapack/cholesky_test_support.h | dc7a3f065a7d8dd1fd947d35a6da960847e7dd23c3b50e09ed93d3c87357b3fe |
| tests/dense_lapack/cholesky_faults.h | 390d554668b2eb4febd3b3a6db1a72140a6e552f0b7d6bd88bcd0103577f09a3 |
| tests/dense_lapack/cholesky_faults.cc | d4c62d48d5d54579063ea6442e85d890ffc26621c599adcc0687fe102df09af8 |

The final standalone snapshot is external
`p05-reference-cholesky-05/source.tar`, SHA256
`685d601abe5b49572a41be9ab82b94ce4e544a0212f17c6bae55610f36e938d9`.
Its external harness links the independently frozen `p04-lu-v2-02` Core,
Dense and reference-context archives and the exact attested foreign archives.
This is standalone evidence, not verification of an integrated commit,
installed Cholesky consumer or release.

The source and ABI proof remain those of the existing
[provider ABI review](provider-abi-review.md): Reference-LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, unsigned annotated tag object
`5ebe92156143a341ab7b14bf76560d30093cfc54`, source-input manifest SHA256
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.

- LP64 static build identity:
  `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`.
- True ILP64 static build identity:
  `8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.

ILP64 library target names end in 64; this is the upstream ILP64 unsuffixed
symbol interface, not EXT_API/suffixed64. Pinned typed `lapack.h` declarations
carry character-length and mangling details. No vendor header or integer type
appears publicly. The audited GNU/libstdc++ boundary is unchanged. Complex
return values within the algorithms remain wholly inside the Fortran stack;
no new C++ complex-return ABI is introduced.

## Storage, arithmetic and failure contracts

Both layouts and independent factor/RHS layouts are implemented. Row-major A
uses explicit n*n caller conversion capacity but copies only the selected
triangle. Hermitian input diagonal reads use real components only. POTRS and
POTRI consume actual triangular factor data, including complex diagonal values.
No opposite-triangle or padding reads, implicit symmetry scans, densification,
allocation, transfer, fallback or synchronization occur.

None of the six routines has WORK or a foreign workspace-query mode. Queries
therefore check metadata and compute exact minimum=preferred conversion
capacities without reading numerical data or calling LAPACK. Plan keys include
routine, scalar, provider, shapes, original/effective leading dimensions,
layouts and triangle. Execution recomputes requirements and validates plan
freshness, capacities, alignment, placement and overlap of simultaneously live
operands/workspace/metadata before packing or mutation.

The private pure arithmetic helper permits extreme tests without forged
nonempty backing spans. Source-specific provider integer bounds include:

- n+64 for POTRF/POSV (ILAENV PO/TRF) and POTRI (TR/TRI and LA/UUM).
- n+1 for POTRF2/POTF2/POTRS loop updates.
- n*effective_lda for POTF2/POTRI strided INTEGER vector cursors and final
  updates in DOT/GEMV/LACGV paths.
- nrhs+1 for nonempty RHS execution.

These are not blanket provider integer bounds on ASC packing bytes. The
actual ILAENV branches return 64. Full fixed-form precision bodies were
compared in `p05-reference-cholesky-03/precision-audit.log`. Real pairs
match after explicit precision renaming. Complex pairs retain declaration
order/intrinsic spelling differences and CPOTF2's REAL around a complex
subtraction versus ZPOTF2's subtraction of real components. No common
synthesized algorithm replaces those source expressions.

Successful POTRF/POTRF2/POTF2/POSV reports authorize borrowed Cholesky reuse.
POTRS preserves factors; POTRI success has no factor-family tag and cannot be
borrowed as a Cholesky factor. Positive factorization INFO retains its
zero-based failed leading-minor index and selected partial data; POSV leaves B
unchanged. Row-major complex partial publication writes selected offdiagonals
and real diagonal components, preserving ignored original imaginary diagonal
components. This is not byte identity with ignored column-major imaginary
output. POTRI's zero-diagonal scan precedes inversion and leaves A unchanged
on positive INFO.

Negative/minimum signed or excessive positive INFO remains a provider defect.
Packed undefined output is not published; direct column-major output is marked
unusable. Structural failure fabricates neither INFO nor a foreign-call flag.
Aliased reports are rejected untouched because resetting them would corrupt
another live input. Empty n=0 calls do not invoke LAPACK; empty POTRS RHS does
not read or pack factors. POSV n>0,nrhs=0 still factors A. Source floating-point
semantics apply: success does not certify finiteness or conditioning.

No Random kernel, BLAS implementation, base dependency or native Cholesky file
was changed.

## Actual numerical and failure evidence

Every final numerical executable reports:

- 192 independent reconstructions: four scalars, both triangles/layouts,
  three n=3 scales, and n=65.
- 768 reused solves with both RHS layouts and nrhs=1/3, independently formed B,
  solution error and normalized residual.
- 192 selected-triangle inverse-product checks.
- 288 POSV cases: independent layouts, three scales and nrhs=0/1/3.
- 384 positive factorization failures: first/last n=3 pivots with
  zero/negative/NaN values and first/last n=65 zero pivots for every algorithm,
  scalar, triangle and layout.

The n=3 real/complex coefficients are independent explicit fixtures. The n=65
oracle is a closed-form bidiagonal-factor product; it reaches NB=64 blocked
branches. Norms accumulate in long-double complex arithmetic and define zero
denominators explicitly. Scales are 2^(-100)/1/2^100 for float and
2^(-900)/1/2^900 for double. Scalar cases also use minimum subnormal and large
finite inputs. Ignored triangles and ignored input imaginary diagonals contain
NaNs; immutable factors and padding are checked bitwise.

Additional tests cover empty sizes, POTRI exact zero diagonals, failed POSV
RHS rollback, shapes/triangles/placement, mixed build provenance, workspace
capacity/alignment/overlap, changed routine/triangle plans, tampered capacity,
report alias and pure 32/64-bit integer extremes. Test-only GNU wrappers inject
negative/minimum/excess INFO for all six double routes in both layouts; these
are fault tests, not substituted numerical algorithms or a global XERBLA hook.

## External verification and allocation evidence

Artifact locators are relative to the sibling evidence root in program state,
not repository-escaping Markdown links. `p05-reference-cholesky-05` passed
eight configurations, each 1/1 CTest tests executed with zero failures/skips:

- `test-lp64.log` and `test-ilp64.log`: Debug.
- `test-release.log` and `test-release-ilp64.log`: Release.
- `test-asan.log` and `test-asan-ilp64.log`: AddressSanitizer/UBSan.
- `test-libc.log` and `test-libc-ilp64.log`: Linux libc interposition.

Sanitizers instrument the new adapter/test code; the prebuilt Core, Dense and
Fortran archives are not claimed newly sanitizer-instrumented. Earlier base
gates are not relabeled as integrated verification of this new slice.

The byte-identical production/helper/header passed complete repository
clang-tidy in `p05-reference-cholesky-04/clang-tidy.log`. Final changed tests
passed `p05-reference-cholesky-05/clang-tidy-final-test.log` and formatting.
The unchanged header passed self-contained C++20/no-exceptions compilation and
strict Doxygen with zero warnings in
`p05-reference-cholesky-03/{header-self-contained,doxygen-warnings}.log`.
The bounded Doxygen run retains all repository warning settings.

Regular tests observe global C++ allocations across whole calls, including the
first actual foreign operation. Additional Linux interposition uses
`p10-dense-05/libc_allocation_probe.cc`, SHA256
`7a520f7481fae419b3189767b5192d75ae234ec72c25e19e5ecb8c1d6d508a38`.
It observes executable-visible C allocator calls from static provider code and
shared libraries, with positive controls for direct libc and shared libstdc++.
All bounded calls observed zero. This does not detect arbitrary private libc
allocation mechanisms.

Source-conditioned archive closure contains 83 actual members, recorded in:

- `p05-reference-cholesky-03/source-allocation-lp64.json`, SHA256
  `ff97a8e063ede6201728a778c44f851bcd25771ba2755449bcc8441203fe4aec`.
- `p05-reference-cholesky-03/source-allocation-ilp64.json`, SHA256
  `e0766b4b5bc30b263e69b99da00e398c417f8102dc0884d435debf0905ab1e21`.

Records contain source-candidate hashes and actual object imports.
`p05-reference-cholesky-04/source-resolution.log` resolves duplicate names
using both actual build graphs: LAPACK selects INSTALL/lsame.f and
SRC/xerbla.f. XERBLA I/O/STOP and unrelated ILAENV routine branches are explicitly
conditioned as unreachable after checked entry, not silently discarded.

Reached runtime imports are memcmp, memset and _gfortran_concat_string. Both
the [GNU primary source](https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-11.4.0/libgfortran/intrinsics/string_intrinsics_inc.c)
and exact installed runtime disassembly show the latter writing caller storage
with memcpy/memset, without allocation. Runtime SHA256:
`f7379c9331de9d66c03b439a070e8056b7419fc4213320d647952b998d026b30`;
disassembly: `p05-reference-cholesky-02/runtime-concat-disassembly.log`.
This closes the bounded source/runtime path, not all LAPACK, shared providers
or unknown runtimes.

Initial compile/style/audit failures remain in candidates 01–03. A signedness
assertion and test structure were corrected; the initially unknown runtime
import was reviewed from source/disassembly. No failing test was weakened or
skipped. No MdeCpp source, fixture, table or prose was used.

## Integration handoff and remaining gates

The parent owns CMake/ABI/header registration, coverage rows, installed
applications and full regression. Add the source only to the existing optional
facet. The test requires its new test/fault sources, existing Dense allocation
probe, source-root include path for the private pure helper, pinned private
config/includes, and GNU wrapping of dpotrf_, dpotrf2_, dpotf2_, dpotri_,
dpotrs_ and dposv_. No new component or base linkage is required.

Exact bounded rerun from the external candidate directory:
`cmake --build build-lp64 -j2 && ctest --test-dir build-lp64 -V`.

Before complete integration credit, execute installed C++-only Cholesky
consumers, relocation/isolation, the integrated build/sanitizer matrix and
unchanged BLAS/Random regressions on the integrated identity. None is asserted
by this standalone handoff.

Remaining required P05 scope includes full-storage condition/equilibration/
refinement and expert drivers; packed/banded/tridiagonal/RFP positive-definite
systems; pivoted semidefinite Cholesky; symmetric/Hermitian indefinite variants;
general band/tridiagonal LU; triangular families and RFP conversions.
P06–P11, full-profile closure, shared/alternative-platform evidence, actual
owner/license approvals and final release-quality evidence remain governed by
the unchanged runbook.

## Integration review: original row-major stride correction

The root integrator read the full frozen header/source/private helper and this
review. QuerySingle and QueryPair still placed original row-major leading
dimensions among ABI-bounded plan dimensions. Valid empty LP64 descriptors
with an ASC stride above INT32_MAX were therefore rejected even though the
actual packed foreign leading dimension is one. Only original source strides
move to identity options; actual foreign dimensions remain ABI-bounded, and
execution still rejects changed-stride plans before any foreign call.

A new real null-backed empty-descriptor regression covers four scalars, both
triangles, the five raw query routes, empty successful-factor/POTRS reuse and
stale-stride execution. No huge nonempty backing allocation is fabricated.
The old source with corrected new test is frozen in external
`p05-cholesky-stride-02`; LP64 executes1 CTest and fails40 actual assertions,
zero skips. Candidate01 failed compilation because a broad test-helper include
introduced unused constants; the test now directly includes what it uses and
has its own bounded setup helper. Both failed records are retained.

Corrected code is frozen in external `p05-cholesky-stride-03/source.tar`,
SHA256 `382946467f06de01067cbb9a16b05795f132e86280795f3c46542d2746cdf90a`.
Updated ASC identities are:

- Public header:
  `0125c2b9ed0ee5bcd9335f768abed5ff663521fa1d3c43fe1c94bd76afe8a166`.
- Adapter:
  `8262edc54fd742a4040f3575d332ad789dd036181f4bd4151530f46eec0826b1`.
- New `tests/dense_lapack/cholesky_stride_test.cc`:
  `66e92b71cdb7bb2831c49d9b34c6bd5df10c281c57a8e9a4baa63c501e434e84`.

Other original candidate05 files are unchanged. Both-ABI Release and Clang19
ASan/UBSan each pass2/2 CTests with zero skips, selecting the complete existing
numerical test plus the new stride test. Sanitizers instrument the new
adapter/tests, not prebuilt Core/Dense/provider/runtime dependencies.

The final follow-up test also executes genuine singleton POTRF/POTRS with
INT64_MAX original row stride, one real backed scalar per operand and the
independent exact expectations4->factor2 and RHS8->solution2. This extends the
empty regression without weakening it. Candidate04 archive SHA256 is
`799882d1f15c997ba2236e351e4a3d4e10109f7e149473f806fe0156227abdd0`;
its new stride test SHA256 is
`c16c2907d317c433e212c846f3d3a79750b1ed8f156512025fe0688293c18ed4`.
Production/header/helper and the original numerical test remain byte-identical
to candidate03. Candidate04 again passes2/2 CTests in each of both-ABI Release
and Clang19 ASan/UBSan, zero skips. Its raw command/log/JUnit/source records
are under external `p05-cholesky-stride-04`.

The earlier empty-only test passed strict Clang18 tidy in
`logs/p05-cholesky-stride-tidy-01.log`; final singleton-test/source/header strict
checks and integrated/installed gates remain required. The integration index
currently contains only the separate advanced-LU v4 checkpoint, not these
Cholesky files.
