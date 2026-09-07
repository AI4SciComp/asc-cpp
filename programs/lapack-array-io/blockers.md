# Program blockers and required external decisions

No unavoidable local implementation blocker has been established. GCC11
Fortran and repository-pinned Clang18 formatting/tidy tools were prepared
externally without privileged installation. Baseline Debug/Release and the
external reference LP64 and true-ILP64 upstream tests passed. Missing platform/provider
verification remains a required gate, not a pass or a reason to stop
independent work.

## LAPACK-REDISTRIBUTION — pending owner/reviewer decision

Affected work: import/distribution of upstream material and the final provider
notice inventory, including any extra-precision dependency. ADR 0017 requires
reviewer approval for direct source/data adaptation; the existing provenance
review requires owner approval of new notice inventory. Program direction does
not establish that approval. The concrete license paths, hashes and proposed
redistribution contents must be assembled before requesting the decision.

Unblocked: exact external dependency preparation, source-derived inventory,
independent ASC implementation, local tests, native algorithms and array I/O.
Closure: actual approval of the concrete recorded materials by the responsible
owner/reviewer. No such approval is claimed here.

## LP64-GEEQUB-SUBNORMAL — unmet mathematical-success gate

The exact pinned LP64 S/D/C/Z GEEQUB route returns INFO=1 and zero computed
row scales for a tested nonzero subnormal matrix. Direct calls outside ASC
reproduce it; the true ILP64 route succeeds on the same fixture. The reviewed
power-helper evaluation order explains the ABI-dependent intermediate
overflow. Exact code/provider/runtime/source identities, preserved failing
mathematical logs and passing fidelity tests are in
[lu-equilibration-review.md](lu-equilibration-review.md).

The adapter preserves the raw numerical failure and documented partial
outputs, not a false singularity certificate. Fidelity/error-report tests do
not close mathematical success. No upstream patch or new provider identity
has been silently approved. A conforming disposition and fresh verification
remain required for this gate. All other ordinary LU, native algorithms,
Matrix Market and later independent family work continue.

## GERFS-TINY-ESTIMATE — unmet finite-estimate mathematical gate

Both exact LP64 and true ILP64 S/D/C/Z GERFS providers can return nonfinite
FERR for the finite scalar system A=AF=B=minimum_normal/1024 and X initially
0.75. The exact solution is one and the independently weighted estimate is
finite, but unscaled inverse application overflows before the error weights.
Actual real FERR is Inf; complex N is NaN and T/C is Inf. Raw INFO remains zero.
ASC preserves raw results and marks an accuracy warning, not successful finite
error estimation. The original failed mathematical expectations and distinct
passing source-fidelity probes remain in
[lu-refinement-review.md](lu-refinement-review.md). A conforming disposition
and fresh mathematical verification are required; no provider patch is approved
or hidden. Independent work continues.

## GESVX-TINY-CONDITION — unmet unscaled expert-driver mathematical gate

For the same scalar system, explicit FACT=N/F on both ABIs/all four scalars
returns X=1 and growth=1, but RCOND=0 and INFO=n+1 although the exact reciprocal
condition is one. Its guarded inverse-norm estimator cannot safely undo the
intermediate scaling. FERR has the distinct nonfinite results above; guarded
BERR remains finite. The failed mathematical probe retains24 failed expectations
and72 printed cases per ABI. Explicit FACT=E succeeds on this fixture with
EQUED=R, RCOND=1 and finite estimates; it is a different requested mode, never
an implicit fallback. See [lu-driver-review.md](lu-driver-review.md).
Faithful status/partial-output tests do not satisfy the unmet mathematics gate.

## LU-INTEGER-ARITHMETIC — source-audited correction in progress

The pinned S/C GETRI query calls SROUNDUP_LWORK before its query return. A
checked integer N*64 can still round to the signed ABI limit plus one, or reach
it during the helper's upward epsilon multiplication. D/Z directly return a
floating LWKOPT which can round down in true ILP64. Guards must precede foreign
entry and preserve the independently checked integer preferred capacity.
The broader registered LU call graph also needs explicit provider-INTEGER
vector-cursor/blocked-loop bounds, not merely ASC address/size validation.
Pure private integer tests, not fabricated huge live arrays, are the regression
mechanism. The existing original LU files are excluded from the current v4
freeze while this separate correction is developed. This is unfinished work,
not an unavoidable execution boundary or a completed large-size safety gate.

## PROVIDER-WORKSPACE-CONTEXT — correction and regression required

The generic public workspace validator intentionally permits host/pinned-host
storage. The explicit Serial reference provider context currently admits only
host storage. A new helper adversarial test demonstrated that relying on the
generic validator alone can admit pinned workspace while rejecting the same
placement for operands. The new helpers are corrected independently; existing
LU and Cholesky routes need a whole-facet context admission audit, source
correction where absent, and per-role no-mutation/no-provider-call tests. QR
already has the explicit check and is adding dedicated regression evidence.
Do not change the neutral public workspace contract or claim placement closure
before the provider-specific checks pass. Independent family work can proceed.
