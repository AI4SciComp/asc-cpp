# P05 positive-definite band factor and solve slice

Status: implementation candidate frozen; both-ABI scoped numerical and
sanitizer verification passed. Root integration remains required and no
full-family/native coverage is claimed.
Scope is exactly the twelve pinned S/D/C/Z PBTRF, PBTF2 and PBTRS routines.
PBSV/PBCON/PBRFS/PBSVX/PBEQU and every other remaining P05 row remain required.

## Isolation and inputs

Worktree `asc-cpp-p05-band`, branch `feature/lapack-p05-band`, starts at
`3d5909d6c26ba2d274749106bda448e2280b8330`. Root integration and earlier
placement worktrees are not modified. No commit or push is part of this bounded
owner task. Raw artifacts are external under `p05-band-NfJwApO7`, relative to
the existing program evidence root.

Actual repository instructions, applicable runbook mission/protocol,
architecture, P01/P04/P05 and cross-cutting sections were read before editing.
Live [Google C++](https://google.github.io/styleguide/cppguide.html) and
[Python](https://google.github.io/styleguide/pyguide.html) guides were retrieved
on 2026-09-07. Existing C++20,
Google-derived Clang18 rules and accepted repository compatibility exceptions
remain binding. No MdeCpp material is used.

## Descriptor decision

Reuse `LapackPositiveDefiniteBandView` in
`include/asc/dense/lapack/structured_view.h`, with the integrator-approved
additive layout constructor and layout accessor. Retain its old column-major
constructor, storage metadata and class member layout. `order()` already
exists; no duplicate size vocabulary or duplicate descriptor is introduced.

For zero-based coordinates and physical stride ld, column upper is
AB[kd+i-j+j*ld], column lower is AB[i-j+j*ld], row upper is
AB[i*ld+j-i], and row lower is AB[i*ld+kd+j-i]. The physical table is ld-by-n
column-major or n-by-ld row-major, never a densified mathematical matrix.
Full n*ld backing is checked even for padding/corner slots. The only existing
consumers at the base commit are the descriptor's focused unit tests; there
is no existing production kernel that could silently misinterpret row layout.
The BLAS triangular-band descriptor is not a substitute: it excludes legal
nonempty LAPACK kd>=n cases.

## Source audit in progress

Pinned PBTRF/PBTF2 factor AB into upper U with A=U^H U or lower L with
A=L L^H (transpose for real). PBTRS takes const factors and overwrites each
RHS with its solve, with independent RHS layout packing implemented. All three
routines have no caller numerical WORK parameter. Row-band conversion does
use caller-owned O(n*(kd+1)) band storage, not n*n densification.

Pinned ILAENV selects NB=1 for PBTRF kd<=64 and NB=32 for kd>64; test both
actual algorithm branches. PBTF2 is absent from lapack.h and requires a
private compiler-derived, directly probed prototype route. PBTRF and PBTRS
are present in the audited pinned header. The source's PBTF2 test AJJ<=0 has
no NaN predicate: raw INFO=0 is not a finite-factor guarantee. Preserve this
distinction and do not relabel nonfinite results as detected non-positive
definiteness.

## Diagnostic evidence, not final immutable verification

Exact pinned source commit is
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, tag object
`5ebe92156143a341ab7b14bf76560d30093cfc54`. Source-input manifest identity is
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
The existing full upstream inventory bytes have SHA256
`1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7`.
Exact twelve-row extraction, including declarations/argument-document hashes,
is external `exact-twelve-inventory-01.json`, SHA256
`2615f77b305262318f8a6b71e536a9978679f4fd1c798dee411c24c0733597a0`.
This is a slice extraction from the source-generated denominator, not a new
guessed inventory or LAPACKE-only denominator.

Provider identities remain LP64 build
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`
and true ILP64 build
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
Both are the already attested Reference-LAPACK 3.12.1/Reference-BLAS stacks;
neither uses an allocating LAPACKE row-major adapter. PBTRF/PBTRS prototypes
come from audited `lapack.h` SHA256
`225f20409a7d6674953f5af1de8c47188bb792ca11d46d465bb28dde917fcafc`.
PBTF2 is genuinely absent there. Its private generated prototypes retain
GFortran 11.4 integer width, trailing size_t character length, and audited
libstdc++ 20230528 complex layout. The independent direct probe includes
compiler-generated declarations, not ASC's handwritten private declarations.

Both actual ABI direct PBTF2 probes pass all four scalar/upper-lower routes,
with compiler-generated prototypes, real INFO width/sentinels, positive
failure and empty calls. See `pbtf2-prototypes-{lp64,ilp64}-03.h`,
`pbtf2-direct-probe.cc`, and `pbtf2-direct-test-{lp64,ilp64}-01.log` under
the external slice directory. The first local compiler command lacked the
required GCC tool-directory prefix; the failure log is retained. No privileged
installation or provider rebuild/change occurred.

The initial descriptor regression passes in
`structured-test-diagnostic-01.log`. Both ABI diagnostic runs now pass 13/13
tests with zero skips (`test-{lp64,ilp64}-diagnostic-05.log`). These are
incremental live-worktree checks, not immutable final evidence. Tests include
independent wide-precision reconstruction and scaled residuals, n=96/kd=65
blocked execution, kd>=n, both layouts and independent RHS layouts, repeated
multi-RHS solves, real/complex inputs, padding/corner and imaginary-diagonal
NaNs, source-matched partial failure, raw triangular solve fidelity, pure
32-/64-bit source integer bounds, and typed injected foreign failures.

Retained diagnostic harness failures: the first adapter harness omitted the
installed `lapacke_mangling.h` include directory (build diagnostic 01). The
first validation test tried to construct device/managed band descriptors;
the established descriptor correctly rejects these before provider entry,
so the test now asserts descriptor rejection and separately tests every
workspace role. Its abort/backtrace remains preserved. The next validation
test expected kInvalidArgument for a nonempty null workspace; the existing
neutral validator's actual contract returns kMemoryAccess. This exact
expectation was corrected without weakening the rejection/no-call/no-mutation
assertions (failed tests diagnostic 03/04 remain). No production change was
needed for either validation-test correction.

PBTRS intentionally has no native-Cholesky diagonal predicate. Its raw API
preserves actual negative/nonreal triangular components; independent products
of these raw triangles solve correctly. Zero/NaN raw-factor cases match direct
source INFO=0 and nonfinite outputs, and are fidelity tests, not successful
finite-solution gates. PBTF2 NaN INFO=0 and blocked PBTRF NaN positive INFO
remain distinct exact-source behaviors, not fabricated uniform semantics.

## Source-conditioned arithmetic and allocation boundary

`internal_band_limits.h` performs pure bounded integer checks, tested at both
actual integer maxima without imaginary nonempty buffers. All sources evaluate
KD+1 even before a zero-order return, so KD must be strictly below the selected
maximum. N/KD/foreign LDAB and N/NRHS/foreign LDB are checked independently.
Source PBTF2/PBTRS unit-step loops require their final increment to fit, not
only their last accessed index. PBTRF's actual kd>64 loop has step 32; its
exact terminal 1+32*ceil(n/32) is checked without overflow. n=0 quick returns
do not acquire those nonempty-loop bounds.

For upper PBTF2, KN=min(kd,n-j) is at most min(kd,n-1); SCAL, real SYR,
complex HER/LACGV and their last strided cursor updates require
1+KN*max(1,LDAB-1) to fit. Lower PBTF2's vector stride is one, so it does
not acquire an unrelated LDAB-scaled vector bound. Blocked PBTRF calls POTF2
on IB<=32 and LDAB-1: upper SCAL/GEMV and lower DOT/GEMV/LACGV strided
vectors have length at most IB-1, covered by the checked 31-entry bound
(or min(n,32)-1 for a smaller matrix). Nested positive dimensions and matrix
leading dimensions are valid by kd>64, IB<=32, and LDAB>=kd+1. I2<=kd-IB
and I3<=IB are used only when positive. Source I+IB, I+II-1, JJ+I+KD-1
and trailing-block coordinates are bounded by n+1 or kd+1 under their actual
branch conditions; they do not justify a blanket n*LDAB foreign-integer cap.

PBTRS always calls TBSV with INCX=1. Its lower route evaluates J+KD before
MIN(N,J+KD), including J=N, so n+kd must fit for nonempty solves. The upper
route only subtracts KD and does not inherit that lower-only bound. Empty
n or nrhs suppresses TBSV loops, but not argument validation. Original ASC
row strides live in plan options; actual packed foreign LDAB=kd+1 and
LDB=max(1,n) live in ABI dimension fields. Real empty wide-row-stride tests
exercise this role separation. Packing counts and summed bytes remain
ASC-sized, checked for scalar/aggregate overflow and all workspace aliases.

External `audit-band-closure.py` records both full graphs and the separately
source-conditioned graphs in `static-closure-{lp64,ilp64}-01.json`. Each
full graph has 59 static objects; the checked graph has 58 after excluding
only XERBLA, whose outer and nested invalid-argument routes are prevented by
the above admission rules. No allocation symbols appear on the checked graph;
its actual external leaves are logf, lroundf, memcmp, memcpy and memset. The
full graph explicitly retains GFortran write/stop/string-trim leaves reached
through XERBLA; they are not silently discarded as universal allocation proof.
Pinned PBTRF has fixed local WORK(33,32), compiled with the attested
`-O2 -frecursive` flags, not a hidden dynamic n*n buffer. Actual static-call
malloc/calloc/realloc/aligned_alloc/posix_memalign probes and ASC C++ new
probes surround queries/executions and selected direct probes. Dynamic-runtime
internals are not interposed, and no other LAPACK family inherits this evidence.

Previous GEEQUB-subnormal, GERFS-tiny-estimate and GESVX-tiny-condition
mathematical gates remain explicitly unmet; this separate band slice does not
close them or authorize an upstream patch. Owner redistribution review also
remains a distinct gate, not approved by implementation authorization.

## Exact next task

The frozen LP64 and true ILP64 Clang18 strict passes both completed successfully.
Hand off only the fourteen owned code/test files and this review to root.
Root must atomically
register the new header/source, update ABI/header ownership and exact twelve
inventory mappings, and rerun its coherent both-ABI/provider-free/installed
consumer and full-program gates. Do not modify the provider identity or
restart the layout/source design. The external harness is
`p05-band-NfJwApO7/CMakeLists.txt`; a reproducible next verification command is
`ctest --test-dir p05-band-NfJwApO7/build-release-lp64-candidate-01
--output-on-failure -j 2` with the evidence-root prefix expanded and a new
external raw-log path.

## Frozen candidate and available verification

Frozen source is external `p05-band-NfJwApO7/source-candidate-01`, identical to
the owned worktree files by pre/post SHA checks. `candidate-01.sha256` lists
all fourteen code/test identities, and has SHA256
`a36aff8202ae926976df21e797134f909d741fbce15bd8616ddb6fb0e59fbf77`.
The source archive including the separately reviewed placement helper is
`source-candidate-01-with-dependency.tar`, SHA256
`a5149519af85d09a63bf3c0ccb15b3a97c309210deed964e362f5015e9b38dc2`.
No commit, push, branch merge or external repository mutation was performed.

Key code identities:

| File | SHA256 |
| --- | --- |
| `include/asc/dense/providers/lapack_cholesky_band.h` | `4912a0b8e6b3afba02ecac3850c8c6b45f2808374ddb72f7b316640387bf1629` |
| `include/asc/dense/lapack/structured_view.h` | `d299869ff264a118bdf9793c4e623bb96472f62ce54a6c6ee3368ddb53f019b6` |
| `src/dense/lapack/reference_cholesky_band.cc` | `461420f1f5604f77906a7bb6267237dbbadfad1e9f8d09725e9ff627556b83d6` |
| `src/dense/lapack/internal_band_abi.h` | `b54abdaf843cf73dc4e3b6bdfccece5e140e8b05d284cebb2eb7ab9000aa3582` |
| `src/dense/lapack/internal_band_limits.h` | `8eed133f8b9d2ea182098c488b7cf7681292f254d824ccee6b269a20e0e5b9ee` |

The existing descriptor preimage at base 3d5909d is
`3e0d013a7806303eee34e398bb537a0aaeea5854c64b3384787fbd0016d6975c`;
its existing test preimage is
`56fa2ecbce13755bbbb12fe882e20efc6936976cd104fd6530217e8581996baa`.
The additive change does not add/reorder class members or change the old
six-argument Create, column order, diagonal-row or storage semantics.

All six actual test lanes passed 14/14, zero skips: Debug and Release for
LP64/true ILP64 (`test-{debug,release}-{lp64,ilp64}-candidate-01.log`), and
Clang19 ASan+UBSan for both ABIs (`test-asan-{lp64,ilp64}-candidate-02.log`).
The four scalar math tests, four scalar direct-source fidelity tests, four
scalar validation/fault tests, pure integer-limits test and existing descriptor
test are distinct. Fault endpoints are explicitly test-only and never count
as numerical/provider capability. Production predicates/packing and tests
were frozen before these runs.

`evidence-summary-01.json` verifies all six nonzero registered test inventories,
positive completion/zero-skip records and hashes actual logs, CMake caches and
compile commands; its SHA256 is
`a99f6cd51e3ef69dab28f31b9b51caaba1e7023a4e85546de470deb0bc253fa9`.
`build-inputs-01.sha256` records every reused Core/Dense/provider archive,
both generated ABI configs, private layout/context dependencies and GFortran,
quadmath and libstdc++ runtime leaves; its SHA256 is
`9642eb37cb858dcbb6e0bf4f8fbd26ea0dd2ef845c7c00b0a153ec3dd7caeb3a`.
These dependency archives are the explicitly recorded frozen p04-lu-v2-02
builds, not a claim that new root integration was tested. The borrowed private
context helper is exactly the already reviewed placement candidate
`86c8606e1590945f35781c041249380214ac4e0f39cee9bf1309d311ea2fdb94`.

Sanitizers instrument the new C++ band adapter and tests. The reused existing
Core/Dense/provider-selection archive and Fortran/BLAS objects are not
instrumented; red zones, source audit, direct ABI probes and allocation
observations complement but do not remove that limitation. The first sanitizer
build correctly diagnosed upstream lapacke_config's C-linkage complex-return
declarations because the diagnostic harness omitted the existing SYSTEM
third-party include convention. Candidate-02 changes only that external
harness classification, not source, tests, warning levels or assertions.
The failed candidate-01 build logs remain preserved.

Strict frozen Clang18 format, provider-free/no-exception public/private-header
compilation and both native-ABI private-header compilation pass. Scoped
Doxygen 1.9.8 HTML+XML with warnings-as-errors passes; the generated XML is
checked for all 24 provider declarations and all eight public band-descriptor
members, including old/new Create arities. The original XML-only harness
reported missing parameters despite retaining complete XML parameterlists;
using repository HTML+XML/autobrief semantics produced zero warnings with
unchanged source documentation. A first count helper mistakenly inspected a
private constructor retained in XML; the corrected helper filters public
members and checks the exact expected public inventory. Both failed harness
records are retained; no public declaration/documentation requirement was
disabled.

Both frozen Clang18 strict passes cover the adapter, math/fidelity/validation
tests, typed fault support, pure integer tests and existing descriptor test:
`tidy-candidate-01.log` and `tidy-ilp64-candidate-01.log`, exit zero under
the actual repository warnings-as-errors configuration. No enabled check was
removed. Intermediate include-cleaner, helper-size and test bit-comparison
findings were fixed with direct includes, smaller test helpers and explicit
byte-representation assertions; the original failed strict logs remain.

Root registration requirements: add the one new provider `.cc` to the existing
Dense-owned provider target, install/register the single optional public header,
and retain the two new private band headers plus existing layout/context
helpers. Register S/D/C/Z invocations of `band_cholesky_test.cc`,
`band_cholesky_fidelity_test.cc`, and `band_cholesky_validation_test.cc`, and
the separate `band_cholesky_limits_test.cc`. The descriptor test already has a
root registration and only gains additive cases. Fidelity/validation tests
need the actual generated provider config and SYSTEM foreign include path;
validation additionally links `band_cholesky_fault_support.cc` with GNU ld
wrapping all twelve `spbtrf_` through `zpbtrs_` symbols. Math/fidelity/validation
reuse existing allocation_audit/allocation_probe with the existing six C
allocation wraps. No native routine count or unrelated package mapping changes.
