# PTTRF/PTTRS checked factor reuse

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
