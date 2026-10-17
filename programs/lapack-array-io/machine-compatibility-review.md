# Machine-threshold compatibility

The S/D LABAD slice exposes two checked `asc::Labad` overloads through the Dense
LAPACK facet. This is a required compatibility operation in the inventory.
It does not estimate machine constants or change the numerical environment.

## Exact pinned operation

Both definitions at LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` were read in full. Their executable
bodies return immediately. The former LOG10/SQRT correction for non-IEEE
machines is entirely commented out. There are two mutable real scalar pointers,
no arrays, loops, INTEGER/CHARACTER/INFO arguments, hidden arguments, mutable
SAVE state, allocation or floating-point arithmetic. SMALL and LARGE retain
their exact representations. No ordering or finiteness check is appropriate
for the pinned identity operation.

Both actual provider archives define the two symbols, but their installed
lapack.h files omit the declarations. Private declarations match exact GNU11
LP64 and global-ILP64 emissions, with no signature normalization needed. A
read-only search of 18 registered worktrees found no existing LABAD adapter
or named commit. Native prerequisites pass 800 guarded pairs in each ABI,
including normal thresholds, subnormals, signed zeros, negative values,
infinities and signed quiet NaNs. Exact sources, archive/header digests,
emissions, binaries and command records are retained in external
`completion-execution-01/labad-prerequisite-review-01.json`.

## Checked contract

The two operands each have length and increment one and host or admitted
pinned-host storage. Their reachable storage must be disjoint from each other
and from the provider/report. Metadata overlap rejects before any report reset
or numeric read. Other structural errors reset the report and preserve both
operands. Valid calls copy representations into two private scalars and enter
the exact native routine once. A bytewise native change is a provider defect
with partial/unusable reporting. Caller bytes remain unchanged on every return.
The success report records complete validity and absent native INFO, argument,
diagnostic index and factor family. No query, plan, workspace, allocation,
transfer, synchronization, fallback or threshold transformation is introduced.

## Maintained validation

The source tests separate exact identity from native fidelity, first/repeated
calls, serial allocation checks, four-worker concurrency, actual ABI probes,
ordinary-return fixtures and structural validation. The public installed
consumer uses only public ASC headers and ASC::dense_lapack, retaining the same
400 pairs per scalar and rejected operand aliasing.

Five typed native-return fixtures cover unchanged behavior, one-bit changes
to each or both private scalars, and exchanged scalar representations. Each
executes a guarded actual native call first. Every fixture runs with distinct
finite values, opposite signed zeros and opposite signed quiet NaNs. Bit changes
test even NaN payload differences; all unexpected writes stay inside the live
private scalar objects. No deliberate invalid memory access or native crash
probe is used. Live metadata aliases require zero native entry and exact byte
preservation. Shapes, increments, device/managed placement and pairwise operand
overlap retain explicit rejection checks.

Eight ordinary Release/Debug static/shared profiles pass 88/88 tests, with
zero failures or skips. Native emitted-signature probes pass 800 cases per ABI.
Each scalar retains 400 identity/fidelity pairs, first and reused calls, and
15 typed return-fixture cases. Metadata validation covers 148 single-precision
and 74 double-precision aliases per profile. All six implementation/test
translation units pass strict analysis. The first strict attempt flagged a
floating-object memcmp and two include issues; byte-array representation
comparison and exact includes resolve those diagnostics without changing the
identity contract. Original diagnostics and the successful rechecks are retained.

All four relocated packages pass their public consumer. Installed strict
analysis additionally required splitting three adjacent declarations; fresh
consumer builds against those unchanged production packages pass all 800
fixtures and alias checks again. Original consumer results and source copies
remain immutable. The corrected installed consumer receives its own strict
check; this change does not require rebuilding unchanged production libraries.

Four standalone header checks, ten package-manifest tests and three architecture
checks pass. Source and four installed surfaces each contain 171 public headers;
both ABI comparisons add two Labad exports and remove none. The actual CI
selector retains all 1,959 previous cases and adds eleven operation tests plus
two header checks, for 1,972 selected tests; its required-case guard passes.
Doxygen covers 171/171 headers, 2,839 public members and 32 explicit programme
pages without warnings. These results support only two callable-unverified
contracts/modes, with no Reference verification or change to Native20.

All earlier failures, including LARTGS finite-range and complex LARTG
extreme-result prerequisites, remain open. Deferred memory-fault observations,
ASLR-changing instrumentation, remote/security operations and broader
platform/provenance admission remain recorded separately.
