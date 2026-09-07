# ADR 0023: explicitly selected Dense LAPACK provider facet

Status: Accepted implementation direction under the user-approved
ASC-CPP-LAPACK-IO runbook, 2026-09-07. Provider redistribution approval is
separate and has not been asserted.

Dense owns the optional `dense_lapack` component and `ASC::dense_lapack`
target. It is a facet of Dense; the six modules and their forbidden edges
remain unchanged. Provider-free `ASC::dense` owns native implementations and
neutral LAPACK vocabulary. `ASC::cpp` stays provider-free.

Use explicit provider overloads. Native calls take the existing execution
context and cannot fall through to an external provider. Reference calls
require a provider object constructed by the optional facet. Foreign handles,
integer/logical representations, callbacks and SDK headers stay private.
Do not use link-order discovery, global registration or mutable selection.

Provider-only headers are absent from common umbrellas and unavailable
provider installations. Ordinary consumers never download dependencies.
Configuration defaults to LAPACK disabled; enabled configurations require an
explicit prepared prefix, exact source/build identity and integer ABI.
Full-profile configuration must fail on missing required entries.

Component manifests, actual CMake targets, install/export closures, header
owners and their validators must change together when this facet is added.
These planned names do not assert that a target exists yet.

Verification includes provider-free configurations with LAPACK discovery
disabled, requested-unavailable component rejection, C++-only installed
provider consumers, relocated prefixes, static/shared isolation, exact link
dependencies and no accidental alternate BLAS/LAPACK resolution.
