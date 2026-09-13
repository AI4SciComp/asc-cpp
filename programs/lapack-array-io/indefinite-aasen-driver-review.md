# Single-stage Aasen driver contract and delivery

Status: **engineering delivery checked; required mathematical, native workspace
and native empty-call gates remain failed**. These six drivers receive only
`implemented_unverified` Reference status at integration.
Programme: **FULL_PROGRAM_INCOMPLETE**. Prerequisites are the integrated Aasen
producers (`c02b549`) and solves (`cb05d524`).

The exact six required rows are S/D/C/Z SYSV_AA and C/Z HESV_AA at Reference
commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Full source hashes and twelve
actual GNU LP64/true ILP64 prototype emissions are preserved under the existing
continuation's `aasen-driver-prerequisite-01`. Each signature has eleven pointer
arguments and a trailing character length; A and native INTEGER pivots are
outputs. No provider, runtime, notice or numerical environment is changed.

## Checked contract

A is the original selected triangle of a square symmetric or Hermitian matrix;
B has N rows and arbitrary nonnegative NRHS. SY uses transpose symmetry,
including complex input; HE uses adjoints and ignores original imaginary
diagonal components. Pivots are a mutable exact-N unit-stride output. Factors
and positive one-based pivots have the already reviewed single-stage Aasen
storage and common-origin contract. Factor output can be used with checked
SYTRS_AA/HETRS_AA. No classic, rook, RK or two-stage descriptor is expanded.

Queries read metadata only. Plans bind original and effective A/B strides and
layouts, provider/scalar identity, triangle, shape and pivot increment. Empty
N is a validated noncall with no workspace; NRHS=0 with N>0 still factorizes.
The native LDB remains at least N for NRHS=0. Checked arithmetic covers the
producer's 65*N and terminal strided COPY cursor, the driver's 3*N even for
zero RHS, and the active solve's loop endpoints, LDA+1 and RHS SWAP cursor.
Scalar workspace minimum is max(2*N,3*N-2) for active N. Preferred size follows
the pinned driver/callee recommendations with safe scalar rounding. Native
INTEGER pivot lifetimes and row-A/all-HE-A/row-B packing use caller workspace.

Execution calls the exact named driver. Its internal native queries are part
of that provider implementation; ASC does not query numerically or introduce
an allocation or fallback. INFO and private pivots start at the native INTEGER
minimum; WORK(1) and the GTSV diagonal witness region are seeded. On return,
WORK(1) must contain the exact scalar preferred recommendation (imaginary zero),
INFO must be in 0..N, and pivots must satisfy p[0]=1 and i+1<=p[i]<=N.

Positive INFO originates in GTSV and the driver subsequently finishes the
solve and overwrites WORK(1). At N=1 the zero witness is the factor A(1,1);
for larger N it is WORK(N+INFO-1), using Fortran indexing. A consistent positive
INFO publishes factors/pivots and reports kNumerical, singular, zero-based
index and documented partial output. Packed B is withheld, direct B may have
changed, and no usable solution is promised. NRHS=0 cannot report this solve
failure. Missing/malformed native outputs are provider defects, withholding
packed A/B and public pivots. INFO zero alone is no accuracy certificate.

## Native empty workspace requirement

The driver always queries both callees before factorization. Real and HE
recommendations include max(1,2*N,3*N-2). Complex SY recommends max of its two
callee query values, returning zero at N=0. CSYSV_AA documents and initially
accepts zero LWORK at N=0, while its factor dependency requires one. ZSYSV_AA
also returns zero, below its own documented minimum one. These are retained
requirements, not corrected provider code. A separate query-contract process
checks dependency closure. Four separate mandatory native CSYSV_AA processes
exercise the legal zero-workspace call for both triangles and NRHS=0,3;
NormalReturnGuard detects a nested Fortran STOP without treating exit zero as
success. They cannot be waived by the checked empty noncall.

## Finite delivery evidence

Reviewed mode coverage is 192: six exact routines, both triangles, four
independent A/B layouts, zero/active RHS, and minimum/preferred workspace.
Both actual GNU 11.4 integer ABIs pass 1,008 native cases with guarded native
INTEGER/CHARACTER/scalar storage and twelve actual compiler-emitted signature
comparisons. Each separate 288-case query-contract probe retains sixteen
failures. Direct native records show exit 93 for all four documented legal
CSYSV_AA empty calls per ABI; normal main return is required by the test.

Each ordinary scalar/process covers 672 cases, including N=0,1,2,3,7,67,
NRHS=0,1,2,3, both triangles, independent A/B layouts, minimum/preferred work,
singular inputs (including zero RHS), scaling controls, allocation and storage
guards. Independent factor reconstruction, known solutions and wide residuals
remain separate from native fidelity. The public solve also reuses produced
factors/pivots. Every ordinary process and all native fidelity processes pass.

The maintained 432-case range matrix retains the existing scalar/zero-diagonal
2-by-2/diagonal representable fixtures at six scales from min-normal/8 through
.75*maximum. At .75*maximum, C/Z SY fail all three kinds and C/Z HE fail the
complex-offdiagonal 2-by-2. Factor reconstruction, direct-native fidelity, all
real cases and lower-scale controls pass. Both the driver and factor-reuse
solutions retain the unchanged known-solution, residual and finiteness
requirements: 1,024 failed assertions per ABI/profile, twice the preceding
solve-only count because two callable routes are checked. These extend
`AASEN-SOLVE-RANGE`; existing `AASEN-FACTOR-RANGE` source-bound reproductions
remain prerequisite limitations without another unchanged producer sweep.

Fault injection passes 1,472 cases/class/ABI, covering missing/partial-width
INFO and pivots, malformed pivot/INFO ranges, omitted/nonfinite/partial-complex
WORK recommendation, consistent/false singularity and missing/nonzero/NaN
singular witnesses, with N=1 and N=3, zero/active RHS, both triangles,
independent layouts and minimum/preferred work. Real-only and LP64 controls
are executed. Required publication guards retain packed A/B and public pivots
on provider defect, and withhold packed B on singular numerical failure.

Structural tests pass 408 rejection cases for each SY class and 416 for each
HE class, plus 32 protected metadata queries, sixteen protected empty noncalls,
original/effective stride limits and 58 synthetic source-count checks/class.
HE always requires A packing, including column-major A/B. The first fixture
adaptation had a helper argument-order compile error and an obsolete input-
pivot check. Another supposed alias was actually adjacent storage for
complex<double>: old pivot bytes [0,16) ended where A bytes [16,96) began.
Moving the pivot test address to [32,48) exercises a real overlap for every
scalar. Failed sources/logs and the objective corrections are preserved;
no production rejection or mathematical requirement changed.

Concurrency passes 32 groups/class with four workers and eight repeats:
1,792 native calls and 256 stale-plan rejections/class, including zero-RHS
factorization and singular outputs. Provider/plans are shared; all writable
A/pivot/B/work/report storage is private. No timing limits are changed.

Sixteen actual Linux profiles select 600 processes: 492 pass, 108 required
failures, zero skips. Each of twelve static/shared, LP64/true ILP64,
Release/Debug/ASC-ASan+UBSan profiles passes39/48, retaining four complex range
processes, one query-contract process and four empty native STOP processes.
Their failed assertions total 12,288 mathematical and 192 workspace checks,
plus 48 failed native empty processes. Each of four TSan concurrency profiles
passes6/6. Pinned Fortran and BLAS internals are not sanitizer-instrumented.
Every selected/executed test ID, invocation exit, complete raw log and binary/
source/configuration identity is retained; no mandatory gate is waived.

Four relocated static/shared consumers pass 768 driver/factor-reuse cases each,
using installed public headers and `ASC::dense_lapack`, with both ABIs,
triangles, independent layouts, minimum/preferred workspace and empty/active
systems. The maintained public example is
`tests/dense_lapack/installed_lu/indefinite_aasen_driver_main.cc`.
Twenty strict translation units pass, with twelve unchanged first production/
test executions explicitly reused and six corrected units plus two installed
consumer units freshly checked. The placement-new include uses the existing
repository IWYU keep convention; all object-lifetime requirements remain.

Four standalone-header checks, twelve new/zero removed exports per ABI, ten
package-manifest checks and three architecture checks pass. The maintained CI
selector includes 1,415 processes and explicitly requires all 48 driver runtime
processes plus the two new header checks. Doxygen covers 155/155 headers and
2,669 public members with zero warnings. Installed production libraries match
the final Release profiles. Exact identities are bound by
`aasen-driver-final-audit/audit.json` before six-row normalization.

The preceding pushed solve commit `cb05d524` has successful general CI and
CodeQL jobs, while its selected-family runs fail. Four actual push artifacts
each select/execute 1,365 tests without skips: LP64 passes 1,180 with 185 failures,
ILP64 passes 1,184 with 181 failures, in both linkages. Exactly five new failures
are the four solve range gates and its native query contract; all preceding
failures remain. Provider tests pass 111/111 per profile. The full artifacts and
comparison are preserved in `aasen-driver-remote-01/hosted-followup`.
These hosted results are source-bound to that commit, not the driver changes. Explicit feature
and PR alert queries record 3,152 open alerts, including the same twelve security
findings. The 44 new solve notes concern 80-byte matrix/72-byte pivot descriptor
parameters; they were read and remain open. No alert is suppressed or dismissed,
no numerical requirement is closed by CodeQL success, and no wider provider/
platform admission is inferred. Native20 and experimental RobustPpsvx remain
separate; full-programme, XBLAS and owner/provenance gates stay open.

The same `rk-inverse-profile-recovery-01/latest-handoff.json` records the exact
unfinished action. After owned commit/push and draft PR update, continue the
ready two-stage Aasen producer prerequisite `P05.required.hetrf_aa_2stage`,
then its consumers/drivers after exact inventory/source review. This is the
same full remaining-programme assignment, with no new family permission needed.
