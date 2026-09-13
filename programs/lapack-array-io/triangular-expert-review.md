# Triangular condition and error-bound candidate review

The active dependency-satisfied P05 slice maps actual S/D/C/Z TRCON and TRRFS
from pinned Reference-LAPACK3.12.1. These eight required rows remain unregistered
and not_started in the release mapping; no capability denominator changes.
Source admission is external `p05-triangular-expert-source-01/inventory.json`.
This root self-review is incomplete and gives no independent-agent approval.

## Preserved implementation and observed executions

External `p05-triangular-expert-candidate-03/source` contains ten owned files:
two typed public headers, one adapter, one source-count helper, a public-only
consumer, count and pinned-signature tests, and three fault-test files.
`owned.json` binds their exact hashes. Production preserves selected-triangle
and ignored unit-diagonal semantics. Immutable A/B/X and independently laid
inputs pack only into caller-owned workspace. TRRFS computes FERR/BERR and
never refines or writes X. Local inactive operations have absent native INFO.
Each of eight foreign calls starts INFO at the exact native minimum. No
positive INFO has a singularity meaning in these routines. Negative estimates
are provider defects; nonfinite estimates remain numerical accuracy warnings.
No report certifies mathematical conditioning or provider-wide allocation.

Candidate01's two builds failed range-loop-copy warnings; candidate02 fixes
that warning. Its six1/1 GNU Release/Debug and ASC-only ASan/UBSan lanes pass
6656 workflows and104448 checks each, but strict finds two unused includes.
Candidate03 removes those includes and adds fault/count/signature tests. All
six4/4 lanes pass: retained6656/104448 public checks,6272 fault profiles with
52416 assertions,17842 pure source-count checks and8 full typed signatures.
There are zero CTest skips. Both ABIs are the preserved actual LP64/true ILP64
static providers. Each lane freshly compiles this adapter and current test
objects, rehashing prior60-TU ASC production libraries/dependencies. Foreign
archives remain unsanitized; no installed or full-project execution is claimed.
Fault tests distinguish real cold/warm calls from test-only native interception,
observe linked-static C allocator calls, and enforce exact C++ allocation
counts in regular GNU modes. Shared runtime allocations are outside that probe.

Candidate03 production passes strict LP64. Its public consumer needs explicit
Status/norm include ownership and extraction of the long Errors verifier;
remaining strict tests are being checked individually. Original failed strict
records stay immutable. No failing assertion or rule was disabled.

## Source and acceptance obligations still open

Both installed archives contain all8 entry symbols. A conservative83-object
closure per ABI has no writable static symbols; its external imports are cabs,
cabsf,memcpy,memset after recording the preflight-excluded XERBLA diagnostic
edge. These object facts alone do not prove the full source/control/allocation
contract. Sixteen actual GNU compiler-emitted source signatures are captured;
the final comparison to installed typed declarations remains required.

LACN2 uses3*n in both real and complex variants. Real workspace is3*n plus
n private native integers; complex uses2*n plus n underlying-real entries.
Source bounds also cover NRHS+1 and native leading dimensions; inactive calls
use ASC loops and need no foreign narrowing. The full transitive LATRS/RSCL
control and finite-extreme review, precision-pair deltas, stronger ignored-read
observation, all operand/metadata alias modes, allocator closure, public header
isolation, installed consumer and atomic root registrations remain pending.

The prior286 Reference partial rows,1827 required not_started rows and20 native
implemented_unverified rows remain unchanged. All required46 mathematical
failures,130 missing required XBLAS routines and wider P00-P11 acceptance gates
remain visible and open. These scoped successes close none of those gates.

## Completed candidate06 checkpoint

Candidate06 is the current preserved implementation at
`p05-triangular-expert-candidate-06/source`. Its audit SHA256 is
`566fe87610e796155c698ea7cd6705a7e2c72f1f551fa5e0d26bae832451147a`; completion-record/record.json passes its immutable
rehash. The audit binds148 original records plus its final seal,54 fresh
adapter/test objects,30 binaries and their link commands, all actual compiler
inputs, prior audited production libraries,166 foreign objects and all source
identities. The registered root mapping is still unchanged at286 partial rows.

Candidate04 corrected only test include ownership/bool formatting and extracted
long verifier functions with unchanged assertions; all12 strict checks pass.
Candidate05 added8 ordinary mathematical tests. Candidate06 splits five
multi-declaration statements without changing any fixture/assertion; both
current strict mathematical checks pass, prior10 owned files remain identical.
Current six lanes each execute12 tests:4passes and8required mathematics
failures, zero skips. All retained public/fault/count/signature counts above
are unchanged. These72 executions total24passes and48failures; every CTest
exit is8 and remains8 in normalized evidence. No mathematical pass is inferred.

For A=B=minNormal/8,X=1, each TRCON returns RCOND0 although its exact scalar
condition is1. TRRFS yields infinite FERR for this tiny equation and for
A=B=maxFinite,X=1; the analytic scalar weighted inverse bound is finite.
All direct raw controls reproduce the wrapper values with INFO0. The BERR
expectation includes the source's safe-denominator term; its conservative
nonzero tiny value is not mislabeled as an adapter error. Source proof and
16 actual emitted-type comparisons are sealed under
`p05-triangular-expert-source-01/source-review.md` and emitted-compare-* records.

Next is root integration of the exact11 new files with v16 metadata, eight
partial rows, new public/header/source-policy registrations and the public
installed consumer. Fresh coherent-root integration/installed tests are required;
full alias/read-observation/platform/concurrency/normalized gates remain open.
