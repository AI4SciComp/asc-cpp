# PBTRF/PBTF2/PBTRS contract and range continuation

Status: twelve existing S/D/C/Z routines are integrated and implemented;
normalized contracts and the bounded local range gate pass. Full verification
remains incomplete. The [original source review](band-cholesky-review.md) and
[INFO correction](band-info-review.md) retain their historical evidence.

## Contract closure

The twelve exact LAPACK 3.12.1 source rows were checked against pinned commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` and the unchanged upstream inventory.
Their normalized contracts have 64 modes: 32 factor modes across two routines,
four scalar types, two triangles and two layouts; 32 solve modes additionally
select RHS layout independently. Real factors use transpose; complex factors
use conjugate transpose. Original imaginary diagonal inputs are not read.
PBTRS consumes raw triangular factors and retains their complex components,
including imaginary diagonals. Its fidelity tests do not certify nonfinite
solutions as mathematical success.

No native routine has a caller WORK argument or workspace query. Exact checked
plans expose only caller-owned band and RHS conversion storage; column-major
operands avoid conversion. The source-conditioned integer bounds, alias checks,
empty native calls, padding preservation and source-specific partial publication
remain unchanged. The two corrected full-width INFO sentinels still reject
unwritten and partial-width foreign INFO. No public API, production source,
provider arithmetic, ABI or installed-example source changes in this checkpoint.

## New numerical class

`band_cholesky_range_test.cc` independently checks scaled identities at twice
the minimum normal, minimum normal divided by 1,024, twice the minimum
subnormal, and maximum finite scalar. Shapes n/kd = 1/0, 3/2 and 96/65 reach
scalar, unblocked and actual blocked paths. Every scalar covers both factor
entries, triangles and band layouts, followed by both RHS layouts with two
right-hand sides. Complex original imaginary diagonals contain NaN.

Each profile executes 384 factor cases and 768 solves. Assertions require
finite factors, positive real diagonals, squared-diagonal reconstruction,
exact zero off-diagonals, finite solutions, error from the known solution one,
and an independent original-system residual. Relative bounds are 64 epsilons;
the reference value is the actual stored scale and wide arithmetic avoids
underflow in the oracle on the tested Linux profile. Padding, factors reused
by solves, workspace guards and allocation boundaries are checked separately.
Taking the reciprocal after the Cholesky square root passes these range cases.
This result does not change the separately failing PT or GB algorithms.

## Evidence and reuse

All raw artifacts are external under
`master-continuation-20260910-01/continuation-20260912-01`, relative to the
existing programme evidence root. `pb-final-audit/audit.json` binds the new
executions, exact inputs, unchanged prior evidence and metadata checks.

| Check | LP64 | True ILP64 |
| --- | --- | --- |
| New static Release range tests | 4/4 | 4/4 |
| New static Debug range tests | 4/4 | 4/4 |
| New static ASan/UBSan range tests | 4/4 | 4/4 |
| New shared Release range tests | 4/4 | 4/4 |
| Reused prior engineering profiles | 58 passes | 58 passes |
| Reused relocated static/shared consumers | 2/2 | 2/2 |

All executions have zero skips. `pb-reuse-audit-02/audit.json` resolves 43
unchanged source/header/helper dependencies against the INFO-correction commit
and rechecks original package artifacts and JUnit hashes. These are explicitly
reused executions, not newly generated results. The initial reuse helper named
a nonexistent private context header before collecting evidence; the corrected
helper follows actual quoted includes recursively. That harness issue did not
alter any test or production input. Both strict checks for the new translation
unit pass with the repository configuration. A failed Doxygen link to a
programme file outside its configured input set is retained; the reference
now uses its literal repository path. The pinned foreign provider remains
uninstrumented in the ASC sanitizer profiles.

CI now requires all four new range tests and the existing 13 PB engineering
tests in its actual selector audit. Coverage and evidence indexes are amended
atomically, preserving all historical executions and all frozen native, array-I/O
and robust PPSVX records. The 12 PB routes move from in-progress to
implemented-unverified, with reviewed contracts. Reference counts are now
92 implemented-unverified, 314 in progress, 1,707 not started and zero fully
verified, out of 2,113 required rows. The reviewed-contract count is 132.

## Remaining work

Concurrency and TSan, remaining shared Debug/sanitizer profiles, hosted and
wider platform/provider checks, and normalized full execution evidence remain
required. They are not numerical failures established by this range class.
Continue those feasible PB checks and then the existing classic indefinite
families; preserve unrelated mathematical and external blockers. The overall
programme status remains **FULL_PROGRAM_INCOMPLETE**.
