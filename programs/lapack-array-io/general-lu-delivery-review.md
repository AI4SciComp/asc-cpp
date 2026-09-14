# General LU public delivery and contract review

This closes the maintained public-consumer gap for the existing S/D/C/Z
GETRF, GETRF2, GETF2, GETRS, GETRI and GESV adapters. It adds no numerical
implementation or foreign dependency. The 24 separate Reference rows now have
reviewed callable contracts (`implemented_unverified`). Normalized Reference
mode/class execution records and wider required platform admission remain
incomplete; no Reference row receives verified credit. The native20 contracts,
artifacts, test classes and execution records remain unchanged.

The review reuses the actual source/report/publication findings in
[LU layouts](lu-layout-review.md), [INFO](lu-info-review.md),
[expert INFO](lu-expert-info-review.md) and
[returned pivots](lu-output-pivot-review.md), including their later root imports.
The original delivery left the adapters and existing tests unchanged. The later
Reference metadata correction is recorded below. Four GESV query
comments now correctly describe the already supported row/column layouts;
public declarations, calling conventions and behavior are unchanged.

## Supported contract and finite modes

All factors use one-based sequential partial-pivot swaps: applying the swaps
in forward order to the original matrix produces L*U. A successful borrowed
factor view retains the provider identity; a singular or unusable result cannot
certify a factor. GETRS reuses immutable factors/pivots for N/T/C solves and
independent RHS layouts. Real C remains a distinct legal enum with T semantics.
The existing shared GETRF/GETRS native contracts and their two/twelve mode IDs
are retained exactly. GETRF2/GETF2 each add two reviewed layouts, GETRI two
layouts, and GESV four independent A/RHS layouts, per scalar: 96 Reference mode
IDs across these 24 routines. Minimum/preferred inverse workspace is a required
test class, not another catalogue routine.

Queries and execution retain explicit checked caller workspace and row packing.
GETRI makes its actual LWORK=-1 query with valid descriptors; execution does
not requery. Empty inverse query still calls the provider; empty execution is
local with no fabricated INFO. GESV with N>0, NRHS=0 still factors A. Exact
undersized workspace, stale plans, strides, placement, aliasing, allocation,
full-width INFO/pivot seeds and returned-pivot range checks remain in the
required tests. Packed outputs publish only on their documented return paths;
direct column-major foreign writes cannot be rolled back. Singularity publishes
documented partial LU and preserves an unsuccessful packed RHS. Native
nonfinite arithmetic does not certify finiteness or conditioning.

## Maintained installed and concurrent consumer

`examples/general_lu` uses only public ASC headers, `ASC::dense_lapack` and
`Threads::Threads`. Root package integration copies, builds and runs it after
installation and relocation. It requires the separately prepared pinned
provider matching the installed actual integer ABI, with no historical
evidence directory or private-header path in the maintained consumer.

A fixed dyadic 3-by-3 matrix has a zero first pivot, nontrivial subsequent
swaps and nonzero complex components. Independent known solutions produce two
RHS sets in verifier arithmetic. Both padded A/RHS layouts, all three factor
algorithms and all N/T/C solves check solutions, residuals, P_seq*A=L*U,
unchanged factors/pivots and guards. GETRI checks both inverse products at
preferred workspace; existing layout tests independently retain minimum and
preferred workspace. The ordinary tolerance is 128 scalar epsilons. Explicit
finiteness checks prevent NaN inverse/reconstruction results from passing an
unordered comparison. The small bounded values do not require a wider exponent
range than double, so the oracle does not assume x86 long-double range.

Four threads repeat the real-provider calls with separate contexts, plans,
workspace, buffers and reports. Reuse within a thread is immutable; no shared
workspace or globally injected callback state is used. A normal-return guard
rejects an early process exit from foreign code. Race-detection records cover
ASC and the consumer, not the uninstrumented pinned Fortran provider.

## Execution and identities

Raw records are in `master-continuation-20260910-01/`. `lu-consumer-{abi}-01`
passes the actual installed public test in both relocated ABI packages.
`lu-delivery-debug-{abi}-01` and `lu-delivery-release-lp64-01` each pass the
actual 27-test selection with no skips: public consumer237, normal-return249,
reference LU250–253, INFO254–257, expert INFO258–261, output pivots262–265,
layouts266–269, expert LU270–273 and counts298. Final-source consumer repeats
and remaining profiles are recorded below when executed. These IDs belong to
those configured listings, not to an assumed permanent test numbering.

`lu-consumer-style-01` retains four style findings; attempt02 passes strict
analysis. Initial passing run snapshots precede the explicit nonfinite oracle
checks and remain preserved. Final-source repeats must cover that change.
The selected source snapshots include both adapter bodies and affected helpers,
headers, tests and consumer. The provider commit/tree, LP64 and global ILP64
archive attestations and compiler settings are the unchanged prepared inputs
in [mixed-general review](mixed-general-review.md). No C++ typedef substitution
is used as proof of ILP64.

The compact `lapack-lu-delivery-native-evidence-extension.json` binds the prior
mapping and exact native execution record. Native route/contract/artifact
comparison permits reuse after the separate Reference status changes in eight
shared rows; it does not claim the later whole source tree was tested earlier.
Retained PT, PPSVX, GEEQUB and other mathematical failures remain independent
required gates. This delivery does not complete P04, P09 or the full programme.

Final `lu-delivery-{debug,release,sanitizer}-{abi}-01` passes27/27 in all six
prepared profiles. The first two Debug and LP64 Release runs precede the
nonfinite-oracle checks; their separate final consumer02 repetitions pass and
`lu-executed-source-comparison-01/comparison.json` binds unchanged adapter/test
inputs and the final consumer. Other profiles already contain the final source.
`lu-delivery-tsan-{abi}-01` passes the real concurrent public test237 in both
ABIs, with ASC-only instrumentation and the recorded setarch workaround.
`install-lu-delivery-{abi}-01` passes installation, relocation, public execution,
export inspection and provider-free runtime isolation. `lu-integration-{abi}-01`
passes all eight actual configured affected architecture/header/documentation/
package commands, including the newly maintained consumer. No skips occur.
`lu-consumer-style-03` and `lu-documentation-01` pass final source analysis and
strict documentation generation/audit. The final coverage/backlog/whitespace
checks are `lu-record-checks-02`; callable Reference24, verified Reference0,
partial Reference338, not-started1751, reviewed contracts48, required2113.

The same integration includes a concrete hosted formatting repair in the
mixed-positive test: parenthesized comparisons preserve the original predicate
while passing both Clang18 and19. The original test and mapping-extension
artifact are retained in `positive-hosted-style-amendment-01` and
`lu-record-amendment-01`. The affected two ordinary/concurrency tests229/231
pass in all six profiles and both TSan profiles after the repair:
`positive-debug-lp64-04`, and the other mode/ABI `positive-*-02` runs.
`positive-style-portability-01` passes full tracked Clang18 formatting and
strict analysis of the changed test; no numerical tolerance or case changes.

Fresh hosted checks on `c4e6d4710629538bddf4a074aa200fb974b0b99d`
(tree `bdb7a151400e104896e1bd85df46fb8bafae2a77`) complete: CI34519820006
and CodeQL34519820167 pass. Family34519820028 passes72/76 per actual ABI,
with exactly the four retained PT mathematical failures and zero skips. Raw
artifacts and JUnit audit are `lu-hosted-final-01`; no later source is credited.

The first maintained partial Reference execution records and separate source
expectations are now in the [execution identity review](lu-execution-evidence-review.md).
They preserve all previous executions and do not complete the remaining classes
or platform gates.


## Reference GETRF/GETRS metadata preflight correction

The current Reference execution paths check numeric and workspace overlap with
all four live metadata objects (provider, plan, workspace and report) before
resetting the report or constructing the expected plan. The objects must also
be mutually disjoint. Every supplied workspace region participates, including
unused regions and empty operations. Overlap returns `kInvalidArgument` with
all metadata and numeric storage unchanged. Other structural failures retain
the established report reset and leave numeric storage unchanged. Address-range
comparison uses subtraction so an unchecked workspace byte count cannot wrap
an endpoint sum. Public declarations, valid numerical paths, foreign integer
conversions, provider identity and numerical outcomes are unchanged.

This corrects a source-confirmed ordering defect: the earlier adapter reset the
report before checking any overlap with live metadata. GETRS could subsequently
read aliased pivot or diagonal values while constructing the expected plan.
No malformed alias fixture was executed against that earlier implementation.

The maintained `lu_metadata_test.cc` contains 1,032 bounded rejection cases per
scalar: 144 factor cases and 888 solve cases. They span both matrix layouts,
independent solve RHS layouts, N/T/C, all four metadata objects, every workspace
region and empty/nonempty matrices. Each case observes zero calls through the
existing eight typed GNU entry wrappers, checks allocation counts, and compares
all metadata objects and numeric/workspace arrays with their pre-call byte
snapshots, including padding. Fifty-six ordinary/stale-plan/empty controls per
scalar retain reset behavior, successful native entry, exact singleton results
and guards. Setup factorization uses valid bounded inputs.

The solve-factor factory validates raw pivots. Its pivot alias fixtures therefore
use real live signed-index subobjects equal to one: a plan requirement and the
report diagnostic index. They do not reinterpret unrelated object bytes as
pivot values. Factor-matrix and RHS descriptors are rejected without numerical
reads. These are ordinary return-value tests; they use no protected pages,
signals, ASLR changes or malformed foreign arguments. They do not replace the
separately deferred calibrated memory-observation or instrumentation gates.

The eight shared GETRF/GETRS inventory contracts, their 56 native mode IDs,
native implementation selections and accepted native executions remain frozen.
The correction applies to the separate Reference overloads documented in
`asc/dense/providers/lapack.h`. A reviewed mapping extension will bind the
Reference source/header correction and supplemental tests without attributing
new behavior or execution to the native route. No new callable or verified row
is added. The exact completed ordinary validation is recorded with the local
implementation checkpoint; previous failures and deferred admission remain open.


The final `lu-metadata-preflight-*-02` records in
`master-continuation-20260910-01/continuation-20260912-01/completion-execution-01/`
pass 168/168 across Release/Debug, static/shared and actual LP64/ILP64, with
zero failures or skips. Each 21-test selection includes the four new metadata
tests, existing reference LU/INFO/pivot/layout regressions and the public LU
example. Initial ABI runs pass20/20 each; profile01 passes168/168 before the
final test-helper extraction. Profile02 verifies all product, header, CMake and
workflow inputs are unchanged and only the metadata test source differs;
it reuses successful configuration and rebuilds/reruns the entire selection.

Strict02 passes the adapter and unchanged typed INFO shim. Strict04 passes the
final metadata test; installed-strict01 passes the public consumer. Earlier
strict attempts retain the function-size and redundant-expression findings;
helpers were extracted and combined compile-time assertions split without
removing cases or checks. Four installed01 packages pass the unchanged public
concurrent example after relocation, with public-only include paths and resolved
runtime libraries. Shared wrapper observations bind the actual production PIC
objects; the installed examples use production libraries. The pinned foreign
Fortran/BLAS/runtime remains uninstrumented.

The integration checks pass four header builds/tests, ten package manifests and
three architecture checks. All five 168-header public surfaces pass, and both
ABIs retain exactly the prior exports. The actual workflow guard selects1897
processes, adding exactly four metadata tests and removing none; this is selector
validation, not execution of the whole selected suite. Doxygen passes168/168
headers,2811 documented members and28/28 explicit programme pages with zero
warnings. The final audit and reviewed evidence extension bind these records.
No previous numerical failure or deferred admission gate is waived.
