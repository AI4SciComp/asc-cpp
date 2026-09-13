# Native Householder QR implementation review

Implementation-owner self-review, 2026-09-07; this is not repository-owner
approval. This freezes only the native basic Householder QR
slice of P06. It is not approval of a license/import request, a completed P06
provider family, a least-squares driver, or a full project/installed test gate.
No MdeCpp source, tests, tables or prose were used.

## Scope and contracts

Twelve typed overloads implement `Geqrf`, `FormHouseholderQ` and
`ApplyHouseholderQ` for float, double, complex<float> and complex<double>.
GEQRF covers tall, square, wide and empty matrices with native packed R/tails,
explicit exact-length contiguous tau and caller scalar scratch. Safe scaled
norms avoid naive squaring and alpha-beta overflow, including subnormal stored
diagonals. Complex factors use the raw real-beta convention
H^H*[alpha;tail]=[beta;0], H=I-tau*v*v^H, Q=H0*...*H(k-1).

The separate-output Q conveniences preserve a successful native factor. They
form full/economy Q or apply full Q on the left/right, real N/T or complex N/C.
They are deliberately not named or credited as complete ORG/UNG/ORM/UNM expert
interfaces. Native unpivoted QR does not certify numerical rank or conditioning.
Full least-squares/provider requirements remain in the runbook.

Both row/column-major BLAS descriptors are used directly, preserving padding.
There is no foreign fallback, allocation, packing, transfer or synchronization.
The existing serial context permits only host memory; pinned/device/managed
placements are rejected without broadening Core access. Scratch capacities and
increments are checked on each call, not obtained from a stale cached plan.
Reports initialize before ordinary validation. Overlapping report storage is
rejected before resetting it. Structural/nonfinite-input failure leaves
numerical outputs unchanged; computed norm/update overflow reports
partial/unusable output with zero-based reflector diagnostic. Native operations
never fabricate foreign INFO or called_provider. Failed raw buffers cannot be
accepted as a successful factor. BLAS and Random are unchanged.

## Exact identities

The external evidence root, relative to the repository, is
`../asc-cpp-evidence/lapack-array-io`. Final native source snapshot is
`p06-native-qr-04/source.tar` under that root,
SHA-256 `2ea10318d0f645a34c2c104e63c7f7f9450e54558470a871691ed7a29f19ca2e`.

| File | SHA-256 |
| --- | --- |
| `include/asc/dense/lapack/qr.h` | `cb5af236141fe99f9e14a1ae7a438eb7ddeff97ea81addc20b65acf272f029b5` |
| `src/dense/lapack_qr.cc` | `a362791fb1e7eb084d2a77510bf17432f7fdcb05ad45925e3c546ad329056a69` |
| `tests/dense/lapack_qr_test.cc` | `22aec42c0cf4cb95cdb6678fd59bb852b6b8a20e75dfa9dd4eeb533f9a1658c8` |
| Existing foundation TU compiled by diagnostics | `df1d29ca7dd14307e0c0e834bfcc24d786a6a1e6157a9d048cf0210a47b468e2` |

The detailed external
`p06-native-qr-04/verification-summary.md`
has SHA-256
`e7a00dc4ad932e64e42fb5f9eec0217f684416b4f1cd29bf365cc744da6fdc36`.
Every named build/run directory contains full argument-vector commands,
source-before/after identities, dependency/library/executable hashes and raw
logs in `record.json` and `command.log`. These are local paths, not remote CI
links or actual pull requests.

## Verification actually executed

The final `p06-native-qr-04` lanes all pass: GCC 11.4 Debug and Release with
C++20/-fno-exceptions/warnings-as-errors; Clang 19 ASan+UBSan; Linux/glibc C
allocator interposition; standalone public-header compile; actual clang-tidy
18.1.8 on both owned TUs with repository configuration and warnings-as-errors.
`build-*` and `run-*` identify each exact command. Final direct diagnostics
compile the frozen existing foundation TU plus native QR and link the exact
matching-mode Core archive from `p03-sparse-13`; they do not claim to be a full
Dense/project build.

Each runtime executes and then emits 248 successful profile records, covering
all four scalar types, both input/output layouts, 5x3/3x5/3x3/1x1/0x3/3x0/0x0,
general/rank-deficient repeated-column/triangular/zero matrices, and normal,
near-maximum, small-normal and minimum-subnormal scales. Independent widened
arithmetic checks Q*R reconstruction, Q^H*Q, and left/right N/adjoint
applications with multiple RHS. Full/economy shapes, preservation of factor/tau,
padding and red zones are checked. The only absolute residual allowance is
explicit denormal-rounding error, not an additive one that hides tiny inputs.

Additional executed assertions cover hand-derived 3-4-5 and complex phase
reflectors, an overflowing naive alpha-beta with representable correct result,
implicit tail annihilation without taking explicit Q as input, short/wrong
workspace and tau, aliases, invalid flags, inaccessible placement, NaN input,
norm overflow, report initialization/partial failure, failed factor rejection,
INT64_MAX-by-zero bounded empty traversal and byte-exact report alias rollback.
Two narrow analyzer exclusions are limited to deliberate invalid flags of the
existing fixed-uint8_t enums; 99 is a valid underlying representation, not a
C++ out-of-range conversion. No production validator or numerical test is
disabled.

These profiles are not 248 CTest tests. Each is an actually exercised group
inside a directly executed test process; whole-project CTest and installed
static/shared isolation remain integration gates below. Sanitizers own the
allocator in their lane. Exact zero allocation observations come from regular
Debug/Release operator-new probes and the separate glibc interposer, whose
positive controls see direct malloc and shared libstdc++ allocations. This is
bounded Linux/runtime evidence, not a universal claim about private libc paths
or all platforms. No unexpected CTest skip or zero-test run is counted.

## Raw reflector interoperability

The optional external oracle consumes native packed factors using the actual
Reference LAPACKE ORGQR/UNGQR work interfaces. It explicitly copies to an
oracle-only column-major buffer and supplies workspace; no dependency/call is
added to the native implementation. Each of the separate LP64 and ILP64 runs
passes 32 combinations: four scalar types, 5x3/3x3/3x5/1x1, both native layouts.
Full Q agrees with the native convenience. This verifies raw conventions, not
ASC reference-provider API coverage.

Pinned reference source commit is
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, v3.12.1. Exact separately installed
archives, headers, Fortran runtimes and ABI flags are hashed in
`build-interop-{lp64,ilp64}-04`. Final driver SHA-256 is
`94a2a6c96e78476d1cd99c9ec59932f8d15618065c6e6920d763d028756ac9c6`;
`run-interop-{lp64,ilp64}-final` and `tidy-interop-02` pass. Initial and
intermediate drivers are preserved in external tar files, not relabeled as the
final source.

## Preserved failures and exact next integration task

Early local fixture compilation corrected a nonliteral constexpr context,
nonexistent report member and wrong enum spelling. Candidate01's incorrect
pinned-success expectation failed after its real32 profiles; existing host-only
semantics were preserved and the rejection tested. Candidate01 style findings
were corrected or narrowly justified; candidate02 passed Debug/style.
Candidate03 added report-alias/hostile-empty tests and passed Debug/style;
Release/ASan preparation found absent historical scoped Dense archives before
compiler execution. Candidate04 compiles the exact frozen foundation TU
directly. Initial external oracle compilation lacked
HAVE_LAPACK_CONFIG_H, leaving C99 types despite LAPACK_COMPLEX_CPP; the documented
switch fixed it. Driver formatting/direct-include diagnostics were also
corrected and rerun. All failed records remain external.

Next task: the integrator registers only the frozen native header/source/test
and existing allocation probe/tests-root include, updates public ownership,
ABI/Doxygen inventories atomically, then configures/builds the complete exact
tree and runs nonzero full Debug/Release/sanitizer CTest with
`ctest --test-dir <external-exact-tree-build> --no-tests=error --output-on-failure`.
Static/shared installed Dense-only component-isolation and strict Doxygen gates,
coverage/evidence integration and reviewable commit are still required.
Do not restart native QR design or fold later provider identities into this
record. P06 provider QR/LQ/QL/RQ/RZ/generalized/blocked and least-squares scope is
still required; this native slice does not reduce it.
