# Rotations with a nonnegative result

The S/D LARTGP slice adds two checked `asc::Lartgp` overloads to the existing
rotation header and Dense LAPACK facet. The compiled scalar validator also
serves the original LARTG overloads; both distinct sign conventions retain
separate native calls, mathematical fixtures and report expectations. No
complex rotation, BLAS performance claim, fallback or provider change is added.

## Exact sources and interface

Both definitions at pinned LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` were read before implementation.
LARTGP computes `[CS SN; -SN CS] * [F; G] = [R; 0]` with R nonnegative,
signed CS/SN and squared coefficients summing approximately to one. G=0 returns
CS=sign(F), SN=0, R=abs(F), including CS=-1/R=+0 for negative-zero F. Otherwise
F=0 returns CS=0, SN=sign(G), R=abs(G). These differ from LARTG's nonnegative
cosine convention; they are not aliases for the same operation.

There are five real scalar pointers, no INTEGER/CHARACTER/INFO arguments and
no array indexing. Both installed provider archives define both symbols and
lapack.h declares them. Actual GNU11 LP64/global-ILP64 emissions match those
prototypes after normalizing input-pointee const only. Source machine constants
come from SLAMCH/DLAMCH on each entry. Commented SAVE/FIRST statements do not
create mutable shared cache state. The large-input scaling loop is capped at
20; the small-input loop has no explicit counter cap but scales positive finite
nonzero IEEE values upward by the finite power-of-two SAFMX2 until they exceed
the threshold. No native nonfinite input is used for this admission.

A read-only search of 18 registered worktrees found no prior LARTGP route.
The two exact providers each pass 3,874 finite prerequisite cases, with zero
mathematical or guard failures. These include both signed zeros, subnormals,
normal values, finite extremes, and adjacent values around both LARTGP scaling
thresholds. The original probe compile failure was misleading indentation;
a fresh attempt adds braces and formatting with unchanged tests and tolerances.
Exact source, reuse, emissions and command identities are bound in external
`lartgp-prerequisite-review-01.json` and its prerequisite records.

## Checked execution

Inputs are finite values copied before execution. C/S/R are three distinct live
host or admitted pinned-host scalars with length/increment one. Metadata aliasing
with the live provider/report rejects before report reset. Other structural
errors reset the report and preserve all numeric outputs. No old output value
is read. Nonfinite inputs reject locally without native entry. By-value inputs
may originate in output objects because argument values are already copied.
There is no query, plan, caller workspace, allocation, transfer or synchronization.
Floating-point control settings are unchanged; arithmetic may raise ordinary flags.

Private native input copies must remain bitwise unchanged. Each output starts
with a NaN sentinel. Require finite C/S with magnitude at most one and non-NaN,
nonnegative R. Any violated condition returns kProvider, partial/unusable and
no publication. All three outputs otherwise publish together; finite R succeeds,
positive infinite R reports kNumerical with accuracy warning and complete
publication validity. Native INFO, native argument, diagnostic index and factor
family remain absent. Zero input still enters native code. Invalid finite/zero
tuples can satisfy the structural range checks without being accurate; the
checked report does not independently certify arbitrary native numerical writes.
No clipping or alternative numerical algorithm is used.

## Validation and open gates

Eleven new maintained tests separate S/D mathematical, fidelity, concurrency,
return-fixture and validation processes plus the emitted-signature native probe.
The original eleven LARTG tests remain selected because their shared validator
is affected. New scalar grids have 1,936 input pairs plus a 3-4-5 example, first
and reused output calls, guarded neighbors and serial allocation checks. A wider
independent norm checks both original equations, coefficient unit length and
signs. Dimensioned error is bounded by 64 epsilons times norm plus four least
subnormals, without an additive unit floor. Native fidelity remains bitwise and
separate. Four independent workers keep private mutable state.

Nineteen typed return fixtures per scalar execute a guarded actual baseline
before changing return behavior: omitted fields, invalid signed coefficients,
NaN R, both infinities, negative finite R, changed private input copies and
wrong finite/zero tuples. The const-qualified installed input prototype still
receives mutable private copies in the checked adapter; the mutation fixtures
change only those copies. Validation preserves the existing live-metadata byte
snapshots, shape/increment, placement, pairwise overlap and nonfinite rejection
checks. The installed C++ consumers use public headers and ASC::dense_lapack only;
both the unchanged LARTG consumer and new LARTGP consumer are required against
each relocated package.

Eight Release/Debug, static/shared, actual LP64/global-ILP64 profiles pass
176/176 combined old/new tests, with zero failures or skips. Six production/test
translation units and both installed consumers pass strict analysis. Both final
emitted native probes pass all 3,874 guarded cases. S/D metadata rejection
covers 222/111 aliases respectively for each operation. All four relocated
packages pass both consumers: eight processes and 5,924 independent fixtures
per package across the two sign conventions. The unchanged LARTG public consumer
therefore executes against the new production library as well.

Four standalone header checks, ten package-manifest tests and three architecture
checks pass. Source and four installed surfaces each have 170 public headers;
both ABI export comparisons add two Lartgp symbols and remove none. Doxygen
covers 170/170 headers, 2,837 public members and 31/31 explicit programme pages
with zero warnings. The actual CI selector and required-test guard retain all
1,948 previous selections and add eleven, for 1,959. These are scoped local
executions and selection checks, not full-suite or hosted admission. Only two
new LARTGP contracts/modes receive callable-unverified registration; the original
LARTG contracts remain unchanged while their shared artifacts are explicitly
rebound to the current implementation.
No full Reference verification or wider admission is claimed. C/Z LARTG's failed
extreme-R prerequisite and every earlier numerical failure remain open.
Deliberate-fault memory observations, ASLR-changing sanitizer/TSan runs,
remote/security operations and broader platform/provenance/owner gates remain
required and deferred under the current user instruction.
