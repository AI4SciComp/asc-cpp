# Single-stage Aasen solve contract and delivery

Status: **engineering delivery checked; required mathematical and native WORK
gates remain failed**. Programme: **FULL_PROGRAM_INCOMPLETE**. The producer
milestone is pushed as `c02b549`; these six consumers receive only
`implemented_unverified` Reference status at feature integration.

The exact pinned rows are S/D/C/Z SYTRS_AA and C/Z HETRS_AA at Reference commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Their six source instances and twelve
GNU LP64/true ILP64 compiler emissions are retained under
`aasen-solve-prerequisite-01`. Native probes pass 1,008 ordinary, query-fidelity
and singular-output cases per ABI. The separate 288-case native workspace
requirement fails 16 cases per ABI, all complex symmetric zero-order queries.
No provider source, dependency, notice or floating-point environment is changed.

## Checked contract

The selected immutable square A and exact-n `kAasen` raw pivots originate
together from the same provider/scalar/triangle single-stage SYTRF_AA/HETRF_AA
call. Common numerical origin is a caller precondition; raw metadata does not
certify it. The existing raw pivot descriptor is sufficient. No classic, ROOK,
RK or two-stage factor-view factory is expanded.

The diagonal and first selected offdiagonal hold T. Other selected entries
hold shifted unit triangular multipliers as documented by the producer API.
Pivots are positive, one-based, first entry one and each later target in its
own index..N range. SY uses transpose symmetry, including complex symmetric
input; HE uses adjoints and conjugates the opposite offdiagonal copy of T.
Raw Hermitian factor coefficients are retained, including their diagonal
components. The original-Hermitian ignored imaginary-diagonal policy is not
applied to factors. Unselected entries and padding remain untouched.

The checked query reads metadata only and binds both original/effective A/B
layouts and strides, provider/scalar identity, triangle, shapes and pivot tag.
Empty N or NRHS is a validated noncall with no numerical reads or workspace,
before pivot-value checks. Active execution uses caller scalar WORK with
minimum `3*N-2`, a rounded preferred recommendation, exact-n provider INTEGER
pivots with explicit lifetimes, and explicit row-A/row-B packing. Column inputs
are direct. Arithmetic checks cover `3*N`, `2*N`, `LDA+1`, loop endpoints,
strided RHS SWAP cursors and all pointer/packing byte products. No allocation,
native query, hidden copy outside caller storage or fallback is introduced.

Native WORK is scratch during execution, not a returned workspace-size value.
Its diagonal interval is seeded before the call so a claimed singular pivot
cannot read indeterminate caller scratch. INFO starts at the full-width native
INTEGER minimum. Private input pivots are checked after the call. Missing,
partial-width, negative/out-of-range INFO, changed pivots, or a positive INFO
without the corresponding exact zero WORK diagonal are provider defects.
Packed B publication is withheld on failure; direct B may have changed.

Source GTSV can return positive INFO even though the AA routine's header prose
lists only zero and negative values. AA still performs its final triangular
solve and reverse swaps after that return. A consistent positive INFO is
reported as `kNumerical`, singular, with zero-based diagnostic index and
unusable solution output. Factors/pivots stay immutable; no solution or exact
unchanged direct B is promised. INFO zero certifies completion, not finiteness,
conditioning or accuracy. Structural rejection preserves arrays and scratch;
unsafe report/metadata aliases also preserve the report.

## Native query discrepancy

Real and HE routines use native LWORK minimum one when N or NRHS is zero.
Complex SY checks `max(1,3*N-2)` independently of NRHS and returns `3*N-2` in
query mode, including -2 at N=0. S/C/CH queries use SROUNDUP_LWORK; D/Z/ZH use
the native real conversion. Normal empty native calls leave WORK untouched.

The independent required query predicate is that a reported minimal workspace
must satisfy the documented native minimum. CSYTRS_AA/ZSYTRS_AA fail this for
both triangles and all four tested RHS counts at zero order: 16 failures per
ABI. Source fidelity to -2 passes separately. This extends the existing
`AASEN-EMPTY-WORK` provider decision; it is not fixed by ASC's empty noncall.

## Required representable-range failure

Both actual ABIs execute 432 range cases per scalar class: scalar N=1,
zero-diagonal 2-by-2, and diagonal N=3 fixtures; six scales from min-normal/8
through .75*maximum; two triangles, independent A/B layouts and three active
RHS counts. Original matrices, exact Aasen factors, known solutions and RHS
are representable. Each fixture first runs the checked producer and independent
factor reconstruction. The reconstruction and every direct-native comparison
pass. All real mathematical processes and .25*maximum controls pass.

At .75*maximum, C/Z SY fail all three fixture kinds, while C/Z HE fail the
complex-offdiagonal 2-by-2 kind. Each ABI retains four failed mathematical
processes and 512 failed assertions: 192 known-solution, 192 residual, and 128
per-element finiteness checks. The 192 failed parameter groups cover both
triangles, all independent layouts and all active RHS counts. No other scale
or fixture group fails. This is `AASEN-SOLVE-RANGE`, consolidated with the
existing large-complex arithmetic decision. The scalar SY case executes
GTSV's `B(N,J)=B(N,J)/D(N)` directly; its exact solution is 1/4 (or 1/2, 3/4).
This source path and the existing GNU large-complex division reproduction
identify a provider arithmetic cause without another unchanged broad sweep.
Successful native fidelity does not satisfy the required mathematical gate.

Original results are `aasen-solve-range-{abi}-01`, with full assertion logs
and a separate failed-group index. Fault checks pass 1,024 cases/class/ABI;
concurrency checks pass 32 groups/class/ABI with four workers and four repeats,
sharing protected read-only factors/pivots and immutable plans. The initial
structural test incorrectly expected an original one-column LD=max to reach
the native call unchanged. The reviewed common helper compacts its effective
LD to one. `aasen-solve-guards-oracle-fix` preserves the failed source and
classification; above-native original strides remain rejected. No runtime
or mathematical requirement changed for that correction.

## Finite delivery evidence

Sixteen actual GNU 11.4 Linux profiles select 552 processes: 492 pass, 60 retain
required failures, and none skip. Each of twelve static/shared, LP64/true ILP64,
Release/Debug/ASC-ASan+UBSan profiles passes 39/44. Each of four TSan concurrency
profiles passes 6/6. Required failures are the four complex range processes and
one native WORK query process in each normal profile: 6,144 mathematical and
192 workspace assertions across those profiles. All tests remain mandatory.
Pinned Fortran and BLAS internals are not sanitizer-instrumented.

Each ordinary scalar/process covers 608 cases and each range scalar/process
432 cases. Fault injection covers 1,024 cases/class; structural tests cover
408 rejection cases/class plus protected unread queries and empty noncalls,
original/effective strides and synthetic source INTEGER bounds. Concurrency
has 32 groups/class with four workers and four repeats, each using 512 native
calls, 512 empty noncalls and 512 stale-plan rejections. Factor and pivot pages
are protected read-only on the admitted Linux profiles.

The first final CTest invocation completed with exit 8. Its evidence parser
then double-counted repeated `TestContext::Finish` diagnostic summaries. The
parser correction requires equal adjacent summaries and counts each pair once.
The completed 44-process result was recovered from unchanged source, binary,
selection and raw-log identities; that invocation was not repeated. Earlier
context-spelling, structural-oracle and strict-style failures remain preserved
with their corrections. No mathematical predicate, tolerance or input changed.

Four relocated static/shared public consumers pass 768 producer-to-solve cases
each, including both ABIs, triangles, independent A/B layouts, minimum/preferred
workspace, empty and active systems. The maintained example is
`tests/dense_lapack/installed_lu/indefinite_aasen_solve_main.cc`; it uses only
installed public headers and the `ASC::dense_lapack` target. Eighteen strict
translation units pass: 16 production/test units and two installed-consumer
units, with six unchanged first strict executions explicitly reused. Final
probes compiled against actual emitted declarations pass 2,016 guarded native
cases total, retaining 16 required query failures among 288 query cases/ABI.

Four standalone-header checks, twelve new/zero removed public symbols per ABI,
ten package-manifest checks and three architecture checks pass. The maintained
CI selector includes 1,365 processes and explicitly requires all 44 new runtime
processes. Hosted producer CI exposed a missing entry in the provider-free
header audit: a local reproduction failed, then passed after adding exactly
the producer and solve headers to its existing explicit provider-facet list.
Its provider-free expected inventory and all audit predicates remain intact.
Doxygen and final installed-library identity checks are bound in
`aasen-solve-final-audit/audit.json` before normalized row delivery.

The parent commit's four hosted selected-family profiles each execute 1,319
processes without skips. Their only new failures are the six producer range
gates and one native WORK gate; prior failures persist and provider tests pass
111/111 each. These are parent-source results. Explicit feature and PR CodeQL
queries report 3,108 open alerts, including the same twelve security findings.
The fifteen new producer notes concern descriptor parameter size and an
intentional exact native WORK-write check; they were read and remain open.
Default-ref alert counts do not establish feature closure. No alert is
suppressed or dismissed and no hosted consumer or wider-platform admission is
inferred. The native20 and experimental RobustPpsvx milestones remain separate.

The existing `rk-inverse-profile-recovery-01/latest-handoff.json` binds commands,
source inputs, exact results and the unfinished delivery action. Complete the
owned feature commit/push and continue `P05.required.hesv_aa`: the six exact
SYSV_AA/HESV_AA drivers. Their final WORK[0] rewrite and zero-RHS factorization
require a driver-specific singular/empty contract. Independent remaining
programme rows continue while the consolidated provider decisions remain open.
