# Reference basic QR implementation review

Implementation-owner self-review, 2026-09-07, not repository-owner approval.
This freezes the basic full-storage reference QR slice only. No license/import
approval, complete P06 family, installed-component gate, or complete project
regression is asserted. No MdeCpp source, tests, tables or prose were used.

## Implemented scope

The optional Dense provider header adds 32 typed declarations: query and
execution for each of 16 actual pinned routines:

- S/D/C/Z GEQRF and GEQR2.
- S/D ORGQR and ORMQR.
- C/Z UNGQR and UNMQR.

These are raw expert interfaces, separate from native `FormHouseholderQ` and
`ApplyHouseholderQ`. Generation covers 0 <= k <= n <= m with k inferred from
the contiguous tau view, overwriting m-by-n A with the first n columns of Q.
Application consumes order-by-k raw reflectors, preserves them and tau, and
supports left/right, real N/T and complex N/C. Partial k is valid without a
whole-factor certificate. Factorization supports rectangular A and exact
min(m,n) tau. Native code and native evidence remain unchanged.

Both matrix layouts are supported. Caller-owned scalar WORK and
layout-conversion regions hold live correctly aligned scalar objects. Full
factorization input and C are packed in full. Q generation/application packs
only strict lower reflector tails; implicit unit diagonals, upper entries and
output-only columns are not read numerically. Padding is preserved. Raw
provenance, lifetime and absence of concurrent mutation remain caller duties.
Only the selected CPU context's host-accessible placement is accepted; no
allocation, transfer, densification, synchronization or fallback occurs.

Each query/execute requires a caller-owned report. GEQR2 computes fixed WORK(N)
without calling the provider. Other queries execute the actual LWORK=-1 path,
using real caller operands and one local scalar WORK(1), after source-proven
preflight. Their pinned query paths read no numerical operand values.
Empty query calls are still real calls; empty execution is a validated
no-op with absent INFO. Generation k=0,n>0 executes to form identity columns.
No rank, finiteness, conditioning or accuracy certificate is inferred.

Plans bind routine, scalar, provider identity, dimensions, flags, effective
foreign leading dimensions, original ASC strides and exact capacities.
Original ASC strides are options, not LP64-narrowed dimensions. A real backed
1-by-1 row-major INT64_MAX-stride test exercises query, packing and execution.
Genuine zero-storage 0-by-ABI_MAX factor and ABI_MAX-by-zero Q cases also run.
Execution does not repeat the foreign query.

All reachable operands, all supplied workspace regions and live borrowed
metadata are checked for overlap before writes. Aliased report storage is
rejected without resetting it. Structural failures do not call LAPACK or
mutate numerical outputs. Nonzero INFO is a provider defect for these routines:
raw signed INFO survives; negative argument indices survive except an
unrepresentable negation of minimum INTEGER. Packed matrix outputs are not
published after defects; direct outputs and directly written tau must be
treated as unusable. Only successful actual factorization receives the
Householder-QR factor-family tag.

## Source-pinned workspace arithmetic

The pinned ILAENV branches select NB=32. Raw preferred counts are
GEQRF n*32 (one for k=0), generation max(1,n)*32, and application NW*32+4160,
where NW is max(1,C.columns()) on the left or max(1,C.rows()) on the right.
The fixed application TSIZE is 65*64, independent of the selected NB.

Private pure count helpers reject source-integer products, sums, terminal
loop increments and ASC packing byte overflow before any foreign call.
These tests use exact integers, not fabricated huge array backing spans.

For S/C, the checked initial float conversion must remain below the exclusive
signed INTEGER bound before SROUNDUP_LWORK's integer reconversion. If it rounded
down, multiplication by 1+FLT_EPSILON must also remain below that bound.
At the upper LP64 boundary 2147483520 is safe but 2147483521 is not.
The corresponding tested true-64-bit boundary is 9223371487098961920; adding
one is unsafe. Tests also cover independent exact 2^24-neighbor constants.

D/Z QR assigns integer LWKOPT directly to double. The checked raw integer is
retained as a lower bound because the returned double can round down:
288230376151711776 rounds to 2^58. Tests include 2^53 neighbors and the
exclusive 2^63 boundary. Query scalar results, including imaginary zero, are
checked against the pinned formula; malformed, nonfinite, underreported and
unrepresentable results are rejected. These QR checks do not retroactively
verify GETRI or another wrapper; the separate LU audit owns those corrections.

## Frozen identities

All paths below are relative to the feature repository. Full dependency and
upstream routine hashes are in the external evidence directory
`../asc-cpp-evidence/lapack-array-io/p06-reference-qr-05`, hereafter E.

| File | SHA-256 |
| --- | --- |
| include/asc/dense/providers/lapack_qr.h | efec65fba584e857fcc67fc8d7cec70b8d3573f6628a75b998c20a32e5866d22 |
| src/dense/lapack/reference_qr.cc | 42c767133298843fc188b04861717b01c06762fa0db3ef9a5b550d33151e3b03 |
| src/dense/lapack/internal_qr_counts.h | b6467730381499d825ea286c0f5e535da325b4678e6535bb349e022c4b698b5e |
| tests/dense_lapack/reference_qr_test.cc | 746a49cdaab80c57506cd1ffb82102a4bc5b70a76de820a41573f3a3e33fc99c |
| tests/dense_lapack/qr_test_support.h | 86afc93fda447af82e6e5476b1e16231b4934bb22ab42f00c551229288e899c8 |
| tests/dense_lapack/qr_counts_test.cc | 19215b82fc32ac52373b0c0bb147e19b3af2090604304a7f19f1dc3d6f04db15 |
| tests/dense_lapack/qr_faults.h | de75df050c125a44f03b436e5d6d166ac01c8b7ae30da1f67d06456253f78249 |
| tests/dense_lapack/qr_faults.cc | 8808e1852f6d8cc5a574e7e5279111f605708b01298e001b00fec90523434bdc |
| tests/dense_lapack/qr_workspace_placement_test.cc | 4d5695ba42063a540bd858d4e1ea50079e62184179e794426431a8df417aa8c0 |

E/source.tar SHA-256 is
`f47e1a4cc0c128eb3caf2eb2478b7236589b55684350c29e2dcd0d4db6aec2be`.
This is a frozen diagnostic source archive, not an asserted Git commit.
The final placement regression has its own source archive, described below;
it is not retroactively included in the numerical archive.

E/verification-summary.md SHA-256 is
`50a6ea1ae1c0db09a9e16a087ccaa869263c45a339ae6ddcec1dec607896c014`.
E/evidence.sha256 is
`0c87bca6ec22268554ba8da721c08274026c2c33f034e0ef6d1b1d7c7184edda`;
it records final source archives, diagnostic libraries, configure/build/test
records, command logs, JUnit and full CTest logs for both final snapshots.

Reference source is LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, source lock digest
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
The exact source routine and helper hashes are in E/upstream-audit.sha256.
LP64 provider build identity is
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`;
true ILP64 is
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
They use the actual nonsuffixed Fortran symbols with separately checked
32/64-bit INTEGER declarations, not an LP64 library relabeled as ILP64.

The diagnostic compiles current frozen foundations
`df1d29ca7dd14307e0c0e834bfcc24d786a6a1e6157a9d048cf0210a47b468e2`.
Provider construction comes from the separate frozen candidate01
reference_lu.cc, hash
`03f8ed2d3d8093488182c89e0f563b6c84de618da123c5a990d8140f378828c3`,
and its private layout helper
`138fab5a7f8d9e7782cadf929e5fdfb853d54745ce70f1da7ec9eeefc77c6205`.
This prevents concurrent LU edits from changing a running diagnostic.
No LU capability is credited by that compilation. The final combined tree
must be tested separately. E/dependencies.sha256 records those sources,
both actual provider archives/headers/configurations, GNU runtime libraries,
Core archives and the allocation observer. GCC Debug and Release diagnostics
link the respective historical P03 Debug and Release Core archives; sanitizer
diagnostics link the Clang19 instrumented Core archive. These are not
full-project builds.

## Available verification and limits

Candidate05 Debug, Release, Clang19 ASan+UBSan, and separate Linux/glibc
C-allocation lanes each pass 5/5 actual CTests per ABI, zero failures and zero
skips. Records and commands are
E/test-{lp64,ilp64}-{debug,release,asan,libc}/record.json.
No unexecuted declaration is counted as a verified capability.

Each scalar process executes 72 factor-case events and 60 Q-mode groups
(180 generation calls and 480 apply calls). Boundary combinations can repeat;
these are executed events, not unique coverage rows or CTest counts.
Every full LastTest.log contains 528 profile events across four scalars.
Earlier candidates' JUnit captured only a truncated passed-output prefix.
Candidate05 uses enlarged passed-output capture; full external JUnit and
build-*/Testing/Temporary/LastTest.log files retain all 528 events.

Independent widened arithmetic constructs Q as products of raw reflectors,
checks Q^H Q and Q R reconstruction, and compares Q generation and application
against those products. Tests include tall/square/wide/empty, repeated-column
rank deficiency, zero matrices, 131-by-129 blocked execution, minimum/preferred
WORK, both layouts, all application flags and multiple right-hand sides.
Ignored input entries are NaN sentinels; padding and preserved factors/tau are
checked. Tests inject all-scalar negative/minimum/positive INFO and malformed
queries, validate report-surviving failures and row-major no-publication,
and reject short/aliased workspaces, stale capacities, bad tau strides/lengths,
unsupported flags and placements before foreign execution.

The C-allocation lane replaces, rather than stacks, the C++ new observer with
an executable-symbol Linux/glibc interposer. Positive controls observe direct
malloc and shared libstdc++ operator-new allocations. Checked query/execute and
failure paths observe zero allocations. This is exact-runtime evidence, not a
claim about private allocators on every platform. ASan owns allocation in its
separate lane; the uninstrumented pinned Fortran archives are not represented
as fully sanitizer-instrumented providers.

Strict Clang18 tidy passes the four owned TUs. GCC11 C++20 -fno-exceptions
public-header standalone compilation passes. Actual Doxygen1.9.8 with HTML
enabled passes without warnings and has 32 functions, 32 parameter lists and
32 return sections in namespace XML. This is a header diagnostic, not the
complete repository Doxygen/ABI coverage gate.

The separate final workspace-placement snapshot is
`../asc-cpp-evidence/lapack-array-io/p06-reference-qr-placement-02`.
Its source.tar SHA-256 is
`ea4cc677c90204a16355e7be513d59108b76aa3d366ba5cde62919695fa41316`.
All eight ABI/configuration lanes pass 1/1 actual CTest with zero failures or
skips, linking the corresponding exact candidate05 diagnostic library.
Each process executes 96 rejection cases: four scalars, four routes,
nonempty scalar/layout workspace roles and pinned/device/managed placements.
They require kMemoryAccess, no foreign call, absent INFO, unchanged outputs
and zero observed allocations. Strict Clang18 tidy passes this additional TU.
This is a separate 1/1 execution, not a relabeled 6/6 numerical run or a GPU
hardware test. The provider checks its own context against every workspace
region; the storage-neutral foundation's placement policy was not changed.

## Preserved failures and remaining gates

Initial syntax diagnostics caught wrong existing API spelling and were fixed.
Candidate01 double LP64 math passed; its strict tidy findings were mechanical
and then corrected. Candidate02 configure attempts used system CMake3.22 and
an unavailable Ninja executable; later diagnostics use CMake4.1.2 and Make.
Its build also caught a concurrent LU private-header dependency; that snapshot
and failure remain intact, and later diagnostics explicitly use frozen LU
construction sources. Candidate03 passed both ABI CTests.

Candidate04 initially treated authoritative foreign headers as ordinary ASC
warning inputs under Clang19; its unused C-linkage complex-return factory
declarations triggered -Wreturn-type-c-linkage. The separate sanitizer project
marks only the third-party include directory SYSTEM, retaining all ASC
warnings-as-errors and all tests. The original failed builds remain recorded.
An XML-only Doxygen run reported phantom missing parameter/return docs despite
complete XML; the repository-compatible HTML-enabled rerun passed without
changing the header or disabling warnings. Candidate04 also passed its final
Debug tests. Final self-review then found that new pure packing-count tests
were not yet the production gate: candidate04 used the older shared checked
packing helper. Candidate05 routes QR packing through its independently tested
private count helper plus the existing aggregate-byte check. Capacities and
publication semantics are unchanged; all eight numerical lanes were rerun.
Placement01's runtime checks passed, but strict tidy rejected two unused
includes. Final placement02 removes only those includes and passes all eight
runtime lanes plus strict tidy; the failed style log remains preserved.

Root integration still must register the supported header, private source,
private count helper and tests atomically with ABI/ownership/coverage consumers,
then run the complete exact integrated Debug/Release/sanitizer suites and
installed static/shared Dense-only provider isolation. Test-only GNU symbol
wrapping is static-link fault evidence, not a claim that it intercepts calls
inside an already linked shared provider. Integration must retain an applicable
fault lane rather than silently skip it.

No source/provider import owner approval, CUDA gate, release/package publication,
new PR, or full P06 closure is asserted here. P06 QR/LQ/QL/RQ/RZ,
compact/blocked/generalized factorizations, remaining Q experts and all
least-squares drivers beyond this slice remain required by the runbook.

The root integrator's exact next task is to register this frozen slice and run
its complete combined-tree gates. The already completed local diagnostic is
reproducible with E expanded to its local directory:

`ctest --test-dir E/build-lp64-debug --no-tests=error --output-on-failure --test-output-size-passed 100000`

The matching ILP64 command and all other lane commands are retained in the
records. No additional P06 slice will start in the shared feature worktree;
the next assignment requires the separate linked worktree directed by root.
