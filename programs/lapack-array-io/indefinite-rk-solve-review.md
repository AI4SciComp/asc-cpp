# RK factor solve contract review

Status: **implemented; numerical acceptance blocked by required range failures**.
Programme status: **FULL_PROGRAM_INCOMPLETE**.

This family covers S/D/C/Z SYTRS_3 and C/Z HETRS_3 at pinned LAPACK 3.12.1
commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Implementation parent is
pushed `1178656`, the RK producer delivery. Six exact source/inventory rows,
twelve actual GNU Fortran 11.4 LP64/true-ILP64 prototype emissions, guarded
native probes and the pre-implementation public decision are preserved under
`continuation-20260912-01/rk-solve-prerequisite-01`. Read-only source preparation
began while the preceding producer TSan profiles finished; no producer inputs
were changed. The public design was recorded after that delivery was pushed.

## Storage and numerical semantics

`lapack_indefinite_rk_solve.h` adds twelve query/execute overloads. A, E and B
have the same real or complex scalar type, including complex Hermitian E.
A is immutable square raw RK factor storage: D's diagonal and the strict
unit triangular U/L coefficients. E is immutable contiguous exact-n storage
for D's 2-block offdiagonals. Pivots are immutable exact-n kRook entries from
the same scalar/provider/triangle and RK TF2/TRF operation. Numerical content
and common origin are caller preconditions. Existing borrowed factor factories
remain unchanged and cannot certify this raw A/E/pivot storage.

SY uses transpose symmetry, including complex symmetric matrices; HE uses
adjoints. Global P interchanges precede and follow the two triangular solves.
Each signed negative partner records its own interchange. Active preflight
checks complete adjacent pairing, nonzero/bounds and each individual upper
at-most-index or lower at-least-index target. Old interleaved ROOK factors
are incompatible even when their pivot protocol has the same enum value.
The source also names BK producers, but no SYTRF_BK/HETRF_BK files exist in
the pinned SRC inventory; no such producer is invented or admitted here.

Using one-based indices, upper E(i)=D(i-1,i) and lower E(i)=D(i+1,i).
Boundary E, 1-block E and the unused partner of each 2-block are not referenced
or numerically validated. Native guarded tests fill ignored slots with NaNs.
Full raw Hermitian factor coefficients are retained: 1-block scaling reads
real(Aii), while 2-block divisions consume the raw complex diagonal and the
appropriately oriented E/conjugate(E). Row packing must not normalize factors
as if they were original Hermitian matrix input.

B is mutable n-by-nrhs RHS storage, overwritten by X. A/E/IPIV are native
inputs and remain unchanged; no SYCONV factor conversion or restoration occurs.
Completed singular raw factors are accepted without adding a source-absent
singularity scan. Native INFO is zero or argument errors -1 UPLO, -2 N,
-3 NRHS, -5 LDA, -9 LDB. There is no positive singular diagnosis, RCOND,
FERR, BERR or finiteness/conditioning guarantee. Source scalar reciprocal and
2-block division arithmetic are preserved without fallback or normalization.

## Workspace, validation and ABI

There is no native WORK, LWORK or workspace-query route. Metadata-only ASC
queries require n private provider-width INTEGER entries for active calls.
Execution starts those trivial lifetimes in caller byte storage. Row A adds
n*n live T layout entries and row B adds n*nrhs; column arrays and E are
direct inputs. Plans bind routine/scalar/provider ABI, triangle/symmetry,
shapes, vector sizes/increments and original/effective matrix strides/layouts.
Original metadata must fit native INTEGER. Source N+1, NRHS+1 and positive
BLAS terminal 1+NRHS*effectiveLDB are checked; packing and total byte products
are checked independently. Empty n=0 or nrhs=0 returns before pivot-value reads,
with no numerical-array access, workspace or native call.

Operands, scratch and live provider/plan/workspace/report metadata are mutually
disjoint and accessible to the explicit serial CPU context. Metadata aliases
preserve the report; other preflight resets it while preserving arrays and
scratch. Full-width INFO starts at INTEGER minimum. Missing/partial/nonzero
INFO or changed private input pivots produce a provider defect with raw INFO
and unusable output; row-packed B publication is withheld, while direct
column B may already have changed. No allocation, transfer or global-state
change occurs. Concurrent calls may share immutable inputs/providers/plans
and need private B, scratch and reports.

All six pinned C declarations match actual GNU prototype emissions after only
input-pointee const is erased. Signed INTEGER width, scalar types, parameter
order and trailing size_t UPLO length remain exact. All 144 guarded native
cases per ABI pass for n=0/1/2/3, nrhs=0/1/2 and both triangles, including
ignored-E NaNs and complete A/E/IPIV, metadata and padding guards. This is an
admitted GNU boundary, not a portable Fortran ABI claim.

## Preserved native range failures

Before ASC consumer implementation, each ABI ran 144 native range cases:
n=1/2, both triangles, all six variants and six scales from min_normal/8 to
0.75*maximum. All native argument/input/padding guards pass, with INFO=0.
Twenty mathematical cases per ABI fail despite exact representable X=ones:

- Twelve scalar cases: all six variants and both triangles at min_normal/8.
  The reciprocal overflows; real output is infinity and complex output is NaN.
- Eight complex 2-block cases: four complex variants and both triangles with
  E's two components at 0.75*maximum. Finite B=original_A*ones yields NaN X.

The other 124 cases per ABI pass their mathematical checks. Original guard
and failed mathematical commands remain in `native-range-{lp64,ilp64}-01`.
These prerequisite observations did not grant ASC numerical acceptance.
Adapter and integration checks were still unfinished at that checkpoint;
their final bounded results follow. All earlier programme blockers remain intact.


## Engineering checkpoint

The smallest double LP64 run passes mathematical and native-fidelity processes,
496 cases each. Both actual ABIs then pass all twelve ordinary processes,
496 cases per process, with zero skips. Cases cover n=0/1/2/3/7/67,
nrhs=0/1/3, both triangles and independent padded layouts, RK TF2/TRF origins,
completed singular factors, scaled matrices and ignored-E NaNs. Known-solution
and wide residual checks are independent of the native solve. No scalar WORK
is requested; row packing and provider INTEGER requirements are checked.

Each ABI's range/guarded-ABI run has seven passes and six required mathematical
failures, 160 failed assertions and zero skips. Every range process executes
104 cases: 96 representable solutions and eight explicitly nonrepresentable
controls. All six native-fidelity processes and the 144-case guarded ABI test
pass. `BLOCK-INDEFINITE-RK-SOLVE-RANGE` records the reproduced native scalar
reciprocal and complex large-block failures without changing INFO=0 or input
bytes. The first LP64 production-source strict check passed. Other engineering
and integration checks were still unfinished at that checkpoint; final results
follow. Numerical acceptance remains blocked.


## Boundary and concurrency checkpoint

Both ABIs pass all six fault processes: 1,920 cases per variant including
896 empty noncalls. Full-width INFO seeding, omitted/partial writes, nonzero
INFO and private pivot mutation are checked without altering A/E. Rejected
row-packed B publication and direct-column behavior remain explicit.

Both ABIs pass all six structural processes: 408 rejections per variant,
32 protected-memory queries, 24 empty noncalls, and 26 source INTEGER count
checks. Original unused LDA and LDB are separately tested at the actual native
INTEGER maximum on 1-by-1 arrays with inaccessible payloads; LP64 also rejects
maximum+1. These add 32 LP64 or 16 ILP64 metadata-only queries per variant.

All six concurrency processes pass per ABI. Each uses 32 groups, four workers
and four repeats: 512 native solves, 512 empty noncalls and 512 rejected plans.
Both triangles, independent padded layouts, n=3/67, and TF2/TRF RK origins are
covered. Shared A/E/pivots are placed on read-only pages on Linux, including
poisoned ignored E entries. RHS, scratch and reports are private to each worker;
residuals, known solutions, inputs and guards are checked. Sanitizer profiles
and final-source validation were still pending at that checkpoint.


## Final bounded evidence

Twelve static/shared LP64/true-ILP64 Release/Debug/ASC-ASan+UBSan profiles
each execute 43 required processes: 37 pass and six mathematical range gates
fail 160 assertions. Four TSan profiles pass six concurrency processes each.
Canonical totals: 540 processes, 468 passes, 72 required failures, 1,920 failed
assertions and zero skips. No mathematical failure is converted to WILL_FAIL,
waived or counted as passing numerical acceptance.

Ordinary mathematical/native-fidelity processes each execute 496 cases. Range
processes each execute 104 cases: 96 representable solutions and eight explicit
nonrepresentable controls. All native-fidelity, guarded ABI, full-width INFO,
fault, structural, protected-memory and concurrency processes pass. Final
evidence binds eighteen family source/build inputs under
`rk-solve-final-source.json`. Final profiles are retained under
`rk-solve-{static,shared}-{release,debug,sanitizer,tsan}-{lp64,ilp64}-final`.
ASC/tests are instrumented; pinned Fortran/BLAS internals are not. These passes
do not extend provider admission to those internals or other platforms.

Four relocated installed consumers pass 3,168 cases each using public headers
only. They cover TF2_RK/TRF_RK origins, independent layouts, both triangles,
empty/singular/scaled systems, one/multiple right-hand sides and n=3 factors
requiring both independent RK interchanges. Eighteen strict translation units
and four standalone header checks pass. Final guarded direct probes pass
144 cases per ABI with all twelve actual GNU-emitted prototype comparisons.
Exports add twelve symbols per ABI and remove none. All four installed
libraries match final Release producers after normal CMake installation.
Package checks cover four manifest contexts and six fixtures; architecture
and dependency checks pass. The actual CI selector includes all 43 new runtime
processes and two additional header processes. Warning-free Doxygen and all
source/evidence identities are bound in `rk-solve-final-audit/audit.json` and
the delivery audit.

The initial strict-check failures remain recorded: missing direct includes,
constexpr naming, isolated declarations, two oversized helpers and a nested
conditional. Refactoring preserved every assertion/tolerance and exact input
object-byte comparison, including NaN payloads and signed zero. Original and
final installed-consumer runs remain separate retained records.

`BLOCK-INDEFINITE-RK-SOLVE-RANGE` keeps scalar reciprocal overflow and complex
large-block division failures explicit. All six normalized routes remain
`implemented_unverified`, with 96 reviewed modes. No Reference row is promoted
to verified. The earlier RK producer native-empty-E gate, all other numerical
failures, XBLAS, CodeQL alert, platform/provider and complete normalized
execution-record blockers remain intact. Continue SYCON_3/HECON_3 condition
estimators, then remaining RK inverse/driver dependents. Programme status is
**FULL_PROGRAM_INCOMPLETE**.
