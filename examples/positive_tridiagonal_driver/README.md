# Positive-definite tridiagonal driver

This public consumer solves independent ordinary systems with all four
scalar types, checks diagnostics, and reuses returned factors through the
documented borrowed and owned PT interfaces. It covers both full RHS layouts
and conjugated upper-factor reuse for complex Hermitian matrices.

Build an ASC installation with the optional Reference LAPACK component and
the pinned, separately installed provider described in
[installation](../../docs/installation.md). Configure this directory with
`ASCCpp_DIR` pointing to that installation, `ASC_CPP_LAPACK_ROOT` pointing to
the matching provider, and the documented explicit runtime libraries. Then
build and run `ctest --no-tests=error` from the example build directory.
Only exported `ASC::dense_lapack` and public headers are used. The local
normal-return helper checks that the example actually reaches normal return.

This ordinary example does not establish acceptance of extreme-value PTSV
requirements. The retained required failures and precise supported scope are
recorded in
`programs/lapack-array-io/positive-tridiagonal-driver-review.md` in the
repository programme records.
