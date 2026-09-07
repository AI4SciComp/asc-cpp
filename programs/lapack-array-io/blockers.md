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

## LU-INTEGER-ARITHMETIC — corrected, v5 combined verification passed

The pinned S/C GETRI query calls SROUNDUP_LWORK before its query return. A
checked integer N*64 can still round to the signed ABI limit plus one, or reach
it during the helper's upward epsilon multiplication. D/Z directly return a
floating LWKOPT which can round down in true ILP64. Guards must precede foreign
entry and preserve the independently checked integer preferred capacity.
The broader registered LU call graph also needs explicit provider-INTEGER
vector-cursor/blocked-loop bounds, not merely ASC address/size validation.
Pure private integer tests, not fabricated huge live arrays, are the regression
mechanism. The correction and old-code controls are frozen and tested for both
real ABIs in [lu-integer-review.md](lu-integer-review.md), then imported into
v5. V4 evidence excludes them. The final v5 f021fb8 product passed full
LP64/true ILP64 each344 and installed5 each with zero skips. The distinct
new reflector-cursor gap below remains open; these results do not close it.

## PROVIDER-WORKSPACE-CONTEXT — corrected, v5 combined verification passed

The generic public workspace validator intentionally permits host/pinned-host
storage. The explicit Serial reference provider context currently admits only
host storage. A new helper adversarial test demonstrated that relying on the
generic validator alone can admit pinned workspace while rejecting the same
placement for operands. The seven existing LU/Cholesky adapters and four missing
pivot admission sites are now corrected and imported into v5. Exact candidate
02 passes 35/35 affected tests in Debug/Release/scoped sanitizers for both ABIs;
old workspace and operand controls fail for both ABIs. See
[provider-placement-review.md](provider-placement-review.md). Root retains the
prior explicit Host/Pinned pivot predicate as well as the new context check;
that defensive amendment passed the final v5 f021fb8 combined source run:
LP64/true ILP64 each344 and installed5 each, zero skips. QR/helpers have
separate placement controls. Neutral public workspace policy and unused
zero-byte compatibility remain unchanged. Exact normalized per-mode evidence
is separate from these completed command-level checks.

## LAQGE-TINY-MATH — unmet finite scaled-output mathematical gate

Both ABIs/all four scalars reproduce overflow in the pinned C[j]*R[i]*A[i,j]
order after GEEQU on diag(minimum_normal/1024, 1): R[0]=1/minimum_normal and
C[0]=1024 are finite, but their product overflows before multiplication by the
tiny entry. The exact scaled diagonal should be one. The independent failing
mathematical probe and separate passing fidelity/allocation/source-closure
tests are retained in [lu-helpers-review.md](lu-helpers-review.md).
ASC reports nonfinite scaled output as a numerical accuracy warning while
preserving raw A and actual EQUED. It does not reassociate, patch the provider,
or credit this gate as passed. Independent implementation continues.

## GELSY-FIXED-ZERO-COLUMN — unmet minimum-norm/optimality gate

The pinned DGELSY on actual LP64 and true ILP64 reproduces INFO=0, rank=0,
X=[0,0] for A=[[0,1],[0,0]], B=[1,0], fixed-column flags JPVT=[1,0]
and RCOND=1e-12. Its squared residual is one and A^H residual is nonzero;
the independently known minimum-norm X=[0,1] has zero residual. The source
starts its rank decision with the forced zero leading column and exits before
considering a free nonzero column. Successful provider fidelity/ABI reports
do not satisfy this mathematical mode gate. No flags, RCOND, routine or source
are silently substituted. The independent mathematical gate is external
`p06-rank-revealing-fixed-zero-01/source/optimality_probe.cc`, SHA256
`df81908f3adf8612c0a62bdf58384c22c141a05bb46c0254bcbce58355dcc273`.
Both builds exit zero; actual `logs/test-{lp64,ilp64}/record.json` commands
and evidence exit one, with no CTest-count credit. The respective record hashes
are `a2dae14fc27a53b6d4becfeb49612297864a35e0e1cb4d41c1f1017c78bd55f1`
and `813b8ad10ee0260bc74b5fc1001ae20448814f5d80631f200cdabf441ab4b15c`.
Root read the exact probe and confirmed its known-optimum/residual and raw
records. Earlier bootstrap fidelity-only zero exits are separate, not
optimality evidence. Other modes and independent required families continue;
no full GELSY correctness is claimed.

The new separate `p06-rank-revealing-fixed-zero-02` probe extends this gate to
S/D/C/Z on both real provider ABIs, with explicit RCOND=0.0001. It is not the
same threshold/source as the earlier D-only 1e-12 probe. Root read the complete
new source, SHA256
`f4b5d3e2877ad9b09d755a87c153f5a31c9a905b97e3b28a9e28b48ab2e36084`.
All eight commands genuinely exit1: raw INFO0/rank0/X0, residual squared1,
nonzero normal residual; independent X=[0,1] gives zero residual and minimum
norm. Source/provider/binary identities and failed outputs remain external in
`logs/test-{lp64,ilp64}-{s,d,c,z}/record.json`; no CTest pass credit is assigned.

## REFLECTOR-ROW-CURSOR — newly identified QR/least-squares preflight gap

The exact 3.12.1 LARF1F and LARFB sources pass a matrix row to strided BLAS.
The final INTEGER cursor advance, including N*LDC+1 in COPY/AXPY/SCAL, is
not implied by a valid ASC last-element address or checked LWORK. This affects
already integrated reference QR and imported least-squares count checks, not
just the new rank-revealing slice. Root confirmed the pinned GEQR2, ORG2R,
ORM2R, LARF1F/LARFB and compact-WY call sites. Ordinary v5 full CTests do not
close this extreme-count gate. Add source-conditioned pure integer regressions
and explicit effective-foreign-stride guards, preserve legitimate original
ASC-only wide strides, and rerun affected exact-source lanes. No fabricated
large live span or integer-width typedef simulation is an acceptable test.

Root has now added those guards. Integer-only tests for both actual limits
pass, and the same independent least-squares query regression fails on the
frozen old helper and passes on the correction. Both count test TUs pass
strict Clang18. Full corrected-source provider/installed/sanitizer lanes
remain required. See [reflector-cursor-review.md](reflector-cursor-review.md).

## BAND-IGNORED-DIAGONAL — identified before root integration

Root reviewed all fourteen frozen band code/test files and found that row-major
factor packing copies whole complex input diagonals. Although NaN fixtures
show no mathematical effect, this reads ignored imaginary components. Strict
no-ignored-read semantics require real-only packing and component-wise partial
publication. PBTF2's prior CHER and blocked PBTRF's prior HERK can normalize
trailing diagonals beyond the failed pivot; simply retaining or clearing all
trailing imaginary parts would violate exact raw output semantics.

The exact source proof agrees between root and the isolated owner. An amended
dependency freeze with prefix-limited imaginary publication and direct-source
failure tests is being prepared in the new band-expert worktree. The original
band freeze remains unchanged. Root's new public-only band consumer is not
registered or included in v6; no band row is credited yet. This local fix does
not block independent expert, indefinite or rank work.

## GEQP3-EMPTY-EXECUTION — actual provider call required before integration

Root review found that isolated rank candidate01 locally emulates the JPVT
permutation for M=0, N>0. Its passing fidelity checks are not evidence of an
actual GEQP3 execution. That frozen candidate remains unchanged and is not
registered in the root coverage mapping. The approved correction executes the
real provider with canonical foreign LDA=1 and N live caller scalar staging
slots: zero-length SWAP calls still form addresses A(1,J). The original ASC
stride remains in the plan key. The actual outer query returns one, whereas
fixed-column execution needs at least N workspace elements for nested ORMQR
validation. Preflight also checks N+1 and the nested N*32+4160 query arithmetic
and scalar rounding before its empty quick return. N=0 alone may return
locally. No hidden allocation or dummy out-of-bounds address is permitted.

The isolated owner has implemented this correction and direct-source tests
for all scalar types, free/mixed/all-fixed flags, actual call counters, padded
zero-row backing, minimum-work rejection and original ASC LD=INT64_MAX.
Fresh candidate02 verification and root review remain required. Candidate01's
eight 16/16 diagnostic lanes are explicitly pre-correction evidence only.
