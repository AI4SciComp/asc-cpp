# Classic indefinite contract and range continuation

Status: **implemented, numerical acceptance blocked; FULL_PROGRAM_INCOMPLETE**.
The existing S/D/C/Z SYTRF/SYTF2/SYTRS and C/Z HETRF/HETF2/HETRS APIs,
backend and installed example are preserved from `75b189d`. The preceding
[INFO correction](indefinite-info-review.md) is complete within its recorded
scope and is not repeated as new work.

## Reviewed contracts

The eighteen exact pinned source rows are normalized to 168 mode records.
Six TRF entries cover both triangles, both layouts and four workspace choices:
minimum, preferred, the minimum useful reduced block size, and the size just
below that threshold. Six TF2 entries cover both triangles and layouts. Six
TRS entries cover both triangles and independent factor/RHS layouts. The
original tests already exercise reduced NB thresholds eight for SYTRF and
two for HETRF at order 67; those assertions are preserved.

The contracts retain full scalar types, classic signed paired block pivots,
source INTEGER arithmetic, caller workspace, mutation and aliasing rules.
Complex symmetric factors use transpose and complex diagonals; Hermitian
original input ignores imaginary diagonals through selected-component packing.
Borrowed raw factors retain their full block coefficients. The correct upper
factor equation is U*D*U^T or U*D*U^H, as established in the preserved
[source review](indefinite-review.md). Empty operations do not call the provider.

## Required numerical failures

New tests use scales twice the smallest normal, minimum-normal/1024, twice
the smallest positive subnormal, and maximum finite. Factorization uses
scale*I at orders 2 and 67, both triangles/layouts and TRF/TF2. Its independent
closed-form oracle requires D=scale*I, identity U/L and pivots, and explicitly
finite coefficients. Solves use order one, A=B=scale, two RHS columns and both
RHS layouts, reusing each actual factor route. They require finite X, known
solution one and a separate residual against the original scalar matrix.

The two tiny scales fail for every scalar/symmetry class. TF2 and the
LASYF/LAHEF blocked panels compute a reciprocal before scaling: exact zero
multipliers become nonfinite. At order 67, blocked TRF propagation also yields
spurious INFO=4 for upper or 64 for lower, although the input is nonsingular
with condition one. Unblocked results retain INFO=0. TRS likewise forms a
reciprocal before scaling the RHS and returns Inf or Inf/NaN with INFO=0
instead of exact X=1. Both control scales pass the mathematical requirements.

Independent typed native calls agree for all cases. Hermitian direct inputs
use the same mathematical matrix after the documented imaginary-diagonal
normalization. Comparison preserves finite values, infinity signs and NaN
classification; it makes no NaN payload claim. Provider agreement is recorded
separately from mathematical acceptance. No assertion or tolerance is waived,
and no hidden scaling, provider replacement or alternate algorithm is added.

## Evidence and remaining work

All raw records remain under
`master-continuation-20260910-01/continuation-20260912-01` in the existing
external evidence root. `indefinite-contract-01` binds all eighteen upstream
rows and the exact reciprocal/SCAL source expressions. The new range matrix
covers actual LP64/ILP64 static/shared Debug, Release and ASC ASan/UBSan.
Each profile retains twelve mathematical failures and 1,584 failed assertions
among 24 processes; all twelve direct-provider comparison processes pass,
with zero skips. The pinned foreign provider remains uninstrumented.

`indefinite-range-final-audit-02/audit.json` binds all 288 new range process
results and explicitly reuses the unchanged 276 preceding engineering passes
and four relocated consumers. Strict checks pass the new translation unit in
both actual ABIs; formatting, coverage, backlog, public surface, documentation
and the actual CI selector audit pass. Doxygen retains 140 headers, 2,422
documented members and zero warnings. The first range audit encountered CTest's
truncated passing JUnit output; its correction checks the already captured full
verbose logs without rerunning tests or relaxing the 64-case count per process.

`docs/contracts/lapack-indefinite-contract-evidence-extension.json` updates
eighteen classic contracts and 24 PB/GB registration hashes. Reviewed contracts
are now 150, with 110 implemented-unverified Reference rows and zero fully
verified Reference rows. Historical executions and native20 remain unchanged.

These eighteen rows remain numerically blocked. Hosted/wider admission and
full execution records remain required. The independent concurrency work below
is complete in its bounded scope; continue the missing rook families and retain
the range failures as ordinary required CTest gates. No frozen native20,
array-I/O or robust PPSVX acceptance is rerun.

## Concurrent factorization and reuse

The added six-scalar concurrency test uses four workers, each with independent
matrices, pivots, workspace and reports, against one immutable provider and
shared factorization plan. Workers also share one actual immutable nominal
factor and its underlying matrix/pivots. No global allocation observer or
synthetic provider-fault wrapper is active in this test.

Inputs contain a nonsingular 2-by-2 block and distinct 1-by-1 blocks, with
complex symmetric diagonals and Hermitian conjugation checked independently.
Closed-form factors and pivots, finite solutions, known solutions, original
matrix residuals, padding and immutable buffers are verified. Orders 7 and 67,
both triangles/layouts, blocked/unblocked entries, two RHS layouts and
minimum/preferred/reduced workspace cross all 168 reviewed option modes.
The source contains 32 mode groups per scalar: 416 factor attempts, including
32 stale-plan rejections and 384 reported native entries, plus 1,024 solves.
The final simultaneous calls distinguish preflight rejection, first/last
singular INFO and success, then check the separate reports after a barrier.

All sixteen actual LP64/ILP64 static/shared Debug/Release/ASan+UBSan/TSan
profiles pass six of six processes, with zero skips. Strict checks pass in both
ABIs. The first LP64 and ILP64 runs established the fixture at preferred
workspace; the expanded tests add the remaining workspace policies. Three
nested-conditional style findings were corrected without suppressing a checker
or weakening an assertion; all earlier records remain external.

`indefinite-concurrency-final-audit/audit.json` binds the 96 new passes and retains the earlier
276 engineering passes, 144 range-comparison passes, 144 required mathematical
failures and four relocated consumers. Source/header/prior-test identities are
unchanged. TSan instruments ASC and the concurrency test, not the pinned
Fortran archives; this is no claim of instrumented provider-internal coverage.
The matching contract extension refreshes 42 classic/GB/PB artifact indexes
and records recovered PB hosted evidence. Coverage, backlog, actual CI selector,
public surface, links, documentation consistency and Doxygen checks pass.
The next implementation is the eighteen rook routes; their exact source rows
and twelve compiler-emitted TF2 signatures are retained in `rook-prerequisite-01`.
