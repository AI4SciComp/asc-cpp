# Reference machine constants

The S/D LAMCH slice adds two `asc::Lamch` overloads and the ten-member
`LapackMachineParameter` selector to the existing machine header. A separate
translation unit executes LAMCH; the existing LABAD compatibility operation
retains its implementation and behavior.

## Exact source and ABI

Both INSTALL/slamch.f and INSTALL/dlamch.f, plus INSTALL/lsame.f, were read
at pinned commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. The ten selectors
are E/S/B/P/N/R/M/U/L/O. LSAME makes them case-insensitive. An unknown native
selector returns zero; the checked API exposes only the ten documented modes
and rejects other enum values locally.

The source always assumes rounding: R=1, E equals half C++ epsilon on the
admitted binary profiles, and P=E*radix. It does not inspect the live rounding
mode. S is the safe minimum; U is the normal underflow threshold. The remaining
parameters are radix, significand digits, minimum/maximum exponent and maximum
finite value. Safe-minimum arithmetic may raise ordinary flags, but no control
settings change. There are no arrays, loops, workspace, native INFO or mutable
SAVE state. Co-located LAMC3 source emissions do not add LAMC3 routes.

Actual GNU11 emissions return float/double and take a character pointer plus
a size_t hidden length in both LP64 and global ILP64. Configured installed
LAPACK_slamch_base/LAPACK_dlamch_base declarations match after removing only
input-pointee const. The configured single return is float, not a promoted
double. Both native archives define both symbols. Eighty prerequisite cases
per ABI pass all ten uppercase/lowercase selectors, repeated calls and guarded
selector preservation, with exact independently specified binary constants.
The 18-worktree reuse search found 16 comments referring to constants derived
from LAMCH and no existing public adapter or named commit. Existing helper
constants remain unchanged. Sources, reuse, actual emissions, headers, archives
and executable results are bound in external
`completion-execution-01/lamch-prerequisite-review-01.json`.

## Checked execution and limits

The result is one live host or admitted pinned-host scalar with length and
increment one. Provider/report metadata are disjoint from each other and the
output. Metadata aliases reject before report reset or numeric access. Other
structural errors reset the report and preserve the old output. All 246 invalid
values of the fixed-byte enum reject locally. A valid execution calls native
LAMCH once with a mutable private selector and the exact hidden length one.
The old output is never read; there is no query, plan, workspace, allocation,
transfer, synchronization or fallback.

A changed private selector, nonfinite result, nonnegative minimum exponent, or
nonpositive value for another selector returns kProvider with partial/unusable
reporting and no publication. Otherwise the scalar publishes successfully with
complete validity and absent INFO/argument/index/factor fields. Sign/finiteness
checks do not certify arbitrary finite native values. The wrong-finite fixture
explicitly retains that limitation.

## Maintained checks

Eleven LAMCH processes separate S/D exact constants, native fidelity,
concurrency, ordinary-return fixtures and validation, plus the actual ABI probe.
All eleven LABAD processes remain selected with them. Thirty selector/old-output
fixtures per scalar use finite, negative-zero and quiet-NaN initial outputs,
first/reused calls and serial allocation checks. Four workers retain private
outputs/reports. Seven return fixtures run for each of ten selectors and each
scalar: unchanged result, zero, wrong sign, NaN, infinity, changed private
selector and a wrong finite value. Each starts with a guarded actual native
call; no invalid memory access is used.

Live metadata aliases require zero entry and exact preserved bytes. Explicit
shape/increment and device/managed-placement checks, every invalid enum value
and valid controls remain included. Both public consumers use only installed
ASC headers and ASC::dense_lapack: the unchanged LABAD consumer and new LAMCH
consumer must pass against each relocated package.

All eight ordinary Release/Debug static/shared profiles pass 176/176 combined
LABAD/LAMCH processes, with no failures or skips. The six implementation/test
translation units pass strict analysis; the initial ABI-probe diagnostic was
one unused include, moved into the conditional emitted-signature section.
Both maintained probes on the final source pass all 80 native selector cases.
Original strict diagnostics and the earlier native probe remain retained.

Each LAMCH scalar has 30 selector/old-output fixtures, 70 typed return cases,
246 invalid-enum rejections and 74/37 live metadata aliases for S/D. The old
LABAD validation remains unchanged. All four relocated packages pass both
public consumers, for eight processes and 860 fixtures per package across the
two operations. The two installed translation units receive strict analysis
against those exact public package headers.

Source and four installed surfaces each contain 171 public headers. Both ABI
export comparisons add two Lamch functions and remove none. The actual CI
selector retains all 1,972 previous cases and adds the eleven LAMCH cases, for
1,983 selections, and its required-case guard passes. Four standalone header
checks, ten package-manifest tests and three architecture checks are included
in the final integration audit. Doxygen covers 171 headers, 2,842 public members
and 33 explicit programme pages without warnings.

Only two callable-unverified routes and twenty reviewed selector modes are
registered after final audit. No full Reference verification or Native20 change
is implied. Every earlier numerical failure and deferred memory-fault
observation, ASLR-changing instrumentation, remote/security action and broader
provider/platform/provenance admission remains open.
