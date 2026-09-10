# PPSVX numerical disposition

The pinned Reference PPSVX numerical acceptance remains blocked. The required
scalar assertions remain justified and unchanged. Candidate22 reproduces candidate21's complete
failure signatures in all six Linux configurations; candidate21 already binds
the comparison to candidate18. Candidate06's direct-provider reproduction is
still relevant. No new broad numerical sweep, provider change, tolerance change,
input rescaling, fallback, or floating-point environment change is needed to
classify these failures.

The user's 2026-09-10 decision now authorizes an original first-party algorithm
experiment, with the pinned compatibility path unchanged. The separately named
[robust candidate](packed-cholesky-expert-robust-experiment.md) passes the
unchanged scalar predicate and bounded generalization in both actual ABIs.
This resolves the experiment's five arithmetic sites; it does not turn the
Reference failures below into passes or authorize adoption, root integration,
notice approval or full-platform promotion. The remaining numerical strategy
decision is review/adoption of that concrete candidate, not permission to begin
its already-executed implementation.

The fixture is `N=NRHS=1`, original `A=B=[a]`, with positive finite `a`, exact
`X=1`, and condition one. Supplied factors are rounded `sqrt(a)` and pass the
independent relative reconstruction check. Both supplied equilibration choices
use `S=1`; their original and equilibrated scalar systems coincide. FACT E's
intended equilibrated matrix also has condition one. These facts justify the
analytic scalar requirement without treating arbitrary RCOND estimates or FERR
estimates as universal guarantees. The unchanged finite FERR oracle is
`128*epsilon + 4*normal_min/a`, with the second term omitted after equilibration.
It follows from the fixture's solution-error allowance and refinement safe floor.

Per configuration there are **four failed test processes**, 2,400 assertions,
and **176 failed composite mathematical assertions**. There are **44 distinct
scalar/mode/value cases**, each repeated for two triangles and two layouts.
They occupy three input regimes and expose **five arithmetic causes** below.
The two diagnostic causes within a regime overlap the same cases; their case
counts must not be added. Equal printed real/complex outputs do not collapse
SPPSVX/DPPSVX/CPPSVX/ZPPSVX into fewer catalogue entries.

Provider: Reference-LAPACK 3.12.1, commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, GNU Fortran 11.4,
LP64 and true global ILP64 (`-fdefault-integer-8`). The retained provider is
unchanged. Sixteen selected Fortran source hashes were checked against the
pinned inventory. The table diagnoses intermediate arithmetic from those
scalar source expressions; it does not claim new foreign-code instrumentation.

| Cause and affected cases | Required property and observed result | Pinned expression and disposition |
| --- | --- | --- |
| Tiny condition estimate: all four scalars, `a=denorm_min` or `2*denorm_min`, FACT N/F, supplied EQUED N/Y. 24 cases. | Condition one; observed RCOND=0 and INFO=2. X remains approximately one. | xPPCON's two xLATPS solves require an inverse larger than the scalar range. `SCALE=SCALEL*SCALEU` and the guarded unscale can return with RCOND still zero. A scaled condition-estimation algorithm/provider decision is required. |
| Tiny forward estimate: the same 24 cases. | Finite fixture-specific FERR allowance; observed FERR=Inf despite finite near-unit X. BERR is finite and near one because of the safe floor. | xPPRFS's KASE=1 branch solves with xPPTRS **before** multiplying by the small weight. The unweighted scalar inverse overflows although the weighted result is representable. A scaled weighted-estimation algorithm/provider decision is required. |
| Maximum-value condition estimate: all four scalars, `a=max_finite`, FACT N/F, supplied EQUED N/Y. 12 cases. | Condition one; observed RCOND=Inf, INFO=0. | xPPCON evaluates `(ONE/AINVNM)/ANORM`. The rounded inverse estimate is subnormal; the first reciprocal overflows before division by finite ANORM. A robust evaluation/provider decision is required. |
| Maximum-value refinement diagnostics: the same 12 cases. | Finite FERR and BERR in [0,1] for the analytic fixture; observed FERR=Inf and BERR=NaN, X approximately one, INFO=0. | xPPRFS accumulates `abs(B)+abs(A)*abs(X)` in the working type; approximately `2*a` overflows. The residual can also overflow for the rounded X slightly above one, and the subsequent ratio/weights propagate nonfinite values. A scaled residual/weight evaluation/provider decision is required. |
| Subnormal equilibration: all four scalars, the two tiny `a` values, FACT E. Eight cases. | Equilibrated scalar coefficient approximately one and original X=1; observed X=0, RCOND=0, FERR/BERR=NaN, INFO=2. | xLAQSP uses `CJ*S(I)*AP`; xLAQHP's diagonal uses `CJ*CJ*REAL(AP)` (DBLE for Z). The scale product overflows before multiplication by subnormal AP. A reviewed provider/algorithm decision covering scale-product evaluation is required. |

For S/C, `denorm_min=2^-149`; for D/Z it is `2^-1074`. `max_finite` is
`(2-2^-23)*2^127` or `(2-2^-52)*2^1023`, respectively. Exact hexadecimal X,
RCOND, FERR, BERR, finite oracle bounds, and native INFO for each of the 44
cases are retained in the machine-readable disposition artifact below.

All these cases compare equal to direct pinned calls in the unchanged `Direct`
and `Mapping` checks. ASC retains native INFO and raw diagnostics, returns
`kNumerical`, and reports `kAccuracyWarning` / `kDocumentedPartial`; that
accepted warning behavior does not satisfy the separate numerical gate.
The mapping, guard, and rounded-factor checks pass. There is no evidence here
of an ASC mapping defect or an objectively faulty scalar oracle.

The original evaluation decision is recorded as granted for a first-party
experiment. Adoption and the corresponding wider admission remain separate;
all four pinned Reference numerical rows remain incomplete. The experiment
addresses all five sites with the same scalar domain and assertions.
Reassociating only equilibration would not close the other four sites. This
packet proposes no dependency patch, upgrade, redistribution or contract
waiver. The pending notice/metadata amendment is a separate decision.

Evidence is relative to the existing external `asc-cpp-evidence/lapack-array-io`
root:

- `p05-packed-cholesky-expert-candidate-22/numerical-disposition.json` binds the
  44 exact signatures, source hashes, raw record, counting units, and scope.
- `p05-packed-cholesky-expert-candidate-22/mathematical-signatures.json` binds
  all six candidate22/candidate21 comparisons. Its 22 distinct printed strings
  reflect identical real/complex values, not 22 distinct scalar cases.
- `p05-packed-cholesky-expert-candidate-21/active-boundary-audit.json` binds
  the candidate18 comparison and retained provider identities.
- `p05-packed-cholesky-expert-candidate-06/numeric-audit.json` and the retained
  `packed_cholesky_expert_math_test.cc` contain the earlier direct comparison.

No historical failure is removed or converted to an expected-success test.
