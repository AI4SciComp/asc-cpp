# Checked scalar Euclidean norms

This independent P09 leaf adds the four required SLAPY2, DLAPY2, SLAPY3 and
DLAPY3 routes through four real Lapy2/Lapy3 overloads. The optional Dense LAPACK
facet owns the new scalar-norm header. There is no new module, provider,
algorithm fallback or imported numerical implementation.

## Source and checked contract

All four exact source definitions in LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` were read. They compute two- or
three-component Euclidean norms using the pinned scaled arithmetic. Dependencies
are the existing SLAMCH/DLAMCH and, for LAPY2, SISNAN/DISNAN. They have no integer
parameters, workspace, pivot history or INFO argument. Actual GNU11 LP64 and
global ILP64 external prototype emissions verify every scalar pointer and the
REAL/DOUBLE PRECISION return type against the selected C declarations. Provider
identity and transitive helper ABI still matter even without integer arguments.

Inputs are ordinary real values copied before entry. The caller supplies one
live host or admitted pinned-host output scalar through a checked vector view,
with length and increment both one, plus a mandatory report. No query or
workspace is needed. The provider, report and reachable output span must be
disjoint. Metadata aliasing rejects before resetting the report; other structural
errors reset it and preserve the numerical destination. The old destination
value is never read. All-zero inputs still call the selected provider.

A finite nonnegative return is published successfully, including negative zero.
A negative return, including negative infinity, is a provider defect and leaves
caller output unchanged. A NaN or positive infinity is published with numerical
accuracy-warning status and complete publication validity. INFO and native
argument fields remain absent on every path because these functions have no
such channel. Reports identify the actual routine and provider; they do not
certify correct rounding or independently prove the accuracy of an arbitrary
injected finite provider result. NaN-input precedence, including NaN combined
with infinity, follows the pinned routine. Floating-point control settings are
unchanged; arithmetic may raise ordinary status flags. No clipping, rescaling
outside the provider, allocation, transfer or synchronization occurs.

## Numerical prerequisites and retained oracle correction

Each actual ABI executes 16,800 native cases: both real precisions, the complete
20-value Cartesian grid for two and three components, signed zero, smallest
subnormal, minimum normal, ordinary signs, large finite endpoints, infinity and
NaN. Every input and adjacent guard is compared bitwise after return. Actual
emitted signatures include the function return type, not just pointer sizes.

The initial prerequisite had 2,054 failed comparisons per ABI and no finite
error diagnostics. The bound local libstdc++11 three-argument hypot implementation
normalizes infinity by infinity and produces NaN. Its source was read and hashed.
The corrected oracle explicitly classifies NaN and infinity inputs, then uses
hypot for finite inputs. During corrected execution it counts exactly 2,054 old
library mismatches per ABI; all 16,800 original cases pass. The input grid and
finite error tolerance remain unchanged. The original failed attempts, source,
correction and library identity are retained externally. No provider defect is
claimed from those invalid comparator results.

## Maintained verification and pending admission

Seventeen maintained tests cover four mathematical grids, four separately
reported native-fidelity grids, four concurrent tests, four report-fault and
structural-validation tests, and one raw ABI probe. Each two-component grid has
400 cases and each three-component grid 8,000, plus an independently known
3-4-5 or 3-4-12-13 Pythagorean check. The finite tolerance is four epsilons times
the expected norm or half the smallest subnormal, whichever is larger. This
checks subnormal results without imposing a wider long-double exponent range.
Serial execution is allocation-audited. Four workers use private mutable state
and an immutable shared provider, outside the serial allocation auditor.

Typed linker faults first check the native 5/13 baseline, then exercise zero,
negative zero, negative values, smallest subnormal, maximum finite, both
infinities and NaN. Tests retain the exact returned value or check complete
output preservation as required. They also reject wrong output lengths and
increments, device/managed placement and output/report overlap before foreign
entry. These are bounded ordinary validation tests, with no deliberate crash,
process termination or memory-protection experiment.

Final Release/Debug, static/shared, LP64/global-ILP64 execution passes all
136 selected processes across eight actual configurations, with zero failures
and skips. Five production/test translation units and the unchanged public
installed consumer pass strict analysis after bounded naming/macro/include
corrections. Four relocated public consumers each pass 192 signed/scaled
Pythagorean scenarios, infinity reports and invalid-output checks. Both final
maintained native probes pass all 16,800 cases against actual emitted signatures.

Four standalone header checks, ten package-manifest tests and three architecture
checks pass. Source plus four installed surfaces contain 164 public headers;
both ABI symbol comparisons add four exports and remove none. The actual CI
selector includes 1,817 tests, retaining every prior entry and adding seventeen
norm processes plus two header checks; its maintained guard passes. This is
local selection evidence, not a full-suite or hosted execution claim. Four
reviewed contracts/modes are registered as callable-unverified; no normalized
Reference verification is granted.

Deliberate-fault observation, sanitizer/TSan execution requiring address
randomization changes, remote/security operations and wider platform admission
remain required and deferred under the latest user instruction. Existing tests,
failures, Native20 and provenance/owner gates are preserved. Full programme
acceptance remains open. Raw records are the `scalar-norm-*` artifacts under
`master-continuation-20260910-01/continuation-20260912-01/completion-execution-01/`
in the established external evidence root.
