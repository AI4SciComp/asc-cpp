# Checked real plane rotations

This P09 slice adds checked SLARTG and DLARTG through two `asc::Lartg`
overloads in the optional Dense LAPACK facet. C/Z LARTG remain unimplemented
with a recorded failed extreme-range prerequisite. No numerical fallback,
provider modification, new module or complex route is inferred from real tests.

## Source and API contract

All four LARTG definitions and LA_CONSTANTS in pinned LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` were read. The five scalar arguments
are F, G, C, S and R. The real rotation is `[C S; -S C] * [F; G] = [R; 0]`,
with C nonnegative and squared coefficients summing approximately to one.
G=0 returns C=1, S=0 and the original F, including its signed zero. Otherwise
F=0 returns C=0, S=sign(G), R=abs(G); nonzero F determines the sign of R.
Scaled and unscaled source branches use compile-time precision constants.
There are no native integer, character, hidden-length, INFO, workspace, array
index or loop arguments. Both actual provider archives define all four symbols,
but their installed lapack.h declares none. Private S/D declarations match
actual GNU11 emissions from each ABI without signature normalization. Emitting
the Fortran90 signatures used an out-of-tree compilation of the exact
LA_CONSTANTS module; installed provider archives were not rebuilt or changed.

Inputs are finite real values copied before execution. Three distinct live
host or admitted pinned-host destinations have checked length/increment one.
No query, workspace, allocation, transfer or synchronization is needed. Provider
and report metadata must be disjoint from each other and every output. Metadata
alias rejection preserves every byte, including the report. Other structural
failures reset the report and preserve all numeric output. The old destination
values are not read. By-value inputs may originate in output objects because
they are copied before the operation begins. Independent calls share immutable
provider state and keep all mutable state separate.

Nonfinite F/G reject locally with kNumerical, no entry and unchanged outputs.
Even all-zero finite inputs enter the native routine. Local NaN sentinels guard
all three returned values. Changed input copies, a nonfinite/out-of-range C or
S, or NaN R yield kProvider with partial/unusable reporting and no publication.
Finite C in [0,1], S in [-1,1] and finite R publish together successfully;
infinite R publishes with kNumerical, accuracy warning and complete publication
validity. INFO, native argument, diagnostic index and factor family stay absent.
Neither finite input nor successful structural checks certify correct rounding
or accuracy of arbitrary finite native writes. Finite values and zero tuples
that satisfy range checks can still be mathematically wrong; explicit fixtures
retain that distinction. Floating-point control settings remain unchanged and
ordinary arithmetic flags may be raised.

## Bounded prerequisites and deferred complex failures

A read-only search of the 18 registered worktrees found no prior checked LARTG
route; no sibling work was changed. Each actual ABI first executed 16,456 guarded
finite native cases across S/D/C/Z. An expanded diagnostic retained the entire
original grid and added neighboring finite endpoints, for 22,984 cases per ABI.
Both attempts retain 496 failed complex dimensional-result comparisons per ABI
and zero guard or real-scalar failures. The failures include infinite components
near maximum finite magnitude. For extremely small norm excess above maximum,
long double cannot resolve that excess: these are failed finite rounded-output
accuracy gates, not a claim that the exact mathematical norm is representable.
The original tolerances and failed cases are preserved. C/Z API work is deferred
under `LARTG-COMPLEX-EXTREME-R`; no provider change or waiver is authorized.
Raw source, prototype emissions, commands, binaries, dependencies and results are
bound in external `lartg-prerequisite-disposition-01.json`.

## Maintained checks and admission

Eleven maintained tests separate S/D mathematics, native fidelity, concurrency,
return fixtures and validation, plus a guarded native ABI process. Each numeric
scalar grid has 1,024 signed input pairs and a 3-4-5 example, with first and
reused destination calls. Values include both signed zeros, least subnormal,
minimum normal, subnormals, neighbors of each scaling threshold, ordinary values
and finite endpoints. A wider independent norm checks the coefficients, unit
length, both original rotation equations, signs and dimensional R accuracy.
The dimensional tolerance is 64 epsilons times the input norm plus four least
subnormals; there is no additive unit floor. Native fidelity is separately
bitwise. Four independent workers exercise signed/scaled fixtures; allocation
observation runs serially on first and reused calls.

Eighteen bounded typed return fixtures per scalar first execute a guarded real
native baseline, then exercise omitted fields, invalid coefficients, NaN R,
infinite R, input-copy mutation and numerically wrong finite/zero outputs.
Metadata tests form descriptors over existing live metadata and reject before
numeric reads. They also check wrong shapes/increments, inaccessible placement,
all pairs of output overlap, NaN/infinite inputs and valid by-value alias use.
The installed C++ consumer uses public headers and `ASC::dense_lapack` only,
with the same 2,050 independent real fixtures, repeated outputs and local
nonfinite-input rejection. It does not import private declarations or typedefs.

All eight Release/Debug, static/shared, LP64/global-ILP64 profiles pass 88/88
selected tests with zero failures or skips. S/D metadata rejection covers
222/111 live-object aliases respectively. Six maintained translation units and
the installed consumer pass strict analysis. All four relocated consumers pass
2,050 fixtures each. Both final emitted-signature ABI probes pass 2,050 guarded
cases. Initial test-only macro/type-name compile errors and strict isolated-
declaration/include findings are retained and corrected without changing any
numerical assertion or disabling a check.

Four header checks, ten package-manifest tests and three architecture checks
pass. All five public surfaces contain 170 headers; each ABI adds exactly two
Lartg exports and removes none. Doxygen covers 170/170 headers, 2,835 public
members and 30/30 explicit programme pages with zero warnings. The actual CI
selector and required-test guard cover 1,948 tests: all 1,935 earlier selections
remain, with eleven rotation tests and two header checks added. This is local
selection evidence, not full-suite or hosted execution. Two real contracts/modes
are eligible for callable-unverified registration; no Reference verification or
complex implementation is claimed. The generated backlog carries scalar-specific
blockers separately from the unchanged missing-provider-symbol list.

Exact records use the external `rotation-*` evidence artifacts. Deliberate-fault memory observations,
sanitisers/TSan requiring address-randomization changes, remote/security and
broader platform admission remain required and deferred under the user constraint.
All earlier failures, Native20 and provenance/owner gates remain open or retain
their exact previously accepted scope. The full programme is incomplete.
