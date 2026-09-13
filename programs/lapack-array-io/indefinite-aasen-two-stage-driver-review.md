# Two-stage Aasen driver contract and delivery

Status: six checked drivers are callable with required numerical and native
workspace failures retained under `P05.required.hesv_aa_2stage`. Reference
verification is incomplete; final audit and atomic mapping govern delivery.
The producers and consumers are integrated through
`d58b8c315ddcf8d5e07bc544fb8cb35a861474f1`; their retained numerical and native
TB-query requirements remain failed. The full P00–P11 programme is incomplete.

## Exact pinned execution

The inventory contains SSYSV_AA_2STAGE, DSYSV_AA_2STAGE, CSYSV_AA_2STAGE,
ZSYSV_AA_2STAGE, CHESV_AA_2STAGE and ZHESV_AA_2STAGE. Their actual GNU LP64
and true ILP64 emissions contain fourteen pointer arguments: UPLO, N, NRHS,
A, LDA, TB, LTB, IPIV, IPIV2, B, LDB, WORK, LWORK, INFO, followed by the GNU
hidden CHARACTER length. The twelve emitted headers and exact source inputs
are retained in `aasen-two-stage-driver-prerequisite-01` in the current
continuation evidence area. Emission alone is not runtime ABI verification.

All six routines first issue a dual query to their two-stage factor producer,
even during ordinary calls and when NRHS is zero. Both TB[0] and WORK[0] are
written. An outer query of either or both capacities then returns without
reading or changing A, pivots or B. The producer query forms `(3*192+1)*N`
in native INTEGER; its `577*N` expression and the driver's INTEGER conversion
of WORK[0] must be covered in addition to the already reviewed producer and
consumer active arithmetic. Workspace/query units are scalar entries.

Real and Hermitian drivers require LTB>=max(1,4*N) and LWORK>=max(1,N).
Complex symmetric drivers admit LTB>=4*N and LWORK>=N, including zero at N=0.
Their empty native query recommendations are zero and are legal for the actual
two-stage dependencies. This differs from the older single-stage driver issue.
Even an ordinary empty native driver writes its two first query slots, so a raw
ABI probe provides one live TB and WORK placeholder with outer guards.

Ordinary execution factors A for every N>0, including NRHS=0. Only factor
INFO=0 enters the consumer. Positive factor INFO skips the solve and preserves
B; A, TB and both pivot arrays contain the documented factorization output.
The driver restores the optimal WORK[0] after ordinary execution. S, C and CH
use SROUNDUP_LWORK; D, Z and ZH use direct assignment. Complex SY uses
transpose and HE uses adjoints. The factor producer's original Hermitian
imaginary-diagonal handling and both factor/pivot encodings remain unchanged.

## Checked public contract to implement

Use mutable square A, contiguous persistent TB with its exact supplied LTB,
two separate mutable exact-N ASC index vectors, and mutable N-by-NRHS B.
Triangle, scalar, symmetry, layouts, original/effective strides, capacities
and provider identity belong to the plan. Queries inspect metadata only.
All operands, workspace and live metadata must be accessible and disjoint.
N=0 is a validated noncall; N>0 with zero RHS still needs factor workspace and
publishes the factorization. Existing factor factories and pivot-family
meanings are preserved.

Reuse caller-owned live scalar WORK with minimum N and the checked rounded
192*N recommendation, two placement-constructed native INTEGER arrays and
explicit row/Hermitian matrix packing plus row RHS packing. TB remains in its
original caller storage. Full-width INFO starts at the native minimum. Validate
the source-derived TB block width, both pivot sequences, final WORK witness and
positive-INFO zero band-U diagonal before publication. A valid positive INFO
publishes documented partial factors and leaves B unchanged. Provider defects
withhold packed outputs; direct native outputs may have changed. No allocation,
fallback, provider modification or mathematical waiver is introduced.

## Frozen initial finite probes

The native ordinary set has 2,448 cases per ABI: the prior consumer's 2,304
triangle/order/RHS/independent-capacity and LASWP boundary cases, plus 144
singular driver cases at N=1/3/67, NRHS=0/3 and paired minimum/preferred
capacities. The added cases are required by the driver's factor-on-zero-RHS
and positive-factor-INFO behavior. Check every native INTEGER guard, both
pivot arrays, selected/ignored storage, known solutions and independent
residuals. Full reconstruction at NRHS=3 covers each distinct producer input;
other RHS counts retain their solution, factor-structure and mutation checks.

Retain the producer's 288 query cases per ABI: both triangles, all six scalar
classes, N=0/1/2/7/65/193/30001/30003, and either/both query flags. Check both
observable query outputs against the exact source in the fidelity probe.
The separate required query gate checks each requested recommendation against
its unchanged capacity requirement. The established CSY TB rounding cause
remains visible; ordinary execution cannot certify that query requirement.

After the native contract probe, implement and execute the maintained public
ordinary/range, failure, workspace/alias, protected-memory and concurrency
requirements. Deliver strict analysis, actual ABI/linkage profiles, installed
consumers, public headers/exports, package/architecture integration, CI
selection and documentation before normalized coverage and the owned feature
commit. Use the existing latest handoff; continue the ready packed-indefinite
chain and the full remaining programme after this driver slice.

Current guarded native results pass 2,448 ordinary cases and 288 exact
query-fidelity cases in each actual ABI. Each separate 288-case required query
gate exits 1 with four failed capacity assertions. CSY at N=30001 returns
17,310,576 TB entries for a 17,310,577-entry optimum, in both triangles and
both modes requesting TB. The existing `AASEN-TWO-STAGE-TB-QUERY` cause is
retained. All native scalar/pivot guards, empty and zero-RHS behavior,
singular RHS preservation and ordinary mathematical checks pass. Exact
compiler/dependency/library/executable inputs and raw failures are recorded in
`native-01` and `native-classification.json`; no wrapper is yet normalized.

## Maintained checked execution

Initial static Release checks pass twelve ordinary mathematical/fidelity
processes in each actual ABI, with 760 cases per scalar class and no skips.
The checked zero-RHS path still factors nonempty A, retains both pivot arrays
and the caller's full TB capacity, and preserves B. Empty N remains a checked
noncall. These records bind the actual initial source and executable inputs;
they do not certify later edits or a wider platform.

Range and fault checks execute eighteen processes per ABI: twelve pass, six
required mathematical processes fail, and none skip. Each range class retains
1,024 cases with exactly representable RHS columns at the producer and consumer
scales. Every solution failure group equals its preserved consumer counterpart.
Every factor failure group equals its preserved producer counterpart for each
independent RHS layout. There are 34,368 failed finiteness assertions per ABI:
27,648 reconstructed-factor and 6,336 solution assertions from the existing
GBTRF/GBTF2 tiny-pivot reciprocal cause, plus 384 solution assertions from the
existing large-complex GBTRS/TBSV division cause. All factor and solution native
fidelity checks pass. The complete raw-group comparison is preserved in
`aasen-two-stage-driver-range-classification.json`; no new broad provider
reproduction or mathematical waiver is needed to identify these same causes.

Fault checks pass 4,480 cases per scalar class per ABI: 35 fault/control modes,
both triangles, independent A/B layouts, N=1/3, NRHS=0/2, and independent
minimum/preferred TB/WORK. The producer's full-width INFO, both pivot sequences,
block-width and singular-diagonal witnesses remain checked. Added final WORK
omission, nonfinite, zero, incorrect and partial-complex writes exercise the
native driver's restored recommendation. Full-width LP64 writes and real-only
no-op controls remain accepted. Invalid provider output withholds public pivots
and packed A/B, while valid positive INFO publishes partial factors with B
unchanged. Accepted outputs pass reconstruction and, when applicable, independent
known-solution/residual checks. Allocation and storage guards pass.

Structural validation passes 608 rejections per scalar class per ABI. The
76 rejection modes cover both triangles and independent A/B layouts, the ten
operand-pair aliases, workspace/metadata separation, original strides and stale
plans. Protected-memory queries exercise N and NRHS=0/1/2 without reading
numeric values; checked empty-N execution remains a noncall. Original unused
LDA/LDB bounds are checked separately before packing.

Concurrency passes 128 groups per scalar class per ABI with four independently
owned mutable A/TB/P/Q/B/workspace/report sets sharing one immutable provider
and plan. Each class executes 512 serial mathematical baselines, 3,584 native
parallel calls and 512 stale-plan rejections. Eight repetitions retain both
triangles, independent layouts/capacities, N=7/67 and NRHS=0/3. Singular and
zero-RHS B preservation, exact serial-output comparisons and guards pass.

Strict analysis, installed consumption and the finite runtime profiles below
are terminal. Reference verification remains incomplete because the required
workspace and numerical gates above remain failed.

## Final scoped delivery

Sixteen actual GNU Linux profiles cover LP64 and true ILP64, static/shared
linkage, Release, Debug, ASC-ASan+UBSan and separate TSan concurrency. Twelve
normal profiles each pass 37/44 processes; four TSan profiles each pass 6/6.
The canonical total is 552 processes: 468 pass, 84 required failures, zero skips.
The twelve normal profiles retain 412,416 factor/solution finiteness assertions
and 48 native optimal-TB capacity assertions. All selected/executed IDs, raw
exits, complete verbose assertion logs, JUnit outcomes, source/binary/configuration
identities and pinned provider/ABI/runtime inputs are preserved. The pinned
Fortran/BLAS internals are uninstrumented; these checks give no provider-internal
sanitizer or wider-platform admission.

All eighteen current strict translation-unit checks pass, including two
isolated installed consumers. Before-fix sources and diagnostics are preserved
for direct includes, a returned const Status, a logging cast and test helper
size. These repairs do not change the numerical algorithm, finite inputs,
assertions or tolerances. The current native probe again compares all twelve
emitted GNU signatures and passes 2,448 ordinary plus 288 query-fidelity cases
per ABI. Its separate required query gates each retain four failures among
288 cases. Earlier native executions retain their actual source identity.

Four installed consumers use static/shared LP64/ILP64 and relocated paths with
spaces. Each passes 1,920 public driver cases, including independent A/B
layouts, minimum/preferred TB/WORK, empty and scalar cases, N=3/67, singular
factor results and factor-on-zero-RHS behavior. Every active result checks
factor reconstruction; nonsingular positive RHS also checks independent known
solutions and residuals. Ignored storage, partial/empty RHS and caller workspace
guards pass. Package metadata and consumer compilation contain no source or
producer build include paths or implicit BLAS/LAPACK/CUDA discovery.

P11 delivery checks four normal/no-exceptions header processes, twelve new and
zero removed exports per ABI, ten package manifest checks, three architecture
checks and the maintained CI selector's 1,552 IDs. Documentation generation
must cover 158 headers and 2,705 public members without warnings. The final
audit enforces these terminal results and four installed/final Release library
identity comparisons before coverage normalization and owned staging.

Pushed parent `d58b8c315ddcf8d5e07bc544fb8cb35a861474f1` passes general CI
and CodeQL analysis. Four hosted selected profiles each execute 1,506 processes
with no skips: LP64 passes 1,299/fails 207; ILP64 passes 1,303/fails 203. Exactly
six new consumer range gates join all prior failures; provider checks pass
111/111 each. The feature-ref alert read contains 3,244 open alerts, including
the unchanged twelve security findings. All 45 new notes are read with exact
source excerpts: 44 descriptor-parameter notes and one exact integer TB
block-width check. Nothing is dismissed or suppressed. GitHub run/head/job
identity is distinct from absent independent ASC checkout/tree artifacts;
these hosted records grant no dirty-driver or PR merge-tree admission.

Use the existing latest handoff to finish audit/index, owned commit, normal
push and draft PR #47 update. Then continue `P05.required.hptrf` from the live
backlog, its packed-indefinite dependency chain and all remaining ready work.
Native20 and explicitly selected experimental RobustPpsvx remain separate
prior milestones. Full P00–P11 acceptance is incomplete.
