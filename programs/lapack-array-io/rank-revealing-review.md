# Column-pivoted QR and GELSY implementation-owner review

Bounded implementation and isolated verification are complete; integration
and the documented mathematical mode gate remain open. This is an
implementation-owner self-review, not repository-owner approval.
Isolated branch feature/lapack-p06-rank-revealing starts at
3d5909d6c26ba2d274749106bda448e2280b8330. Earlier least-squares output in
feature/lapack-p06-least-squares is frozen and remains immutable.

Bounded required scope: the exact eight S/D/C/Z GEQP3 and GELSY rows.
GELSS/GELSD, SVD and other rank-revealing/orthogonal families remain required
later. All eight typed query/execution routes are implemented. The actual
verified profiles below do not close every required mathematical mode gate.

The actual AGENTS, CONTRIBUTING, P01/P06, mission, execution, architecture and
cross-cutting numerical/evidence/ABI rules were reread. Live official Google
C++ and Python guides were accessed 2026-09-07. Existing six-module,
provider-free, BLAS and Random contracts are unchanged.

## Authorized foundation extension

Append kColumnPivotedQr to the existing LapackFactorFamily enum without
reordering old values. Reuse RawLapackPivotView; no second overlapping raw
index class is introduced. ValidateColumnPermutation and
ConvertColumnPermutationToZeroBased use explicit caller byte scratch and
keep inputs/output destinations unchanged on failure. Scratch can change
during duplicate detection. The final permutation means column j of A*P is
original column raw[j]-1; it is not an LU sequential swap list.

Foundation edits remain limited to the specifically authorized
enum/declaration, raw-factory/converter and targeted test regions. Initial
GCC11 C++20/no-exceptions compile/link/run and Clang18 strict foundation tidy
pass. Final candidate02 CTest includes the full existing foundation test in
both ABIs and all four configurations described below.

## Pinned source findings and checked contracts

Pinned source is Reference-LAPACK 3.12.1 commit
6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca, externally prepared under
asc-cpp-evidence/lapack-array-io/lapack-source.

S/D GELSY executable minimum is smaller than their nested GEQP3 minimum:
m=3,n=2,nrhs=2 admits outer LWORK=6 but GEQP3 needs seven after the two-entry
offset, so safe full-call minimum is nine. The documented real minimum
reflects the nested requirement. C/Z use a consistent complex minimum and
compute preferred even for empty cases. Do not forward the unsafe real
minimum or hide a foreign XERBLA path.

RCOND has no source range check. Preserve all safe defined finite values
rather than imposing nonnegative policy; explain actual rank-decision
semantics, including thresholds outside ordinary positive tolerance use.
Rank deficiency is not singular failure.

GEQP3 performs fixed-column JPVT rearrangement even when m=0. Leading fixed
columns preserve their original order; free-column order is not generally
stable. The initial candidate incorrectly substituted a local rearrangement;
that unapproved-emulation choice was identified during root review and is
not final named-provider capability evidence. The correction calls actual
GEQP3 for m=0,n>0. Its outer query remains one, but nested ORM/UNMQR needs
scalar capacity n and checked intermediate 32*n+4160 before returning one.
An explicit n-scalar caller surrogate and canonical LDA=1 keep every
zero-length SWAP A(1,j) address within genuine backing. Only n=0 remains a
local GEQP3 no-output return. GELSY m=0/n=0/nrhs=0 leaves A/B/JPVT unchanged
with rank zero.
Nonempty zero A returns zero solution/rank zero without computing a JPVT
permutation; preserve the original flags on that path. Private 0/1 flag
normalization must not accidentally publish replacement flags.

Blocked LAQPS stores linked-list column indices in real VN2 and uses NINT
to restore them. Possible indices must be exactly representable, not merely
foreign-INTEGER sized: 2^24 for underlying float, 2^53 for double. Apply the
bound when the blocked route can occur and test it with pure integer helpers.

The actual 3.12.1 unblocked QR and LAQP2 call LARF1F, not historical LARF.
Real left LARF1F uses row AXPY, complex LASTV=1 uses row SCAL, and blocked
LARFB uses row COPY. These BLAS integer cursors require the post-element
increment to fit even when the last numerical address already fits. GEQP3
checks (n-1)*effective_LDA+1 except the genuinely inactive real m=1 case.
GELSY also checks its possible RZ row tail and nrhs*packed_LDB+1 except a
real scalar A. No ASC-only source row stride is narrowed. Earlier frozen
reference QR/full-rank-LS cursor closures require a separate root-owned
follow-up; their ordinary numerical passes do not establish that new gate.

## Confirmed open mathematical mode gate

Pinned DGELSY with A=[[0,1],[0,0]], B=[1,0], JPVT=[1,0], RCOND=1e-12
returns INFO=0, rank=0, JPVT=[1,2], X=[0,0] in both actual integer ABIs.
The returned squared residual is one and A^H residual is [0,-1]; the
hand-derived minimum-norm X=[0,1] has zero residual. Source success is not
optimality for this legitimate fixed-zero-column mode. The named route and
flags remain unchanged; no hidden repair or policy rejection conceals it.

Immutable external gate root:
`/home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/p06-rank-revealing-fixed-zero-01`.
`logs/test-lp64/record.json` and `logs/test-ilp64/record.json` each record
command/evidence exit one, not CTest success. They contain exact commands,
source identities and provider archive hashes. Probe source SHA-256 is
df81908f3adf8612c0a62bdf58384c22c141a05bb46c0254bcbce58355dcc273.
LP64 test record SHA-256 is
a2dae14fc27a53b6d4becfeb49612297864a35e0e1cb4d41c1f1017c78bd55f1;
ILP64 test record SHA-256 is
813b8ad10ee0260bc74b5fc1001ae20448814f5d80631f200cdabf441ab4b15c.
Earlier bootstrap zero exits tested fidelity only and are never optimality
credit. Root explicitly retained this failed mode as an open program gate.
The independent all-four-scalar expansion in external
`p06-rank-revealing-fixed-zero-02` fails all eight scalar/ABI mathematical
processes identically, each with command/evidence exit one. Its probe source
SHA-256 is f4b5d3e2877ad9b09d755a87c153f5a31c9a905b97e3b28a9e28b48ab2e36084.

## Preliminary diagnostics, not final evidence

External root `asc-cpp-evidence/lapack-array-io/p06-rank-revealing-diagnostic-01`
contains exact content-addressed runner records. LP64 Release first passed
5/5, then expanded LP64 and true ILP64 Release passed 7/7 each, all with zero
skips. Seven means four scalar mathematical processes, pure-count,
foundation and contract/fault processes; it is not seven routine rows.
The tests exercise independent reflector reconstruction/orthogonality,
optimality/nullspace checks, fixed flags, minimum/preferred work, layouts,
scaling, empty/cutoff behavior, placement and staged failure publication.
Raw failing strict-style candidates are retained; focused fixes and final
style/header/documentation verification remain in progress. These changing
worktree diagnostics will not be relabeled as final snapshot evidence.

Candidate01 at external `p06-rank-revealing-01` is immutable; source archive
SHA-256 c86abfeb3ac45dd39ae6fb2e3447c68e877f701fe06892533af152269e764bb1.
Both true ABIs passed Debug, Release, Clang19 ASan/UBSan and libc-observed
lanes 16/16 each, zero skips, plus strict tidy, format, standalone no-exception
header, actual Doxygen and static source/binary closure. Those are real
historical tests of the initial local-emulation candidate, not final credit
for the corrected m=0 named-provider path. Candidate02 verification must
repeat them and add direct actual-source zero-row permutation comparisons;
the completed results of that repeat are recorded next.

## Final candidate02 identities and evidence

External evidence root:
`/home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/p06-rank-revealing-02`.
The source archive SHA-256 is
75d2f50c80b06e1c03522a9133eb051782f2ff1dde2a3d241911f103e5b736e0;
its independently hashed extracted content identity is
0da250f905dc402440383795819076dae3e4ebf8f848da48260052b53e1d6baf.
The archive is an uncommitted source snapshot based on the branch anchor
above, not a falsely attributed Git commit. This final review text follows
the test freeze; no frozen code, test, header or old evidence was relabeled.

| Final code file | SHA-256 |
| --- | --- |
| `include/asc/dense/providers/lapack_rank_revealing.h` | 80fc2175bf6aa88eb81be5c96764fbf099834d5f20db4ccf9459e0346fc8bf1c |
| `src/dense/lapack/reference_rank_revealing.cc` | a98daa34185e4251013ee2a3ca0f17c5af2fe31f595029f430628a35b4863c3b |
| `src/dense/lapack/internal_rank_revealing_counts.h` | 8b613dab2bb8a1fcfda2d6aace1b3fb5c4b7d68619adaa8c8f0c9c3b26c3de36 |
| `include/asc/dense/lapack/types.h` | 0e383cc83767014a94558bb9f109500d43ab411be7f1ff6909e2c8bb143f45b5 |
| `include/asc/dense/lapack/factor_view.h` | 400feabf1d37e300301feddcc5689c2541496c5d3bfff7edfaa6780ae4347bfb |
| `src/dense/lapack_foundations.cc` | 382f4746f3e7968b9c8f6d4230e5e2d3b1eee17a9f3e6078650b42e86d704676 |

The full fifteen-file code/test identity list and every required command,
exit, raw-log/XML hash and source-before/after identity are checked in
`logs/audit-records/command.log`. That audit log SHA-256 is
35b7b3b78fe24c981e3e5bc4ab561eb23161db912a27b56dc7699569d6ccc599;
its runner record SHA-256 is
17c3ed0ec3eb85b2414c8b5aedcd41925030711ffddfb0d875d2703bbf95a213.

| Actual foreign ABI | GCC11 Debug | GCC11 Release | Clang19 ASan/UBSan | GCC11 libc observer |
| --- | --- | --- | --- | --- |
| LP64, true 32-bit INTEGER | 17/17 | 17/17 | 17/17 | 17/17 |
| ILP64, true 64-bit INTEGER | 17/17 | 17/17 | 17/17 | 17/17 |

Every lane selected and executed seventeen tests with zero skips. Seventeen
means four scalar mathematical processes, eight independent cold first-call
processes and five foundation/count/contract/alias/empty-path processes. It
does not mean seventeen routines. Actual successful stdout contains 720
profile events per lane: 97 GEQP3 and 83 GELSY events per scalar, including
the cold profiles. The eight routine names are the scope denominator, not
the event count. Contract/fault/fixed-zero fidelity processes are not counted
as mathematical-success profiles.

Checks include widened independent A*P=Q*R reconstruction and Q^H*Q,
blocked/unblocked, fixed/free columns, rank deficiency, tall inconsistent
least squares, known wide minimum-norm/nullspace projections, multiple RHS,
all independent A/B layouts, minimum/preferred work, tiny/huge scaling,
known threshold bracketing, exact raw pivot conversion, original-stride
keys and actual singleton backing. Metadata, placement, capacity, overlap,
fault INFO/query/rank/pivot corruption and publication rollback are tested.
The zero-row process compares all scalar/layout/flag cases with real direct
pinned calls, observes the actual foreign INFO/counters, uses genuine caller
surrogate backing and checks untouched A/tau/work padding. Nonpositive
cutoffs on a singular system retain source rank/nonfinite-output behavior;
those checks are explicitly fidelity, not an optimum certificate.

Both ABIs also pass strict Clang18 tidy including owned private headers,
Clang18 format, GCC11 self-contained no-exceptions public-header compilation,
actual Doxygen 1.9.8 warnings-as-errors and exact static closure checks.
All diagnostic builds use C++20, warnings-as-errors and no exceptions.
ASan/UBSan instruments the C++ slice and matching Core dependency; pinned
Fortran archives are not sanitizer-instrumented. The existing sanitizer
allocator-ownership compatibility rule means those lanes are not exact
allocation-count proof. Regular and cold libc-observer lanes enforce zero
allocations around actual queries/executions and failures. The Linux/glibc
observer proves its direct malloc and shared-libstdc++ positive controls;
this is exact-runtime evidence, not a universal platform theorem.

The static object closure excludes only XERBLA argument-error branches
prevented by checked admission. Newly reached csqrt/csqrtf and lround/lroundf
leaves and their direct numeric subgraphs are checked against pinned libm
disassembly, with no unknown or indirect edge. The initial unrecognized-leaf
closure failure remains in the diagnostic history. Source-pinned integer
guards and independent pure-count tests remain separate from dynamic
allocation observation; one is not substituted for the other.

## Exact provider and dependency identities

Reference-LAPACK source commit is
6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca, tree
7217db728e4f7ee87dabf545a1a87a3d6cd30e9b, tag object
5ebe92156143a341ab7b14bf76560d30093cfc54. Provider source-lock identity is
5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a.
Build identities are LP64
7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c
and ILP64
8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97.

| Pinned archive | SHA-256 |
| --- | --- |
| LP64 `liblapack.a` | c7cd5e750cdf993bacf4b27c2b262358754e23633051c7cb9781e5f612595563 |
| LP64 `libblas.a` | e3186556070013a59702288430ef08efa35599155b6c396328dd861b5d685191 |
| ILP64 `liblapack64.a` | 922d310c7fb32acfcc42d3ab4c8209482dba47d06aebddff1f42a7713e919359 |
| ILP64 `libblas64.a` | 3589b8d639895033d842954ded77174491af92cb342fca11a765b316074416cd |

`dependencies.sha256` and its checked `logs/dependency-identities` record
bind the immutable referenceQR05 creation/foundation support, Core13 build
variants, actual foreign headers/configuration, GNU runtime, libstdc++, libm
and allocation/closure probes. No new native numerical route, native rank
certificate or BLAS/Random behavior is credited by this reference slice.

No shared CMake, ABI, mapping, state or registration edit is authorized here.
Parent owns atomic integration. No commit or push was made.

## Handoff and remaining required gates

Next exact verification command:
`python3 /home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/p06-rank-revealing-02/audit_records.py`.

The integrator must finish adapter/test review and atomically register this
provider header/source, private helper, tests and the two foundation symbols;
update public/ABI/source/coverage inventories; then verify the combined
provider-free, LP64/ILP64, installed component and public-consumer lanes on
the actual integration identity. Existing branch work is not an installed
component or PR claim. Preserve the failed fixed-zero mathematical gate and
resolve it explicitly; source fidelity alone cannot close minimum-norm or
optimality requirements for that mode. No owner/license approval is inferred.

GELSS/GELSD, SVD, generalized/constrained least squares, remaining
QR/LQ/QL/RQ/RZ, compact/blocked/tall-skinny/positive-diagonal and generalized
orthogonal families remain required beyond this bounded eight-row slice.
Their absence is not hidden by the native QR or earlier twelve-route
full-rank least-squares work. The full P00–P11 program scope remains unchanged.
