# Reference full-rank least-squares self-review

Implementation-owner self-review completed 2026-09-07; this is not repository
owner approval. The bounded implementation is ready for parent integration.
It implements the twelve pinned S/D/C/Z GELS, GELST and GETSLS drivers with
twenty-four typed query/execution declarations, not the entire P06 package.

The isolated branch is feature/lapack-p06-least-squares, based on unverified
local source anchor a3e1e61d9258f23f4e864d594f2b96e2770dc245. No commit, push,
shared registration, ABI/schema or native/previous-QR edit was made. The
original dirty worktree was not modified. New capabilities are identified by
frozen content hashes below, not represented as part of that anchor commit.

The actual repository instructions, complete P06/P01 and applicable numerical,
workspace, report, ABI, architecture and cross-cutting sections were read.
The live [Google C++ guide](https://google.github.io/styleguide/cppguide.html)
and [Google Python guide](https://google.github.io/styleguide/pyguide.html)
were accessed 2026-09-07. Accepted repository compatibility rules, C++20,
self-contained headers and the six-module boundary remain unchanged.
No MdeCpp code, tests, tables or prose were used.

## Implemented contract

The supported header is
[lapack_least_squares.h](../../include/asc/dense/providers/lapack_least_squares.h).
GELS executes the actual QR/LQ route; GELST the actual compact-WY route;
GETSLS the actual GEQR/GELQ route. None substitutes for another, uses normal
equations or infers numerical rank. Full rank remains a caller assumption.
The pinned zero-A INFO=0 convention is explicitly not a full-rank certificate.

A is m-by-n; B must have exactly max(m,n)-by-nrhs capacity. N reads m B rows
and returns n solution rows; real T/complex C reads n and returns m.
Both layouts are independently supported. Every B, including column-major B,
requires explicit caller packing storage, and only input rows are read.
Row-major A is also packed. All workspace regions are checked against the
explicit serial provider's host-only access contract.

GELS/GELST publish documented residual coordinates, not original residual
vectors; upstream scaling can leave those coordinates scaled. GETSLS publishes
only documented solution rows, preserving its undocumented trailing rows.
Transformed A is not exposed as a certified reusable factor.

Valid positive INFO preserves the exact raw INFO, zero-based diagonal,
kSingular/kDocumentedPartial and meaningful transformed A/input-row B
intermediates. No solution is valid and scaling may not be undone. Negative
or impossible INFO is a provider defect: packed outputs are withheld, while
direct column-major A can be unusable. Reports distinguish actual foreign
queries, foreign execution and native empty-result publication. Aliased report
storage is rejected before reset; other calls initialize it before validation.

## Source-pinned arithmetic and private ABI

GELS minimum is max(1,k+max(k,nrhs)), preferred max(1,k+32*max(k,nrhs)).
SGELS uses SROUNDUP_LWORK, but CGELS returns REAL(WSIZE) directly; D/Z use
direct double conversion. Nested ORM/UNM QR/LQ nrhs*32+4160 arithmetic and
returned scalar reconversion are checked before the outer call.

Pinned ILAENV supplies GELST NB=1, so raw minimum/preferred coincide.
S/C GELST round upward; D/Z directly convert. GETSLS has actual -2 minimum
and -1 preferred queries, with nested GEQR/GELQ and GEMQR/GEMLQ queries.
The implementation checks foreign m*n tuning arithmetic, block counts,
k*blocks+5 and k+5 T sizes, nested and final scalar reconversions, block
metadata exactness, terminal loops and wide strided-vector cursors.
An outer larger WORK cannot repair an internally underreported T size.

Pure tests cover 32/64-bit INTEGER limits, 2^24/2^53 conversion boundaries and
raw versus returned versus required capacities. Real-backed zero-sized and
singleton descriptors test extreme original strides without fabricated spans.
Original A/B strides remain in query identity without narrowing ASC-only
strides to LP64.

GELST is absent from pinned lapack.h. Exact GNU11 generated prototypes from
each pinned S/D/C/Z GELST source were retained for ordinary INTEGER and
-fdefault-integer-8. They establish int/long integer pointers, actual C++
complex types and the trailing size_t character length. Actual tests enter
every scalar symbol in both true ABIs. No foreign types enter supported ASC
declarations.

## Verified evidence and exact identities

Artifacts are outside the source tree under the sibling evidence root
asc-cpp-evidence/lapack-array-io. These locators are intentionally plain paths.

Main: p06-least-squares-03/source.tar, SHA-256
db321ba019d96d33e0c68312dff98f18322cc22a2b9bd4451470abdec7960a21.
Runner content identity:
3c7ccc54335a3bfd96f78571a6d9565f951dc462aee8d51e7de93d0e51117305.

Separate regression: p06-least-squares-workspace-01/source.tar, SHA-256
d6c5c3f630c05074c25e2c6e5fe3eee1b2a34fd09ffe3ec697de325e6ef33171.
Runner content identity:
40de7afccfae2c4f5cb7f4befa5865e4f5a9e2ea36c8c475c34f9ef05e9ed67e.

| Source | SHA-256 |
| --- | --- |
| Supported header | 865f5b6ac1bbe5100cb475a94cfd8a85fe568eaf441c9d28cf496479e582feaa |
| reference_least_squares.cc | d92fb8d858328dd552a7e7fcebbf4a1e39960c7c87cb2cfa4fde680daf926831 |
| internal_least_squares_counts.h | 7f3d553186e2dcc9e9a8f4544ec5d27a814752db69c041d78678c5915f4c03a5 |
| owned-source.sha256 manifest | ca00ad23160d8b6994748ab5d65675f67d04317eecdfe7ef5dc597cd2a151209 |

The owned-source manifest records all ten new code/test files. Dependencies
are exact frozen reference-QR05/provider construction, matching Core builds,
actual LP64/ILP64 LAPACK/BLAS archives and GNU runtimes; the full dependency
manifest hash is
ddfaad386ff58bb49914caf8bae9258ea8dc504d1f4b284f18abe0c432e3f7b6.

Reference LAPACK 3.12.1 commit:
6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca.
Tree: 7217db728e4f7ee87dabf545a1a87a3d6cd30e9b.
Tag object: 5ebe92156143a341ab7b14bf76560d30093cfc54.
Source lock:
5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a.
LP64 build:
7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c.
True ILP64 build:
8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97.

| ABI | Debug | Release | Clang19 ASan+UBSan | libc observer |
| --- | --- | --- | --- | --- |
| LP64 main | 6/6 | 6/6 | 6/6 | 6/6 |
| ILP64 main | 6/6 | 6/6 | 6/6 | 6/6 |
| LP64 separate workspace | 1/1 | 1/1 | 1/1 | 1/1 |
| ILP64 separate workspace | 1/1 | 1/1 | 1/1 | 1/1 |

All configure/build/CTest commands exit zero with no skipped tests and
--no-tests=error. Main counts are four scalar processes plus pure-count and
contract processes. Each main lane retains 2240 executed profile events,
not unique upstream coverage rows: each scalar has 184 GELS, 184 GELST and
192 GETSLS events. The separate workspace regression is not a relabeled 7/7.

Independent prescribed solutions, orthogonal inconsistent residuals and
nullspace vectors verify optimality, minimum norm, forward answers and
unscaled residual-coordinate norms. Cases include both independent layouts,
legal transpose choices, minimum/preferred WORK, empty/scalar/square/tall/wide
and multiple RHS, blocked 131-by-129, tall-skinny 17000-by-8 and extreme scaling.
Actual INFO=2 failures and zero-A behavior are distinct from injected provider
defects. Tests also cover aliases, capacity, malformed queries, every workspace
role's placement, misalignment, stale original strides, selected publication,
padding and absence of numerical query mutation or extra outer query.

Strict Clang18 tidy passes six owned TUs. Formatting passes all ten code/test
files. Standalone C++20/no-exceptions header inclusion and actual Doxygen1.9.8
pass; XML has 24 functions, 24 parameter lists and 24 return sections.

Operation allocation probes are zero; libc replacement probes have positive
controls in every process. This is exact Linux/glibc evidence, not a universal
platform claim. Fortran archives are not sanitizer-instrumented; ASC/Core are.
Both static closures contain 242 objects from twelve required roots. XERBLA
argument-error branches are explicitly excluded after source preflight.
The additional _gfortran_concat_string leaf is checked against exact runtime
hash/disassembly and uses only stack plus memcpy/memset. No allocation leaf is
silently allowed. The 242-source closure manifest hash is
6b5713b2dc60c1ed1f33ef62ff187a10bc87ee12adf343961f97b0ac03c32a60.

The detailed external p06-least-squares-03/verification-summary.md is
8f095666eac7aec6980d54c4d776c1a96fd77ea1556217828a9feb2ef816744f.
Its evidence.sha256 ledger is
4bea0e040e0307022af90923e4527229f2fa3188e263a9d16ae53882bf0a9f05,
covering 227 locators for exact commands, raw logs, complete CTest/JUnit,
libraries, archives, manifests and earlier failed candidates.

## Preserved failures and remaining gates

Candidate01 passed both Release suites but failed style; candidate02 passed
LP64 runtime but found test direct-include issues. Its initial static closure
failed on the previously unaudited concatenation leaf. Those logs and source
identities remain intact; final evidence does not relabel them. One earlier
interactive compile caught a const/mutable test memory-view mismatch before
candidate01; no archived raw log is claimed for that diagnostic.

Parent integration must atomically register the header/source and tests,
update supported-header ownership, ABI/docs and the twelve actual coverage
rows, then run exact combined-tree regressions and installed static/shared
Dense-only provider isolation. These are required gates, not completed by
standalone diagnostics. Repository owner license/import approval is separate
and remains unclaimed.

GELSY/GELSS/GELSD, GEQP3 and all remaining compact/generalized or other P06
families are unimplemented by this slice and remain required. Native coverage
is unchanged. No new PR or full P06 completion is claimed.

Next task is parent review and atomic integration of these frozen files.
To repeat the completed main local test from external p06-least-squares-03:

ctest --test-dir build-lp64-debug --no-tests=error --output-on-failure --test-output-size-passed 1000000

A subsequent implementation starts in its separately assigned linked worktree;
this reviewed output remains immutable.
