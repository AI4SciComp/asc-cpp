# ADR 0024: complex Dense lifecycle and typed LAPACK foundations

Status: Accepted implementation direction under the user-approved
ASC-CPP-LAPACK-IO runbook, 2026-09-07.

Dense storage additionally permits the standard float/double complex
specializations, with actual C++20 copy/destruction traits checked.
Storage eligibility is distinct from each algorithm's scalar constraints.
Real BLAS overloads and Random filling/preparation keep their existing
numerical behavior. Ordered reductions remain unavailable for complex values.

Complex objects in host raw storage must have their lifetimes started by
typed construction; byte-zeroing does not establish this requirement.
Construction of device/managed complex objects is unsupported until an
explicit valid provider lifecycle route exists. Reject unsupported creation
before allocation or host dereference. Existing arithmetic creation remains
unchanged. Complex cloning explicitly copies already live objects and
retains the existing synchronous resource/context contract. Public factories
document any initialization done even by an uninitialized complex factory.

LAPACK descriptors use checked ASC 64-bit dimensions, backing spans, placement,
alignment, unique writable mappings and explicit borrowed lifetimes.
Reuse compatible BLAS descriptors; band-factor, RFP, tridiagonal and
family-tagged factor representations remain distinct.

Workspace plans identify routine, actual scalar signature, dimensions,
options, provider/build and integer ABI. Separate scalar/real/integer/logical,
pivot/layout/complex-conversion capacities are caller-owned. Capacity
arithmetic, query rounding, aliasing and stale identity are validated before
mutation. No hidden pack/allocation/transfer or precision change is permitted.

Reports are initialized before validation and remain accessible on failure.
A non-called provider has no fabricated INFO. Raw signed INFO and routine-
specific outcomes/usable partial outputs are preserved. Signed one-based raw
pivot encodings are family/variant tagged, with separately named conversions
where mathematically meaningful; block pivots are not ordinary permutations.

Verification follows P01 lifecycle, negative compile, overflow/alias,
workspace, factor metadata, report and LP64/ILP64 obligations. New public
declarations need useful Doxygen and self-contained/no-exception tests.
