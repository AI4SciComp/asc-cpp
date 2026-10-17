# RK indefinite driver contract and delivery

Status: **callable, implemented-unverified; required mathematical gates fail**.
Programme status: **FULL_PROGRAM_INCOMPLETE**. No Reference row is fully verified.

The exact pinned interfaces are S/D/C/Z SYSV_RK and C/Z HESV_RK. All six
sources match their inventory hashes at Reference-LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Twelve GNU LP64/ILP64 emissions
and signature comparisons are retained under the existing continuation's
`rk-driver-prerequisite-01`. Each ABI's recovered native prerequisite passes
180 workspace queries and 120 guarded executions, including empty, singular,
zero-RHS and minimum/preferred workspace cases. These are prerequisite results,
not adapter acceptance.

## Source and public contract

SYSV_RK uses transpose symmetry, including complex symmetric A; HESV_RK uses
conjugate transpose and ignores original diagonal imaginary components. A is
square, E and output pivots are contiguous exact-n vectors, and B has n rows.
The driver queries TRF_RK, executes that factorization, and invokes TRS_3 only
if factorization succeeds. N>0 with NRHS=0 still factorizes A. ASC N=0 validates
metadata and plans, then completes without accessing arrays or the provider.

Selected A holds D's diagonal and strict U/L. E holds upper D(i-1,i) or lower
D(i+1,i); signed adjacent pairs have independent directional pivot targets.
These are the existing global-permutation RK factors, produced together by
the driver's TRF_RK stage. They meet the common-origin precondition of the raw
RK consumers. Classic/interleaved ROOK factories remain unchanged. No separate
factorization or solve algorithm is substituted for the named driver.

Active scalar WORK has minimum one and preferred checked rounded 64*n entries.
The source converts the factor query result to INTEGER and, for S/C, rounds
again; the existing checked representability bound covers both steps. Every
normal return restores preferred WORK, including singular output. Private
provider INTEGER pivots occupy caller byte storage with explicit lifetimes.
Row A/B and all original Hermitian A use caller-owned packing. E and column B
are direct. Source cursor counts, byte bounds and original LDA/LDB are checked.
Zero-column B uses a valid local dummy and native LDB at least n.

Queries read no arrays. Structural rejection preserves numerical storage and
scratch; metadata/report alias rejection also preserves the report. Execution
seeds full-width INFO and private pivots with INTEGER minimum and both complex
WORK components with negative values. INFO, returned WORK, own-index pivots,
E structural zeros and separated 2-block zeros are checked before publishing
packed A/B or public pivots. Source-consistent positive INFO requires a zero
1-block diagonal and publishes completed singular factors while preserving B.
Provider defects withhold packed output and public pivots; direct A/E/B may
already have changed. INFO=0 does not certify numerical accuracy. The provider,
floating-point environment, allocation/transfer policy and existing kernels
remain unchanged.

## Executed initial checks

Both actual-ABI Release suites pass all twelve ordinary mathematical/fidelity
processes with zero skips, 496 cases per process. The maintained independent
RK reconstruction and RHS residual/known-solution oracles cover both triangles,
independent A/B layouts, minimum/preferred workspace, zero/scalar/paired and
orders 3/7/67, zero/one/three RHS columns, singularity and scaled matrices.
Driver-produced factors are reused through checked TRS_3. Padding, ignored
triangle, E/pivot and workspace guards are retained. Production strict analysis
passes. The first test-unit strict run identified three include issues; its
raw diagnostics and pre-fix source are preserved before the mechanical fix.

Both Release range suites pass six native-fidelity processes and fail all six
required mathematical processes, with zero skips and 960 failed assertions
per ABI. Real classes run 128 cases per process; complex classes run 160.
Two tiny scalar values fail finite X, exact X=1 and wide residual requirements.
The .75*max complex 2-block also fails despite finite exact X=(1,1); .25*max is
a passing control. NRHS=0 remains a factor-only control. All non-mathematical
checks and direct-provider comparisons pass.

These failures extend `BLOCK-INDEFINITE-RK-SOLVE-RANGE` through the driver's
TRS_3 call. No new provider strategy, fallback or waiver is inferred. Original
scalar requirements and the existing 2-block requirements are retained. The
source-preserved provider cannot simultaneously retain these failures and
pass their mathematical gates.

The raw initial records are `rk-driver-initial-{lp64,ilp64}-01`; range records
are `rk-driver-range-{lp64,ilp64}-01`. Commands, exits, selected tests, JUnit,
verbose logs and source hashes are retained outside the repository.

## Final scoped delivery evidence

All sixteen profiles completed without an interruption or retry. Twelve
static/shared LP64/ILP64 Release, Debug and ASC-ASan+UBSan profiles each execute
43 processes: 37 pass and six required range processes fail, with 960 failed
mathematical assertions and zero skips. Four TSan concurrency profiles each
pass six processes. The total is 540 processes, 468 passes, 72 required failures,
11,520 failed mathematical assertions and zero skips. Provider Fortran/BLAS
internals are not instrumented. INFO=0 and native fidelity remain separate
from mathematical acceptance.

Each scalar class has 1,024 maintained malformed-output cases across sixteen
INFO, pivot, E and WORK faults. Full-width and partial writes, invalid pairs,
omitted E, inconsistent positive INFO and incorrect WORK are checked before
publication. There are 240 structural rejections per class, plus protected
unread queries and empty calls. Concurrent tests use 32 groups, four workers
and eight repetitions: 1,792 native calls and 256 stale-plan rejections per
class, with shared immutable provider/plans and private writable storage.

Four relocated public consumers pass 768 cases each, including driver-origin
TRS_3, CON_3 and TRI_3 reuse. Eighteen strict translation units and four
standalone header tests pass. The final native probe passes 288 guarded cases
per ABI against six original GNU compiler emissions per ABI. Twelve public
symbols are added per ABI and none removed; installed production libraries
match the final Release producers after the normal installation transform.
Ten package-manifest and two architecture/dependency checks pass. The actual
CI selector includes 1,272 tests and explicitly requires all 43 new driver
runtime processes. Doxygen covers 152 headers and 2,633 members without warnings.

The first structural run exposed an incorrect E/pivot overlap fixture for
float: E bytes [0,8) touched pivot bytes [8,24) without overlapping. Moving E
to the first output pivot establishes the intended overlap in every class;
all original rejection assertions remain. Strict findings were fixed by direct
includes, helper extraction and equivalent constexpr branches. The corrected
installed consumer was rebuilt against the same preserved relocated libraries.
Doxygen needed this review added to its maintained input list. A native-audit
script incorrectly required another probe's textual success marker; the
actual guarded run had already completed all 288 cases and exited zero, and
was reused after correcting the audit to the maintained guard's behavior.
Every original failed attempt remains in the evidence area. No mathematical
requirement, native routine or provider arithmetic changed.

`rk-driver-final-audit/audit.json` binds sources, selected/executed identities,
commands, exits, binaries, provider/configuration and these scoped results.
`rk-driver-profile-results.json` retains the failing CTest exits. Source-bound
contracts enumerate 192 legal option modes across the six exact inventory rows;
that is a review count, not a verification count. The schema-2 driver evidence
extension preserves historical normalized execution records and Native20.

Original hosted inverse-commit evidence is separate. The 7add1a8 push profiles
execute 1,227 tests each: LP64 has 1,060 passes/167 required failures and ILP64
1,064 passes/163 required failures, with zero skips and upstream 111/111 each.
Their failure sets match the prior condition delivery plus 31 inverse gates.
General CI passes, but the CodeQL alert gate remains failed with 3,059 open
alerts, including twelve security findings. This supplies no hosted credit to
the uncommitted driver source.

## Remaining programme work

The driver is implemented-unverified because its mathematical requirements and
full programme/provider/platform/normalized-execution gates remain open.
`BLOCK-INDEFINITE-RK-SOLVE-RANGE` stays in the existing owner-decision packet.
No patch, numerical waiver, new robust route or wider provider adoption is
approved. The earlier native20/array-I/O milestone and explicitly selected
experimental RobustPpsvx remain separate.

After the owned feature commit and normal push, continue
`P05.required.hetrf_aa` and its dependency-ready consumers, then the live
P04–P09/P11 queue. Use the same current state and
`continuation-20260912-01/rk-inverse-profile-recovery-01/latest-handoff.json`.
No new single-family instruction is needed.
