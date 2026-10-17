# Two-stage Aasen producer contract and current work

Status: six producers are callable, with required numerical and native optimal
TB-query failures retained. Runtime and strict checks pass within the recorded
Linux configurations. No new Reference row is verified; P00–P11 remains incomplete.

The six exact required producers are S/D/C/Z SYTRF_AA_2STAGE and C/Z
HETRF_AA_2STAGE at Reference commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Inventory hashes and twelve actual
GNU LP64/true ILP64 prototype emissions are recorded in the existing
continuation's `aasen-two-stage-prerequisite-01`. All have eleven pointer
arguments and the pinned hidden CHARACTER length. Provider sources, notices,
runtime selection and numerical environment remain unchanged.

## Storage and execution contract

The selected original A triangle is symmetric under transpose for SY,
including complex SY, or Hermitian under adjoint for HE. Original HE diagonal
imaginary components are ignored through explicit packing. TB is a persistent
contiguous scalar output of length LTB, with two separate exact-N contiguous
mutable pivot outputs. Queries only inspect metadata. N=0 is a checked noncall.

For active N, LTB must be at least 4*N and scalar scratch at least N. ILAENV
recognizes the `2` at name position eleven and initially selects NB=192. Native
execution sets LDTB=floor(LTB/N), then reduces NB for TB and scratch capacity:
NB=min(192,floor((LDTB-1)/3),floor(LWORK/N)). Preferred WORK is 192*N,
with the pinned scalar rounding accounted for in the integer plan. Both TB and
WORK capacities affect the numerical factorization; all combinations are tested.
Queries independently request TB, WORK or both. Unqueried arrays are unchanged.
Native complex SY permits zero capacities at N=0 and returns zero recommendations;
real and HE require and recommend at least one. No empty-call defect is inferred
from the single-stage routines.

TB[0] stores NB. Band LU has diagonal row 2*NB in zero-based indexing and
leading dimension LDTB, including upper fill-in rows. Its pivot sequence is
interleaved band elimination, distinct from outer symmetric swaps. The first
min(N,NB) outer pivots are identities; subsequent p[i] are in i+1..N.
Band pivots q[i] are in i+1..min(N,i+NB+1). The unit triangular factor in A is
shifted by NB: lower entries A(i,j-NB), or adjoints/transposes of upper
entries A(j-NB,i). Keep original LTB, triangle, symmetry, A, TB and both pivot
arrays together. The existing kAasen report family does not make single-stage
and two-stage storage interchangeable; the report also identifies the native
routine. No existing factor-view factory is broadened.

The checked adapter uses caller scalar and packing buffers plus an explicitly
constructed native INTEGER array of 2*N elements. Both pivot sequences, INFO
and the TB block-width output start with sentinels. The singular band diagonal
is seeded before native entry. Valid positive INFO requires a zero band-LU
diagonal witness; publish factors and both pivot outputs with singular,
documented partial validity. Malformed output withholds public pivots and packed
A, while direct A and TB may change. Native WORK has no final recommendation
promise on ordinary execution and is not tested as one. TB padding within LTB
is unspecified; unselected A and A padding remain unchanged.

Integer admission follows 4*N, 192*N, N+NB-1, explicit TB/WORK subscript
products, strided COPY/SWAP/LACGV terminal cursors and the reviewed panel GETRF
and band GBTRF closure. The consumer-facing TB length is never silently clipped.
Plans bind original/effective strides, LTB, options and scalar/provider identity.

## Finite prerequisite evidence

Both actual integer ABIs compile and link all six calls against actual GNU
emissions. Each native ordinary invocation exits zero with 972 cases: both
triangles; independent minimum/intermediate/preferred TB and WORK capacities;
N=0,1,2,7,65,193; and singular N=1,3,7. N=193 crosses the preferred block
boundary. Guarded INFO, both native pivot arrays, scalar outputs, A padding
and ignored triangle checks pass. The independent wide reconstruction first
reverses band elimination and its interleaved swaps, then forms the shifted
triangular congruence and reverses outer symmetric pivots. Singular factors
retain the same reconstruction requirement.

Each separate 288-case workspace-contract process exits one with four failures.
At N=30001, CSYTRF_AA_2STAGE assigns CMPLX(577*N) to TB[0] without the upward
rounding used by SSYTRF/CHETRF. The required optimal capacity is 17,310,577,
which rounds down to 17,310,576 in float; allocating that recommendation yields
LDTB=576 and lowers NB to 191. Both triangles and TB-only/both queries reproduce
the deficient optimal recommendation. Minimum capacity is still satisfied.
Other scalar classes, WORK-only queries and N=0 controls pass. These are native
optimal-workspace failures, not mathematical factorization failures or proof of
an out-of-bounds access. The provider is retained unchanged; this requirement
must remain visible in maintained CTest and the owner packet.

## Checked implementation evidence

The finite checked mode matrix is six routines × two triangles × two A layouts
× three TB capacities × three WORK capacities, or 216 modes. Ordinary tests
exercise 324 cases per scalar class, including orders 0, 1, 2, 7, 65 and 193
and three singular controls. Both ABIs pass independent reconstruction.
The first HE fidelity tests exposed an objectively incorrect oracle: conjugating
the fixture mirror introduced negative imaginary zero on original HE diagonals,
while the checked API correctly packs ignored imaginary components as positive
zero. Direct native inputs now use that same documented normalization. Exact
output-byte comparisons and mathematical requirements are unchanged. Both
corrected HE fidelity processes pass in both ABIs; original failures, sources
and binaries remain in `aasen-two-stage-fidelity-oracle-fix`.

Range tests execute 512 cases per scalar class: four matrix kinds, eight finite
scales, both triangles/layouts, and independent minimum/preferred TB and WORK.
All native fidelity processes pass. Every scalar class fails required finite
reconstruction at 2*denorm_min, min/1024 and min/8 for the three nonscalar kinds.
There are 144 failed cases and 2,304 nonfinite reconstruction assertions per
class, or 13,824 assertions per ABI. Scalar and min/larger controls pass,
including huge complex values. Exact case maps and native-source linkage are
in `aasen-two-stage-range-classification.json`. This is the retained GBTRF/GBTF2
reciprocal-overflow cause already consolidated in the owner packet. No provider
source, tolerance, valid input or mathematical requirement is changed.

Fault tests pass six processes per ABI, 960 cases per scalar class. Thirty
fault/control kinds cover INFO, both private native INTEGER arrays, TB block
width and singular band-diagonal witnesses. Both layouts/triangles, N=1/3 and
independent minimum/preferred capacities are included. Missing or malformed
outputs withhold public pivots and packed A; accepted singular output is checked
by reconstruction. LP64 partial-width controls and true ILP64 rejection both run.
Allocation guards require zero query/provider-wrapper allocations.

Structural validation passes six processes per ABI: 232 rejection cases per
class, 12 protected-memory metadata queries, four protected empty executions,
original-leading-dimension admission/stale-plan checks and 42 pure count checks.
TB and both pivot arrays participate in operand, scratch and metadata alias
rejection. Rejected calls preserve all reachable operands and caller scratch.

Concurrency passes six processes per ABI. Each scalar class has 32 groups
spanning N=7/67, both triangles/layouts and independent minimum/preferred TB and
WORK. Four serial baselines per group receive independent reconstruction,
including a singular baseline and distinct scales. Four workers share a plan
with separate operands/scratch for eight repetitions: 896 parallel native calls
and 128 stale-plan rejections per class. Complete output bytes match their
validated serial baselines; all guards and report checks pass.

## Final scoped delivery

The sixteen final profiles use actual GNU Linux LP64 and true ILP64, static and
shared libraries, Release, Debug, ASC-ASan+UBSan and separate TSan concurrency.
Twelve normal profiles each pass 37/44 processes; four TSan profiles each pass
6/6. The total is 552 executed processes: 468 pass, 84 required failures, zero
skips. Required failures remain 165,888 reconstruction assertions and 48 native
optimal TB-query assertions across the twelve normal profiles. The final source,
actual selected IDs, executable/configuration/provider identities, full logs,
JUnit outcomes and exact exits are bound in `aasen-two-stage-profile-results.json`
and the per-profile records. Pinned Fortran/BLAS internals are uninstrumented;
ASC sanitizer success is not provider-internal instrumentation or wider admission.

The final native rebind compiles against all twelve actual GNU prototype
emissions. Both ABIs pass 972 native ordinary cases and retain four assertions
in each 288-case native workspace process. These final probes also execute the
legal zero-capacity C/Z SY empty calls; the original native-01 probe's empty
capacity-one scope remains separately preserved. The final maintained source
has eighteen passing strict translation-unit checks, including two isolated
installed consumers. Twelve exact passing second-run inputs are reused, while
four test TUs rerun after removing unused direct includes. Earlier diagnostics
and helper/source snapshots remain in the strict-repair records; no test
assertion, finite mode or provider-output requirement was weakened.

Four installed consumers cover static/shared LP64/ILP64, relocated paths with
spaces and C++-only Dense LAPACK package lookup. Each executes 864 public API
cases: all six classes, both triangles/layouts, independent three-level TB and
WORK capacities, empty/scalar/singular controls and larger reconstruction.
Consumer strict fixes use equivalent boolean expressions, size-correct band
index arithmetic and repository naming. The final consumer executables were
rebuilt and tested against unchanged installed libraries/packages. Previous
consumer sources, binaries and raw successful runs remain preserved. No source
or producer build include path, implicit BLAS/LAPACK/CUDA discovery or unrelated
ASC component is admitted into the copied consumer.

P11 verifies four standalone normal/no-exceptions header processes, twelve new
and zero removed exported ASC symbols per ABI, ten package manifest checks,
three architecture/dependency checks and the maintained CI selector's exact
1,461 configured IDs. Doxygen must cover 156 headers and 2,681 public members with
zero warnings as the required documentation gate. The documentation command
also compares all four installed production libraries with final Release
producer identities. Those commands follow this review update; the final audit
requires their terminal success before atomic coverage/evidence normalization.
No historical run is relabelled as a new whole-tree execution.

The preceding driver commit is
`80e1d01576b92a799541901c9a0fd73f90db26f2`. Its general CI and CodeQL jobs pass.
Four hosted selected profiles each execute 1,415 processes without skips:
LP64 passes 1,221 and fails 194; ILP64 passes 1,225 and fails 190. Exactly nine new driver
gates join all earlier required failures; provider checks pass 111/111 each.
The source-bound results are in `aasen-two-stage-remote-01/hosted-followup`.
Execution provenance distinguishes GitHub feature/run/job identity from the
absent independent artifact checkout/tree manifest. No PR merge-tree numerical
admission or hosted credit for uncommitted two-stage source is inferred.

Explicit feature-ref CodeQL alerts total 3,184 open, including the same twelve
security findings. All 32 new notes were read with exact source excerpts: 31
large-descriptor-parameter notes and one exact WORK integer-encoding equality
note in the driver. They remain open; no suppression, dismissal, inferred
security closure or unsupported public-signature change is made. The current
owner packet retains the numerical, security, notice, dependency and platform
decisions. The pinned provider and three user instruction files are preserved;
Native20 and experimental RobustPpsvx remain separate prior milestones.

Use the same `rk-inverse-profile-recovery-01/latest-handoff.json` for the exact
unfinished delivery command. Finish owned commit/push/draft47 update if pending,
then continue `P05.required.hetrs_aa_2stage`, its drivers and the full remaining
ready programme. No new single-family instruction is required.
