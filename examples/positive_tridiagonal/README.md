# Positive-definite tridiagonal factor reuse

This optional Reference-LAPACK example factors an ordinary, independently
constructed real/Hermitian tridiagonal matrix once and solves two different
multiple-RHS systems. It exercises all four scalar types, both RHS layouts,
borrowed lower factors, and resource-owned upper factors. Complex orientation
conversion explicitly conjugates the off-diagonal. D remains real.

Configure this directory against an installed ASC package with the admitted
`dense_lapack` component, using `ASCCpp_DIR`, `ASC_CPP_LAPACK_ROOT`, and the
explicit runtime list required by the installation guide. Build and run CTest
with `--no-tests=error`. Only public headers and `ASC::dense_lapack` are used;
there is no dependency on an evidence directory or ASC source headers.

The ordinary example does not close the separate extreme-value numerical gate:
the pinned PTTRS scalar reciprocal can overflow for a subnormal D even when the
exact solution is representable. No alternate algorithm or fallback is selected.
