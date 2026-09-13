# RK factor producer contract review

Status: **implemented; active mathematical checks pass; native empty-order provider gate failed**.
Programme status: **FULL_PROGRAM_INCOMPLETE**.

This family covers S/D/C/Z SYTF2_RK/SYTRF_RK and C/Z HETF2_RK/HETRF_RK
at pinned LAPACK 3.12.1 commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`.
Implementation parent is pushed `ffc4bd8`. Exact sources, hashes, twelve
inventory rows, twenty-four actual GNU Fortran 11.4 LP64/true-ILP64 prototype
emissions and the public contract decision made before implementation are
retained under `continuation-20260912-01/rk-factor-prerequisite-01`.
Earlier deliveries and both user instruction files remain unchanged.

## Storage, ABI and workspace

`lapack_indefinite_rk.h` adds twenty-four public query/execute declarations.
A and E have the same scalar type: real for S/D, complex for C/Z, including
Hermitian operations. Native dimensions, IPIV and INFO use the selected signed
INTEGER width. The six unblocked symbols are absent from pinned `lapack.h`;
private declarations derive from actual GNU emissions and guarded executed
probes. The six blocked symbols use the pinned declarations. One trailing
GNU `size_t` UPLO length is part of this admitted boundary; no portable Fortran
ABI is invented. Final active probes pass 168 cases per ABI against actual emitted declarations.

The symmetric factorization is `A=P*U*D*U^T*P^T` or its lower counterpart;
Hermitian operations use adjoints. A holds only D's diagonal and strict unit
triangular multipliers. Upper E(i)=D(i-1,i), E(1)=0; lower
E(i)=D(i+1,i), E(n)=0, using one-based indices. E is zero for 1-by-1 blocks
and for the unused partner of each 2-by-2 block. The corresponding selected
2-block offdiagonal in A is zero. E is persistent output, not workspace.

Pivots contain independent signed adjacent negative pairs. Upper interchanges
run in descending index order and lower in ascending order, with each i
exchanged with abs(IPIV(i)). Targets are at most i for upper, at least i for
lower. The source documentation's lower second-interchange sentence has a
k-1 wording error; the source uses k+1. The report uses `kRook` for this pivot
protocol. Existing ROOK factor factories reject RK routine names and storage;
no existing factor type or solver is changed or advertised as an RK consumer.

All original Hermitian input is privately packed, even column-major, reading
only real diagonal components and selected offdiagonals. Complex symmetric
input retains complete selected coefficients. Row-major A also needs private
packing. Normal publication changes only selected A and logical E/pivots,
preserving the opposite triangle and padding.

Queries inspect metadata only, with no numerical reads or native query.
TF2 requires no scalar WORK; TRF requires minimum one and preferred 64*n,
with actual S/C upward rounding and D/Z conversion from the pinned ILAENV
branch. Its minimum reduced block size is eight for SY and two for HE.
Supplied capacity is capped at preferred, retaining minimum/reduced/preferred
native execution. Both variants need n provider INTEGER entries in
caller byte storage; execution begins their trivial lifetimes. Packed A needs
n*n live scalar layout entries. Empty n=0 requires no workspace or array
access and makes no native call.

Plans bind routine/scalar/provider ABI, n, triangle, symmetry, layout, original
and effective leading dimensions, and E/pivot sizes/increments. Both vectors
must be contiguous and exact-n. Original metadata, effective foreign strides,
N+1 and strided BLAS terminal cursors are checked; blocked N*64 and actual
rounding are checked even for minimum-work execution. Panel LDW=N and all
strided vector counts are bounded by the same factor-count checks. Packing
products and total workspace bytes are checked independently.

A, E, pivots, scratch and live provider/plan/workspace/report metadata are
mutually disjoint. Storage is host/pinned and accessible to the explicit serial
CPU context. These synchronous calls allocate nothing, transfer nothing,
change no global state and apply no numerical fallback. Concurrent calls
require separate writable storage and reports. Metadata aliases preserve the
report; other preflight failures reset it and preserve numerical arrays.

Full-width INFO is seeded at INTEGER minimum. The adapter checks INFO in
0..n, complete pivot structure, structural-zero E slots and zeroed A 2-block
positions before publishing packed A and public pivots. E and direct
column-major symmetric A may already have changed on provider defects;
all outputs are then unusable. Positive INFO retains complete raw factors/E/
pivots as documented partial output and diagnostic_index=INFO-1. An exact-zero
reported diagonal gives a singular outcome; other nonfinite source behavior
retains partial-result status. INFO=0 is not a finiteness or conditioning
certificate. No post-factor zero scan replaces native INFO.

## Preserved native empty-order failure

All six pinned unblocked routines initialize E before their main loop checks
n=0. Upper writes E(1); lower writes E(0); INFO remains zero. The native guard
probe passes an interior pointer into five allocated live scalar objects, so
both writes are safely observed in allocated storage. All twelve empty cases
per ABI fail the unchanged-E assertion while other guards pass. Original
failed commands remain under `executed-abi-{lp64,ilp64}-01/run-empty`.

ASC's documented empty noncall avoids this native path. The required
`indefinite_rk_native_empty` test preserves the provider contract failure
without WILL_FAIL, skips or weakened assertions. This remains a provider
acceptance blocker, distinct from successful ASC empty execution.

## Preserved engineering checkpoints

The smallest double LP64 run passes both mathematical and native-fidelity
processes, 280 cases each. Both engineering ABIs then pass all twelve ordinary
processes. Coverage includes n=0/1/2/3/7/67, both triangles, padded row/column
layouts, independent paired interchanges, completed singular factors, scaled
matrices and minimum/reduced/preferred blocked workspace. Reconstruction
forms the unit triangular factor and explicit D in wide arithmetic, multiplies
them and undoes the global permutation. It uses no native solve or factor
update recurrence. Fidelity compares selected factors, E and pivots against
actual direct native calls, with guarded native metadata and workspace.

Both ABIs pass all twelve range processes, 372 cases each: 360 representable
factor cases and twelve explicitly nonrepresentable controls. Tiny identity,
2-block and coupled 3-by-3 cases cover min_normal/8, min_normal/2, min_normal,
one, maximum/4 and 0.75*maximum. Large complex offdiagonal components remain
finite, with representable expected multipliers. The nonrepresentable control
has an exact final 1-block diagonal of magnitude 2*maximum, proven in wide
arithmetic and checked for native fidelity without a finite-factor claim.
The 168-case active ABI process also passes. Each fourteen-process range/ABI
run has thirteen passes, the required native-empty failure with twelve failed
assertions, and zero skips.

At that earlier checkpoint, fault, structural, concurrency, installed-consumer
and final-profile checks were still pending. Those engineering results did
not promote any Reference inventory row to verified.

The completed engineering checkpoint additionally passes all six fault processes
per ABI, 628 cases each including 92 empty noncalls. Tests cover full-width
INFO seeds, omitted/partial writes, invalid INFO, malformed pivots, changed E,
zeroed factor-slot corruption and bad returned WORK. Structural checks pass
312 rejection cases per symmetric variant and 328 per Hermitian variant,
plus 24 protected-memory queries, eight empty executions, eight incompatible
ROOK provenance rejections and 26 count checks per variant. The initial
validation build error (three missing explicit empty-memory constructors) and
its source snapshot remain retained; no test had run before that correction.

Each ABI passes six concurrency processes, each with 24 groups of four workers
and four repetitions: 384 native calls, 384 empty noncalls and 384 structural
rejections, with shared immutable provider/plans and private writable storage.
The first four relocated installed consumers pass 1,152 cases each. Those
runs remain retained; expanded consumers now pass 1,440 cases each, including
minimum and preferred workspace plus exact SY 8N/7N and HE 2N/1N reduced-block
thresholds. The initial ordinary engineering modes did not exercise the SY
8N threshold; this coverage gap and the original sources are preserved in
`rk-factor-style-and-workspace-finding-01`. Production numerical code is unchanged.

After test helper extraction and threshold expansion, both engineering ABIs
run all 44 processes: 43 pass, the required native-empty process fails twelve
assertions, and zero skip. Strict checks initially found test include/style/
helper-size issues and an imprecise WORK-fault applicability condition; raw
failures and pre-edit sources remain. At that checkpoint, strict completion and final-profile/ABI/installed-source
checks were still pending. Their completed results follow.


## Final bounded evidence

Twelve actual static/shared LP64/true-ILP64 Release/Debug/ASC-ASan+UBSan
profiles each run 44 required processes: 43 pass and the native-empty process
fails twelve unchanged-E assertions. All four TSan profiles pass six
concurrency processes each. Canonical totals are 552 processes, 540 passes,
twelve required failures, 144 failed assertions and zero skips. No failure is
converted to WILL_FAIL or accepted as a pass.

All twelve ordinary mathematical/native-fidelity processes pass 280 cases each.
All twelve range processes pass 372 cases each, including 360 representable
cases and twelve explicitly nonrepresentable controls. Fault, full-width INFO,
structural, protected-memory, old-ROOK provenance and concurrency assertions
pass in the applicable profiles. The final twenty-one source/build inputs are
frozen under `rk-factor-final-source.json`; profile evidence is retained in
`rk-factor-{static,shared}-{release,debug,sanitizer,tsan}-{lp64,ilp64}-final`.
Provider Fortran/BLAS internals remain uninstrumented; ASC/test sanitizer passes
do not extend admission to those internals or other platforms/providers.

The final four relocated installed consumers pass 1,440 cases each and use
public headers only. Eighteen strict translation-unit checks and four
standalone header checks pass. Twenty-four actual emitted prototype checks
and 336 active guarded direct ABI cases pass; the twenty-four direct empty
cases retain their required E failures. Exports add twenty-four symbols per
ABI and remove none. All four installed libraries match the final Release
producers after the normal CMake shared-library installation transform.
Package checks cover four manifest contexts and six fixtures; architecture
and dependency checks pass. The actual maintained CI selector includes all
44 new required runtime processes. Final Doxygen and normalized evidence
results are bound by `rk-factor-final-audit/audit.json` and the delivery audit.

`BLOCK-INDEFINITE-RK-EMPTY-E` remains the new provider blocker. Active factor
mathematics pass the tested domain; the native empty-order contract does not.
All twelve normalized rows remain `implemented_unverified`, with 120 reviewed
modes. No Reference row is promoted to verified. Previous numerical, provider,
XBLAS, CodeQL alert, platform and complete-execution-record blockers remain.
Continue SYTRS_3/HETRS_3 consumers of the separate RK A/E/pivot storage, then
condition, inverse and driver dependents. The programme remains
**FULL_PROGRAM_INCOMPLETE**.
