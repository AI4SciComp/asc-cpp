# QR-compressed trajectory analysis

This installed public consumer selects the optional pinned Reference provider.
It checks S/D/C/Z trajectory eigenpairs, POD/refinement modes, diagnostics,
independent layouts, minimum/preferred work and immutable plan reuse. Returned
packed QR factors and tau are reused through public ORMQR/UNMQR declarations.
Both explicit-Q and packed-QR output paths are exercised.

The separately required SGEDMDQ JOBZ=Q mathematical failure is retained in the
maintained required-math tests; this ordinary example uses explicit and POD-
factored modes and does not establish full numerical or platform acceptance.

Configure this directory against a relocated ASCCpp installation containing
`dense_lapack`, explicitly supplying the admitted `ASC_CPP_LAPACK_ROOT` and
`ASC_CPP_LAPACK_RUNTIME_LIBRARIES`. Link only `ASC::dense_lapack`. No source-tree
include path or historical evidence directory is part of this consumer.
