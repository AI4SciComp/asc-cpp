# RK condition-estimator contract review

Status: **implemented; numerical acceptance blocked by required range failures**.
Programme status: **FULL_PROGRAM_INCOMPLETE**.

Six S/D/C/Z SYCON_3 and C/Z HECON_3 routes use pinned LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Implementation parent is pushed
`7c00b77`, the RK solve delivery. Six exact source/inventory rows and twelve
actual GNU 11.4 LP64/true-ILP64 prototype emissions are retained under external
`continuation-20260912-01/rk-condition-prerequisite-01`. Read-only source
preparation began while preceding delivery documentation finished. Public
contract decisions and native execution followed its commit and push.

## Public contract

`lapack_indefinite_rk_condition.h` exposes twelve query/execute overloads.
Immutable A/E/pivots have matching complete TF2_RK/TRF_RK provenance; completed
singular raw factors are accepted. Existing interleaved ROOK storage is not
interchangeable. Each signed paired pivot obeys its own upper/lower target
bound. E has the same scalar type as A, with upper E(i)=D(i-1,i) and lower
E(i)=D(i+1,i), one-based. Ignored boundary, 1-block and partner slots are never
numerically read or validated. Full raw Hermitian factor coefficients survive
row packing. No source-absent singular-divisor or finiteness scan is added.

ANORM/RCOND use the associated real type. Complex one-norm uses Euclidean
scalar modulus. ASC explicitly requires finite nonnegative ANORM, including
empty n=0; native checks only negative ANORM. Queries read that scalar and
metadata, not array contents or old RCOND. Plans bind provider/scalar/triangle,
shapes, E/pivot metadata, original/effective A stride/layout and active
zero-norm class. Positive norms can reuse one plan. Original LDA fits native
INTEGER even when unused.

Active n>0 and norm>0 use 2*n live T WORK entries and n private native INTEGER
pivots. Real variants add simultaneous n INTEGER IWORK entries in the second
half of kInteger; complex variants need no IWORK or RWORK. Row A adds n*n live
T layout storage. Column A and E are direct inputs. LACN2's 3*n arithmetic,
packing products and byte totals are checked. No LWORK, native workspace query
or returned WORK scalar exists.

After structural checks, n=0 writes RCOND=1 and nonempty norm=0 writes RCOND=0
without workspace, numerical-array reads or a native call. Active source
zero 1-block early returns remain native INFO=0/RCOND=0. INFO starts at full
INTEGER minimum and RCOND at -1 immediately before calling. Nonzero/missing/
partial INFO or changed private input pivots are provider defects. Negative
RCOND is provider-invalid; nonfinite RCOND is retained with an accuracy warning,
partial validity and numerical status. Finite nonnegative estimates complete
without clamping or certifying accuracy. No positive singular INFO or diagnostic
pivot index is invented.

Inputs/output/scratch/live metadata must be disjoint and accessible to the
explicit serial CPU context. Structural failures preserve RCOND and buffers;
metadata aliases preserve report, otherwise report resets first. Calls allocate
nothing, transfer nothing and change no global state. Concurrent calls share
immutable A/E/pivots/provider/plans with private writable RCOND/workspace/report.

## Native prerequisites and failures

Each ABI passes 144 guarded cases and 144 small mathematical controls, covering
n=0/1/2/3, U/L, zero/positive norm and completed singular 1-blocks. Actual emitted
prototypes match pinned C declarations after input-pointee const erasure only.
Native input, metadata, INFO and workspace padding guards pass.

Each ABI's 192 range cases has 184 representable original norms and eight
complex 2-block controls whose true norm exceeds the real type at offdiagonal
components 0.75*maximum. The latter are outside ASC finite-ANORM admission.
All guards pass; 32 required mathematical cases fail despite exact RCOND=1:
24 min-normal/8 scalar/2-block cases across all six variants and both triangles,
and eight min-normal/2 complex 2-block cases. Remaining admitted controls pass.
The new blocker is `BLOCK-INDEFINITE-RK-CONDITION-RANGE`.

Fortran reverse-communication traces identify the min-normal/2 cause: every
TRS_3 result component and true inverse norm is representable, but the final
alternating-vector SCSUM1/DZSUM1 sum overflows before LACN2 divides it by 3*n.
The estimate becomes infinity and RCOND becomes zero. At min-normal controls,
the sum stays finite and RCOND is near one. LP64 trace evidence is in
`rk-condition-estimator-cause-01/lp64`; ILP64 is in `cause-02/ilp64`.

Initial range compilation rejected misleading indentation before execution;
the separate formatted range-probe-02 retained all checks and passed compilation.
Initial ILP64 trace compilation rejected implicit INTEGER8-to-REAL4 denominator
conversion; an explicit kind conversion for exact denominators 1/2 fixed it.
Original sources/logs and successful LP64 trace evidence remain preserved.
No provider kernel, assertion or tolerance changed.

At this prerequisite checkpoint the public header/backend and ordinary tests
were prepared; adapter and integration verification remained unfinished.
Final bounded results follow. Earlier blockers and frozen deliveries remain intact.


## Initial adapter checkpoint

The smallest double LP64 mathematical/native-fidelity processes pass 104 cases
each. Registered tests cover n=0/1/2/3/7/67, both triangles/layouts and both
TF2_RK/TRF_RK origins, completed singular 1-blocks, scaled diagonal-block
systems and n=3 matrices requiring both independent interchanges. Analytic
block-diagonal and wider 3-by-3 adjugate condition oracles are independent of
native output. RCOND native-byte fidelity, immutable A/E/pivots, ignored-E
NaNs, workspace and allocation checks pass in this initial subset. Broader
engineering and final acceptance work were unfinished at that checkpoint.

## Adapter range and workspace findings

All twelve ordinary processes pass 104 cases each in both ABIs. Initial ILP64
real tests aborted at the reused 560-byte private INTEGER fixture guard: n=67
real CON_3 requires 1,072 bytes. A family-specific fixture retains every guard
and allocates both n-entry INTEGER roles; frozen ROOK fixtures are unchanged.
Original aborts/sources and corrected engineering `-02` records are preserved.

Each ABI's adapter range run executes 128 cases per class and retains six
required mathematical process failures, with 208 failed assertions. Six fidelity
processes and the 144-case native ABI process pass; zero tests skip. Real
classes have 128 representable-norm cases; each complex class has 120 plus eight
nonrepresentable-norm query/execute rollback controls. These controls enforce
finite ANORM admission without substituting a false norm or waiving a test.
RCOND, A/E/pivots, scratch and report rollback assertions all pass. The adapter
range failures match the separately executed native kernels exactly.

## Fault, structural, concurrency and installed checkpoint

All six fault processes pass in both ABIs, 1,280 cases per variant including
768 empty/zero-norm noncalls. Sixteen modes cover omitted/partial/nonzero INFO,
changed private pivots, omitted/partial/negative/nonfinite RCOND, finite zero
and above-one outputs. INFO/RCOND seeds, exact raw diagnostics, no clamping,
input immutability and workspace guards remain checked.

All six structural processes pass per ABI: 236 rollback cases, 24 protected
queries, 16 empty noncalls, ten 3*N boundaries and eight LP64/four ILP64 unused
original-LDA queries. All six concurrency processes also pass per ABI: sixteen
groups, four workers/eight rounds, 768 native calls, 128 zero-norm noncalls and
128 structural rejections. Regular and completed singular factor inputs A/E/P
are mapped read-only on Linux; writable state is private to each worker.

Four isolated relocated installed consumers pass 1,248 cases each for both
static/shared ABIs. They use only exported headers and package targets, both
RK factor origins, triangles/layouts, orders 0/1/2/3/7/67, scaled and singular
controls and zero-norm branches. First-run artifacts are retained as `-01`.
Initial strict tests found include/function-size/conditional issues; helper
refactoring preserves cases, guards, assertions and tolerances. Final strict,
profile and normalized acceptance work was pending at that checkpoint.

## Final bounded evidence

Twelve static/shared LP64/true-ILP64 Release/Debug/ASC-ASan+UBSan profiles each
execute 43 required processes: 37 pass and six mathematical range gates fail
208 assertions. Four TSan profiles pass six concurrency processes each.
Canonical totals are 540 processes, 468 passes, 72 required failures, 2,496
failed assertions and zero skips. No mathematical failure is waived, marked
WILL_FAIL or counted as passing numerical acceptance.

Every ordinary mathematical/native-fidelity process executes 104 cases. Every
range process executes 128 cases: real variants have 128 finite original norms;
complex variants have 120 plus eight nonrepresentable-norm rollback controls.
All required finite-norm mathematical failures remain visible. Native fidelity,
144-case ABI probes, full-width INFO/RCOND-write fault diagnostics, structural
rollback/protected-memory checks and concurrent read-only factors pass.
Seventeen exact family source/build files are frozen in
`rk-condition-final-source.json`; final profiles are under
`rk-condition-{static,shared}-{release,debug,sanitizer,tsan}-{lp64,ilp64}-final`.
ASC and tests are instrumented; pinned Fortran/BLAS internals are not.

Four final relocated installed consumers pass 1,248 cases each, using public
headers/package targets with both RK factor origins, triangles/layouts,
empty/singular/scaled blocks and zero/positive norm branches. The successful
original `-01` consumers remain preserved; `-02` validates final helper
refactoring. Eighteen strict translation units and four standalone headers
pass. Direct final probes pass 144 guarded mathematical cases per ABI against
the twelve original GNU-emitted declarations. Twelve dynamic symbols are added
per ABI and none removed. Four installed libraries match Release producers
after the normal CMake RPATH installation transform.

Package checks cover four manifest contexts and six fixtures; architecture and
dependency checks pass. The actual CI selector contains 1,012 processes,
including all 43 new runtime and two header processes. Warning-free Doxygen
covers 150 public headers and 2,597 members. Source hashes, raw command records,
profiles, strict include closures, installed artifacts and provider/compiler
attestations are bound by `rk-condition-final-audit/audit.json` and the delivery
audit. Original compile/fixture/style failures and all numerical failures remain
preserved. No source assertion, case, tolerance or provider arithmetic changed
to obtain an engineering pass.

All six normalized routes remain `implemented_unverified`, with 96 reviewed
modes. `BLOCK-INDEFINITE-RK-CONDITION-RANGE` records scalar/2-block reciprocal
failures and complex LACN2 sum overflow despite a representable true inverse
norm. Finite zero RCOND may be inaccurate even when the call completes; native
fidelity and INFO=0 do not certify its accuracy. Earlier RK empty-E, solve,
other numerical, XBLAS, CodeQL alert, platform/provider and full normalized
execution-record blockers remain intact. No Reference row is verified.
Continue SYTRI_3/HETRI_3 and their TRI_3X dependencies, then RK drivers.
Programme status remains **FULL_PROGRAM_INCOMPLETE**.
