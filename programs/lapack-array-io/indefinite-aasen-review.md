# Single-stage Aasen factorization contract and delivery

Status: **callable implementation; required mathematical and native WORK gates fail**.
Programme status: **FULL_PROGRAM_INCOMPLETE**. The six Reference rows are
`implemented_unverified`; none is fully verified.

The exact pinned interfaces are S/D/C/Z SYTRF_AA and C/Z HETRF_AA. Their six
source instances match the inventory at Reference-LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Twelve actual GNU LP64/ILP64
compiler emissions are preserved under the current continuation's
`aasen-factor-prerequisite-01`. Its final `native-02` probe passes 288 cases per
ABI, including canaries around the actual native scalar arguments and INFO,
workspace/pivot guards, native fidelity and independent reconstruction.

## Source and checked public contract

`QuerySytrfAaWorkspace`/`SytrfAa` and `QueryHetrfAaWorkspace`/`HetrfAa` operate
on selected square A and exact-n contiguous output pivots. SY uses transpose
symmetry, including complex symmetric input. HE uses conjugate transpose and
the established original-Hermitian ignored imaginary-diagonal policy. Row A
and all original HE A use explicit caller packing; other symmetric column A
is direct. Padding and unselected entries are preserved.

The diagonal and first selected offdiagonal store tridiagonal T. Remaining
entries hold shifted unit triangular multipliers: zero-based lower A(i,j)
stores L(i,j+1) for i>j+1, and upper A(i,j) stores U(i+1,j) for j>i+1. The first
L column and first U row are those of the identity. Positive one-based IPIV
records symmetric interchanges in increasing index order, with IPIV(1)=1 and
each subsequent target in its own index..N range. Undoing these interchanges
in reverse reconstructs A from L*T*L**T/H or U**T/H*T*U. These factors have
`kAasen` provenance and do not extend classic, ROOK, RK or two-stage views.

Pinned ILAENV selects NB=64. N>1 requires at least 2*N scalar entries and
prefers rounded 65*N; intermediate 3*N selects NB=2. Real and Hermitian
routines use minimum/preferred one for N<=1. Complex symmetric routines
instead require max(1,2*N) and report preferred 65*N, including native zero
for N=0. ASC empty plans require no storage and execution is a validated
noncall. Queries read no numerical arrays and do not issue native queries.
The checked arithmetic includes rounding conversion, 65*N, N+NB, N*NB+1,
byte totals, original/effective LDA and the initial strided COPY terminal
cursor 1+N*LDA. Native INTEGER output pivots have explicit lifetimes in caller
byte storage. There is no hidden allocation or change to the provider or FP
environment.

The source reports INFO=0 or argument errors -1/-2/-4/-7. Singular T may
complete with INFO=0; completion does not certify invertibility or accuracy.
Execution seeds full-width INFO/private pivots and both WORK components.
Unexpected INFO, invalid full-width permutation indices or incorrect WORK
are provider defects. Packed A and public pivots are withheld then; direct
symmetric column A may already have changed. Structural rejection preserves
numerical storage and scratch, and unsafe report/metadata aliases also
preserve the report.

## Executable evidence

The finite matrix covers actual Linux GNU 11.4 LP64 and true ILP64, static and
shared linkage, Release, Debug and ASC-ASan+UBSan, plus four TSan concurrency
profiles. Twelve normal profiles each pass 38 of 45 processes and retain seven
required failures. All four TSan profiles pass six processes. The canonical
selection totals 564 processes: 480 passes, 84 required failures and zero skips.
The pinned Fortran/BLAS internals are uninstrumented. Wider provider/platform
admission and full-programme acceptance remain separate gates.

Ordinary mathematical and native-fidelity processes each exercise 132 cases
per scalar class: both triangles/layouts; minimum/intermediate/preferred WORK;
N=0/1/2/3/7/67; singular N=1/7/67; and normal scaling. Range processes each have
360 cases per class. The independent oracle extracts shifted unit triangular
multipliers and T, multiplies in wide arithmetic and reverses the pivots.
Native byte fidelity is a separate process and passes throughout.

Fault processes exercise 256 cases per class, covering 16 native mutations in
16 modes. Full/partial INFO and pivots, invalid positive permutations, and
missing/nonfinite/incorrect WORK test publication boundaries. Validation has
108 structural rejections per class, protected unread queries/empty calls,
source INTEGER bounds and rollback/report-alias checks. Each concurrency class
has 16 groups, four workers and eight repeats, with 448 native calls and 64
stale-plan rejections using shared immutable providers/plans and private
writable arrays and scratch.

Four relocated public consumers each pass 288 analytic factor/permutation
cases, including N=67 and all WORK choices. The current consumers were rebuilt
against the same preserved installed libraries after an equivalent explicit
byte-span comparison and printf conversion. Twenty strict translation units
pass. Four standalone header checks, twelve added/no removed dynamic exports
per ABI, ten package checks and two architecture checks pass. The maintained
CI selector includes all 45 new runtime processes and both header checks in its
1,319-process selection. Doxygen and installed-library identity are recorded
in the final audit alongside source and provider hashes.

The source-bound `aasen-factor-final-audit/audit.json`, normalized coverage
extension and existing latest handoff retain the precise execution identities.
Engineering results do not close either required provider failure below.

## Required numerical disposition

`AASEN-FACTOR-RANGE` affects all six producer rows. Scaled identity controls at
orders 1/3/67 pass. The coupled order-3 case has leading block
`[[0,t,t/2],[adj(t),0,0],[adj(t/2),0,0]]`; the order-67 case appends a scaled
identity. Here adj means identity for SY and conjugation for HE. Exact Aasen
factors have zero leading T diagonal, T(0,1)=t, T(1,2)=0, and shifted multiplier
1/2. Every tested exact factor is representable.

At component scale min-normal/8, all scalar classes produce nonfinite factors.
The pinned xLASYF_AA/xLAHEF_AA panel explicitly forms ONE/offdiagonal before
SCAL, reaching the established reciprocal-overflow mechanism. At component
scale .75*max, complex SY/HE also fail reconstruction with finite outputs;
.25*max remains a passing control. Native factor bytes match the checked
route. These extend existing indefinite factor arithmetic categories through
different panel entry points; no provider correction, new robust route,
mathematical waiver or notice adoption is authorized by the result.

## Native workspace contract

`AASEN-EMPTY-WORK` affects CSYTRF_AA/ZSYTRF_AA: query and normal N=0 execution
return optimal WORK zero, below the documented minimum one. The original ABI
probe checks fidelity to the source and does not establish validity of that
recommendation. Two maintained processes separately check native fidelity and
the documented WORK contract, with 72 cases per process. Fidelity passes; the
contract retains eight failed assertions per normal profile. The six reviewed
contracts still have 72 option modes. This concrete missing predicate was
recorded before extending the profile; ASC's empty noncall does not repair it.

## Evidence corrections and reuse

Each normal profile retains six range failures with 323,952 failed assertions
(53,976 per real class and 54,000 per complex class), plus the eight native WORK
assertions. Across twelve profiles, 3,887,424 mathematical and 96 native WORK
assertions fail. CTest's configured JUnit cap truncates the large assertion
payloads; the complete verbose logs preserve every failure and TestContext
total. The corrected audit checks those totals against the full stream, while
JUnit preserves selected/executed process outcomes. The original failed audit
remains separate. No test predicate, case or tolerance was weakened.

The first static LP64 Release invocation completed 43 processes with 37 passes,
six range failures and CTest exit 8, then appended the two newly required native
WORK processes with exit 8. Other original normal invocations ran all 45 with
exit 8. All four original TSan invocations exited zero. After a strict finding,
only a direct include was added to the WORK probe; both strict ABI checks and
only its two affected processes were rerun in each of the twelve normal builds.
The canonical selection uses 43 unchanged original core processes plus those
two current WORK processes. Every original full invocation and the first 43+2
composite remain preserved; no composite is represented as a fresh whole run.

Initial source/consumer style failures and the missing FILE_SET header were
corrected with their original records retained. The frozen header-manifest
oracle explicitly gained the new public header and changed its count from 152
to 153; all previous entries and checks remain. No provider source, numerical
requirement, support guard or existing factor-view factory changed.

See `programs/lapack-array-io/owner-decisions.md` for the exact unresolved provider
choices. No correction, new robust route or numerical waiver is authorized.
Continue the exact inventoried Aasen solve consumers and their dependencies;
Native20 and separately selected experimental RobustPpsvx stay protected.
