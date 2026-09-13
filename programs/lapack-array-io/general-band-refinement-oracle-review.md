# GBRFS safeguarded diagnostic oracle correction

The maintained GBRFS public contract retains the pinned small-denominator
safeguards. Two assertions in the existing tiny-fixture test contradicted that
contract: BERR was compared with the unsafeguarded residual, and FERR was
required to be below256epsilon despite a finite conservative bound of order16
or larger. This correction preserves every input, layout, transpose mode and
tolerance, along with the genuine required finite-FERR failure. No product
algorithm, provider, floating-point environment or numerical contract changes.

## Derivation and old/new assertions

The existing fixture has N=1, KL=KU=0, positive real A=AF=a=min_normal/8,
raw pivot1 and three exact dyadic solution columns. B=aX is exactly represented.
For C/Z, magnitude means the documented CABS1. Let v=|X|. The true residual
is zero and its denominator is d=2av=min_normal*v/4. The source definitions
give NZ=2, SAFE1=2min_normal and SAFE2=SAFE1/u, where the pinned binary-profile
xLAMCH epsilon u is half C++ epsilon. This fixture satisfies d<SAFE2.

Consequently the safeguarded backward estimate is8/(8+v), while the true
backward error remains zero. The scalar weighted inverse bound is
(2u*d+SAFE1)/(a*v)=4u+16/v. These quantities follow directly from the scalar
equation and its safeguarded weights; no copied inverse-estimation algorithm
or universal FERR theorem is used.

| Check | Previous assertion | Corrected assertion |
| --- | --- | --- |
| Actual solution and true residual | Existing independent solution/forward/residual checks | Unchanged |
| Finite, nonnegative diagnostics | FERR and BERR finite/nonnegative | Unchanged; FERR still fails |
| Tiny BERR | Within64epsilon of the true zero residual | Within the same64epsilon of8/(8+v) |
| Tiny FERR | Positive and below256epsilon | Finite and within64epsilon relative error of4u+16/v |
| Ordinary fixtures | Existing small-error bounds | Unchanged |

The new helper checks its fixture assumptions: scalar positive real A, exact
B=A*X, nonzero v and the safeguarded denominator branch. Real v values are
1,3/4,1/2; complex values9/8,3/4,5/8. Thus finite bounds are approximately
16,64/3,32 for real and128/9,64/3,128/5 for complex. There are12 distinct
scalar/RHS equations,36 scalar/transpose/RHS evaluations and144 RHS/layout
contexts per ABI, from48 adapter calls. Transposing a real scalar diagonal
does not create a new mathematical equation.

## Preserved failure and executed correction

The pinned provider still applies an unscaled inverse before multiplying by
the small error weights. It returns INFO0, correct X, the safeguarded finite
BERR, and infinite FERR. ASC preserves its documented accuracy-warning report.
All four extreme CTest processes remain ordinary failures. There is one
provider overflow cause here; duplicate failed checks are not separate bugs.
Existing direct-call reproductions remain in the
[general-band expert review](lu-band-expert-review.md); no broad reproduction
loop or provider-fidelity substitute is introduced.

Raw evidence is under `master-continuation-20260910-01/`.
`gbrfs-oracle-review-01` preserves the original test/header/helper identities,
derivation and raw failing output hashes. `gbrfs-oracle-{lp64,ilp64}-03` executes
the unchanged ordinary and extreme fixtures against the installed frozen ASC
candidate and each actual ABI:4/8 pass, four extreme processes fail, zero skips,
CTest exit8. Each extreme process now has72 failures instead of108: the
incorrect BERR assertion is gone, while finite-FERR and analytic-bound failures
remain. The corrected tiny BERR values pass in all144 RHS/layout contexts per
ABI. `gbrfs-oracle-audit-03` binds the exact expressions and observed values.

Initial isolated builds02 failed because their driver added conversion warning
flags absent from the actual maintained target. Source03 matches the recorded
C++20/-Wall/-Wextra/-Wpedantic/-Werror baseline. No maintained compiler gate or
helper expression was weakened or changed. Both failed builds and original
source02 remain retained. Strict checking with the full repository configuration
passes in `gbrfs-oracle-style-01`.

The reviewed correction is in the maintained test. All twelve static/shared
Debug, Release and ASC-only sanitizer profiles in actual LP64/ILP64 execute
eight tests: four ordinary passes and four extreme failures,72 assertions each,
zero skips or sanitizer diagnostics. `gbrfs-integrated-audit-01/audit.json` binds
every process, expression and output hash. Static profiles build the root
target; shared profiles compile this exact maintained translation unit with
the configured profile flags and link its existing production-object observation
DSO. Frozen producer sources are unchanged; provider/runtime instrumentation
is not claimed. This is not an installed-consumer execution claim.

The schema2 index amendment changes only this test hash in20 existing
general-band artifact sets. It preserves all mode/class states and old
execution records under the explicit GBRFS identity extension. Product sources,
public APIs, examples, ABI and package inputs remain those of LASCL revision
`dbc924d7aaada7f51dc1359739f7b54f981af6af`; their package/concurrency results
remain applicable through this checked unchanged-input comparison. The hosted family selector adds all eight ordinary/extreme
GBRFS tests; these previously remained required in the full programme matrix.
No test is skipped, removed, marked WILL_FAIL or relabeled as mathematical
success. Reference verification counts and the2113 denominator do not change.
