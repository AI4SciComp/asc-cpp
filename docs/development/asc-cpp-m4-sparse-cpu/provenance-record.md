# Milestone 4 Sparse CPU provenance record

Status: Frozen clean-room boundary

Date: 2026-07-28

Milestone 4 production, tests, benchmarks, examples, and documentation are new
project-owned work derived from the approved architecture contracts and
elementary sparse-matrix definitions.

No source, test, literal vector, table, prose, or mechanical translation may
be copied from MdeCpp, the deleted asc-cpp implementation, a third-party
sparse library, a provider SDK, or the completed later-milestone branch.

```text
asc-cpp baseline:
33b261ea33616a6395c4ad3b20646093103344f7

asc-cmake v0.1.0:
8a7dcbad3a97267cce59810aff24de800a3497a7
annotated tag object:
620b2e912ac5bac7561e09529a65cce965ebc920

MdeCpp comparison checkout:
f6294e9079262682ce63ae7ff2d8a643e658bf5d
retained unrelated modification: Makefile
```

Canonical coordinate ordering, CSR/CSC invariants, prefix-offset
construction, explicit conversions, and CSR SpMV use elementary definitions
from the frozen contract. Independent tests derive their own small structures
and numerical results.

No third-party dependency or provider is approved. Detected BLAS/LAPACK,
oneMKL, and cuSPARSE remain inventory only and are not configured, included,
linked, invoked, or claimed.

ASCCpp remains Apache-2.0. The retained license SHA-256 is
`c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4`.
MdeCpp remains behavior/provenance evidence only under its distinct recorded
license.
