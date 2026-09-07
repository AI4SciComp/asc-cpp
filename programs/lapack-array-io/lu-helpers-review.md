# Reference LASWP/LAQGE helper implementation evidence

This is a separate implemented eight-row S/D/C/Z helper slice with completed
bounded frozen checks, pending integration. It does not
change the previously frozen ordinary/advanced LU or equilibration source
identities and does not close P04 or the full provider profile. All paths to
raw evidence below are relative to the external program evidence root.

## Exact source and private interoperability

The Reference-LAPACK 3.12.1 source commit/tree/manifest and both actual ABI
provider archives in `provider-abi-review.md` apply unchanged. All eight full
source argument sections and executable bodies were read before binding.
Actual pinned source SHA256 values are:

| Source | SHA256 |
| --- | --- |
| `SRC/slaswp.f` | `55402df99bbbcb228c9cac279a7afa7b20ede80fe1eb81fc1e321a2470a33bb0` |
| `SRC/dlaswp.f` | `8f50f034f7c1c075db12dcb277e743fdd4938d788fde864a1adcca70fedbc102` |
| `SRC/claswp.f` | `1a53df55ff57288a51eaa1a50d9d6dccf6254e03ac8a78e54a4ac88942c9359e` |
| `SRC/zlaswp.f` | `f117b1062509b0013ee4aa5e9f2338e072ddb5793be5869c3d0e5a454768a45e` |
| `SRC/slaqge.f` | `685370bca7b7afccf7ccaddd22fb1b325a36b02312200204affd7efdf08fd7e0` |
| `SRC/dlaqge.f` | `5132c2bef38be6293fbe0a25d07a6001b727ecef18678be250fe89f1edef8c8f` |
| `SRC/claqge.f` | `63212ed13fd72089a6e5d5987446a4a340ed0cadb85f2febb70c835360566085` |
| `SRC/zlaqge.f` | `7f9e8836ec39bfc4fdde6c6be9be20019784fce4f390ad694cf1a8e7f1d1e519` |

LASWP has authoritative declarations in the pinned `lapack.h`. LAQGE does
not. Its private declarations are derived from the exact provider compiler,
GNU Fortran (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0, executable SHA256
`61bf7aa223e378dba0978c92e951c95a4e8124f8efda72f0e1fd9166a35c6bd4`.
Actual `-c -fc-prototypes-external -fsyntax-only` runs, with
`-fdefault-integer-8` for ILP64, exited zero for all four pinned LAQGE sources:
`logs/p04-lu-helpers-laqge-{lp64,ilp64}-generated-prototypes-02.log`.
The compiler emits ordinary underscored names, int/long pointer arguments,
explicit C++ std::complex specializations and trailing size_t EQUED length.
No signature, mangling, complex layout or character length was guessed.
The already audited exact libstdc++/Fortran character and complex ABI still
applies. A different compiler/provider ABI is not authorized by these probes.

The first prototype invocation emitted declarations but then attempted an
unwanted link phase and failed on the extracted compiler's linker-plugin
location. Both failed revision-01 logs are retained; adding `-c` makes this
strictly the intended front-end check without changing the provider.
The live [GNU interoperability option documentation](https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gfortran/Interoperability-Options.html)
and [argument conventions](https://gcc.gnu.org/onlinedocs/gfortran/Argument-passing-conventions.html)
were consulted. Compiler-generated prototypes are specific ABI evidence, not
portable bind(C) declarations. New ordinary C++ consumers need no Fortran
compiler; these checks are external dependency/audit preparation only.

`lu-helpers-direct-probe.cc` includes the actual compiler-produced declarations
and calls pinned LAQGE without ASC. Both ABI compile/run revision 01 pass:
`logs/p04-lu-helpers-direct-{lp64,ilp64}-{compile,run}-01.log`. Independent
long-double scaling equations, EQUED and character redzones, unchanged R/C
and matrix padding, all four scaling branches and exact threshold boundaries
are checked. Direct LAMCH S/P outputs also equal the IEEE minimum/epsilon used
for checked source-branch validation. This ABI/source comparison supplements,
not replaces, independent mathematical checks.

## Checked contracts

`QueryLaswpWorkspace`/`Laswp` use zero-based first_row and row_count, mapping
to source K1=first_row+1 and K2=first_row+row_count. The raw payload remains
one-based sequential swap destinations with the LU encoding tag. Only raw
slots first_row+j*abs(INCX) are read and checked in [1,m]. Negative INCX
reverses application order, not the row/pivot association. GETRF's stronger
lower bound on individual pivots is deliberately not imposed on this helper.
Zero increment, zero selected count and zero columns are explicit no-ops.

Foreign pivots occupy caller kInteger capacity through the last selected
position; only selected slots are constructed/written. Prefix and gap bytes
remain untouched. Nonempty row-major calls pack/unpack original A using
explicit m*n scalar objects, preserving mathematical values and padding.
Private pure integer predicates check final forward IX and I updates,
negative initial IX arithmetic, and 32-column/tail loop completion. A single
reverse swap may legally use the minimum signed provider increment; its
absolute magnitude is handled unsigned without overflow. Extreme predicate
tests do not forge nonempty backing spans or allocate enormous arrays.

`QueryLaqgeWorkspace`/`Laqge` use existing typed real statistics and
LapackEquilibration, not duplicate enums. Finite [0,1] condition ratios and
finite nonnegative AMAX select the exact pinned thresholds. Used R/C vectors
are contiguous exact m/n and finite positive. Unused vectors may be empty or
full and their values remain unread. Stats determine the source branch rather
than being recomputed; their exact bit patterns bind plans. Empty matrix calls
do not inspect statistic/scale values and only produce kNone locally. A
nonempty no-scaling route calls actual LAQGE without reading A or packing
values; its row-major plan still reserves the declared live packing objects.

Both routines lack native INFO. Reports always retain absent native_info,
including actual foreign calls, and never certify a successful LU factor.
Structural failures precede all mutation/provider entry. Unknown or wrong
returned EQUED is a provider defect with unusable results and retained raw
matrix effects; no INFO is invented. All CPU placement, descriptor/workspace
alias, capacity, alignment, metadata, statistic and provider-identity checks
are performed before foreign entry. No static selection or handler is added.

## Actual diagnostic progress and preserved mathematical failure

Initial both-ABI optimized compile/test revision 01 passes all four scalar
processes. Revision 02 fails compilation because test code incorrectly
default-constructed MutableMemoryView; it was corrected to an explicit empty
host view without weakening checks. Both-ABI revision 03 then passes all four
processes, including workspace/operand faults, changed plan/statistic bits,
unsupported placement, unused slots/scales, pure integer boundaries, actual
minimum negative INCX, empty/wide ASC strides and injected EQUED defects.
These remain diagnostic live-owned-file runs against immutable dependencies
`lu-helpers-base-YjvMFDAG`, not final frozen integrated evidence. That dependency
snapshot derives from `lu-equilibration-stride-68Q9JMsI` and adds only the frozen
driver enum/header. It does not use the integrator's changing LU/QR sources.

A genuine tiny-input mathematical gate is unmet even with scales produced by
GEEQU, not arbitrary inconsistent scale vectors. For A=diag(tiny,1), with
tiny=minimum_normal/1024, both ABIs/all four scalars produce GEEQU INFO=0,
R0=1/minimum_normal, C0=1024, ROWCND=minimum_normal, COLCND=1/1024 and AMAX=1.
LAQGE selects B. The mathematical scaled matrix is identity, but source
C[J]*R[I]*A[I,J] overflows the first two-factor intermediate: real A00=Inf,
complex A00=(Inf,NaN). Independently reordered long-double arithmetic gives
exactly one. This is not singularity and not successful finite scaling.

The original full-mathematical-success probe is preserved as
`lu-helpers-tiny-mathematical-expectation.cc`; both raw runs exit 4 in
`logs/p04-lu-helpers-tiny-{lp64,ilp64}-run-01.log`. No source patch or numerical
reassociation is inferred. The accepted explicit postflight policy preserves
all raw scaled matrix values and applied EQUED, returns kNumerical with
kAccuracyWarning and kDocumentedPartial for nonfinite logical scaled values,
and leaves native_info absent. Finite entries retain their computed values;
nonfinite entries have no finite-accuracy claim. The no-scaling branch neither
reads nor certifies existing values. A passing fidelity/warning test is not a
passing finite-scaled-output mathematical gate. Upstream numerical review is
required for this fixture; no license/owner approval or provider patch is
inferred. The earlier GEEQUB, GECON/GERFS and GESVX tiny-input gates remain
independently open.

## Final source freeze and verification ledger

Final source snapshot: `lu-helpers-final-N72v6pbu`. The predecessor
`lu-helpers-frozen-PibQtdHT` has identical numerical/test sources and differs
only in public comments clarifying host admission and the visibility of raw
row-major packing after invalid EQUED. The final header and all owned source
identities are:

| File | SHA256 |
| --- | --- |
| `include/asc/dense/providers/lapack_lu_helpers.h` | `b50719af14b485eea3df189fa4c35f641f5ec124d77890b744d0e99fc717669c` |
| `src/dense/lapack/reference_lu_helpers.cc` | `b73efca20b8177e04ae26da0a377ef0f735cbfc0b48adf14d8fc5220102f06c0` |
| `src/dense/lapack/internal_lu_helpers.h` | `7533a0d442f83b031da028e4e8d8a2c226ddb0fb6e2bc9bbf7522fa97b085422` |
| `tests/dense_lapack/lu_helpers_test.cc` | `d6a581f1c1332d62ac4b1c12bf14fcb3a899a899567c2d4902ca59e8608bfe69` |
| `tests/dense_lapack/lu_helpers_test_support.h` | `1846000ad1fda24775b41a2345bc71ef0835805ce0baba2d672921d2c15b04ec` |
| `tests/dense_lapack/lu_helpers_scaling_test.h` | `b86ddca32b6657ba147272f446063249a9abd33c4679389805842b5f89d3734d` |
| `tests/dense_lapack/lu_helpers_failure_test.h` | `7120392d4e3c0765a2deb888384db9cb78850cfb46008e600d371890633298a6` |
| `tests/dense_lapack/lu_helpers_faults.h` | `985f6e7133e17389c4928cc07e9bdc1a1a51d0b2050f3467fb59efdaf6e1ab65` |
| `tests/dense_lapack/lu_helpers_faults.cc` | `1431505a973a07c34f2aed711cfe634abfd0c9a902ca14ef696e0de8f66e5415` |

Both actual ABI predecessor optimized revision 07 and address/undefined
sanitizer revision 01 pass all four separately selected scalar processes.
Final comment-exact snapshot optimized revision 08 and sanitizer revision 02
also pass all four processes for both ABIs, with zero process failures.
Commands are
`bash build-lu-helpers.sh lu-helpers-final-N72v6pbu ABI 08` and the same command
with `ABI 02 asan`, with the snapshot given as its absolute external path.
Logs are `logs/p04-lu-helpers-ABI-MODE-compile-REV.log` and
`logs/p04-lu-helpers-ABI-MODE-test-SCALAR-REV.log`, where ABI is lp64/ilp64,
MODE is optimized/asan and SCALAR is s/d/c/z. These are direct native
executables, not zero-test or skipped CTest invocations.

Sanitizers instrument the new helper adapter/tests, frozen equilibration and
original LU adapter, and workspace foundation. Previously built Core/Dense
archives, Reference-LAPACK, BLAS and compiler runtimes are not instrumented.
The result is scoped sanitizer evidence, not a full-provider ASan claim.
First/repeated calls and structural/quality failures observe C++ allocation
probes and statically wrapped C allocators; no allocation is observed.

Frozen predecessor strict format/header/tidy revision 01 and Doxygen revision
01 pass. Final header format/self-contained compile and Doxygen revision 02
pass; final clang-tidy revision 02 also passes. Doxygen extracts all 16 public
function overload declarations. These are bounded header checks, not a full
installed documentation build. Raw logs are
`logs/p04-lu-helpers-{format,header,tidy}-frozen-02.log` and
`logs/p04-lu-helpers-doxygen-frozen-02.log`; exact external drivers are
`check-lu-helpers.sh`, `helpers-header-check.cc`, and `helpers-Doxyfile`.

`audit-lu-helpers-closure.py` audits actual static archive definitions and
undefined edges for all eight roots. Both ABI
`logs/p04-lu-helpers-static-closure-ABI-01.json` pass: 11 reachable objects,
zero external leaf symbols, and no excluded XERBLA/other branch or symbol.
LASWP has no foreign object dependency; LAQGE reaches its real LAMCH and
LSAME only. The JSON records exact LAPACK and BLAS archive SHA256 values.
This is source-conditioned static closure, not dynamic runtime interposition.

Additional final tests include forward/reverse/strided swaps across 31/32/33
and 64/65 column blocking boundaries, raw selected destination ranges weaker
than GETRF, prefix/gap preservation, and an actual single-swap minimum signed
INCX for each ABI. LAQGE covers N/R/C/B and exact threshold neighbors,
non-square padded layouts, genuine finite GEEQU pipelines, tiny overflow
fidelity, repeated plan reuse, nonfinite input, unused NaN/full and genuinely
empty R/C vectors, malformed EQUED, and unchanged sentinels. Empty wide ASC
row strides and stale original-stride plans are tested with real empty
descriptors for both operations. Overflow boundary tests use only integer
metadata predicates, never fabricated nonempty matrix storage.

## Preserved review failures and remaining gates

Strict intermediate revision 01 exposed unused includes, missing test
nodiscard and const Status locals preventing moves. All were corrected with
no check suppression. Revision 04 added a mistaken positive pinned-host test:
Serial::CanAccess admits only host, so setup correctly failed kMemoryAccess.
The failure logs and `logs/p04-lu-helpers-lp64-abort-backtrace-04.log` are
retained. Revision 05 tests the actual pinned rejection and passes all four
scalars for both ABIs.

Revision 06 then uncovered a real adapter omission: the neutral workspace
validator admits pinned-host as CPU-addressable, but helper execution had
not separately applied its chosen execution context. Both ABI scalar-s runs
failed 11 assertions against immutable `lu-helpers-check-r2NAAsPu`.
The new helper CheckPlan now checks every workspace region with the explicit
context before mutation; pinned integer and layout regions are rejected,
including unused descriptors, and corresponding tests pass in revision 07.
The neutral foundation was not changed. A separate context/workspace audit of
earlier frozen provider wrappers remains required; this helper correction
does not claim that broader gate.

The integrator owns pending atomic source/header/test registrations, public
inventory/hash consumers, exact upstream coverage mapping, installed consumer
and full Debug/Release/component regressions. Required registration is one
optional provider header, `reference_lu_helpers.cc`, and a helper executable
built from `lu_helpers_test.cc`, `lu_helpers_faults.cc`, existing allocation
audit and Dense allocation probe sources. Link test-only GNU wraps for
`dlaswp_`/`dlaqge_` plus the existing C allocator wrappers, and register s/d/c/z
as distinct tests. The new tests also call existing GEEQU. No base Dense or
other-module foreign dependency is introduced.

No helper claim covers native LASWP/LAQGE, other family helpers, full ordinary
LU mathematical closure, advanced/specialized families, global thread gates,
redistribution approval or the full P00–P11 program. The LAQGE tiny finite-
scaled-output gate remains explicitly unmet, despite verified faithful
warning behavior.

## Separate installed GERFS oracle assessment

An integrator consumer retained a failed fixed 128*epsilon componentwise
forward oracle for single-real transpose A rows [2^-6,2^-7;32,64], X columns
[1,3] and [2,4], followed by X*=0.99f and GERFS. Direct pinned calls reproduce
the exact result for both ABIs, without modifying frozen GERFS. For C=A^T,
the exact infinity-norm condition number is 8193. SGERFS stops on rounded
componentwise backward error, not a condition-independent forward bound.

`gerfs-condition-oracle-probe.cc` and
`logs/p04-gerfs-condition-oracle-{lp64,ilp64}-{compile,run}-01.log` record
INFO=0, BERR=0 and X0=[0.99976563453674316,3]. Wider residual arithmetic gives
componentwise backward error 1.90696068125381e-8, normwise relative forward
error 7.81218210856120e-5, and the rigorous independent residual-based bound
1.56243642171224e-4. Returned FERR=0.0019533236045390368 is conservative for
this fixture. The second RHS similarly satisfies the independent residual
bound and finite FERR. This is ordinary condition-sensitive float stopping,
not the separate nonfinite-estimate tiny-input failure. FERR remains an
estimate generally, not a universal rigorous theorem. The original installed
failure was not deleted or reclassified as passing.

Next exact integrator task: register the header, provider source and four
scalar test invocations described above, update inventory/coverage consumers
atomically, then run installed and full regression gates on that exact
candidate. All bounded final process exits and strict checks are recorded
above; no local helper test remains running. Further implementation must use
a separately linked agent worktree as directed by the integrator; no new
shared-root ownership is taken.
