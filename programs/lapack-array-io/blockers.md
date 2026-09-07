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
