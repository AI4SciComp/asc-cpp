# Last nonzero matrix row and column extents

The eight S/D/C/Z ILALC and ILALR routes expose `asc::Ilalc` and `asc::Ilalr`
with metadata-only workspace queries. The result is the one-based last nonzero
column or row, equivalently the leading extent containing every nonzero entry.
All-zero and empty matrices return zero. Both padded matrix layouts are
supported without changing which logical axis is scanned.

## Exact pinned operation

All eight source definitions at LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` were read in full. Each returns an
actual ABI-width INTEGER and receives mutable pointers to M, N, A and LDA.
There are no CHARACTER or hidden arguments, INFO, allocation, mutable SAVE
state or external numerical dependencies. The reviewed source only reads A.
Both installed headers omit all eight declarations; actual GNU11 LP64 and
global-ILP64 emissions match the private declarations exactly. Both pinned
archives define all eight symbols. A read-only search across 18 registered
worktrees found no adapter or named commit to reuse.

The column scan returns immediately for N=0. Otherwise it probes the last
column's endpoints before scanning columns in reverse order. The row scan
returns immediately for M=0; otherwise it probes the last row's endpoints
before scanning upward in each column. Its `MAX(I,1)` subscript remains in
bounds when both operands of Fortran logical AND are evaluated. Native column
scanning with M=0,N>0 and row scanning with M>0,N=0 is not admitted: the checked
API handles every empty orientation locally. Native prerequisite tests call
only source-proven empty orientations and bounded positive shapes.

Nonempty M and N must be strictly below the selected INTEGER maximum to reserve
loop terminal values; LDA must fit that INTEGER. Existing checked count
arithmetic also bounds packing products, and matrix descriptors bound actual
reachable byte spans. Extreme count tests use pure arithmetic without creating
fictitious arrays. Exact comparison with zero treats either signed zero as
zero, infinities and NaNs as nonzero, and a complex entry as zero only when
both components are zero. Padding is excluded. No tolerance, conjugation,
finite-value filter, transpose counterpart or replacement algorithm is used.

Native prerequisites pass 32,472 cases per ABI, with zero result or storage
guard failures. Positive shapes combine M,N in {1,2,3,5}, two padding sizes,
each single-entry support, all-zero, dense and checkerboard patterns. Markers
cover signed zeros, signs, subnormals, maximum finite values, infinity, NaN,
and pure-imaginary complex values. An independent complete C++ scan supplies
the exact index oracle. Emissions, source review, provider locks and raw command
records are retained in `completion-execution-01/matrix-support-prerequisite-review-01.json`.

## Checked contract

Input matrix storage is read-only host or admitted pinned-host memory. The
result is a live host `index_t`; its old value is not read. All operand spans,
provider, plan, workspace descriptor, report and supplied workspace regions
must satisfy the established disjointness rules. Metadata overlap rejects
before any report reset or numeric access. Other preflight failures reset the
report while preserving input, result and scratch. Queries inspect metadata
only and do not enter the provider.

Column-major execution requires no packing. Row-major execution requires
M*N live scalar entries in the explicit layout-conversion region. It packs
logical values into column-major order and calls the same axis routine once.
All empty shapes publish zero locally, with no packing or native call. Plans
bind routine, scalar, dimensions, leading dimension, layout and provider.
The returned INTEGER must lie between zero and the relevant axis dimension,
and all private native integer arguments must remain unchanged. A violation
returns provider/partial/unusable and withholds the result. A valid result
publishes with success/complete. Runtime range checks cannot establish the
mathematical correctness of an arbitrary in-range result.

There is no native INFO, argument index, diagnostic index or factor family.
Calls allocate, transfer and synchronize nothing. Floating-point control modes
remain unchanged; comparisons may raise ordinary flags. Concurrent calls share
only immutable provider state and keep mutable workspace, output and report
private. Pinned Fortran/BLAS/runtime internals remain uninstrumented.

## Maintained checks

The maintained grid covers both layouts, the positive prerequisite fixtures
and all empty orientations, with independent exact mathematical and native
fidelity tests kept separate. Every input element and padding byte is preserved,
output and scratch guards are checked, and plans, workspace descriptors and
provider metadata remain unchanged. First and reused calls include allocation
audits. Concurrency uses four workers with private storage.

Seven normal-return modes exercise correct, negative, oversized and incorrect
in-range results, and changed private M, N or LDA. Each observation calls the
real routine with valid bounded arguments first and checks its result and
integer guards. No caller matrix is deliberately corrupted. Metadata tests use
live objects and reject numeric descriptors or output references before load;
no protected pages, signals or foreign invalid accesses are used. Placement,
workspace capacity/alignment/aliasing, stale plans, local empty completion and
pure integer limits have dedicated checks. The installed example uses public
headers and `ASC::dense_lapack` only.

Eight ordinary Release/Debug static/shared profiles pass 328/328 tests with
zero failures or skips. Both emitted-signature native probes pass 32,472 cases.
Each of four relocated packages passes 65,056 public fixtures, including both
layouts, empty shapes, repeated outputs and stale-plan rejection. All six
implementation/test translation units and the installed consumer pass strict
analysis. The initial test-view argument-order error and strict include/style
diagnostics were corrected before these final runs; original attempts remain
recorded without reducing the test domain or weakening checks.

Each route rejects 464/320/460/316 metadata aliases for S/D/C/Z respectively.
The actual CI selector retains all 1,996 previous cases and selects 2,039 with
its required-case guard passing: 41 operation processes and two new header
checks. All five source/installed public surfaces contain 173 headers. Both
ABI export comparisons add exactly sixteen query/execute overloads and remove
none. Documentation and integration records are retained with the final audit.
These results support eight callable-unverified routes and sixteen reviewed
layout modes. They grant no Reference verification and do not change Native20,
previous failed executions, deliberate-fault deferrals, remote/security
operations or broader platform admission.
