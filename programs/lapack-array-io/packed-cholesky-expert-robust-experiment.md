# PPSVX first-party numerical experiment

This is the preserved experiment record. The subsequent conditional adoption
and opt-in integration decision is in the
[integration review](packed-cholesky-expert-robust-integration.md); the original
experiment-only authorization below describes its historical scope.

The user's 2026-09-10 instruction authorizes original numerical implementation
and isolated staged evaluation addressing all five causes in the
[disposition](packed-cholesky-expert-disposition.md). The pinned Reference
provider and compatibility route, every existing requirement, case and
tolerance remain unchanged. This decision does not authorize upstream/MdeCpp
copying or patching, dependency or floating-point environment changes, bundling,
notice approval, merge/release, root integration or automatic Reference credit.

The named `asc-cpp-ppsvx-numerical-decision-and-prototype.md` was not found in
the checkout or the initial nearby/home search. Its location was requested;
the explicit user instruction supplies the authority for current work. This
record does not claim to reproduce the missing document.

## Algorithm and transformations

The external candidate `ppsvx-robust-experiment-01` adds 24 separately named
query/execute declarations for the four scalar types: `RobustPpsvx`,
`RobustPpsvxEquilibrated`, `RobustPpsvxFactored` and their workspace queries.
Reports identify `asc_robust_[sdcz]ppsvx_v1`, with no native INFO and
`called_provider=false`. The existing provider object supplies context and
ABI admission identity; this route makes no LAPACK numerical call. It is an
original first-party packed algorithm, not a fallback in the Reference route.

Each real component is represented by a working-precision mantissa and a
checked signed 64-bit binary exponent. Complex components have independent
exponents. Products and quotients operate on bounded mantissas and checked
exponents; square roots split exponent parity. Addition aligns two mantissas.
A term below the working-precision rounding threshold can be absorbed in that
addition, but remains represented in its source operand. This extends exponent
range, not significand precision. It avoids the loss of a tiny matrix entry
that a single common-scale conversion could cause. Exponent exhaustion is an
explicit failure, not a clamped diagnostic.

Let A and b denote the original system and D a positive real diagonal matrix.
In exact arithmetic C=D A D, d=D b and x=D y give
`C y=d` if and only if `A x=b`: multiply the first equation by D inverse and
substitute x. Hermitian symmetry and positive definiteness are preserved since
`z* C z=(D z)* A (D z)>0` for nonzero z. These transformations apply to real
and complex scalars without changing triangle or conjugation semantics.

- FACT N uses D=I. An original packed Cholesky computes a rounded factor L
  (or U=L conjugate-transpose) of A. Every factor component is rounded to the
  public scalar and reloaded before later arithmetic uses it.
- FACT E computes rounded `S(i)=1/sqrt(A(i,i))` safely and uses the existing
  scale-selection thresholds. When selected, it publishes rounded C=D A D
  and d=D b. The factorization uses that same published C. Rounding means
  `M=L L*` approximates C; neither equality to C nor exact equivalence to the
  unrounded original system is asserted. Independent tests measure the
  original-system residual and forward error, including this rounding.
- FACT F reads the documented supplied factor and C in their common
  coordinates. With EQUED Y, it forms d=D b and publishes x=D y; with N it
  uses D=I. It never refactorizes C. Upper factors map to the internal lower
  factor by conjugate transpose, including complex diagonal components.
  Reusing returned E factors therefore applies exactly the same rounded M
  and D as the first call, with a new original right-hand side.

Inverse actions solve with M using scaled packed triangular substitutions.
Applying these actions to each basis vector gives an estimate of the inverse
norm without publishing an overflowing inverse. RCOND is evaluated as
`1/(norm(C)*norm(M^-1))` in scaled arithmetic, with Euclidean complex modulus
and consistent Hermitian row/column norms. Thus the tiny inverse and the large
reciprocal never need to fit the public scalar independently. This is a
condition estimate for the represented equilibrated system and rounded
factors, not a claim of an exact condition number for arbitrary supplied AFP.

Refinement forms `r=d-C*y` and
`q=abs1(d)+abs1(C)*abs1(y)` without working-type exponent overflow. It permits
three corrections using the unfloored backward ratio. BERR retains the
documented safe floor `(n+1)*normal_min` for tiny denominators, so a zero
equation can legitimately report BERR=1. That convention is separate from
convergence. A failed convergence check produces an accuracy warning.

Forward weights are `w=abs1(r)+gamma*q`, where
`gamma=8*(n+1)*epsilon/(1-8*(n+1)*epsilon)`. The factor eight budgets real
operations in complex products/sums; this remains an estimated bound, not a
universal error theorem. Each weighted basis vector `w(j)*e(j)` enters the
inverse action **before** solving. Summed absolute results estimate
`norm(D*abs1(M^-1)*w)/norm(x)` in original coordinates. A zero solution norm
uses an absolute estimate. No artificial normal-minimum term is needed in
these weights: the scaled operations do not underflow at the public type's
normal boundary. In particular an exact homogeneous equation has zero weight.
The existing scalar FERR assertion, including its allowed safe-floor term,
is unchanged.

All outputs are staged in explicit caller scratch; there is no dense conversion
or hidden allocation. The plan contains `2*p+2*n*nrhs+5*n+2*nrhs` private
entries with explicit size/alignment. Nonallocating placement array construction
establishes their object lifetimes. Numeric range loss at publication returns
`kOverflow`, an accuracy warning, unchanged numeric outputs and no factor
authorization. This includes nonzero-to-zero loss or subnormal rounding beyond
eight epsilon relative error. Finite low condition or failed refinement can
publish documented partial results with `kNumerical`. N=0 remains a local
write-only completion; N>0/NRHS=0 still factors and estimates condition.

## Five-cause disposition

The original fixtures, exact observed values/INFO and direct-provider comparisons
remain in the [Reference disposition](packed-cholesky-expert-disposition.md).
The table below describes the new route's response, without changing those rows.

| Existing cause | Original first-party operation | Experimental disposition |
| --- | --- | --- |
| Tiny inverse condition estimate | Scaled basis triangular solves and inverse norm | Unchanged condition-one assertion passes at both smallest subnormals, N/F, all scalars. |
| Tiny weighted forward estimate | Apply each weight before its inverse action | Unchanged finite FERR assertion passes; no intermediate unweighted inverse is narrowed. |
| Maximum reciprocal/norm evaluation | Evaluate the reciprocal of the scaled norm product | Unchanged finite condition-one assertion passes at maximum finite input. |
| Maximum residual/error evaluation | Scaled residuals, denominators, weights and row sums | Unchanged finite FERR and BERR-in-[0,1] assertions pass. |
| Subnormal equilibration | Per-component scaled D*A*D and D*b | FACT E passes the unchanged original X=1 and equilibrated condition-one assertions, with returned-factor reuse tested separately. |

## Executed finite matrix

Evidence paths here are relative to external
`asc-cpp-evidence/lapack-array-io/ppsvx-robust-experiment-01`. The final audit
records actual test IDs, commands, exit statuses, source hashes and every
retained attempt. GNU11 Debug/Release and Clang19 ASC-only ASan/UBSan use each
prepared LP64/true ILP64 provider, not an integer-typedef simulation.

Per numerical configuration, the four scalar tests exercise all six original
values, four FACT/equilibration modes, two triangles and two layouts: 384
composite cases and 2,304 assertions. The mathematical predicate and fixture
domain are unchanged. Reference-only direct-call mapping assertions remain in
the untouched Reference test; the new test checks first-party report identity.
The Reference process count of four is distinct from its 176 failed assertions,
44 unique scalar/mode/value cases and five overlapping causes.

Each configuration also runs 192 ordinary solve/reuse profiles (53,665 checks),
1,600 concurrent solve/reuse profiles (447,109 checks), and 46,080 bounded
generalization profiles (3,323,137 checks per scalar). Generalization covers
N=0/1/3/5, NRHS=0/1/3, both triangles, all 16 independent packed/full layouts,
four modes and factor reuse at five exponent scales from subnormal to near
maximum. An independently implemented pivoted Gauss-Jordan oracle uses
long double in tests only; production retains float/double significands.

Safety covers 2,080 profiles and 613,889 checks per allocation-wrapped process:
metadata-only queries, stale triangle/count plans, one-byte-short and misaligned
workspaces, numeric/report preservation, output guards and zero operation
allocations. Mixed diagonal systems `[8*denorm_min, 1, max/4]` solve and reuse
E factors across the scalar exponent range. Sixteen publication-loss cases
require explicit range failure; 384 nonfinite supplied fixtures require input
rejection. The process and concurrent workers verify an unchanged rounding mode.

Concurrency uses independent buffers, workspaces, reports and provider contexts,
independent known solutions and 32 real nonpositive first-party calls for
diagnostic isolation. It exercises the actual algorithm, without synthetic
numerical callbacks or shared-workspace requirements. Both actual staged ABI
installations pass GNU11 ASC-only TSan. The established `setarch x86_64 -R`
workaround changes process ASLR only. Provider/runtime code is not instrumented;
this new numerical route does not enter it. Candidate22's separate real-provider
concurrency evidence remains intact.

The load observer instruments the actual adapter and consumer translation units
using Clang19 `-O1 -fsanitize-coverage=trace-pc,trace-loads`, with the existing
observer compiled separately. It brackets each query/rejection or Execute call,
watching complete initialized containing arrays for AFP, X, E's S, FERR/BERR
and RCOND. N/E output loads must be absent; F's legitimate AFP/S input loads
must be seen. Twenty intentional old-output byte reads calibrate detection,
and ordinary writes still solve correctly. Whole-Execute observation is
appropriate here because the new route has no foreign-entry or post-return
partial-publication phase. Candidate22's actual pre-native boundary observation
remains the evidence for the Reference route. Blind spots include optimized-away
loads, uninstrumented library/foreign instructions and other compiler/platform
builds; this is scoped instruction evidence, not a claim of foreign coverage.

The final GNU Debug/Release selections each pass 11/11. Each final sanitizer
selection passes the same 11 tests and times out only the load observer during
concurrent build/analysis activity. With those jobs complete, each identical
observer binary passes its unchanged 120-second selector in about 58 seconds,
with 693,781 checks and all controls passing. These are explicit supplemental
closures, not a rewritten claim of a single-invocation 12/12 final run.

Both GNU11 Release staged installations export the actual public header and all
24 new symbols through `ASC::dense_lapack`. Relocated consumers use only the
installed ASC include directory and intended exported target; provider archives
are resolved from an independently relocated provider prefix. No provider or
Fortran runtime is bundled in the ASC install. Ordinary/concurrent/safety tests
pass 3/3 per ABI. Header self-containment passes normal/no-exception compilation
in both ABIs. The staged source/installed surface is 128 headers; strict Doxygen
finds 2,252 documented public members. Strict adapter/header and consumer checks
pass both ABIs, including the observation/allocation build variants.

Staged compatibility gates pass public/count/signature/fault tests and fail all
four unchanged Reference mathematical processes (CTest exit 8). Their 176
ordered failure lines per ABI exactly match candidate22. The installed robust
four-scalar replay passes. Fresh provider-free native tests pass 7/7 per ABI,
and Dense/Sparse array-I/O examples each pass 1/1 without acquiring LAPACK,
Fortran or quadmath runtime dependencies.

## Preservation and remaining admission

The experiment preserves failed prototypes, compile/style failures, the stale
header-checksum failures and load-observation timeout records. The invalid
overflowing generalization fixtures are retained as explicit rejection tests;
no required scalar input or tolerance was removed. Final report semantics were
corrected so a numerical publication failure does not incorrectly say execution
never began. Numerical implementations and all prior candidate22 files remain
separately identified. The current checkpoint does not supersede historical
failures with an expected-success interpretation.

The candidate is an isolated installed public capability, not an integrated or
registered root API. Its pending integration patch includes new files and
matching source-list/header metadata; it is not applied to the feature checkout.
All four Reference PPSVX rows remain unregistered and numerically failed under
the pinned path. The 2,113 upstream denominator, 350 partial registrations,
1,763 not-started rows and zero strict complete/verified Reference rows are
unchanged. Native20/array-I/O remains separately
`SUBSET_REVIEW_READY_FULL_PROGRAM_INCOMPLETE`.

The next decision is review/adoption of this explicitly named first-party
candidate and its staged API/report contract, followed by the existing wider
platform and root-admission gates. The current authorization completes an
experiment; it does not grant those promotions. The pending notice amendment
`380a8792` / proposed notice `9ebeaf30` and merge/release authorization remain
separate. No upstream patch, dependency distribution or floating-point control
change is proposed or performed.

The frozen staged source manifest is
`7adf632698d0e28d87b15ffe377546f298f9bdae99336bdb70120e2fc5981e73`,
on integration base `6124f5bd391e3b9bd57ee59701a73bfc49ff3c4d`.
The six new C++ identities are in `frozen-owned-inputs.json`; the complete
20-file pending patch, including new files, is
`integration-packet-02/pending-integration.patch`, SHA-256
`c3d28360d9a0c0ca624aa07b2f2f69c0a637f338d672b994f37ac3a5c05c1caf`.
`review-audit.json`, SHA-256
`0c85ba1007737306a967738c76c863af5efb47b6590421517893b261773c8d59`,
binds 257 command records, including failures, exact selections and provider
identities. Final program-record checks and synchronization have their own
supplement; they do not create another numerical candidate. The read-only
`resume_review.py` checks selected identities without reapplying a patch.

Implementation review used the live [Google C++ guide](https://google.github.io/styleguide/cppguide.html)
and [Python guide](https://google.github.io/styleguide/pyguide.html), the repository
format/tidy configuration and accepted D003 compatibility rules. The localized
Clang observer ABI uses the existing project mechanism.
