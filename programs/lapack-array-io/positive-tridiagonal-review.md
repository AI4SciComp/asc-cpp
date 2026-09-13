# PTTRF/PTTRS checked factor reuse

## Recovery checkpoint, 2026-09-12

The transport-interrupted session did not lose the PT implementation. Recovery
started at `663c7a4c4b1388c3a2c78093e61f2e6e2f94f9f4` on
`feature/lapack-array-io`, in the existing integration workspace. The public
API, backend and integer-bound helper still match their integration commit
`fb8c86fb390d88065d9f2b6d823a64cf9680c19f` byte for byte. Maintained tests,
public-header checks, exports and installed consumer are already present.
PTTRF/PTTRS must not be restarted as an unimplemented family.

Recovery rechecked all eight pinned source hashes against the recorded LAPACK
commit and tree. The scalar contracts remain:

| Routines | D element type | E and B element type | Native solve UPLO |
| --- | --- | --- | --- |
| SPTTRF / SPTTRS | `float` | `float` | Absent |
| DPTTRF / DPTTRS | `double` | `double` | Absent |
| CPTTRF / CPTTRS | `float` | `std::complex<float>` | Lower or upper |
| ZPTTRF / ZPTTRS | `double` | `std::complex<double>` | Lower or upper |

All PTTRF signatures omit UPLO. D has n elements and E has max(n-1,0)
elements, stored contiguously. The factorization consumes the lower off-diagonal;
complex upper solve factors for the same Hermitian matrix require conjugated
E. PTTRS overwrites B and preserves D/E. Neither native routine has workspace;
ASC requires n*nrhs scalar layout entries only for active row-major solves.
The disjoint operand, workspace and metadata rules and the exact INFO/partial
output semantics in the implementation review below remain unchanged.

The smallest recovery verification was an identity and saved-result audit.
It checked the six original Debug/Release/ASan+UBSan LP64/ILP64 JUnit records:
each has 13 passes, four required mathematical failures and zero skips. Both
saved TSan profiles contain four passes and zero skips. These are historical
executions, not fresh execution credit for the current working tree.

Fresh smoke execution then used the four preserved installed PT consumers,
starting with static LP64, followed by static ILP64 and both shared ABIs.
Each passed its one all-scalar public example with zero skips. No build,
installation, provider acquisition or worktree creation was needed. The
copied examples retain their original source identities. Their guard differs
from the current source by exactly three previously committed Doxygen/comment
lines; recovery checks that exact delta and preserves the initial audit
failure that detected it. This is a replay of installed artifacts, not a new
full-profile or whole-tree acceptance result. Later shared-provider admission
is documented in [its review](shared-provider-admission-review.md); the
static-only wording below records the original integration boundary.

Evidence remains outside source under
`asc-cpp-evidence/lapack-array-io/master-continuation-20260910-01/pt-recovery-20260912-01`.
`audit.py` and `audit.json` bind the pinned sources, current PT files, historical
logs/JUnit, installed executable hashes and all 15 preexisting dirty files.
Each `smoke-{static,shared}-{lp64,ilp64}` directory retains its command, exit
status, raw log and JUnit. The frozen robust PPSVX, Native20 and array I/O
subsets were not rerun. The preexisting GESVX changes remain separate and
unchanged; this recovery does not claim their completion.

The unfinished PT task is numerical acceptance: the recorded scalar reciprocal
overflow still fails four required PTTRS tests. No failing test was removed,
skipped, weakened or reclassified. A provider/algorithm strategy decision and
the remaining profile/platform admission are still needed; implementation
repetition or another identical scalar reproducer cannot close those gates.
The programme milestone remains
`PPSVX_ROBUST_INTEGRATED_EXPERIMENTAL_FULL_PROGRAM_INCOMPLETE`.

## Original implementation and contract review

This slice selects exactly SPTTRF, DPTTRF, CPTTRF, ZPTTRF, SPTTRS,
DPTTRS, CPTTRS and ZPTTRS from the unchanged 2,113 required Reference rows.
It adds the optional-provider public header
`asc/dense/providers/lapack_positive_tridiagonal.h`, its source, maintained
behavioral/fault/observation tests, and a standalone installed example in
`examples/positive_tridiagonal`. Catalogue registration and executed profile
results must be read separately below; selection alone grants no verification.

The pinned source remains Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`. The actual eight source bodies,
PTTS2 variants and ILAENV were read before implementation. Calls use the
prepared provider's installed `lapack.h` prototypes, including complex PTTRS's
hidden CHARACTER length. Real PTTRS has no UPLO argument. No upstream routine
body was copied or modified. GNU/libstdc++/static provider guards remain.

The existing positive-definite tridiagonal matrix descriptor keeps D real for
all four scalar types. PTTRF overwrites it with lower unit-bidiagonal factors
of A=L D L^H, or transpose for real types. Upper factors satisfy A=U^H D U;
for the same Hermitian matrix U=L^H, so the superdiagonal is the conjugate of
the lower factor's off-diagonal. The raw factory explicitly records physical
orientation and does not certify previous execution. The report-bound factory
requires the same scalar's successful PTTRF report and finite positive D and
finite E; callers retain responsibility for common origin.

The move-only resource owner copies an existing factor through DenseArray's
host object-lifetime/allocation policy. It performs O(n) work, retains n real
D and max(n-1,0) scalar E values, and makes at most two explicit allocations.
Changing orientation conjugates complex E. It performs no native call and
manufactures no historical PTTRF report. The resource outlives the owner;
borrowed views follow the documented move/destruction lifetime. Expert calls
remain allocation-free and preserve structure.

Queries inspect descriptor metadata only. Plans bind scalar routine, provider,
order and, for solves, orientation and original RHS shape/layout/leading
metadata. PTTRF and direct-column PTTRS have zero native/ASC scratch; an active
row-major RHS needs exactly n*nrhs live T entries in layout scratch. Finite
positive D and finite E are checked only after structural/workspace validation
and only for active solves. PTTRF N=0 and PTTRS N=0 or NRHS=0 finish locally
without native INFO or numeric reads. Metadata aliases preserve reports;
other preflight errors reset the report without numeric/scratch mutation.

PTTRF INFO=k>0 records the zero-based bad pivot and documented partial output.
k<n means incomplete factorization; k=n means completed factorization with a
nonpositive final D. Neither yields a successful factor certificate. Invalid,
negative, impossible or unwritten INFO is a provider defect. Row solve
publication is withheld on provider defects; direct foreign writes survive.
Nonfinite computed solutions are published without alteration and reported as
accuracy warnings with partial validity and native INFO=0. A PT factor does
not acquire square-root Cholesky `factor_family` semantics.

Native integer checks follow the selected source: PTTRF's cleanup/four-way
loops remain bounded by n; PTTRS's pinned ILAENV returns block size one and
has a final nrhs+1, PTTS2 has active n+1 terminals, and its scalar SCAL path
can form 1+LDB. Pure boundary tests exercise both integer limits without
fabricated huge arrays. Factorization uses O(n) operations; solving uses
O(n*nrhs) plus O(n) checked validation. These are complexity statements, not
speedup claims.

The finite ordinary matrix uses independent dyadic L D L^H construction,
n=0,1,2,3,4,5,9,33, three binary scales, both orientations, both RHS layouts,
0/1/2/4 RHS and two solves. Independent reconstruction and original-system
residual/known-solution checks retain scalar epsilon-scaled tolerances.
First/middle/last bad pivots, guards, aliases, exact short workspace, stale
plans, owner moves, and immutable-factor/plan concurrency are distinct checks.
Four real-provider workers use independent contexts/reports/B/workspace;
allocation audit and synthetic callback state are inactive during concurrency.

Linux protected-page observation covers complete initialized containing arrays
through metadata queries, rejected stale/short-workspace calls and local empty
execution. All four scalar types and both RHS layouts are covered. A child
intentionally reads the same protected storage and must terminate with SIGSEGV;
a normal read/write control runs after access restoration. This observes ASC
regions with the profile's recorded compiler flags, not foreign-code
instrumentation, and does not establish arbitrary function-wide absence of
reads. Active factor input reads and provider output writes are permitted.

## Mathematical disposition

The analytic fixture has n=1, two identical RHS, A=B=a>0 and exact X=1. The
condition is one and X is representable, including the smallest positive
subnormal. This assertion is justified without an extended-range oracle.

| Unique cause | Fixture and expected property | Observed result and source | Disposition |
| --- | --- | --- | --- |
| Scalar reciprocal before scaling | S/D/C/Z PTTRS, a=denorm_min or 2*denorm_min, direct/packed RHS; exact finite X=1 | PTTRF INFO=0. PTTRS INFO=0, X=Inf for real and (Inf,NaN) for complex. PTTS2's n=1 path passes `ONE/D(1)` to xSCAL/xCSSCAL/xZDSCAL before multiplying B. | Direct pinned-provider comparison reproduces the values. ASC maps native output and adds the documented numerical warning. Preserve the required numerical gate. An explicit provider/algorithm strategy decision is needed for this route; neither a dependency patch nor an implicit replacement is authorized here. |

For S/C, denorm_min=2^-149; for D/Z it is 2^-1074. Minimum-normal and
maximum-finite scalar controls pass. There are **four failed test processes,
48 failed assertions, eight distinct scalar/input mathematical cases,
16 layout executions, and one cause** in each executed matching profile.
Two RHS do not make two independent mathematical bugs. The direct-comparison
pass establishes behavioral fidelity only; it does not turn the failures into
successes. No tolerance, input, floating-point environment or pinned provider
was changed. This cause does not block DSGESV/ZCGESV development or other
independent delivery work.

## Execution pointers

Evidence is relative to `asc-cpp-evidence/lapack-array-io`:
`master-continuation-20260910-01`. `pt-source-identities.json` binds the selected
sources. Every `pt-*/selected-tests.json` records actual configured CTest IDs;
command subdirectories contain argv, status and raw logs. Failed build attempts
01/03/06 are preserved. Debug LP64 attempts 04/05/07 retain the unchanged
extreme mathematical failures. Attempt 07 executes 15 tests: 11 pass and the
four required mathematical tests fail, with zero skips. Profile expansion,
scoped tidy/metadata checks and installed relocation are still in progress;
this document does not claim their completion in advance.


The final amended-test runs `pt-debug-lp64-09` and the other five
`pt-{debug,release,sanitizer}-{abi}-02` lanes each execute 17 configured tests:
13 pass, four required mathematical tests fail, none skip. Both normal and
no-exception public header executables are included. No ASan/UBSan findings
occur. `pt-tsan-{abi}-01` each passes four scalar ordinary/concurrent tests
under the existing `setarch x86_64 -R` invocation; ASC is instrumented, the
pinned Fortran provider is not. Private fault signatures additionally compare
all eight wrapper types against their actual installed prototypes.

`install-pt-{abi}-01` installs and relocates the actual integrated Release
libraries and headers, then configures the copied maintained example. Its
runner's command-log name collided with its own build directory; the build
had not run. `install-pt-{abi}-02` resumes that valid installation/configuration
with fresh command logs: public example, runtime audit, public-surface audit,
and a separate provider-free Dense consumer all pass. No historical output
was removed. The provider-free executable has no LAPACK/BLAS/Fortran runtime.
This is installed public capability, not an external-only header build.

Strict source tidy passes in `pt-style-02`; all six amended test/example
translation units pass in `pt-style-03`. `pt-documentation-01` passes Doxygen:
128/128 public headers, 2,257 documented members, zero warnings. The unchanged
validator in `pt-record-check-01` derives 358 partial Reference rows, 1,755
not started, zero strict callable/verified of 2,113; 28 reviewed contracts;
20 native callable/verified rows. The eight new rows are the only changed
mapping rows. The explicit native evidence extension retains the complete
historical execution record and verifies all 20 native rows plus 52 selected
implementation artifacts unchanged. It does not claim a new whole-tree native
run. Affected package/architecture/ABI checks and branch synchronization remain
pending until their actual records are added.


The eight affected configured integration commands pass except for the first
public-file policy check, which correctly found a missing new `.cc` entry in
its independent approved-source list. That exact entry was added;
`pt-metadata-amendment-01` passes the affected check. Both ABI package runs pass
all maintained family consumers, copied PT/robust/native-I/O examples and 28
provider isolation/control cases. `pt-final-checks-01` rebuilds the final exact
native signature assertions and passes four fault tests in each of six lanes,
then passes format, strict signature tidy, coverage, whitespace and the compact
native-evidence bridge's historical-record verification. No validator was
weakened. The existing hosted provider workflow now selects all PT tests,
including the four still-failing mathematical gates, alongside robust PPSVX and
package checks; those failures are intentionally ordinary failed gates.
Hosted execution must be attributed to the actual pushed revision separately.
