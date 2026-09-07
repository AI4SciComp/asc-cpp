# ADR 0026: private verified provider ABI and exact provenance

Status: Accepted implementation direction under the user-approved
ASC-CPP-LAPACK-IO runbook, 2026-09-07. Exact import/redistribution materials
remain subject to ADR 0017 reviewer and notice-inventory owner approval.

Prepare the pinned Reference-LAPACK externally. Preserve original code and
notices separately from ASC-authored code and record source/tag/tree/archive,
license, build options, patches, compiler/runtime and provider binary hashes.
The unsigned upstream tag is not cryptographic authentication. Its source
commit is authoritative despite the upstream CMake patch-version discrepancy.

Prefer audited column-major LAPACKE work interfaces. Missing routes require
private verified interoperability, preferably provider-built bind(C) shims.
Never guess Fortran mangling, hidden character lengths, logical widths,
complex returns or callback signatures. No std::complex aliasing assumption
is justified merely by size equality. Conversion uses explicit caller space.

LP64 and true ILP64 need separately built and tested complete ABI closures,
including BLAS, runtime libraries and headers. A cast test does not verify an
ILP64 provider. Callback bridges must be reentrant and avoid hidden global or
thread-local capture. Rejected ASC metadata must return before a foreign
error handler can print or terminate; no process-wide XERBLA override.

Strict allocation claims require actual foreign-path instrumentation.
Provider-internal allocations are audited independently of ASC allocations.
Required extra-precision paths retain their source-defined accuracy/dependency
requirements; missing XBLAS does not remove them from the full denominator.

Verification includes exact symbols, wrong-width sentinels, callbacks,
negative-call preflight, workspace queries, concurrent calls, link isolation,
strict allocation probes and per-routine numerical/INFO semantics. Local
source preparation is not redistribution or approval evidence.
