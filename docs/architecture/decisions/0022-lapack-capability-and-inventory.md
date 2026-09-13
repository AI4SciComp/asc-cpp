# ADR 0022: source-defined LAPACK capability and separate native evidence

Status: Accepted implementation direction under the user-approved
ASC-CPP-LAPACK-IO runbook, 2026-09-07. Inventory and implementation verification
are separate gates; no release or external reviewer approval is asserted.

The required reference CPU profile uses Reference-LAPACK 3.12.1 source commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, including actual public driver,
computational, expert auxiliary, specialized, mixed-precision, optional
extra-precision and compatibility capabilities. Fortran source declarations,
build inputs and documentation define the denominator. LAPACKE and exports
are cross-checks. There is no assumed four-precision Cartesian product.

The generated inventory preserves source occurrences, their exact bytes,
signatures, build conditions, documentation and classification evidence.
Exclusions require a source-based reason; missing bindings and dependencies
never justify an exclusion. Ambiguous classifications fail denominator closure.

ASC contract mapping and execution evidence are separate inputs. Validators
reject missing rows, duplicate IDs, altered identities, unsupported exclusions,
false verification, skipped/zero tests and missing mode/test obligations.
Incremental reports explicitly remain incomplete. Only strict reference
closure can support a complete reference-capability claim.

Native LU, Cholesky and Householder QR are independent required work.
Reference availability is never proof of native implementation, and a source
declaration is never proof of a checked ASC call. No BLAS or Random numerical
or reproducibility rule changes through this decision.

Verification: deterministic regeneration, adversarial validator tests,
source/ABI probes and per-routine executed mathematical/failure tests required
by the complete ASC-CPP-LAPACK-IO program contract. Its durable execution
records are development-only and excluded from release source archives.
