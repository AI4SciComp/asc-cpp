# Experimental first-party packed positive-definite expert solves

`ASC_CPP_ENABLE_EXPERIMENTAL_ROBUST_PPSVX=ON` explicitly selects an experimental
implementation in the optional `dense_lapack` facet. It defaults to OFF and
requires `ASC_CPP_ENABLE_LAPACK=ON`. Installed packages expose the Boolean
`ASCCpp_EXPERIMENTAL_ROBUST_PPSVX`; consumers link `ASC::dense_lapack` and include
`asc/dense/providers/lapack_cholesky_packed_robust.h`. The admitted development
profile is Linux x86_64, GNU 11.4.0, with actual LP64 or global ILP64. Static
ASC requires the separately attested static provider; shared ASC requires the
separately attested shared provider. Both use pinned Reference-LAPACK 3.12.1.
The shared package retains explicit provider/runtime dependencies and resolves
relocated ASC libraries without requiring a producer-side loader environment.

The four scalar overloads of `RobustPpsvx`, `RobustPpsvxEquilibrated` and
`RobustPpsvxFactored`, and their workspace queries, are separately named.
They never replace or silently fall back from the Reference APIs. Reports use
`asc_robust_[sdcz]ppsvx_v1`, `called_provider=false` and absent native INFO.
The existing provider object supplies execution context and ABI admission;
this first-party algorithm makes no LAPACK numerical call. Its success grants
no Reference catalogue credit or universal PPSVX acceptance.

The original algorithm derivation in
`programs/lapack-array-io/packed-cholesky-expert-robust-experiment.md` defines the arithmetic, unchanged mathematical requirements and finite tested
domain. Each real component carries a float/double significand and checked
binary exponent. Complex components retain independent exponents. This extends
range, not precision; it neither changes the floating-point environment nor
rescales caller data outside the selected API's documented transformations.

For a positive real diagonal D, C=D A D, d=D b and x=D y transform C y=d
to A x=b. FACT N uses D=I. FACT E may publish rounded C, d, D and its rounded
Cholesky factor. All subsequent arithmetic uses the factor rounded to the public
scalar. FACT F consumes the supplied C and rounded factor, with original-system
b and documented D, and applies that factor without refactorization. Reuse of
returned E factors therefore requires its returned C, factor and scales, with
a new original-system RHS. Rounding is included in the original-system error
checks; exact equivalence of the rounded systems is not promised.

If M is the supplied or computed rounded factor product, RCOND estimates
`1/(norm(C)*norm(M^-1))`. It describes the equilibrated system when used and
is not an exact condition certificate for arbitrary raw factors. FERR is an
estimated forward bound using weighted inverse actions, qualified by rounding,
conditioning and factor quality. BERR uses the documented safe floor and can
equal one for a zero componentwise denominator. This convention alone does not
mean refinement failed. There are at most three refinement corrections.

All numeric outputs are staged in explicit caller workspace. Query and
structural rejection inspect descriptor metadata, not numeric output values.
AFP is output in N/E and input in F; old X, FERR and BERR are never numeric
inputs. N=0 completes locally with write-only outputs. N>0/NRHS=0 still factors
and estimates condition. Workspaces, buffers and reports must be independent
between concurrent calls; immutable matching plans may be reused as documented
by the workspace contract. No shared-workspace safety is promised.

Publication rejects overflow, nonzero-to-zero loss, and subnormal rounding loss
exceeding eight working epsilons relative error. It returns `kOverflow` with
`kAccuracyWarning`, unchanged caller numeric outputs and no factor authorization.
It does not substitute zero, infinity or a diagnostic clamp. Finite low RCOND
or failure of refinement can return `kNumerical` with documented partial
outputs. Callers must inspect Status and LapackReport before using outputs.

Condition estimation performs n triangular inverse actions and costs O(n^3).
Each RHS's forward estimate also performs n weighted inverse actions, costing
O(n^3) per RHS. Total cost is O(n^3*(1+nrhs)), including FACT F; factor reuse
avoids factorization but does not make expert diagnostics a quadratic solve.
Scratch storage is O(n^2+n*nrhs), with exact byte size and alignment in the
query result. No hidden allocation or implicit dense conversion occurs.

The maintained extreme oracle explicitly requires long double exponent bounds
at least [-4096, 4096]. This holds in the admitted GNU x86_64 test profile;
platforms with long double equal to double have not received extreme-oracle
verification. The ordinary [public example](../../examples/robust_ppsvx/README.md)
does not depend on a wider type. Supplemental Clang instruction observation
instruments actual first-party adapter and test instructions, with calibrated
negative/input/write controls. It does not instrument installed or foreign
code, and does not admit a new provider platform.

Wider compiler/platform admission, owner notice review and
release authorization remain separate. No upstream algorithm, source archive
or runtime is bundled by this capability.
