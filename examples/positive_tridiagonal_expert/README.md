# Positive-definite tridiagonal expert driver

This public consumer requires the optional Reference provider facet and links
only the exported ASC::dense_lapack target. Configure against an installed ASC
package with the separately prepared pinned provider, using the same integer
ABI. Provider/runtime files are not included in this example or the ASC package.

The four scalar examples construct independent ordinary Hermitian or symmetric
systems. A mutable factor descriptor selects FACT=N; a nominal factor view
selects FACT=F. QueryPtsvxWorkspace reports explicit native, layout-conversion
and flat real-estimate storage. Ptsvx solves and reports RCOND/FERR/BERR, using
independent padded B and X layouts. Old X and estimate values are not inputs.
The example reuses borrowed lower factors and an explicit owned upper copy,
which conjugates complex off-diagonal factors for the same original matrix.

The implementation returns the provider's raw diagnostics and qualified INFO.
These are estimated quantities, and finite ordinary examples do not establish
acceptance at extreme values. The required extreme-value gates remain failed
and are retained separately in the programme numerical-disposition records.
