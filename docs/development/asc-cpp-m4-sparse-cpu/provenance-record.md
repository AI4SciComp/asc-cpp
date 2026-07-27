# Milestone 4 Provenance Record

Status: Complete clean-room review; boundary preserved

Date: 2026-07-26

## Project ownership

Milestone 4 production source, tests, examples, benchmarks, and documentation
are new project-owned work derived from the approved architecture contracts
and standard mathematical definitions.

No source, test, literal vector, table, prose, or mechanical translation may
be copied from MdeCpp, the user-deleted asc-cpp implementation, a third-party
sparse library, or a provider SDK.

## Bound repositories

```text
asc-cpp
remote: git@github.com:AI4SciComp/asc-cpp.git
baseline: 33b261ea33616a6395c4ad3b20646093103344f7

asc-cmake
remote: git@github.com:AI4SciComp/asc-cmake.git
commit: 8a7dcbad3a97267cce59810aff24de800a3497a7
annotated tag object: 620b2e912ac5bac7561e09529a65cce965ebc920
tag: v0.1.0

MdeCpp comparison checkout
path: /home/yicai/repo/MdeRepo/MdeCpp
remote: git@github.com:escapetiger/MdeCpp.git
commit: f6294e9079262682ce63ae7ff2d8a643e658bf5d
retained unrelated modification: Makefile
```

## Mathematical sources

Canonical coordinate ordering, compressed sparse row/column invariants,
prefix-offset construction, coordinate/compressed conversion, and CSR SpMV
use elementary project-owned definitions from the frozen ADRs. Independent
tests derive small expected structures and numerical results directly.

## Dependencies and providers

No third-party dependency or provider adapter is approved. Locally detected
BLAS/LAPACK, oneMKL, and cuSPARSE are inventory only and must not be included,
linked, configured, invoked, or claimed as Milestone 4 evidence.

## License

The existing asc-cpp Apache-2.0 `LICENSE` is unchanged:

```text
SHA-256:
c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4
```

It matches asc-cmake. MdeCpp's distinct license hash is:

```text
230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809
```

MdeCpp remains behavior/provenance evidence only.

## Final review

The production, verification, documentation, portability, benchmark, consumer,
and package artifacts were reviewed against this boundary. Repository scans
found no MdeCpp include, namespace, copied notice, source/test fragment,
numerical table, provider SDK material, or new third-party dependency in the
Milestone 4 files. The Apache-2.0 license file and both recorded hashes remain
unchanged.
